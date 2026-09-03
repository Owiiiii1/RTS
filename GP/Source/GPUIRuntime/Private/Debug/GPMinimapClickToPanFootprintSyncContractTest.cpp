// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/GPCameraPawn.h"
#include "Engine/World.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "ViewModels/GPMinimapPresenter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMinimapClickToPanFootprintSyncContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPMinimapClickToPanFootprintSyncContractPrivate
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

	static FVector2D PolygonCentroid(const TArray<FVector2D>& Corners)
	{
		FVector2D Sum = FVector2D::ZeroVector;
		if (Corners.Num() == 0)
		{
			return Sum;
		}

		for (const FVector2D& Corner : Corners)
		{
			Sum += Corner;
		}
		return Sum / static_cast<float>(Corners.Num());
	}

	static void RunMinimapClickToPanFootprintSyncContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMinimapClickToPanFootprintSyncContract, Warning,
				TEXT("gp.UI.RunMinimapClickToPanFootprintSyncContractTest: missing world or client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPMinimapClickToPanFootprintSyncContract, Log,
					TEXT("gp.UI.RunMinimapClickToPanFootprintSyncContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPMinimapClickToPanFootprintSyncContract, Error,
					TEXT("gp.UI.RunMinimapClickToPanFootprintSyncContractTest FAIL: %s"), Label);
			}
		};

		AGP_PlayerController* PlayerController =
			Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		AGP_CameraPawn* CameraPawn =
			PlayerController != nullptr ? Cast<AGP_CameraPawn>(PlayerController->GetPawn()) : nullptr;
		Expect(PlayerController != nullptr && CameraPawn != nullptr,
			TEXT("A0_LocalCameraPawnPresent"));

		UGP_MinimapPresenter* Presenter = NewObject<UGP_MinimapPresenter>(GetTransientPackage());
		UGP_LocalFoWComponent* Mirror = NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		Expect(Presenter != nullptr && Mirror != nullptr
			&& Presenter->InitializeWithMirror(Mirror),
			TEXT("A1_PresenterMirrorReady"));

		FGP_FoWPresentationUpdate Initial = MakeInitial(1, 1);
		if (Mirror != nullptr)
		{
			Mirror->ApplyServerUpdate(Initial);
		}
		if (Presenter != nullptr && PlayerController != nullptr)
		{
			Presenter->Initialize(PlayerController);
		}
		if (CameraPawn != nullptr)
		{
			CameraPawn->Tick(0.0f);
		}
		if (Presenter != nullptr && CameraPawn != nullptr)
		{
			FBox ResolvedBounds(ForceInit);
			if (CameraPawn->GetResolvedCameraBounds(ResolvedBounds))
			{
				Presenter->ContractBindCameraPawn(CameraPawn);
				Presenter->ContractApplyDisplayedWorldBounds(ResolvedBounds);
			}
		}

		int32 ViewportX = 0;
		int32 ViewportY = 0;
		if (PlayerController != nullptr)
		{
			PlayerController->GetViewportSize(ViewportX, ViewportY);
		}
		const bool bRealViewport = ViewportX >= 2 && ViewportY >= 2;
		if (Presenter != nullptr && !bRealViewport)
		{
			Presenter->ContractSetViewportSizeOverride(1280, 720);
		}
		if (Presenter != nullptr)
		{
			Presenter->ContractRebuildCameraFootprint();
		}

		const FGP_MinimapCameraFootprint FootprintA =
			Presenter != nullptr ? Presenter->GetCameraFootprint() : FGP_MinimapCameraFootprint();
		Expect(FootprintA.bIsValid && FootprintA.NormalizedCorners.Num() >= 3,
			TEXT("A2_InitialFootprintValid"));

		const FVector LocationA =
			CameraPawn != nullptr ? CameraPawn->GetActorLocation() : FVector::ZeroVector;
		FVector ViewA = FVector::ZeroVector;
		FRotator RotA = FRotator::ZeroRotator;
		float FovA = 0.0f;
		const bool bViewA = CameraPawn != nullptr
			&& CameraPawn->GetPresentationView(ViewA, RotA, FovA);
		const FVector2D CentroidA = PolygonCentroid(FootprintA.NormalizedCorners);
		const int32 RevisionA = FootprintA.Revision;
		TArray<FVector2D> UnclampedA;
		const bool bUnclampedA = Presenter != nullptr
			&& Presenter->ContractTryGetUnclampedNormalizedCorners(UnclampedA);

		FBox CameraBounds(ForceInit);
		const bool bHasBounds =
			CameraPawn != nullptr && CameraPawn->GetResolvedCameraBounds(CameraBounds);
		FVector LocationB = LocationA + FVector(450.0f, -320.0f, 0.0f);
		if (bHasBounds)
		{
			LocationB.X = FMath::Clamp(LocationB.X, CameraBounds.Min.X, CameraBounds.Max.X);
			LocationB.Y = FMath::Clamp(LocationB.Y, CameraBounds.Min.Y, CameraBounds.Max.Y);
		}
		if (FVector2D(LocationB.X - LocationA.X, LocationB.Y - LocationA.Y).Size() < 50.0f
			&& bHasBounds)
		{
			LocationB.X = FMath::Clamp(LocationA.X - 450.0f, CameraBounds.Min.X, CameraBounds.Max.X);
			LocationB.Y = FMath::Clamp(LocationA.Y + 320.0f, CameraBounds.Min.Y, CameraBounds.Max.Y);
		}

		const bool bPanned = CameraPawn != nullptr
			&& CameraPawn->SetCameraAnchorWorldXY(FVector2D(LocationB.X, LocationB.Y));

		const FVector LocationAfter =
			CameraPawn != nullptr ? CameraPawn->GetActorLocation() : FVector::ZeroVector;
		FVector ViewB = FVector::ZeroVector;
		FRotator RotB = FRotator::ZeroRotator;
		float FovB = 0.0f;
		const bool bViewB = CameraPawn != nullptr
			&& CameraPawn->GetPresentationView(ViewB, RotB, FovB);

		const FGP_MinimapCameraFootprint FootprintB =
			Presenter != nullptr ? Presenter->GetCameraFootprint() : FGP_MinimapCameraFootprint();
		const FVector2D CentroidB = PolygonCentroid(FootprintB.NormalizedCorners);
		TArray<FVector2D> ImmediateUnclamped;
		const bool bImmediateCorners = Presenter != nullptr
			&& Presenter->ContractTryGetUnclampedNormalizedCorners(ImmediateUnclamped);

		const FVector WorldCentroidA = Presenter != nullptr
			? Presenter->MinimapNormalizedToWorld(CentroidA, LocationA.Z)
			: FVector::ZeroVector;
		const FVector WorldCentroidB = Presenter != nullptr
			? Presenter->MinimapNormalizedToWorld(CentroidB, LocationAfter.Z)
			: FVector::ZeroVector;
		const FVector2D AnchorDelta(LocationAfter.X - LocationA.X, LocationAfter.Y - LocationA.Y);
		const FVector2D FootprintWorldDelta(WorldCentroidB.X - WorldCentroidA.X, WorldCentroidB.Y - WorldCentroidA.Y);
		const FVector2D ViewDelta(ViewB.X - ViewA.X, ViewB.Y - ViewA.Y);

		UE_LOG(LogGPMinimapClickToPanFootprintSyncContract, Log,
			TEXT("Sync debug RealViewport=%d Panned=%d Rev %d->%d AnchorDelta=(%.1f,%.1f) FootprintDelta=(%.1f,%.1f) ViewDelta=(%.1f,%.1f)"),
			bRealViewport ? 1 : 0,
			bPanned ? 1 : 0,
			RevisionA,
			FootprintB.Revision,
			AnchorDelta.X, AnchorDelta.Y,
			FootprintWorldDelta.X, FootprintWorldDelta.Y,
			ViewDelta.X, ViewDelta.Y);

		Expect(bPanned
			&& FVector2D(LocationAfter.X, LocationAfter.Y).Equals(
				FVector2D(LocationB.X, LocationB.Y), 1.0f)
			&& FMath::IsNearlyEqual(LocationAfter.Z, LocationA.Z, 0.1f),
			TEXT("B_AnchorMovedWithoutTick"));
		Expect(bViewA && bViewB
			&& ViewDelta.Size() > 25.0f
			&& FMath::IsNearlyEqual(FovB, FovA, 0.01f)
			&& RotB.Equals(RotA, 0.01f),
			TEXT("B_PresentationViewMovedPreservingYawZoom"));
		Expect(Presenter != nullptr
			&& Presenter->GetBoundCameraPresentationDelegateCount() == 1
			&& FootprintB.bIsValid
			&& FootprintB.NormalizedCorners.Num() >= 3
			&& FootprintB.Revision > RevisionA,
			TEXT("C_SynchronousPresentationRebuildsFootprint"));
		Expect(AnchorDelta.Size() > 25.0f
			&& FootprintWorldDelta.Size() > 25.0f
			&& (FootprintWorldDelta.X * AnchorDelta.X + FootprintWorldDelta.Y * AnchorDelta.Y) > 0.0f,
			TEXT("C_FootprintTranslatesWithAnchorWithoutSecondInput"));
		Expect(bImmediateCorners
			&& bUnclampedA
			&& ImmediateUnclamped.Num() >= 3
			&& UnclampedA.Num() >= 3
			&& !PolygonCentroid(ImmediateUnclamped).Equals(PolygonCentroid(UnclampedA), 0.002f),
			TEXT("C_CurrentCameraViewCornersMovedWithoutControllerRefresh"));

		if (CameraPawn != nullptr)
		{
			CameraPawn->SetCameraAnchorWorldXY(FVector2D(LocationA.X, LocationA.Y));
		}
		if (Presenter != nullptr)
		{
			Presenter->Shutdown();
		}

		UE_LOG(LogGPMinimapClickToPanFootprintSyncContract, Log,
			TEXT("gp.UI.RunMinimapClickToPanFootprintSyncContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMinimapClickToPanFootprintSyncContract(
		TEXT("gp.UI.RunMinimapClickToPanFootprintSyncContractTest"),
		TEXT("Click-to-pan must move the minimap camera footprint in the same event, without a later Tick."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMinimapClickToPanFootprintSyncContractTest));
}

#endif
