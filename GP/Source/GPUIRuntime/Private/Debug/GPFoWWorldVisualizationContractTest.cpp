// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Combat/GPCombatPresentationComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Scene.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "CoreGlobals.h"
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

	static bool MaterialContainsWorldPositionNode(const UMaterial* Material)
	{
		if (Material == nullptr)
		{
			return false;
		}
#if WITH_EDITORONLY_DATA
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (Cast<UMaterialExpressionWorldPosition>(Expression) != nullptr)
			{
				return true;
			}
		}
#endif
		return false;
	}

	static bool MaterialCustomContains(const UMaterial* Material, const TCHAR* Needle)
	{
		if (Material == nullptr)
		{
			return false;
		}
#if WITH_EDITORONLY_DATA
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (const UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression))
			{
				if (Custom->Code.Contains(Needle))
				{
					return true;
				}
			}
		}
#endif
		return false;
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

		Team1Mirror->DebugConsumeWorldLocationQueryCount();
		TArray<FColor> EncodedPixels;
		int32 EncodedWidth = 0;
		int32 EncodedHeight = 0;
		FVector2D EncodedOrigin = FVector2D::ZeroVector;
		FVector2D EncodedExtent = FVector2D::ZeroVector;
		const bool bEncoded = GPFoWVisualMask::EncodePackedFromLocalFoW(
			EncodedPixels,
			EncodedWidth,
			EncodedHeight,
			EncodedOrigin,
			EncodedExtent,
			Team1Mirror);
		const int32 QueriesDuringEncode = Team1Mirror->DebugConsumeWorldLocationQueryCount();
		Expect(bEncoded && EncodedWidth == 4 && EncodedHeight == 4 && EncodedPixels.Num() == 16
			&& QueriesDuringEncode == 0,
			TEXT("K1_DirectLocalFoWBulkExtractionDoesNotQueryWorldLocations"));
		Expect(EncodedPixels.IsValidIndex(0) && EncodedPixels[0] == FColor(255, 0, 0, 255)
			&& EncodedPixels.IsValidIndex(1) && EncodedPixels[1] == FColor(255, 255, 0, 255)
			&& EncodedPixels.IsValidIndex(2) && EncodedPixels[2] == FColor(0, 0, 0, 255),
			TEXT("K2_PackedKnownVisibleChannelsMatchGameplayCells"));

		float KnownExplored = 0.0f;
		float VisibleExplored = 0.0f;
		float KnownVisible = 0.0f;
		float VisibleVisible = 0.0f;
		float KnownHidden = 0.0f;
		float VisibleHidden = 0.0f;
		GPFoWVisualMask::SamplePackedBilinear(
			EncodedPixels, EncodedWidth, EncodedHeight,
			FVector2D(50.0, 50.0), EncodedOrigin, EncodedExtent, KnownExplored, VisibleExplored);
		GPFoWVisualMask::SamplePackedBilinear(
			EncodedPixels, EncodedWidth, EncodedHeight,
			FVector2D(150.0, 50.0), EncodedOrigin, EncodedExtent, KnownVisible, VisibleVisible);
		GPFoWVisualMask::SamplePackedBilinear(
			EncodedPixels, EncodedWidth, EncodedHeight,
			FVector2D(250.0, 50.0), EncodedOrigin, EncodedExtent, KnownHidden, VisibleHidden);
		Expect(KnownExplored > 0.9f && VisibleExplored < 0.1f
			&& KnownVisible > 0.9f && VisibleVisible > 0.9f
			&& KnownHidden < 0.1f && VisibleHidden < 0.1f,
			TEXT("K3_PackedMaskWorldXYSamplingIsCorrect"));

		const FLinearColor UnexploredColor = GPFoWVisualMask::ComposeSceneColorFromPacked(
			SceneColor, FColor(0, 0, 0, 255), true);
		const FLinearColor ExploredColor = GPFoWVisualMask::ComposeSceneColorFromPacked(
			SceneColor, FColor(255, 0, 0, 255), true);
		const FLinearColor VisibleColor = GPFoWVisualMask::ComposeSceneColorFromPacked(
			SceneColor, FColor(255, 255, 0, 255), true);
		Expect(UnexploredColor.Equals(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f)),
			TEXT("L_UnexploredCompositionIsBlack"));
		Expect(ExploredColor.R < SceneColor.R && ExploredColor.R > 0.05f
			&& FMath::IsNearlyEqual(ExploredColor.R, SceneColor.R * GPFoWVisualMask::ExploredDimFactor, 0.01f),
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
		TArray<FColor> CirclePixels;
		int32 CircleWidth = 0;
		int32 CircleHeight = 0;
		GPFoWVisualMask::EncodePackedFromStates(
			CirclePixels, CircleWidth, CircleHeight,
			CircleCells, 16, 16, 200.0f, FVector2D::ZeroVector);
		float InteriorKnown = 0.0f;
		float InteriorVisible = 0.0f;
		float DeepKnown = 0.0f;
		float DeepVisible = 0.0f;
		float EdgeKnown = 0.0f;
		float EdgeVisible = 0.0f;
		const FVector2D CircleExtent(3200.0, 3200.0);
		GPFoWVisualMask::SamplePacked9Tap(
			CirclePixels, CircleWidth, CircleHeight,
			FVector2D(1600.0, 1600.0), FVector2D::ZeroVector, CircleExtent,
			GPFoWVisualMask::BlurRadiusTexels, InteriorKnown, InteriorVisible);
		GPFoWVisualMask::SamplePacked9Tap(
			CirclePixels, CircleWidth, CircleHeight,
			FVector2D(200.0, 200.0), FVector2D::ZeroVector, CircleExtent,
			GPFoWVisualMask::BlurRadiusTexels, DeepKnown, DeepVisible);
		GPFoWVisualMask::SamplePacked9Tap(
			CirclePixels, CircleWidth, CircleHeight,
			FVector2D(800.0, 1600.0), FVector2D::ZeroVector, CircleExtent,
			GPFoWVisualMask::BlurRadiusTexels, EdgeKnown, EdgeVisible);
		Expect(InteriorVisible > 0.85f && DeepKnown < 0.15f
			&& EdgeVisible > 0.05f && EdgeVisible < 0.95f
			&& !UGP_FoWWorldPresentationSubsystem::UsesCpuSpatialBlur(),
			TEXT("O_GpuSpatialSmoothingProducesSoftMaskBoundary"));
		Expect(Team1Mirror->GetRevision() == 1
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0)) == EGP_FoWState::Explored
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0)) == EGP_FoWState::Unexplored,
			TEXT("P_VisualSmoothingDoesNotMutateLocalFoW"));

		FGP_FoWVisualMaskRuntime Runtime;
		GPFoWVisualMask::BeginNewTarget(
			Runtime, EncodedWidth, EncodedHeight, EncodedOrigin, EncodedExtent, 1);
		Expect(Runtime.BlendAlpha == 0.0f
			&& !UGP_FoWWorldPresentationSubsystem::UsesCpuTemporalLerp(),
			TEXT("Q_NewRevisionResetsGpuBlendAlphaWithoutCpuLerp"));
		GPFoWVisualMask::AdvanceBlend(Runtime, GPFoWVisualMask::BlendDurationSeconds * 0.5f);
		Expect(FMath::IsNearlyEqual(Runtime.BlendAlpha, 0.5f, 0.05f),
			TEXT("R_TemporalBlendAdvancesScalarOnly"));
		const int64 BlendRevisionBefore = Team1Mirror->GetRevision();
		GPFoWVisualMask::AdvanceBlend(Runtime, GPFoWVisualMask::BlendDurationSeconds);
		Expect(FMath::IsNearlyEqual(Runtime.BlendAlpha, 1.0f)
			&& Team1Mirror->GetRevision() == BlendRevisionBefore
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(1, 0)) == EGP_FoWState::Visible,
			TEXT("S_TemporalBlendDoesNotMutateGameplayState"));

		GPFoWVisualMask::BeginNewTarget(
			Runtime, EncodedWidth, EncodedHeight, EncodedOrigin, EncodedExtent, 2);
		Expect(Runtime.MaskRevision == 2
			&& Runtime.PreviousRevision == 1
			&& FMath::IsNearlyZero(Runtime.BlendAlpha),
			TEXT("T_NewRevisionResetsBlendWithoutBakingMillionSamples"));

		const FVector2D MappingOrigin(-100000.0, -100000.0);
		const FVector2D MappingExtent(200000.0, 200000.0);
		const FVector2D CenterWorld(0.0, 0.0);
		const FVector2D CenterUV = GPFoWVisualMask::WorldXYToUV(CenterWorld, MappingOrigin, MappingExtent);
		const FVector2D OutsideUV = GPFoWVisualMask::WorldXYToUV(
			FVector2D(200000.0, 0.0), MappingOrigin, MappingExtent);
		Expect(CenterUV.Equals(FVector2D(0.5, 0.5), 0.001)
			&& GPFoWVisualMask::UVToWorldXY(CenterUV, MappingOrigin, MappingExtent).Equals(CenterWorld, 1.0)
			&& !GPFoWVisualMask::IsUVInBounds(OutsideUV)
			&& FCString::Strcmp(
				UGP_FoWWorldPresentationSubsystem::GetWorldPositionMethodName(),
				TEXT("SceneDepthSvPositionReconstruct")) == 0,
			TEXT("U_WorldXYToUVIsDeterministicAndUsesScenePixelReconstruction"));

		UGP_LocalFoWComponent* Team2Mirror = NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		FGP_FoWPresentationUpdate Team2Initial = Initial(2, 7);
		Team2Initial.ExploredRanges.Add(Range(2, 1));
		Team2Initial.VisibleRanges.Add(Range(2, 1));
		Expect(Team2Mirror != nullptr && Team2Mirror->ApplyServerUpdate(Team2Initial),
			TEXT("V_SecondTeamMirrorAcceptsSnapshot"));
		TArray<FColor> Team1Pixels;
		TArray<FColor> Team2Pixels;
		int32 Team1Width = 0;
		int32 Team1Height = 0;
		int32 Team2Width = 0;
		int32 Team2Height = 0;
		FVector2D Team1Origin, Team1Extent, Team2Origin, Team2Extent;
		GPFoWVisualMask::EncodePackedFromLocalFoW(
			Team1Pixels, Team1Width, Team1Height, Team1Origin, Team1Extent, Team1Mirror);
		GPFoWVisualMask::EncodePackedFromLocalFoW(
			Team2Pixels, Team2Width, Team2Height, Team2Origin, Team2Extent, Team2Mirror);
		float Team1Known = 0.0f;
		float Team1Visible = 0.0f;
		float Team2Known = 0.0f;
		float Team2Visible = 0.0f;
		GPFoWVisualMask::SamplePackedBilinear(
			Team1Pixels, Team1Width, Team1Height,
			FVector2D(150.0, 50.0), Team1Origin, Team1Extent, Team1Known, Team1Visible);
		GPFoWVisualMask::SamplePackedBilinear(
			Team2Pixels, Team2Width, Team2Height,
			FVector2D(150.0, 50.0), Team2Origin, Team2Extent, Team2Known, Team2Visible);
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

		FPostProcessSettings InjectedSettings;
		const int32 LocalPlayerIndex = LocalPlayer != nullptr ? LocalPlayer->GetControllerId() : INDEX_NONE;
		const AActor* ViewTarget = GPPlayerController != nullptr ? GPPlayerController->GetViewTarget() : nullptr;
		const bool bInjected = PresentationSubsystem != nullptr
			&& PresentationSubsystem->TryInjectOwnedView(
				InjectedSettings,
				LocalPlayerIndex,
				ViewTarget,
				true,
				false,
				false,
				GFrameCounter,
				TEXT("ContractLocalGameView"));
		Expect(bInjected
			&& PresentationSubsystem != nullptr
			&& PresentationSubsystem->IsPostProcessBound()
			&& PresentationSubsystem->SettingsContainLocalBlendable(InjectedSettings)
			&& PresentationSubsystem->GetBlendableInjectionCount() > 0
			&& PresentationSubsystem->GetActualViewsSeen() > 0
			&& PresentationSubsystem->OwnsLocalGameView(
				LocalPlayerIndex, ViewTarget, true, false, false)
			&& !PresentationSubsystem->OwnsLocalGameView(
				999, nullptr, true, false, false)
			&& !PresentationSubsystem->OwnsLocalGameView(
				LocalPlayerIndex, ViewTarget, true, true, false),
			TEXT("X2_ActualLocalGameViewReceivesOwnedBlendable"));

		if (PresentationSubsystem != nullptr)
		{
			PresentationSubsystem->DebugSetVisualDebugMode(1);
		}
		Expect(PresentationSubsystem != nullptr
			&& PresentationSubsystem->GetVisualDebugMode() == 1
			&& PresentationSubsystem->GetMidDebugModeValue() > 0.5f,
			TEXT("X3_DebugFullscreenModeReachesMID"));
		if (PresentationSubsystem != nullptr)
		{
			PresentationSubsystem->DebugSetVisualDebugMode(0);
		}
		Expect(PresentationSubsystem->GetVisualDebugMode() == 0
			&& PresentationSubsystem->GetMidDebugModeValue() < 0.5f,
			TEXT("X4_DebugModeRestoresNormalFoW"));

		UMaterial* FoWMaterial = LoadObject<UMaterial>(
			nullptr,
			UGP_FoWWorldPresentationSubsystem::GetMaterialAssetPath());
		Expect(FoWMaterial != nullptr
			&& !MaterialContainsWorldPositionNode(FoWMaterial)
			&& MaterialCustomContains(FoWMaterial, TEXT("SvPositionToTranslatedWorld"))
			&& MaterialCustomContains(FoWMaterial, TEXT("ConvertToDeviceZ"))
			&& MaterialCustomContains(FoWMaterial, TEXT("if (DebugMode > 0.5)"))
			&& MaterialCustomContains(FoWMaterial, TEXT("SceneColor * ExploredDim"))
			&& MaterialCustomContains(FoWMaterial, TEXT("lerp(PrevMask, TargetMask"))
			&& MaterialCustomContains(FoWMaterial, TEXT("if (Ready < 0.5)")),
			TEXT("X5_MaterialUsesScenePixelWorldAndGpuCompose"));

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

		UTexture2D* PreviousBeforePingPong =
			PresentationSubsystem != nullptr ? PresentationSubsystem->GetPreviousMaskTexture() : nullptr;
		UTexture2D* TargetBeforePingPong =
			PresentationSubsystem != nullptr ? PresentationSubsystem->GetTargetMaskTexture() : nullptr;
		const int32 UploadsBeforePingPong =
			PresentationSubsystem != nullptr ? PresentationSubsystem->GetTargetUploadCount() : 0;
		TArray<FColor> PingPongPixels;
		const int32 PingPongWidth =
			PresentationSubsystem != nullptr ? PresentationSubsystem->GetMaskTextureWidth() : 0;
		const int32 PingPongHeight =
			PresentationSubsystem != nullptr ? PresentationSubsystem->GetMaskTextureHeight() : 0;
		PingPongPixels.Init(FColor(255, 0, 0, 255), PingPongWidth * PingPongHeight);
		const bool bPingPonged = PresentationSubsystem != nullptr
			&& PresentationSubsystem->DebugPingPongUploadPackedMask(
				PingPongPixels,
				PingPongWidth,
				PingPongHeight);
		Expect(bPingPonged
			&& PresentationSubsystem->GetPreviousMaskTexture() == TargetBeforePingPong
			&& PresentationSubsystem->GetTargetMaskTexture() == PreviousBeforePingPong
			&& PresentationSubsystem->GetPreviousMaskTexture()
				!= PresentationSubsystem->GetTargetMaskTexture()
			&& PresentationSubsystem->GetTargetUploadCount() == UploadsBeforePingPong + 1,
			TEXT("Z1_OneTargetUploadPerRevisionPingPongsResources"));

		Expect(PresentationSubsystem != nullptr
			&& Cast<UMaterialInstanceDynamic>(PresentationSubsystem->GetPostProcessMID()) != nullptr
			&& PresentationSubsystem->GetPostProcessMID()
				!= PresentationSubsystem->GetLoadedMaterialTemplate()
			&& PresentationSubsystem->GetMaskTextureResolution()
				== UGP_FoWWorldPresentationSubsystem::GetCanonicalMaskResolution()
			&& UGP_FoWWorldPresentationSubsystem::GetCanonicalMaskResolution() == 1000,
			TEXT("Z2_PerPlayerMaskResourcesAreNotGloballyShared"));

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
				TEXT("GetStateForTeamAtWorldLocation")) == nullptr
			&& !UGP_FoWWorldPresentationSubsystem::UsesWorldLocationQueriesForEncode(),
			TEXT("AB_NoArbitraryTeamQueryOrMillionWorldLocationEncode"));

		UGP_LocalFoWComponent* MillionCellMirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		FGP_FoWPresentationUpdate MillionCellInitial = Initial(1, 1, FIntPoint(1000, 1000), 200.0f);
		MillionCellInitial.ExploredRanges.Add(Range(0, 1));
		MillionCellInitial.VisibleRanges.Add(Range(1, 1));
		MillionCellMirror->DebugConsumeWorldLocationQueryCount();
		TArray<FColor> MillionPixels;
		const double EncodeStart = FPlatformTime::Seconds();
		const bool bMillionReady = MillionCellMirror != nullptr
			&& MillionCellMirror->ApplyServerUpdate(MillionCellInitial);
		MillionCellMirror->DebugConsumeWorldLocationQueryCount();
		const bool bMillionEncoded = bMillionReady && MillionCellMirror->BuildPresentationMaskRGBA(MillionPixels);
		const double EncodeMs = (FPlatformTime::Seconds() - EncodeStart) * 1000.0;
		const int32 MillionQueries = MillionCellMirror->DebugConsumeWorldLocationQueryCount();
		Expect(bMillionEncoded
			&& MillionPixels.Num() == 1000 * 1000
			&& MillionPixels[0] == FColor(255, 0, 0, 255)
			&& MillionPixels[1] == FColor(255, 255, 0, 255)
			&& MillionPixels[2] == FColor(0, 0, 0, 255)
			&& MillionQueries == 0
			&& EncodeMs < 250.0
			&& UGP_FoWWorldPresentationSubsystem::GetCanonicalMaskResolution() == 1000,
			TEXT("AC_DirectPackedEncodeIsOneToOneWithGameplayGrid"));
		UE_LOG(LogGPFoWWorldVisualizationContract, Display,
			TEXT("gp.FoW.RunWorldVisualizationContractTest measured MaskEncodeMs=%.3f MaskUploadMs=%.3f"),
			EncodeMs,
			PresentationSubsystem != nullptr ? PresentationSubsystem->GetLastMaskUploadMilliseconds() : 0.0);

		const int64 ProductionRevisionBefore =
			PresentationSubsystem != nullptr ? PresentationSubsystem->GetLastUpdateRevision() : -1;
		const bool bEnemyHiddenBeforeToggle =
			EnemyUnit != nullptr && EnemyUnit->IsHidden();
		if (PresentationSubsystem != nullptr)
		{
			const bool bWasEnabled = PresentationSubsystem->IsVisualizationEnabled();
			PresentationSubsystem->SetVisualizationEnabled(false);
			UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(EnemyUnit, 1, Team1Mirror);
			Expect(!PresentationSubsystem->IsVisualizationEnabled()
				&& EnemyUnit != nullptr
				&& EnemyUnit->IsHidden()
				&& bEnemyHiddenBeforeToggle,
				TEXT("AD1_VisualEnableOffDoesNotRevealEnemies"));
			PresentationSubsystem->SetVisualizationEnabled(true);
			PresentationSubsystem->SetVisualizationEnabled(bWasEnabled);
		}
		Expect(PresentationSubsystem != nullptr
			&& PresentationSubsystem->GetLastUpdateRevision() == ProductionRevisionBefore,
			TEXT("AD2_EnableDisableIsPresentationOnly"));

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
