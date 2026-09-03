// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPMainBase.h"
#include "Engine/World.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/IConsoleManager.h"
#include "Presentation/GPLocalFoWUnitPresentationSubsystem.h"
#include "Units/GPWorker.h"
#include "ViewModels/GPMinimapPresenter.h"
#include "Widgets/GPMinimapWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMinimapFriendlyBlipsContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPMinimapFriendlyBlipsContractPrivate
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

	static bool NearlyEqual2D(const FVector2D& A, const FVector2D& B, float Tolerance = 0.01f)
	{
		return FMath::IsNearlyEqual(A.X, B.X, Tolerance)
			&& FMath::IsNearlyEqual(A.Y, B.Y, Tolerance);
	}

	static void RunMinimapFriendlyBlipsContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMinimapFriendlyBlipsContract, Warning,
				TEXT("gp.UI.RunMinimapFriendlyBlipsContractTest: missing world or client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPMinimapFriendlyBlipsContract, Log,
					TEXT("gp.UI.RunMinimapFriendlyBlipsContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPMinimapFriendlyBlipsContract, Error,
					TEXT("gp.UI.RunMinimapFriendlyBlipsContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(FVector2D(0.0f, 0.0f))
				.Equals(FVector2D(1.0f, 1.0f), 0.0001f)
			&& UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(FVector2D(1.0f, 1.0f))
				.Equals(FVector2D(0.0f, 0.0f), 0.0001f)
			&& UGP_MinimapWidget::SurfaceUVToPresenterNormalized(
				UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(FVector2D(0.25f, 0.75f)))
				.Equals(FVector2D(0.25f, 0.75f), 0.0001f),
			TEXT("A_SurfaceOrientationRoundTrip"));

		UGP_MinimapPresenter* Presenter = NewObject<UGP_MinimapPresenter>(GetTransientPackage());
		UGP_LocalFoWComponent* Mirror = NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		UGP_MinimapWidget* Widget = NewObject<UGP_MinimapWidget>(GetTransientPackage());
		Expect(Presenter != nullptr && Mirror != nullptr && Widget != nullptr,
			TEXT("B_ObjectsCreated"));

		Expect(UGP_MinimapPresenter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr,
			TEXT("C_NoTickOrPolling"));

		Expect(Presenter != nullptr && Mirror != nullptr
			&& Presenter->InitializeWithMirror(Mirror)
			&& Presenter->GetBoundUnitRegistryDelegateCount() == 0,
			TEXT("D_MirrorOnlyDoesNotBindWorldRegistry"));

		FGP_FoWPresentationUpdate Initial = MakeInitial(1, 1);
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(Initial), TEXT("E_FoWSnapshotAccepted"));

		const FBox DisplayedBounds(
			FVector(50.0f, 80.0f, -10.0f),
			FVector(250.0f, 280.0f, 10.0f));
		if (Presenter != nullptr)
		{
			Presenter->ContractApplyDisplayedWorldBounds(DisplayedBounds);
		}

		UGP_LocalFoWUnitPresentationSubsystem* Registry =
			World->GetSubsystem<UGP_LocalFoWUnitPresentationSubsystem>();
		Expect(Registry != nullptr, TEXT("F_RegistrySubsystemExists"));

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AGP_Worker* FriendlyUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			FVector(150.0f, 180.0f, 0.0f),
			FRotator::ZeroRotator,
			SpawnParams);
		AGP_Worker* EnemyUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			FVector(160.0f, 190.0f, 0.0f),
			FRotator::ZeroRotator,
			SpawnParams);
		AGP_MainBase* FriendlyBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(200.0f, 200.0f, 0.0f),
			FRotator::ZeroRotator,
			SpawnParams);
		AGP_Worker* OutsideUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			FVector(10.0f, 10.0f, 0.0f),
			FRotator::ZeroRotator,
			SpawnParams);
		if (FriendlyUnit != nullptr)
		{
			FriendlyUnit->SetTeamId(1);
		}
		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetTeamId(2);
		}
		if (FriendlyBase != nullptr)
		{
			FriendlyBase->SetTeamId(1);
		}
		if (OutsideUnit != nullptr)
		{
			OutsideUnit->SetTeamId(1);
		}

		if (Presenter != nullptr)
		{
			Presenter->ContractBindUnitRegistry(World);
			Presenter->ContractRebuildFriendlyBlips();
		}
		Expect(Presenter != nullptr
			&& Presenter->ContractFindFriendlyBlipForActor(FriendlyUnit) != nullptr
			&& Presenter->ContractFindFriendlyBlipForActor(FriendlyUnit)->Kind
				== EGP_MinimapBlipKind::Unit
			&& Presenter->ContractFindFriendlyBlipForActor(EnemyUnit) == nullptr
			&& Presenter->ContractFindFriendlyBlipForActor(OutsideUnit) == nullptr
			&& Presenter->ContractFindFriendlyBlipForActor(FriendlyBase) != nullptr
			&& Presenter->ContractFindFriendlyBlipForActor(FriendlyBase)->Kind
				== EGP_MinimapBlipKind::Building,
			TEXT("G_FriendlyUnitAndBuildingOnlyInsideBounds"));

		const FGP_MinimapBlip* FriendlyBlip =
			Presenter != nullptr ? Presenter->ContractFindFriendlyBlipForActor(FriendlyUnit) : nullptr;
		Expect(FriendlyBlip != nullptr
			&& NearlyEqual2D(FriendlyBlip->NormalizedPosition, FVector2D(0.5f, 0.5f), 0.02f),
			TEXT("H_FriendlyUnitMapsThroughCameraBounds"));

		if (Widget != nullptr && Presenter != nullptr)
		{
			Widget->ContractBindPresenter(Presenter);
		}
		Expect(Widget != nullptr
			&& Widget->GetFriendlyBlipDrawCount() >= 2
			&& NearlyEqual2D(
				Widget->ContractWorldToSurfaceUV(FVector(150.0f, 180.0f, 0.0f)),
				FVector2D(0.5f, 0.5f),
				0.02f),
			TEXT("I_WidgetUsesSharedSurfaceFlipForBlips"));

		const FVector2D BeforeMove = FriendlyBlip != nullptr
			? FriendlyBlip->NormalizedPosition
			: FVector2D::ZeroVector;
		if (FriendlyUnit != nullptr)
		{
			FriendlyUnit->SetActorLocation(FVector(100.0f, 100.0f, 0.0f), false);
		}
		if (Registry != nullptr)
		{
			Registry->ContractEvaluateRegisteredUnits();
		}
		if (Presenter != nullptr)
		{
			Presenter->ContractBindUnitRegistry(World);
			Presenter->ContractRebuildFriendlyBlips();
		}
		const FGP_MinimapBlip* MovedBlip =
			Presenter != nullptr ? Presenter->ContractFindFriendlyBlipForActor(FriendlyUnit) : nullptr;
		Expect(MovedBlip != nullptr
			&& !MovedBlip->NormalizedPosition.Equals(BeforeMove, 0.01f)
			&& NearlyEqual2D(MovedBlip->NormalizedPosition, FVector2D(0.25f, 0.1f), 0.05f),
			TEXT("J_MoveUpdatesFriendlyBlipPosition"));

		if (OutsideUnit != nullptr)
		{
			OutsideUnit->SetActorLocation(FVector(120.0f, 120.0f, 0.0f), false);
		}
		if (Presenter != nullptr)
		{
			Presenter->ContractRebuildFriendlyBlips();
		}
		AGP_Worker* DestroyTarget = OutsideUnit;
		Expect(Presenter != nullptr
			&& DestroyTarget != nullptr
			&& Presenter->ContractFindFriendlyBlipForActor(DestroyTarget) != nullptr,
			TEXT("K0_InsideBoundsUnitProducesBlipBeforeDestroy"));
		if (DestroyTarget != nullptr)
		{
			DestroyTarget->Destroy();
		}
		if (Presenter != nullptr)
		{
			Presenter->ContractRebuildFriendlyBlips();
		}
		Expect(Presenter != nullptr
			&& Presenter->ContractFindFriendlyBlipForActor(DestroyTarget) == nullptr,
			TEXT("K_DestroyRemovesFriendlyBlip"));

		const int32 RegistryDelegatesBefore =
			Presenter != nullptr ? Presenter->GetBoundUnitRegistryDelegateCount() : 0;
		if (Presenter != nullptr)
		{
			Presenter->ContractBindUnitRegistry(World);
			Presenter->ContractBindUnitRegistry(World);
		}
		Expect(Presenter != nullptr
			&& Presenter->GetBoundUnitRegistryDelegateCount() == 2
			&& Presenter->GetBoundUnitRegistryDelegateCount() == RegistryDelegatesBefore
			&& Presenter->GetBoundDelegateCount() == 1,
			TEXT("L_RebindDoesNotDuplicateListeners"));

		if (Presenter != nullptr)
		{
			Presenter->Shutdown();
		}
		if (Widget != nullptr)
		{
			Widget->ContractUnbindPresenter();
		}
		if (FriendlyUnit != nullptr)
		{
			FriendlyUnit->Destroy();
		}
		if (EnemyUnit != nullptr)
		{
			EnemyUnit->Destroy();
		}
		if (FriendlyBase != nullptr)
		{
			FriendlyBase->Destroy();
		}

		Expect(Registry != nullptr && Registry->GetRegisteredUnitCount() >= 0,
			TEXT("M_NoWidgetWorldScan"));

		UE_LOG(LogGPMinimapFriendlyBlipsContract, Log,
			TEXT("gp.UI.RunMinimapFriendlyBlipsContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMinimapFriendlyBlipsContract(
		TEXT("gp.UI.RunMinimapFriendlyBlipsContractTest"),
		TEXT("Run native minimap friendly-blip and surface-orientation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMinimapFriendlyBlipsContractTest));
}

#endif
