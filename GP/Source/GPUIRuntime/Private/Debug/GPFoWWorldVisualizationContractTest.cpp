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
#include "Presentation/GPFoWPresentationRaster.h"
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
		Expect(FCString::Strcmp(
				UGP_FoWWorldPresentationSubsystem::GetRendererName(),
				TEXT("PerCellBlurredQuadRenderer")) == 0
			&& !UGP_FoWWorldPresentationSubsystem::IsPostProcessActive()
			&& !UGP_FoWWorldPresentationSubsystem::IsMaskProjectionActive()
			&& FString(UGP_FoWWorldPresentationSubsystem::GetPresentationAlgorithmName())
				.Contains(TEXT("PerCell"))
			&& FString(UGP_FoWWorldPresentationSubsystem::GetMaskModelName())
				.Equals(TEXT("None")),
			TEXT("H8_PerCellBlurredQuadRendererIsTheActiveRenderer"));
		{
			FGP_FoWPresentationRaster TinyField;
			GPFoWPresentationRaster::ConfigureField(
				TinyField, 0, 0, 2, 1, 50.0f, FVector2D::ZeroVector);
			GPFoWPresentationRaster::SetCell(TinyField, 0, 0, EGP_FoWState::Visible);
			GPFoWPresentationRaster::SetCell(TinyField, 1, 0, EGP_FoWState::Unexplored);
			FGP_FoWPresentationGeometry TinyGeometry;
			Expect(GPFoWPresentationRaster::RebuildPresentation(TinyField, TinyGeometry)
				&& TinyGeometry.CellTiles == 1
				&& TinyGeometry.VisibleCellsSkipped == 1
				&& TinyGeometry.Quads.Num() > TinyGeometry.CellTiles,
				TEXT("H8B_OnlyNonVisibleCellsEmitTiles"));
			const float HiddenCenter = GPFoWPresentationRaster::SamplePresentationObscuration(
				TinyField, TinyGeometry, FVector2D(75.0, 25.0));
			const float VisibleCenter = GPFoWPresentationRaster::SamplePresentationObscuration(
				TinyField, TinyGeometry, FVector2D(15.0, 25.0));
			const float Boundary = GPFoWPresentationRaster::SamplePresentationObscuration(
				TinyField, TinyGeometry, FVector2D(40.0, 25.0));
			Expect(HiddenCenter > VisibleCenter
				&& Boundary > VisibleCenter && Boundary < HiddenCenter,
				TEXT("H8C_FeatheredTileEdgesBlendTowardVisible"));
		}
		Expect(Team1Mirror->GetRevision() == SmoothingRevisionBefore
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0))
				== ExploredBeforeSmoothing
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0))
				== UnexploredBeforeSmoothing,
			TEXT("H9_SmoothingDoesNotMutateOrPromoteLocalFoWState"));
		Expect(UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells() == 16384
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayQuads() == 98304
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumQuadsPerBatch() == 8000
			&& UGP_FoWWorldPresentationSubsystem::GetSamplePadCells() == 1
			&& UGP_FoWWorldPresentationSubsystem::GetFeatherFraction() > 0.25f
			&& UGP_FoWWorldPresentationSubsystem::GetFeatherFraction() < 0.6f,
			TEXT("H10_PerCellRendererIsBounded"));
		const float RasterCellSize = 50.0f;
		FGP_FoWPresentationRaster CircleField;
		GPFoWPresentationRaster::ConfigureField(
			CircleField, 0, 0, 16, 16, RasterCellSize, FVector2D::ZeroVector);
		for (int32 Y = 4; Y <= 11; ++Y)
		{
			for (int32 X = 4; X <= 11; ++X)
			{
				GPFoWPresentationRaster::SetCell(CircleField, X, Y, EGP_FoWState::Visible);
			}
		}
		FGP_FoWPresentationGeometry CircleGeometry;
		Expect(GPFoWPresentationRaster::RebuildPresentation(CircleField, CircleGeometry)
			&& CircleGeometry.VisibleCellsSkipped == 64
			&& CircleGeometry.CellTiles == 16 * 16 - 64
			&& CircleGeometry.Quads.Num() >= CircleGeometry.CellTiles
			&& CircleGeometry.Quads.Num()
				<= UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayQuads(),
			TEXT("W1_VisibleCellsAreNotDrawn"));

		const FVector2D VisibleCenter(8.0 * RasterCellSize, 8.0 * RasterCellSize);
		const FVector2D DeepUnexplored(1.0 * RasterCellSize, 1.0 * RasterCellSize);
		const float Interior = GPFoWPresentationRaster::SamplePresentationObscuration(
			CircleField, CircleGeometry, VisibleCenter);
		const float DeepHidden = GPFoWPresentationRaster::SamplePresentationObscuration(
			CircleField, CircleGeometry, DeepUnexplored);
		Expect(Interior <= 0.08f, TEXT("W2_InteriorVisibleRemainsClear"));
		Expect(DeepHidden >= 0.90f, TEXT("W3_DeepUnexploredRemainsBlack"));

		int32 TransitionSamples = 0;
		float Previous = Interior;
		bool bMonotone = true;
		for (int32 Step = 0; Step <= 40; ++Step)
		{
			const float T = static_cast<float>(Step) / 40.0f;
			const FVector2D Probe = FMath::Lerp(
				VisibleCenter,
				FVector2D(1.0 * RasterCellSize, 8.0 * RasterCellSize),
				T);
			const float Sample = GPFoWPresentationRaster::SamplePresentationObscuration(
				CircleField, CircleGeometry, Probe);
			if (Sample > Previous + 0.02f)
			{
				++TransitionSamples;
			}
			if (Sample + 0.08f < Previous)
			{
				bMonotone = false;
			}
			Previous = Sample;
		}
		Expect(bMonotone && TransitionSamples >= 3,
			TEXT("W4_FeatherProducesSoftCellEdge"));

		FGP_FoWPresentationRaster ComposeField;
		GPFoWPresentationRaster::ConfigureField(
			ComposeField, 0, 0, 32, 16, RasterCellSize, FVector2D::ZeroVector);
		for (int32 Y = 4; Y <= 11; ++Y)
		{
			for (int32 X = 4; X <= 11; ++X)
			{
				GPFoWPresentationRaster::SetCell(ComposeField, X, Y, EGP_FoWState::Visible);
			}
			for (int32 X = 20; X <= 27; ++X)
			{
				GPFoWPresentationRaster::SetCell(ComposeField, X, Y, EGP_FoWState::Explored);
			}
		}
		FGP_FoWPresentationGeometry ComposeGeometry;
		GPFoWPresentationRaster::RebuildPresentation(ComposeField, ComposeGeometry);
		const float VisibleSample = GPFoWPresentationRaster::SamplePresentationObscuration(
			ComposeField, ComposeGeometry, FVector2D(8.0 * RasterCellSize, 8.0 * RasterCellSize));
		const float ExploredSample = GPFoWPresentationRaster::SamplePresentationObscuration(
			ComposeField, ComposeGeometry, FVector2D(24.0 * RasterCellSize, 8.0 * RasterCellSize));
		const float UnexploredSample = GPFoWPresentationRaster::SamplePresentationObscuration(
			ComposeField, ComposeGeometry, FVector2D(1.0 * RasterCellSize, 14.0 * RasterCellSize));
		Expect(VisibleSample < ExploredSample && ExploredSample < UnexploredSample
			&& ExploredSample > 0.35f && ExploredSample < 0.85f,
			TEXT("W5_UnexploredAndExploredAreSeparateTileStates"));

		FGP_FoWPresentationRaster ShiftField;
		GPFoWPresentationRaster::ConfigureField(
			ShiftField, 40, 40, 8, 8, RasterCellSize, FVector2D::ZeroVector);
		GPFoWPresentationRaster::SetCell(ShiftField, 2, 2, EGP_FoWState::Visible);
		FGP_FoWPresentationGeometry ShiftGeometry;
		Expect(GPFoWPresentationRaster::RebuildPresentation(ShiftField, ShiftGeometry)
			&& ShiftGeometry.CellTiles == 63
			&& ShiftGeometry.VisibleCellsSkipped == 1
			&& ShiftGeometry.Quads.Num() >= ShiftGeometry.CellTiles,
			TEXT("W6_OutsidePriorSampleRebuildsPerCellTiles"));

		Expect(UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells() < 4000 * 4000
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayQuads() < 4000 * 4000,
			TEXT("W7_NoFullWorldMaskOrTileAllocation"));
		Expect(CircleGeometry.Quads.Num()
				<= UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayQuads()
			&& CircleGeometry.CellTiles
				<= UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells(),
			TEXT("W8_HardTileCapIsEnforced"));
		Expect(!UGP_FoWWorldOverlayWidget::StaticClass()->IsChildOf(UActorComponent::StaticClass()),
			TEXT("W9_NoPerPresentationTexelComponent"));
		Expect(Team1Mirror->GetRevision() == SmoothingRevisionBefore
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0))
				== UnexploredBeforeSmoothing,
			TEXT("W10_LocalFoWDataAndRevisionRemainUnchanged"));

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
		Expect(UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells() == 16384
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumQuadsPerBatch() == 8000
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayQuads() == 98304
			&& UGP_FoWWorldPresentationSubsystem::GetSamplePadCells() == 1
			&& !UGP_FoWWorldPresentationSubsystem::IsMaskProjectionActive(),
			TEXT("P_ViewportWorkAndBatchingAreBounded"));

		UGP_LocalFoWComponent* MillionCellMirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		FGP_FoWPresentationUpdate MillionCellInitial =
			Initial(1, 1, FIntPoint(4000, 4000), 50.0f);
		Expect(MillionCellMirror != nullptr
			&& MillionCellMirror->ApplyServerUpdate(MillionCellInitial)
			&& MillionCellMirror->GetGridDimensions() == FIntPoint(4000, 4000)
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells()
				< 4000 * 4000,
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
			FGP_FoWWorldOverlayStats CameraResampleStats;
			CameraResampleStats.SampledGameplayCells = 64;
			CameraResampleStats.CellTiles = 50;
			CameraResampleStats.FeatherQuads = 20;
			CameraResampleStats.OverlayQuads = 70;
			CameraResampleStats.DrawBatches = 1;
			CameraResampleStats.MinCell = FIntPoint(1, 1);
			CameraResampleStats.MaxCell = FIntPoint(8, 8);
			CameraResampleStats.ConsumedSerial = PresentationSubsystem->GetRenderSerial();
			CameraResampleStats.MaskRevision = PresentationSubsystem->GetLastUpdateRevision();
			CameraResampleStats.bCameraResample = true;
			CameraResampleStats.bFallbackActive = false;
			PresentationSubsystem->RecordOverlayStats(CameraResampleStats);
			Expect(PresentationSubsystem->DidLastCameraResample()
				&& !PresentationSubsystem->WasLastFallbackActive(),
				TEXT("S2_CameraResampleDoesNotForceConservativeFallback"));
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
		Expect(AuthorityFoW == nullptr
			|| (FMath::IsNearlyEqual(AuthorityFoW->GetCellSizeCm(), 50.0f)
				&& AuthorityFoW->GetGridDimensions() == FIntPoint(4000, 4000)
				&& FMath::IsNearlyEqual(AuthorityFoW->GetUpdateIntervalSeconds(), 0.1f)),
			TEXT("V2_GameplayGridIs50cmTenHz4000"));

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
