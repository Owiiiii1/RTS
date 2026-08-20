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
#include "Presentation/GPFoWContourField.h"
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
		Expect(UGP_FoWWorldPresentationSubsystem::GetConservativeBoundaryT() > 0.0f
			&& UGP_FoWWorldPresentationSubsystem::GetConservativeBoundaryT() < 0.5f
			&& FString(UGP_FoWWorldPresentationSubsystem::GetContourAlgorithmName())
				.Contains(TEXT("MarchingSquares")),
			TEXT("H8_ConservativeInsetMarchingSquares"));
		{
			FGP_FoWContourField TinyField;
			GPFoWContourField::ConfigureField(
				TinyField, 0, 0, 2, 1, 100.0f, FVector2D::ZeroVector);
			GPFoWContourField::SetCell(TinyField, 0, 0, EGP_FoWState::Visible);
			GPFoWContourField::SetCell(TinyField, 1, 0, EGP_FoWState::Unexplored);
			GPFoWContourField::BuildCenterSamples(TinyField);
			const float HiddenCenter = GPFoWContourField::SampleBilinearObscuration(
				TinyField, FVector2D(150.0, 50.0));
			const float VisibleCenter = GPFoWContourField::SampleBilinearObscuration(
				TinyField, FVector2D(50.0, 50.0));
			Expect(HiddenCenter >= 0.999f && VisibleCenter <= 0.5f + KINDA_SMALL_NUMBER,
				TEXT("H8B_HiddenCellCentersStayFullyObscured"));
		}
		Expect(Team1Mirror->GetRevision() == SmoothingRevisionBefore
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0))
				== ExploredBeforeSmoothing
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0))
				== UnexploredBeforeSmoothing,
			TEXT("H9_SmoothingDoesNotMutateOrPromoteLocalFoWState"));
		Expect(UGP_FoWWorldPresentationSubsystem::GetConservativeBoundaryT() < 0.5f
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayTriangles() == 65536
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumIsoSegments() == 32768
			&& UGP_FoWWorldPresentationSubsystem::GetSubcellsPerCell() == 4,
			TEXT("H10_ContourGeometryIsBounded"));

		auto FillDiscreteCircle = [](
			FGP_FoWContourField& Field,
			int32 CenterX,
			int32 CenterY,
			int32 RadiusCells,
			EGP_FoWState InsideState)
		{
			const int32 RadiusSq = RadiusCells * RadiusCells;
			for (int32 Y = 0; Y < Field.Height; ++Y)
			{
				for (int32 X = 0; X < Field.Width; ++X)
				{
					const int32 DX = X - CenterX;
					const int32 DY = Y - CenterY;
					if (DX * DX + DY * DY <= RadiusSq)
					{
						GPFoWContourField::SetCell(Field, X, Y, InsideState);
					}
				}
			}
		};

		auto SegmentIsDiagonal = [](const FGP_FoWContourSegment& Segment, float CellSize)
		{
			const FVector2D Delta = Segment.End - Segment.Start;
			return FMath::Abs(Delta.X) > 0.05f * CellSize
				&& FMath::Abs(Delta.Y) > 0.05f * CellSize;
		};

		auto CountDirectionBuckets = [](
			const TArray<FGP_FoWContourSegment>& Segments)
		{
			TSet<int32> Buckets;
			for (const FGP_FoWContourSegment& Segment : Segments)
			{
				const FVector2D Delta = Segment.End - Segment.Start;
				if (Delta.SizeSquared() <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const float Angle = FMath::Atan2(Delta.Y, Delta.X);
				Buckets.Add(FMath::RoundToInt(Angle / (PI / 8.0f)));
			}
			return Buckets.Num();
		};

		const float ContourCellSize = 200.0f;
		FGP_FoWContourField CircleField;
		GPFoWContourField::ConfigureField(
			CircleField, 0, 0, 31, 31, ContourCellSize, FVector2D::ZeroVector);
		FillDiscreteCircle(CircleField, 15, 15, 8, EGP_FoWState::Visible);
		GPFoWContourField::BuildCenterSamples(CircleField);
		FGP_FoWContourGeometry CircleGeometry;
		GPFoWContourField::GenerateOverlayGeometry(CircleField, CircleGeometry);
		Expect(CircleGeometry.VisibleIsoSegments.Num() > 8
			&& CircleGeometry.TriangleCount > 0
			&& CircleGeometry.TriangleVertices.Num() == CircleGeometry.TriangleCount * 3,
			TEXT("W1_SyntheticCircleProducesInterpolatedContour"));

		int32 DiagonalSegments = 0;
		int32 AxisAlignedSegments = 0;
		for (const FGP_FoWContourSegment& Segment : CircleGeometry.VisibleIsoSegments)
		{
			if (SegmentIsDiagonal(Segment, ContourCellSize))
			{
				++DiagonalSegments;
			}
			else
			{
				++AxisAlignedSegments;
			}
		}
		Expect(DiagonalSegments >= 8,
			TEXT("W2_ContourContainsNonAxisAlignedEdges"));
		Expect(DiagonalSegments >= 8
			&& AxisAlignedSegments < CircleGeometry.VisibleIsoSegments.Num()
			&& CountDirectionBuckets(CircleGeometry.VisibleIsoSegments) > 4
			&& CircleGeometry.VisibleIsoSegments.Num() > 16,
			TEXT("W3_ContourDoesNotTraceCellRectangles"));

		const FVector2D CircleCenter(15.5 * ContourCellSize, 15.5 * ContourCellSize);
		float MinRadius = TNumericLimits<float>::Max();
		float MaxRadius = 0.0f;
		float RadiusSum = 0.0f;
		int32 RadiusSamples = 0;
		for (const FGP_FoWContourSegment& Segment : CircleGeometry.VisibleIsoSegments)
		{
			const FVector2D Mid = (Segment.Start + Segment.End) * 0.5f;
			const float Radius = FVector2D::Distance(Mid, CircleCenter);
			MinRadius = FMath::Min(MinRadius, Radius);
			MaxRadius = FMath::Max(MaxRadius, Radius);
			RadiusSum += Radius;
			++RadiusSamples;
		}
		const float MeanRadius = RadiusSamples > 0 ? RadiusSum / RadiusSamples : 0.0f;
		Expect(RadiusSamples > 0
			&& MeanRadius > 6.0f * ContourCellSize
			&& MeanRadius < 9.0f * ContourCellSize
			&& (MaxRadius - MinRadius) < 2.2f * ContourCellSize,
			TEXT("W4_VisualCircleRadiusVarianceIsBounded"));

		FGP_FoWContourField UnionField;
		GPFoWContourField::ConfigureField(
			UnionField, 0, 0, 31, 31, ContourCellSize, FVector2D::ZeroVector);
		FillDiscreteCircle(UnionField, 10, 15, 8, EGP_FoWState::Visible);
		FillDiscreteCircle(UnionField, 20, 15, 8, EGP_FoWState::Visible);
		GPFoWContourField::BuildCenterSamples(UnionField);
		FGP_FoWContourGeometry UnionGeometry;
		GPFoWContourField::GenerateOverlayGeometry(UnionField, UnionGeometry);
		int32 UnionDiagonals = 0;
		for (const FGP_FoWContourSegment& Segment : UnionGeometry.VisibleIsoSegments)
		{
			if (SegmentIsDiagonal(Segment, ContourCellSize))
			{
				++UnionDiagonals;
			}
		}
		Expect(UnionDiagonals >= 8
			&& CountDirectionBuckets(UnionGeometry.VisibleIsoSegments) > 4
			&& UnionGeometry.VisibleIsoSegments.Num() > CircleGeometry.VisibleIsoSegments.Num() / 2,
			TEXT("W5_OverlappingCirclesProduceSmoothUnion"));

		FGP_FoWContourField TrailField;
		GPFoWContourField::ConfigureField(
			TrailField, 0, 0, 24, 24, ContourCellSize, FVector2D::ZeroVector);
		for (int32 Index = 4; Index <= 19; ++Index)
		{
			GPFoWContourField::SetCell(TrailField, Index, Index, EGP_FoWState::Explored);
			if (Index + 1 <= 19)
			{
				GPFoWContourField::SetCell(TrailField, Index + 1, Index, EGP_FoWState::Explored);
			}
		}
		GPFoWContourField::BuildCenterSamples(TrailField);
		FGP_FoWContourGeometry TrailGeometry;
		GPFoWContourField::GenerateOverlayGeometry(TrailField, TrailGeometry);
		int32 TrailDiagonals = 0;
		for (const FGP_FoWContourSegment& Segment : TrailGeometry.UnexploredIsoSegments)
		{
			if (SegmentIsDiagonal(Segment, ContourCellSize))
			{
				++TrailDiagonals;
			}
		}
		Expect(TrailDiagonals >= 4
			&& CountDirectionBuckets(TrailGeometry.UnexploredIsoSegments) > 4,
			TEXT("W6_DiagonalExploredTrailIsNotSquareRunSilhouette"));

		bool bHiddenCellsStayObscured = true;
		for (int32 Y = 0; Y < CircleField.Height && bHiddenCellsStayObscured; ++Y)
		{
			for (int32 X = 0; X < CircleField.Width; ++X)
			{
				if (CircleField.Cells[CircleField.CellIndex(X, Y)] != EGP_FoWState::Unexplored)
				{
					continue;
				}
				const FVector2D HiddenCenter(
					(static_cast<double>(X) + 0.5) * ContourCellSize,
					(static_cast<double>(Y) + 0.5) * ContourCellSize);
				if (GPFoWContourField::SampleBilinearObscuration(CircleField, HiddenCenter)
					< 0.999f)
				{
					bHiddenCellsStayObscured = false;
					break;
				}
			}
		}
		const float InteriorVisible = GPFoWContourField::SampleBilinearObscuration(
			CircleField, CircleCenter);
		Expect(bHiddenCellsStayObscured && InteriorVisible <= KINDA_SMALL_NUMBER,
			TEXT("W7_ConservativeVisibleBoundaryDoesNotPromoteHiddenCells"));
		Expect(Team1Mirror->GetRevision() == SmoothingRevisionBefore
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0))
				== UnexploredBeforeSmoothing,
			TEXT("W8_LocalFoWDataAndRevisionRemainUnchanged"));
		Expect(CircleGeometry.TriangleCount
				<= UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayTriangles()
			&& CircleGeometry.VisibleIsoSegments.Num()
				<= UGP_FoWWorldPresentationSubsystem::GetMaximumIsoSegments()
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells() < 1000 * 1000
			&& UGP_FoWWorldPresentationSubsystem::GetSamplePadCells() == 1
			&& UGP_FoWWorldPresentationSubsystem::GetSubcellsPerCell() == 4,
			TEXT("W9_PerformanceCapsRemainBounded"));

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
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayTriangles() == 65536
			&& UGP_FoWWorldPresentationSubsystem::GetMaximumIsoSegments() == 32768,
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
				81,
				12,
				36,
				12,
				8,
				4,
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
