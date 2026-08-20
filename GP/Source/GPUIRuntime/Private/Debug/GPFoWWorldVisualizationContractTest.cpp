// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Combat/GPCombatPresentationComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/GPPlayerController.h"
#include "Presentation/GPFoWVisualMask.h"
#include "Presentation/GPFoWWorldPresentationSubsystem.h"
#include "Presentation/GPHealthBarComponent.h"
#include "Presentation/GPLocalFoWUnitPresentationSubsystem.h"
#include "Units/GPWorker.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UObjectIterator.h"

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

	static TArray<EGP_FoWState> MakeGrid(int32 Width, int32 Height, EGP_FoWState Fill)
	{
		TArray<EGP_FoWState> Cells;
		Cells.Init(Fill, Width * Height);
		return Cells;
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

		Expect(FCString::Strcmp(
				UGP_FoWWorldPresentationSubsystem::GetRendererName(),
				TEXT("PostProcessTextureMask")) == 0
			&& !UGP_FoWWorldPresentationSubsystem::IsOldSlateRendererActive()
			&& FindObject<UClass>(nullptr, TEXT("/Script/GPUIRuntime.GP_FoWWorldOverlayWidget")) == nullptr,
			TEXT("A_OldSlateWorldRendererRemoved"));

		UGP_LocalFoWComponent* Team1Mirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		Expect(Team1Mirror != nullptr && !Team1Mirror->IsReady(),
			TEXT("B_RendererInputStartsNotReady"));
		Expect(UGP_FoWWorldPresentationSubsystem::RequiresConservativeFullObscuration(Team1Mirror),
			TEXT("C_NotReadyIsFullyObscured"));

		const FLinearColor SceneColor(0.8f, 0.7f, 0.6f, 1.0f);
		const FLinearColor NotReadyColor = UGP_FoWWorldPresentationSubsystem::ComposeVisualSceneColor(
			SceneColor, 1.0f, 1.0f, false);
		Expect(NotReadyColor.Equals(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)),
			TEXT("D_NotReadyCompositionIsBlack"));

		const float UnexploredObscuration =
			UGP_FoWWorldPresentationSubsystem::GetObscurationForState(EGP_FoWState::Unexplored);
		const float ExploredObscuration =
			UGP_FoWWorldPresentationSubsystem::GetObscurationForState(EGP_FoWState::Explored);
		const float VisibleObscuration =
			UGP_FoWWorldPresentationSubsystem::GetObscurationForState(EGP_FoWState::Visible);
		Expect(FMath::IsNearlyEqual(UnexploredObscuration, 1.0f),
			TEXT("E_UnexploredMaximumBlackObscuration"));
		Expect(ExploredObscuration > 0.0f && ExploredObscuration < 1.0f,
			TEXT("F_ExploredDimObscuration"));
		Expect(FMath::IsNearlyZero(VisibleObscuration),
			TEXT("G_VisibleNoObscuration"));

		FGP_FoWPresentationUpdate Team1Initial = Initial(1, 1);
		Team1Initial.ExploredRanges.Add(Range(0, 2));
		Team1Initial.VisibleRanges.Add(Range(1, 1));
		Expect(Team1Mirror->ApplyServerUpdate(Team1Initial),
			TEXT("H_InitialSnapshotBuildsVisualInput"));
		Expect(Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0)) == EGP_FoWState::Explored
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(1, 0)) == EGP_FoWState::Visible
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0)) == EGP_FoWState::Unexplored,
			TEXT("I_ExactCellStateEncodingAndCoordinates"));

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

		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(OwnUnit, 1, Team1Mirror);
		Expect(OwnUnit != nullptr && OwnUnit->IsLocalFoWPresentationVisible() && !OwnUnit->IsHidden(),
			TEXT("J1_OwnUnitNeverHiddenByLocalFoW"));
		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(EnemyUnit, 1, Team1Mirror);
		Expect(EnemyUnit != nullptr && EnemyUnit->IsLocalFoWPresentationVisible() && !EnemyUnit->IsHidden(),
			TEXT("J2_EnemyVisiblePresentationVisible"));
		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetActorLocation(CellLocation(0, 0) + FVector(0.0, 0.0, 200.0));
		}
		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(EnemyUnit, 1, Team1Mirror);
		Expect(EnemyUnit != nullptr && !EnemyUnit->IsLocalFoWPresentationVisible() && EnemyUnit->IsHidden(),
			TEXT("J3_EnemyExploredPresentationHidden"));
		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetActorLocation(CellLocation(2, 0) + FVector(0.0, 0.0, 200.0));
		}
		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(EnemyUnit, 1, Team1Mirror);
		Expect(EnemyUnit != nullptr && !EnemyUnit->IsLocalFoWPresentationVisible() && EnemyUnit->IsHidden(),
			TEXT("J4_EnemyUnexploredPresentationHidden"));
		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetActorLocation(CellLocation(1, 0) + FVector(0.0, 0.0, 200.0));
		}
		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(EnemyUnit, 1, Team1Mirror);
		Expect(EnemyUnit != nullptr && EnemyUnit->IsLocalFoWPresentationVisible() && !EnemyUnit->IsHidden(),
			TEXT("J5_EnemyReentersVisiblePresentationRestored"));

		UGP_HealthBarComponent* EnemyHealthBar =
			EnemyUnit != nullptr ? EnemyUnit->GetHealthBarComponent() : nullptr;
		UGP_AbilitySystemComponent* EnemyASC =
			EnemyUnit != nullptr ? EnemyUnit->GetGPAbilitySystemComponent() : nullptr;
		if (EnemyASC != nullptr)
		{
			EnemyASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), 40.0f);
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
		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(EnemyUnit, 1, Team1Mirror);
		Expect(EnemyHealthBar != nullptr
			&& EnemyHealthBar->DoesHealthPolicyAllowVisibility()
			&& !EnemyHealthBar->IsFoWPresentationAllowed()
			&& !EnemyHealthBar->IsComposedHealthBarVisible()
			&& !EnemyHealthBar->IsVisible()
			&& EnemyUnit != nullptr
			&& EnemyUnit->GetCombatPresentationComponent() != nullptr
			&& !EnemyUnit->GetCombatPresentationComponent()->IsLocalPresentationAllowed(),
			TEXT("J6_DamagedEnemyHealthBarCannotLeakWhileHidden"));
		Expect(EnemyUnit != nullptr
			&& EnemyUnit->GetTeamId() == EnemyTeamBeforePresentation
			&& EnemyUnit->GetIsReplicated() == bEnemyReplicatesBeforePresentation
			&& EnemyUnit->IsReplicatingMovement() == bEnemyReplicateMovementBeforePresentation
			&& EnemyUnit->GetActorLocation().Equals(EnemyLocationBeforePresentation),
			TEXT("J7_PresentationGateDoesNotMutateGameplayOrReplication"));

		const int64 EncodingRevisionBefore = Team1Mirror->GetRevision();
		FGP_FoWVisualMaskBuffers Encoded;
		GPFoWVisualMask::EncodeFromLocalFoW(Encoded, Team1Mirror, 32, 32);
		float KnownExplored = 0.0f;
		float VisibleExplored = 0.0f;
		float KnownVisible = 0.0f;
		float VisibleVisible = 0.0f;
		float KnownHidden = 0.0f;
		float VisibleHidden = 0.0f;
		GPFoWVisualMask::SampleBilinear(Encoded, FVector2D(50.0, 50.0), KnownExplored, VisibleExplored);
		GPFoWVisualMask::SampleBilinear(Encoded, FVector2D(150.0, 50.0), KnownVisible, VisibleVisible);
		GPFoWVisualMask::SampleBilinear(Encoded, FVector2D(250.0, 50.0), KnownHidden, VisibleHidden);
		Expect(KnownExplored > 0.9f && VisibleExplored < 0.1f
			&& KnownVisible > 0.9f && VisibleVisible > 0.9f
			&& KnownHidden < 0.1f && VisibleHidden < 0.1f,
			TEXT("K_KnownVisibleChannelEncodingCorrect"));

		const FLinearColor UnexploredColor = GPFoWVisualMask::ComposeSceneColor(SceneColor, 0.0f, 0.0f, true);
		const FLinearColor ExploredColor = GPFoWVisualMask::ComposeSceneColor(SceneColor, 1.0f, 0.0f, true);
		const FLinearColor VisibleColor = GPFoWVisualMask::ComposeSceneColor(SceneColor, 1.0f, 1.0f, true);
		Expect(UnexploredColor.Equals(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)),
			TEXT("L_UnexploredCompositionIsBlack"));
		Expect(ExploredColor.R < SceneColor.R && ExploredColor.R > 0.05f,
			TEXT("M_ExploredCompositionIsDim"));
		Expect(VisibleColor.Equals(SceneColor),
			TEXT("N_VisibleCompositionUnchanged"));

		TArray<EGP_FoWState> CircleCells = MakeGrid(16, 16, EGP_FoWState::Unexplored);
		for (int32 Y = 4; Y <= 11; ++Y)
		{
			for (int32 X = 4; X <= 11; ++X)
			{
				CircleCells[Y * 16 + X] = EGP_FoWState::Visible;
			}
		}
		FGP_FoWVisualMaskBuffers CircleMask;
		GPFoWVisualMask::EncodeFromStates(
			CircleMask, CircleCells, 16, 16, 200.0f, FVector2D::ZeroVector, 64, 64);
		GPFoWVisualMask::ApplySpatialFilter(CircleMask);
		float InteriorKnown = 0.0f;
		float InteriorVisible = 0.0f;
		float DeepKnown = 0.0f;
		float DeepVisible = 0.0f;
		float EdgeKnown = 0.0f;
		float EdgeVisible = 0.0f;
		GPFoWVisualMask::SampleBilinear(CircleMask, FVector2D(1600.0, 1600.0), InteriorKnown, InteriorVisible);
		GPFoWVisualMask::SampleBilinear(CircleMask, FVector2D(200.0, 200.0), DeepKnown, DeepVisible);
		GPFoWVisualMask::SampleBilinear(CircleMask, FVector2D(800.0, 1600.0), EdgeKnown, EdgeVisible);
		Expect(InteriorVisible > 0.85f && DeepKnown < 0.15f
			&& EdgeVisible > 0.05f && EdgeVisible < 0.95f,
			TEXT("O_SpatialSmoothingProducesIntermediateMaskSamples"));
		Expect(Team1Mirror->GetRevision() == EncodingRevisionBefore
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0)) == EGP_FoWState::Explored
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0)) == EGP_FoWState::Unexplored,
			TEXT("P_VisualSmoothingDoesNotMutateLocalFoW"));

		FGP_FoWVisualMaskRuntime Runtime;
		FGP_FoWVisualMaskBuffers FirstTarget = Encoded;
		GPFoWVisualMask::ApplySpatialFilter(FirstTarget);
		GPFoWVisualMask::BeginNewTarget(Runtime, MoveTemp(FirstTarget), 1);
		float BlendKnownStart = 0.0f;
		float BlendVisibleStart = 0.0f;
		GPFoWVisualMask::SampleVisual(Runtime, FVector2D(150.0, 50.0), BlendKnownStart, BlendVisibleStart);
		Expect(Runtime.BlendAlpha == 0.0f && BlendVisibleStart < 0.5f,
			TEXT("Q_NewRevisionResetsInterpolation"));
		GPFoWVisualMask::AdvanceBlend(Runtime, GPFoWVisualMask::BlendDurationSeconds * 0.5f);
		float BlendKnownMid = 0.0f;
		float BlendVisibleMid = 0.0f;
		GPFoWVisualMask::SampleVisual(Runtime, FVector2D(150.0, 50.0), BlendKnownMid, BlendVisibleMid);
		Expect(FMath::IsNearlyEqual(Runtime.BlendAlpha, 0.5f, 0.05f)
			&& BlendVisibleMid > BlendVisibleStart
			&& BlendVisibleMid < 0.99f,
			TEXT("R_TemporalBlendAdvancesPreviousToTarget"));
		const int64 BlendRevisionBefore = Team1Mirror->GetRevision();
		GPFoWVisualMask::AdvanceBlend(Runtime, GPFoWVisualMask::BlendDurationSeconds);
		Expect(FMath::IsNearlyEqual(Runtime.BlendAlpha, 1.0f)
			&& Team1Mirror->GetRevision() == BlendRevisionBefore
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(1, 0)) == EGP_FoWState::Visible,
			TEXT("S_TemporalBlendDoesNotMutateGameplayState"));

		FGP_FoWVisualMaskBuffers SecondTarget = Encoded;
		GPFoWVisualMask::ApplySpatialFilter(SecondTarget);
		GPFoWVisualMask::BeginNewTarget(Runtime, MoveTemp(SecondTarget), 2);
		Expect(Runtime.MaskRevision == 2
			&& Runtime.PreviousRevision == 1
			&& FMath::IsNearlyZero(Runtime.BlendAlpha),
			TEXT("T_NewRevisionBakesAndResetsBlend"));

		const FVector2D MappingOrigin(-100000.0, -100000.0);
		const FVector2D MappingExtent(200000.0, 200000.0);
		const FVector2D CenterWorld(0.0, 0.0);
		const FVector2D CenterUV = GPFoWVisualMask::WorldXYToUV(CenterWorld, MappingOrigin, MappingExtent);
		const FVector2D OutsideUV = GPFoWVisualMask::WorldXYToUV(
			FVector2D(200000.0, 0.0), MappingOrigin, MappingExtent);
		Expect(CenterUV.Equals(FVector2D(0.5, 0.5), 0.001)
			&& GPFoWVisualMask::UVToWorldXY(CenterUV, MappingOrigin, MappingExtent).Equals(CenterWorld, 1.0)
			&& !GPFoWVisualMask::IsUVInBounds(OutsideUV),
			TEXT("U_WorldXYToUVIsDeterministicAndClamped"));

		UGP_LocalFoWComponent* Team2Mirror = NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		FGP_FoWPresentationUpdate Team2Initial = Initial(2, 7);
		Team2Initial.ExploredRanges.Add(Range(2, 1));
		Team2Initial.VisibleRanges.Add(Range(2, 1));
		Expect(Team2Mirror != nullptr && Team2Mirror->ApplyServerUpdate(Team2Initial),
			TEXT("V_SecondTeamMirrorAcceptsSnapshot"));
		FGP_FoWVisualMaskBuffers Team1Mask;
		FGP_FoWVisualMaskBuffers Team2Mask;
		GPFoWVisualMask::EncodeFromLocalFoW(Team1Mask, Team1Mirror, 16, 16);
		GPFoWVisualMask::EncodeFromLocalFoW(Team2Mask, Team2Mirror, 16, 16);
		float Team1Known = 0.0f;
		float Team1Visible = 0.0f;
		float Team2Known = 0.0f;
		float Team2Visible = 0.0f;
		GPFoWVisualMask::SampleBilinear(Team1Mask, FVector2D(150.0, 50.0), Team1Known, Team1Visible);
		GPFoWVisualMask::SampleBilinear(Team2Mask, FVector2D(150.0, 50.0), Team2Known, Team2Visible);
		Expect(Team1Visible > 0.9f && Team2Visible < 0.1f
			&& Team1Mirror->GetLocalTeamId() == 1
			&& Team2Mirror->GetLocalTeamId() == 2,
			TEXT("W_TeamMasksRemainDistinct"));

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
			TEXT("X_ProductionRendererSourceIsLocalFoWMirror"));

		const int32 BuildsBeforeCamera = PresentationSubsystem != nullptr
			? PresentationSubsystem->GetMaskBuildCount()
			: 0;
		const int64 RevisionBeforeCamera = PresentationSubsystem != nullptr
			? PresentationSubsystem->GetMaskRevision()
			: -1;
		if (PresentationSubsystem != nullptr)
		{
			PresentationSubsystem->DebugAdvanceBlend(0.016f);
		}
		Expect(PresentationSubsystem != nullptr
			&& PresentationSubsystem->GetMaskBuildCount() == BuildsBeforeCamera
			&& PresentationSubsystem->GetMaskRevision() == RevisionBeforeCamera,
			TEXT("Y_CameraMotionDoesNotRebuildLocalFoWMask"));

		Expect(PresentationSubsystem != nullptr
			&& PresentationSubsystem->GetPreviousMaskTexture() != nullptr
			&& PresentationSubsystem->GetTargetMaskTexture() != nullptr
			&& PresentationSubsystem->GetPreviousMaskTexture()
				!= PresentationSubsystem->GetTargetMaskTexture()
			&& Cast<UMaterialInstanceDynamic>(PresentationSubsystem->GetPostProcessMID()) != nullptr
			&& PresentationSubsystem->GetPostProcessMID()
				!= PresentationSubsystem->GetLoadedMaterialTemplate()
			&& PresentationSubsystem->GetMaskTextureResolution() == 1024,
			TEXT("Z_PerPlayerMaskResourcesAreNotGloballyShared"));

		int32 OverlayWidgetCount = 0;
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			if (It->GetClass()->GetName().Contains(TEXT("FoWWorldOverlay")))
			{
				++OverlayWidgetCount;
			}
		}
		Expect(OverlayWidgetCount == 0
			&& !UGP_FoWWorldPresentationSubsystem::StaticClass()->IsChildOf(UActorComponent::StaticClass()),
			TEXT("AA_NoPerCellComponentOrLegacyOverlayWidget"));

		Expect(UGP_FoWWorldPresentationSubsystem::StaticClass()->FindFunctionByName(
				TEXT("GetStateForTeamAtWorldLocation")) == nullptr
			&& UGP_LocalFoWComponent::StaticClass()->FindFunctionByName(
				TEXT("GetStateForTeamAtWorldLocation")) == nullptr,
			TEXT("AB_NoArbitraryTeamQuerySurface"));

		UGP_LocalFoWComponent* MillionCellMirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		FGP_FoWPresentationUpdate MillionCellInitial = Initial(1, 1, FIntPoint(1000, 1000), 200.0f);
		Expect(MillionCellMirror != nullptr
			&& MillionCellMirror->ApplyServerUpdate(MillionCellInitial)
			&& MillionCellMirror->GetGridDimensions() == FIntPoint(1000, 1000)
			&& UGP_FoWWorldPresentationSubsystem::GetMaskTextureResolution() < 1000 * 4,
			TEXT("AC_PresentationTextureIsIndependentOfGameplayGrid"));

		const int64 ProductionRevisionBefore =
			PresentationSubsystem != nullptr ? PresentationSubsystem->GetLastUpdateRevision() : -1;
		if (PresentationSubsystem != nullptr)
		{
			const bool bWasEnabled = PresentationSubsystem->IsVisualizationEnabled();
			PresentationSubsystem->SetVisualizationEnabled(false);
			PresentationSubsystem->SetVisualizationEnabled(true);
			PresentationSubsystem->SetVisualizationEnabled(bWasEnabled);
		}
		Expect(PresentationSubsystem != nullptr
			&& PresentationSubsystem->GetLastUpdateRevision() == ProductionRevisionBefore,
			TEXT("AD_EnableDisableIsPresentationOnly"));

		AGP_GameState* GameState = World->GetGameState<AGP_GameState>();
		UGP_FogOfWarComponent* AuthorityFoW =
			GameState != nullptr ? GameState->GetFogOfWarComponent() : nullptr;
		const EGP_FoWState AuthorityBefore = AuthorityFoW != nullptr
			? AuthorityFoW->GetStateForTeamAtWorldLocation(1, CellLocation(0, 0))
			: EGP_FoWState::Unexplored;
		Expect(AuthorityFoW == nullptr
			|| AuthorityFoW->GetStateForTeamAtWorldLocation(1, CellLocation(0, 0)) == AuthorityBefore,
			TEXT("AE_RendererDoesNotMutateGameplayAuthority"));
		Expect(AuthorityFoW == nullptr
			|| (FMath::IsNearlyEqual(AuthorityFoW->GetCellSizeCm(), 200.0f)
				&& FMath::IsNearlyEqual(AuthorityFoW->GetUpdateIntervalSeconds(), 0.2f)),
			TEXT("AF_GameplayGridRemains200cmFiveHz"));

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
