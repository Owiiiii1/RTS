// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/GPCameraBoundsVolume.h"
#include "Camera/GPCameraConfigDataAsset.h"
#include "Camera/GPCameraPawn.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/IConsoleManager.h"
#include "ViewModels/GPMinimapPresenter.h"
#include "Widgets/GPMinimapWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMinimapCameraBoundsContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPMinimapCameraBoundsContractPrivate
{
	static FGP_FoWPresentationUpdate MakeInitial(int32 TeamId, int64 Revision)
	{
		FGP_FoWPresentationUpdate Update;
		Update.bInitialSnapshot = true;
		Update.TeamId = TeamId;
		Update.Revision = Revision;
		Update.GridOriginWorldXY = FVector2D::ZeroVector;
		Update.GridDimensions = FIntPoint(4, 4);
		Update.CellSizeCm = 100.0f;
		return Update;
	}

	static FGP_FoWCellRange Range(int32 Start, int32 Count)
	{
		FGP_FoWCellRange Result;
		Result.StartIndex = Start;
		Result.NumCells = Count;
		return Result;
	}

	static bool NearlyEqual2D(const FVector2D& A, const FVector2D& B)
	{
		return FMath::IsNearlyEqual(A.X, B.X, 0.01f)
			&& FMath::IsNearlyEqual(A.Y, B.Y, 0.01f);
	}

	static bool NearlyEqualXY(const FVector& A, const FVector& B, float ToleranceCm)
	{
		return FMath::IsNearlyEqual(A.X, B.X, ToleranceCm)
			&& FMath::IsNearlyEqual(A.Y, B.Y, ToleranceCm);
	}

	static bool PresentationMatchesBoxXY(const FGP_MinimapPresentation& Presentation, const FBox& Bounds)
	{
		return NearlyEqual2D(Presentation.MapWorldMin, FVector2D(Bounds.Min.X, Bounds.Min.Y))
			&& NearlyEqual2D(
				Presentation.MapWorldSizeCm,
				FVector2D(Bounds.Max.X - Bounds.Min.X, Bounds.Max.Y - Bounds.Min.Y));
	}

	static bool MappingIsFinite(const UGP_MinimapPresenter* Presenter)
	{
		if (Presenter == nullptr)
		{
			return false;
		}

		const FVector2D Normalized = Presenter->WorldToMinimapNormalized(FVector(10.0f, 20.0f, 0.0f));
		const FVector World = Presenter->MinimapNormalizedToWorld(FVector2D(0.5f, 0.5f), 0.0f);
		return FMath::IsFinite(Normalized.X) && FMath::IsFinite(Normalized.Y)
			&& FMath::IsFinite(World.X) && FMath::IsFinite(World.Y);
	}

	static void RunMinimapCameraBoundsContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMinimapCameraBoundsContract, Warning,
				TEXT("gp.UI.RunMinimapCameraBoundsContractTest: missing world or client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPMinimapCameraBoundsContract, Log,
					TEXT("gp.UI.RunMinimapCameraBoundsContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPMinimapCameraBoundsContract, Error,
					TEXT("gp.UI.RunMinimapCameraBoundsContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_MinimapPresenter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_MinimapPresenter::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) == nullptr
			&& UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) == nullptr
			&& UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("NativeTick")) == nullptr,
			TEXT("A_NoTickOrPolling"));

		UGP_MinimapPresenter* Presenter = NewObject<UGP_MinimapPresenter>(GetTransientPackage());
		UGP_LocalFoWComponent* Mirror = NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		Expect(Presenter != nullptr && Mirror != nullptr
			&& Presenter->InitializeWithMirror(Mirror)
			&& Presenter->GetBoundDelegateCount() == 1
			&& Presenter->GetBoundCameraBoundsDelegateCount() == 0,
			TEXT("B_MirrorBoundWithoutWorldScan"));

		FGP_FoWPresentationUpdate Initial = MakeInitial(1, 1);
		Initial.ExploredRanges.Add(Range(0, 2));
		Initial.VisibleRanges.Add(Range(1, 1));
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(Initial), TEXT("C_FoWSnapshotAccepted"));
		Expect(Presenter != nullptr
			&& Presenter->IsMinimapReady()
			&& Presenter->GetMinimapPresentation().WorldSizeCm.Equals(FVector2D(400.0f, 400.0f))
			&& Presenter->GetMinimapPresentation().MapWorldMin.IsZero()
			&& Presenter->GetMinimapPresentation().MapWorldSizeCm.Equals(FVector2D(400.0f, 400.0f)),
			TEXT("D_InvalidCameraFallsBackToFoWGrid"));

		const FBox Cropped(
			FVector(50.0f, 80.0f, -10.0f),
			FVector(250.0f, 280.0f, 10.0f));
		if (Presenter != nullptr)
		{
			Presenter->ContractApplyDisplayedWorldBounds(Cropped);
		}
		Expect(Presenter != nullptr
			&& PresentationMatchesBoxXY(Presenter->GetMinimapPresentation(), Cropped)
			&& Presenter->GetMinimapPresentation().WorldSizeCm.Equals(FVector2D(400.0f, 400.0f))
			&& Presenter->GetMinimapPresentation().GridDimensions == FIntPoint(4, 4),
			TEXT("E_DisplayedBoundsSmallerThanFoWGrid"));

		Expect(Presenter != nullptr
			&& NearlyEqual2D(
				Presenter->WorldToMinimapNormalized(FVector(50.0f, 80.0f, 0.0f)),
				FVector2D(0.0f, 0.0f))
			&& NearlyEqual2D(
				Presenter->WorldToMinimapNormalized(FVector(250.0f, 280.0f, 0.0f)),
				FVector2D(1.0f, 1.0f))
			&& NearlyEqual2D(
				Presenter->WorldToMinimapNormalized(FVector(150.0f, 180.0f, 0.0f)),
				FVector2D(0.5f, 0.5f))
			&& NearlyEqual2D(
				Presenter->WorldToMinimapNormalized(FVector(-1000.0f, 5000.0f, 0.0f)),
				FVector2D(0.0f, 1.0f)),
			TEXT("F_WorldToNormalizedUsesDisplayedNotFoWGrid"));

		const FVector CenterWorld = Presenter != nullptr
			? Presenter->MinimapNormalizedToWorld(FVector2D(0.5f, 0.5f), 9.0f)
			: FVector::ZeroVector;
		Expect(Presenter != nullptr
			&& NearlyEqualXY(
				Presenter->MinimapNormalizedToWorld(FVector2D(0.0f, 0.0f), 0.0f),
				FVector(50.0f, 80.0f, 0.0f),
				0.1f)
			&& NearlyEqualXY(
				Presenter->MinimapNormalizedToWorld(FVector2D(1.0f, 1.0f), 0.0f),
				FVector(250.0f, 280.0f, 0.0f),
				0.1f)
			&& NearlyEqualXY(CenterWorld, FVector(150.0f, 180.0f, 9.0f), 0.1f)
			&& NearlyEqual2D(
				Presenter->WorldToMinimapNormalized(CenterWorld),
				FVector2D(0.5f, 0.5f)),
			TEXT("G_NormalizedCornersAndCenterRoundTrip"));

		const FVector FoWQueryWorld = Presenter != nullptr
			? Presenter->MinimapNormalizedToWorld(FVector2D(0.5f, 0.0f), 0.0f)
			: FVector::ZeroVector;
		Expect(Presenter != nullptr && Mirror != nullptr
			&& NearlyEqualXY(FoWQueryWorld, FVector(150.0f, 80.0f, 0.0f), 0.1f)
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(0.5f, 0.0f))
				== Mirror->GetStateAtWorldLocation(FoWQueryWorld)
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(0.5f, 0.0f))
				== EGP_FoWState::Visible,
			TEXT("H_FoWNormalizedQueryMapsThroughDisplayedBounds"));

		if (Presenter != nullptr)
		{
			Presenter->ContractApplyDisplayedWorldBounds(FBox(FVector::ZeroVector, FVector::ZeroVector));
		}
		Expect(Presenter != nullptr
			&& Presenter->GetMinimapPresentation().MapWorldMin.IsZero()
			&& Presenter->GetMinimapPresentation().MapWorldSizeCm.Equals(FVector2D(400.0f, 400.0f))
			&& MappingIsFinite(Presenter),
			TEXT("I_DegenerateBoundsFallBackSafely"));

		if (Presenter != nullptr)
		{
			Presenter->ContractClearDisplayedWorldBounds();
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGP_CameraPawn* CameraPawn = World->SpawnActor<AGP_CameraPawn>(
			FVector(0.0f, 0.0f, 100.0f),
			FRotator::ZeroRotator,
			SpawnParams);
		Expect(CameraPawn != nullptr, TEXT("J_CameraPawnSpawned"));
		if (CameraPawn != nullptr)
		{
			CameraPawn->ContractClearCameraBoundsVolume();
		}

		const UGP_CameraConfigDataAsset* ConfigCDO = GetDefault<UGP_CameraConfigDataAsset>();
		FBox FallbackBounds(ForceInit);
		const bool bHaveFallback =
			CameraPawn != nullptr && CameraPawn->GetResolvedCameraBounds(FallbackBounds);
		Expect(bHaveFallback
			&& ConfigCDO != nullptr
			&& FallbackBounds.Min.Equals(ConfigCDO->FallbackBounds.Min, 0.01f)
			&& FallbackBounds.Max.Equals(ConfigCDO->FallbackBounds.Max, 0.01f),
			TEXT("K_NoVolumeUsesExactConfigFallbackBounds"));

		if (Presenter != nullptr)
		{
			Presenter->ContractBindCameraPawn(CameraPawn);
			Presenter->ContractBindCameraPawn(CameraPawn);
		}
		Expect(Presenter != nullptr
			&& Presenter->GetBoundCameraBoundsDelegateCount() == 1
			&& bHaveFallback
			&& PresentationMatchesBoxXY(Presenter->GetMinimapPresentation(), FallbackBounds)
			&& !Presenter->GetMinimapPresentation().MapWorldSizeCm.Equals(
				Presenter->GetMinimapPresentation().WorldSizeCm),
			TEXT("L_RebindDoesNotDuplicateCameraDelegates"));

		AGP_CameraBoundsVolume* Volume = World->SpawnActor<AGP_CameraBoundsVolume>(
			FVector(2500.0f, 3500.0f, 0.0f),
			FRotator::ZeroRotator,
			SpawnParams);
		if (Volume != nullptr)
		{
			Volume->SetActorScale3D(FVector(0.02f, 0.03f, 1.0f));
		}
		Expect(Volume != nullptr, TEXT("M_DistinctVolumeSpawned"));
		if (CameraPawn != nullptr && Volume != nullptr)
		{
			CameraPawn->ContractSetCameraBoundsVolume(Volume);
		}

		FBox VolumeBounds(ForceInit);
		FBox ResolvedAfterVolume(ForceInit);
		const bool bVolumeResolved =
			Volume != nullptr
			&& CameraPawn != nullptr
			&& CameraPawn->GetResolvedCameraBounds(ResolvedAfterVolume);
		if (Volume != nullptr)
		{
			VolumeBounds = Volume->GetCameraBounds();
		}
		Expect(bVolumeResolved
			&& Presenter != nullptr
			&& ResolvedAfterVolume.Min.Equals(VolumeBounds.Min, 0.01f)
			&& ResolvedAfterVolume.Max.Equals(VolumeBounds.Max, 0.01f)
			&& PresentationMatchesBoxXY(Presenter->GetMinimapPresentation(), VolumeBounds)
			&& Presenter->GetMinimapPresentation().WorldSizeCm.Equals(FVector2D(400.0f, 400.0f))
			&& NearlyEqual2D(
				Presenter->WorldToMinimapNormalized(VolumeBounds.Min),
				FVector2D(0.0f, 0.0f))
			&& NearlyEqual2D(
				Presenter->WorldToMinimapNormalized(VolumeBounds.Max),
				FVector2D(1.0f, 1.0f)),
			TEXT("N_ValidVolumeBecomesDisplayedBounds"));

		Expect(UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(FVector2D(0.25f, 1.0f))
				.Equals(FVector2D(0.25f, 0.0f), 0.0001f)
			&& UGP_MinimapWidget::SurfaceUVToPresenterNormalized(FVector2D(0.25f, 0.0f))
				.Equals(FVector2D(0.25f, 1.0f), 0.0001f)
			&& UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(FVector2D(0.5f, 0.0f))
				.Equals(FVector2D(0.5f, 1.0f), 0.0001f),
			TEXT("O_BackgroundAndFoWShareOrientation"));

		if (Presenter != nullptr)
		{
			Presenter->Shutdown();
		}
		Expect(Presenter != nullptr
			&& Presenter->GetBoundDelegateCount() == 0
			&& Presenter->GetBoundCameraBoundsDelegateCount() == 0
			&& !Presenter->IsMinimapReady(),
			TEXT("P_ShutdownUnbindsCameraAndMirror"));

		if (Volume != nullptr)
		{
			Volume->Destroy();
		}
		if (CameraPawn != nullptr)
		{
			CameraPawn->Destroy();
		}

		UE_LOG(LogGPMinimapCameraBoundsContract, Log,
			TEXT("gp.UI.RunMinimapCameraBoundsContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMinimapCameraBoundsContract(
		TEXT("gp.UI.RunMinimapCameraBoundsContractTest"),
		TEXT("Run native minimap camera/playable-bounds mapping contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMinimapCameraBoundsContractTest));
}

#endif
