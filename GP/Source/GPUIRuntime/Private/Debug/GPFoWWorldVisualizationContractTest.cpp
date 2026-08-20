// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Combat/GPCombatPresentationComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Presentation/GPHealthBarComponent.h"
#include "Presentation/GPLocalFoWUnitPresentationSubsystem.h"
#include "Presentation/GPFoWWorldPresentationSubsystem.h"
#include "Units/GPWorker.h"
#include "Widgets/GPFoWWorldOverlayWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFoWWorldVisualizationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPFoWWorldVisualizationContractPrivate
{
	static FGP_FoWCellRange Range(int32 StartIndex, int32 NumCells)
	{
		FGP_FoWCellRange Result;
		Result.StartIndex = StartIndex;
		Result.NumCells = NumCells;
		return Result;
	}

	static FGP_FoWPresentationUpdate Initial(
		int32 TeamId,
		int64 Revision,
		FIntPoint Dimensions = FIntPoint(4, 4),
		float CellSize = 100.0f)
	{
		FGP_FoWPresentationUpdate Update;
		Update.bInitialSnapshot = true;
		Update.TeamId = TeamId;
		Update.Revision = Revision;
		Update.GridOriginWorldXY = FVector2D::ZeroVector;
		Update.GridDimensions = Dimensions;
		Update.CellSizeCm = CellSize;
		return Update;
	}

	static FVector CellLocation(int32 X, int32 Y, float CellSize = 100.0f)
	{
		return FVector(
			(static_cast<double>(X) + 0.5) * CellSize,
			(static_cast<double>(Y) + 0.5) * CellSize,
			0.0);
	}

	static void RunWorldVisualizationContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPFoWWorldVisualizationContract, Warning,
				TEXT("gp.FoW.RunWorldVisualizationContractTest: missing authority test world."));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPFoWWorldVisualizationContract, Log,
					TEXT("gp.FoW.RunWorldVisualizationContractTest PASS: %s"),
					Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPFoWWorldVisualizationContract, Error,
					TEXT("gp.FoW.RunWorldVisualizationContractTest FAIL: %s"),
					Label);
			}
		};

		UGP_LocalFoWComponent* Team1Mirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		Expect(Team1Mirror != nullptr && !Team1Mirror->IsReady(),
			TEXT("A_RendererInputStartsNotReady"));
		Expect(UGP_FoWWorldPresentationSubsystem::RequiresConservativeFullObscuration(
				Team1Mirror),
			TEXT("B_NotReadyIsFullyObscured"));

		const float UnexploredObscuration =
			UGP_FoWWorldPresentationSubsystem::GetObscurationForState(
				EGP_FoWState::Unexplored);
		const float ExploredObscuration =
			UGP_FoWWorldPresentationSubsystem::GetObscurationForState(
				EGP_FoWState::Explored);
		const float VisibleObscuration =
			UGP_FoWWorldPresentationSubsystem::GetObscurationForState(
				EGP_FoWState::Visible);
		Expect(FMath::IsNearlyEqual(UnexploredObscuration, 1.0f),
			TEXT("C_UnexploredMaximumBlackObscuration"));
		Expect(ExploredObscuration > 0.0f && ExploredObscuration < 1.0f,
			TEXT("D_ExploredDimObscuration"));
		Expect(FMath::IsNearlyZero(VisibleObscuration),
			TEXT("E_VisibleNoObscuration"));
		Expect(UnexploredObscuration > ExploredObscuration
			&& ExploredObscuration > VisibleObscuration,
			TEXT("F_ThreeVisualValuesDistinctAndOrdered"));

		FGP_FoWPresentationUpdate Team1Initial = Initial(1, 1);
		Team1Initial.ExploredRanges.Add(Range(0, 2));
		Team1Initial.VisibleRanges.Add(Range(1, 1));
		Expect(Team1Mirror->ApplyServerUpdate(Team1Initial),
			TEXT("G_InitialSnapshotBuildsVisualInput"));
		Expect(Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0))
				== EGP_FoWState::Explored
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(1, 0))
				== EGP_FoWState::Visible
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0))
				== EGP_FoWState::Unexplored,
			TEXT("H_ExactCellStateEncodingAndCoordinates"));

		FActorSpawnParameters UnitSpawnParams;
		UnitSpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGP_Worker* OwnUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			CellLocation(2, 0) + FVector(0.0, 0.0, 200.0),
			FRotator::ZeroRotator,
			UnitSpawnParams);
		AGP_Worker* EnemyUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			CellLocation(1, 0) + FVector(0.0, 0.0, 200.0),
			FRotator::ZeroRotator,
			UnitSpawnParams);
		if (OwnUnit != nullptr)
		{
			OwnUnit->SetTeamId(1);
		}
		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetTeamId(2);
		}

		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			OwnUnit,
			1,
			Team1Mirror);
		Expect(OwnUnit != nullptr
			&& OwnUnit->IsLocalFoWPresentationVisible()
			&& !OwnUnit->IsHidden(),
			TEXT("H1_OwnUnitNeverHiddenByLocalFoW"));

		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			EnemyUnit,
			1,
			Team1Mirror);
		Expect(EnemyUnit != nullptr
			&& EnemyUnit->IsLocalFoWPresentationVisible()
			&& !EnemyUnit->IsHidden(),
			TEXT("H2_EnemyVisiblePresentationVisible"));

		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetActorLocation(CellLocation(0, 0) + FVector(0.0, 0.0, 200.0));
		}
		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			EnemyUnit,
			1,
			Team1Mirror);
		Expect(EnemyUnit != nullptr
			&& !EnemyUnit->IsLocalFoWPresentationVisible()
			&& EnemyUnit->IsHidden(),
			TEXT("H3_EnemyExploredPresentationHidden"));

		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetActorLocation(CellLocation(2, 0) + FVector(0.0, 0.0, 200.0));
		}
		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			EnemyUnit,
			1,
			Team1Mirror);
		Expect(EnemyUnit != nullptr
			&& !EnemyUnit->IsLocalFoWPresentationVisible()
			&& EnemyUnit->IsHidden(),
			TEXT("H4_EnemyUnexploredPresentationHidden"));

		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetActorLocation(CellLocation(1, 0) + FVector(0.0, 0.0, 200.0));
		}
		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			EnemyUnit,
			1,
			Team1Mirror);
		Expect(EnemyUnit != nullptr
			&& EnemyUnit->IsLocalFoWPresentationVisible()
			&& !EnemyUnit->IsHidden(),
			TEXT("H5_EnemyReentersVisiblePresentationRestored"));

		UGP_HealthBarComponent* EnemyHealthBar =
			EnemyUnit != nullptr ? EnemyUnit->GetHealthBarComponent() : nullptr;
		UGP_AbilitySystemComponent* EnemyASC =
			EnemyUnit != nullptr ? EnemyUnit->GetGPAbilitySystemComponent() : nullptr;
		if (EnemyASC != nullptr)
		{
			EnemyASC->SetNumericAttributeBase(
				UGP_UnitAttributeSet::GetHealthAttribute(),
				40.0f);
		}
		if (EnemyHealthBar != nullptr)
		{
			EnemyHealthBar->RefreshHealthBarFromAttributes();
		}
		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetActorLocation(CellLocation(0, 0) + FVector(0.0, 0.0, 200.0));
		}
		const int32 EnemyTeamBeforePresentation =
			EnemyUnit != nullptr ? EnemyUnit->GetTeamId() : INDEX_NONE;
		const bool bEnemyReplicatesBeforePresentation =
			EnemyUnit != nullptr && EnemyUnit->GetIsReplicated();
		const bool bEnemyReplicateMovementBeforePresentation =
			EnemyUnit != nullptr && EnemyUnit->IsReplicatingMovement();
		const FVector EnemyLocationBeforePresentation =
			EnemyUnit != nullptr ? EnemyUnit->GetActorLocation() : FVector::ZeroVector;
		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			EnemyUnit,
			1,
			Team1Mirror);
		Expect(EnemyHealthBar != nullptr
			&& EnemyHealthBar->DoesHealthPolicyAllowVisibility()
			&& !EnemyHealthBar->IsFoWPresentationAllowed()
			&& !EnemyHealthBar->IsComposedHealthBarVisible()
			&& !EnemyHealthBar->IsVisible()
			&& EnemyUnit != nullptr
			&& EnemyUnit->GetCombatPresentationComponent() != nullptr
			&& !EnemyUnit->GetCombatPresentationComponent()->IsLocalPresentationAllowed(),
			TEXT("H6_DamagedEnemyHealthBarCannotLeakWhileHidden"));
		Expect(EnemyUnit != nullptr
			&& EnemyUnit->GetTeamId() == EnemyTeamBeforePresentation
			&& EnemyUnit->GetIsReplicated() == bEnemyReplicatesBeforePresentation
			&& EnemyUnit->IsReplicatingMovement() == bEnemyReplicateMovementBeforePresentation
			&& EnemyUnit->GetActorLocation().Equals(EnemyLocationBeforePresentation),
			TEXT("H7_PresentationGateDoesNotMutateGameplayOrReplication"));

		const int64 SmoothingRevisionBefore = Team1Mirror->GetRevision();
		const EGP_FoWState ExploredBeforeSmoothing =
			Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0));
		const EGP_FoWState UnexploredBeforeSmoothing =
			Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0));
		Expect(
			UGP_FoWWorldPresentationSubsystem::ShouldAddConservativeFeather(
				EGP_FoWState::Visible,
				EGP_FoWState::Explored)
			&& UGP_FoWWorldPresentationSubsystem::ShouldAddConservativeFeather(
				EGP_FoWState::Visible,
				EGP_FoWState::Unexplored)
			&& UGP_FoWWorldPresentationSubsystem::ShouldAddConservativeFeather(
				EGP_FoWState::Explored,
				EGP_FoWState::Unexplored)
			&& !UGP_FoWWorldPresentationSubsystem::ShouldAddConservativeFeather(
				EGP_FoWState::Unexplored,
				EGP_FoWState::Visible),
			TEXT("H8_SmoothingOnlyDarkensLessObscuredSide"));
		Expect(Team1Mirror->GetRevision() == SmoothingRevisionBefore
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0))
				== ExploredBeforeSmoothing
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0))
				== UnexploredBeforeSmoothing,
			TEXT("H9_SmoothingDoesNotMutateOrPromoteLocalFoWState"));
		Expect(UGP_FoWWorldPresentationSubsystem::GetSmoothingWidthCellFraction() > 0.0f
			&& UGP_FoWWorldPresentationSubsystem::GetSmoothingWidthCellFraction() < 0.5f
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumFeatherQuads()
				<= UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells() * 4,
			TEXT("H10_SmoothingWidthAndGeometryAreBounded"));

		FGP_FoWPresentationUpdate Team1Delta = Initial(1, 2);
		Team1Delta.bInitialSnapshot = false;
		Team1Delta.ExploredRanges.Add(Range(2, 1));
		Team1Delta.VisibleRanges.Add(Range(2, 1));
		Expect(Team1Mirror->ApplyServerUpdate(Team1Delta)
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(1, 0))
				== EGP_FoWState::Explored,
			TEXT("I_VisibleToExploredOnNewRevision"));
		Expect(Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0))
				== EGP_FoWState::Explored,
			TEXT("J_ExploredNeverReturnsToUnexplored"));
		Expect(!Team1Mirror->ApplyServerUpdate(Team1Initial)
			&& Team1Mirror->GetRevision() == 2
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0))
				== EGP_FoWState::Visible,
			TEXT("K_StaleRevisionCannotRollVisualInputBackward"));

		Team1Mirror->ResetPresentation();
		Expect(!Team1Mirror->IsReady()
			&& UGP_FoWWorldPresentationSubsystem::RequiresConservativeFullObscuration(
				Team1Mirror)
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0))
				== EGP_FoWState::Unexplored,
			TEXT("L_TeamResetClearsPriorVisualState"));

		FGP_FoWPresentationUpdate Team2Initial = Initial(2, 7);
		Team2Initial.ExploredRanges.Add(Range(0, 1));
		Team2Initial.VisibleRanges.Add(Range(0, 1));
		Expect(Team1Mirror->ApplyServerUpdate(Team2Initial)
			&& Team1Mirror->IsReady()
			&& Team1Mirror->GetLocalTeamId() == 2
			&& Team1Mirror->GetRevision() == 7,
			TEXT("M_NewInitialSnapshotRebuildsVisualState"));

		Expect(UGP_FoWWorldPresentationSubsystem::StaticClass()->FindFunctionByName(
				TEXT("GetStateForTeamAtWorldLocation")) == nullptr
			&& UGP_LocalFoWComponent::StaticClass()->FindFunctionByName(
				TEXT("GetStateForTeamAtWorldLocation")) == nullptr,
			TEXT("N_NoArbitraryTeamQuerySurface"));
		Expect(!UGP_FoWWorldPresentationSubsystem::StaticClass()->IsChildOf(
				UActorComponent::StaticClass())
			&& UGP_FoWWorldOverlayWidget::StaticClass()->IsChildOf(
				UUserWidget::StaticClass()),
			TEXT("O_NoPerCellComponentOrUObjectModel"));
		Expect(UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells() == 65536
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumQuadsPerBatch() == 8000
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumFeatherQuads() == 32768,
			TEXT("P_ViewportWorkAndBatchingAreBounded"));

		UGP_LocalFoWComponent* MillionCellMirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		FGP_FoWPresentationUpdate MillionCellInitial =
			Initial(1, 1, FIntPoint(1000, 1000), 200.0f);
		Expect(MillionCellMirror != nullptr
			&& MillionCellMirror->ApplyServerUpdate(MillionCellInitial)
			&& MillionCellMirror->GetGridDimensions() == FIntPoint(1000, 1000)
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells()
				< 1000 * 1000,
			TEXT("Q_MillionCellGridUsesViewportLocalRepresentation"));

		AGP_PlayerController* GPPlayerController =
			Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		ULocalPlayer* LocalPlayer =
			GPPlayerController != nullptr ? GPPlayerController->GetLocalPlayer() : nullptr;
		UGP_FoWWorldPresentationSubsystem* PresentationSubsystem =
			LocalPlayer != nullptr
				? LocalPlayer->GetSubsystem<UGP_FoWWorldPresentationSubsystem>()
				: nullptr;
		Expect(PresentationSubsystem != nullptr
			&& PresentationSubsystem->GetBoundMirror()
				== GPPlayerController->GetLocalFogOfWarComponent(),
			TEXT("R_ProductionRendererSourceIsLocalFoWMirror"));

		const int64 ProductionRevisionBefore =
			PresentationSubsystem != nullptr
				? PresentationSubsystem->GetLastUpdateRevision()
				: -1;
		if (PresentationSubsystem != nullptr)
		{
			PresentationSubsystem->RecordOverlayStats(
				64,
				8,
				1,
				1,
				FIntPoint(1, 1),
				FIntPoint(8, 8),
				PresentationSubsystem->GetRenderSerial());
		}
		Expect(PresentationSubsystem == nullptr
			|| PresentationSubsystem->GetLastUpdateRevision() == ProductionRevisionBefore,
			TEXT("S_CameraViewStatsDoNotMutateFoWData"));

		if (PresentationSubsystem != nullptr)
		{
			UGP_LocalFoWComponent* ProductionMirror =
				PresentationSubsystem->GetBoundMirror();
			const int64 RevisionBeforeToggle =
				ProductionMirror != nullptr ? ProductionMirror->GetRevision() : -1;
			const bool bWasEnabled = PresentationSubsystem->IsVisualizationEnabled();
			PresentationSubsystem->SetVisualizationEnabled(false);
			PresentationSubsystem->SetVisualizationEnabled(true);
			PresentationSubsystem->SetVisualizationEnabled(bWasEnabled);
			Expect(ProductionMirror == nullptr
				|| ProductionMirror->GetRevision() == RevisionBeforeToggle,
				TEXT("T_EnableDisableIsPresentationOnly"));
		}
		else
		{
			Expect(false, TEXT("T_EnableDisableIsPresentationOnly"));
		}

		UGP_LocalFoWComponent* OtherTeamMirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		FGP_FoWPresentationUpdate OtherTeamInitial = Initial(1, 3);
		OtherTeamInitial.ExploredRanges.Add(Range(0, 1));
		Expect(OtherTeamMirror != nullptr
			&& OtherTeamMirror->ApplyServerUpdate(OtherTeamInitial)
			&& OtherTeamMirror->GetStateAtWorldLocation(CellLocation(0, 0))
				== EGP_FoWState::Explored
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0))
				== EGP_FoWState::Visible,
			TEXT("U_MultiplayerMirrorsCanRenderDifferentMasks"));

		AGP_GameState* GameState = World->GetGameState<AGP_GameState>();
		UGP_FogOfWarComponent* AuthorityFoW =
			GameState != nullptr ? GameState->GetFogOfWarComponent() : nullptr;
		const EGP_FoWState AuthorityBefore = AuthorityFoW != nullptr
			? AuthorityFoW->GetStateForTeamAtWorldLocation(1, CellLocation(0, 0))
			: EGP_FoWState::Unexplored;
		PresentationSubsystem = LocalPlayer != nullptr
			? LocalPlayer->GetSubsystem<UGP_FoWWorldPresentationSubsystem>()
			: nullptr;
		Expect(AuthorityFoW == nullptr
			|| AuthorityFoW->GetStateForTeamAtWorldLocation(1, CellLocation(0, 0))
				== AuthorityBefore,
			TEXT("V_RendererDoesNotMutateGameplayAuthority"));

		if (OwnUnit != nullptr)
		{
			OwnUnit->Destroy();
		}
		if (EnemyUnit != nullptr)
		{
			EnemyUnit->Destroy();
		}

		UE_LOG(LogGPFoWWorldVisualizationContract, Log,
			TEXT("gp.FoW.RunWorldVisualizationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GWorldVisualizationContractCommand(
		TEXT("gp.FoW.RunWorldVisualizationContractTest"),
		TEXT("Run source-only world Fog of War visualization contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunWorldVisualizationContractTest));
}

#endif
