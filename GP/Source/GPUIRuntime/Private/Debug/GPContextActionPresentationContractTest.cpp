// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Command/GPCommandRequest.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "UnrealClient.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitBase.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"
#include "ViewModels/GPContextActionPresenter.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPSelectionViewModel.h"
#include "Widgets/GPHUDRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPContextActionPresentationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPContextActionPresentationContractPrivate
{
	static const FGP_ContextActionPresentation* FindAction(
		const TArray<FGP_ContextActionPresentation>& Actions,
		EGP_ContextActionId ActionId)
	{
		for (const FGP_ContextActionPresentation& Row : Actions)
		{
			if (Row.ActionId == ActionId)
			{
				return &Row;
			}
		}
		return nullptr;
	}

	static bool HasAction(
		const TArray<FGP_ContextActionPresentation>& Actions,
		EGP_ContextActionId ActionId,
		bool bEnabled)
	{
		const FGP_ContextActionPresentation* Row = FindAction(Actions, ActionId);
		return Row != nullptr && Row->bVisible && Row->bEnabled == bEnabled;
	}

	static bool HasNoAction(
		const TArray<FGP_ContextActionPresentation>& Actions,
		EGP_ContextActionId ActionId)
	{
		return FindAction(Actions, ActionId) == nullptr;
	}

	static void PrepareLocalView(APlayerController* PlayerController)
	{
		if (!IsValid(PlayerController))
		{
			return;
		}

		if (APawn* Pawn = PlayerController->GetPawn())
		{
			PlayerController->SetViewTarget(Pawn);
		}

		if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			CameraManager->SetGameCameraCutThisFrame();
			CameraManager->UpdateCamera(0.0f);
		}

		ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		UGameViewportClient* ViewportClient =
			LocalPlayer != nullptr ? LocalPlayer->ViewportClient : nullptr;
		int32 SizeX = 0;
		int32 SizeY = 0;
		PlayerController->GetViewportSize(SizeX, SizeY);
		if ((SizeX <= 1 || SizeY <= 1) && ViewportClient != nullptr && ViewportClient->Viewport != nullptr)
		{
			ViewportClient->Viewport->SetInitialSize(FIntPoint(1280, 720));
		}
	}

	static FVector ResolveMarqueeGround(APlayerController* PlayerController)
	{
		PrepareLocalView(PlayerController);

		FVector ViewLoc = FVector::ZeroVector;
		FRotator ViewRot = FRotator::ZeroRotator;
		if (IsValid(PlayerController))
		{
			PlayerController->GetPlayerViewPoint(ViewLoc, ViewRot);
		}

		FVector Ground = ViewLoc;
		const FVector Look = ViewRot.Vector();
		if (FMath::Abs(Look.Z) > 0.05f)
		{
			const float T = (100.0f - ViewLoc.Z) / Look.Z;
			if (T > 50.0f)
			{
				Ground = ViewLoc + Look * T;
			}
		}
		else if (APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr)
		{
			Ground = Pawn->GetActorLocation();
		}

		Ground.Z = 100.0f;
		return Ground;
	}

	static bool ProjectActor(APlayerController* PlayerController, const AActor* Actor, FVector2D& OutScreen)
	{
		if (!IsValid(PlayerController) || !IsValid(Actor))
		{
			return false;
		}
		return PlayerController->ProjectWorldLocationToScreen(Actor->GetActorLocation(), OutScreen, false);
	}

	static bool MakeCoveringRect(
		APlayerController* PlayerController,
		const TArray<AActor*>& Actors,
		FVector2D& OutMin,
		FVector2D& OutMax)
	{
		if (Actors.Num() == 0)
		{
			return false;
		}

		TArray<FVector2D> Points;
		Points.Reserve(Actors.Num());
		for (AActor* Actor : Actors)
		{
			FVector2D Screen = FVector2D::ZeroVector;
			if (!ProjectActor(PlayerController, Actor, Screen))
			{
				return false;
			}
			Points.Add(Screen);
		}

		OutMin = Points[0];
		OutMax = Points[0];
		for (const FVector2D& Point : Points)
		{
			OutMin.X = FMath::Min(OutMin.X, Point.X);
			OutMin.Y = FMath::Min(OutMin.Y, Point.Y);
			OutMax.X = FMath::Max(OutMax.X, Point.X);
			OutMax.Y = FMath::Max(OutMax.Y, Point.Y);
		}
		OutMin -= FVector2D(48.0f, 48.0f);
		OutMax += FVector2D(48.0f, 48.0f);
		return OutMax.X > OutMin.X && OutMax.Y > OutMin.Y;
	}

	static void NeutralizeAuthoredCombat(UWorld* World)
	{
		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (IsValid(Unit) && !Unit->IsA<AGP_Worker>())
			{
				Unit->SetTeamId(-1);
			}
		}
	}

	static bool SetUnitHealth(AGP_UnitBase* Unit, float Health)
	{
		UGP_AbilitySystemComponent* ASC = Unit != nullptr ? Unit->GetGPAbilitySystemComponent() : nullptr;
		if (ASC == nullptr)
		{
			return false;
		}
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), Health);
		return true;
	}

	template <typename TActor>
	static TActor* SpawnOwned(UWorld* World, const FVector& Location, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		TActor* Actor = World->SpawnActor<TActor>(
			TActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (Actor != nullptr)
		{
			Actor->SetTeamId(TeamId);
		}
		return Actor;
	}

	static void RunContextActionPresentationContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPContextActionPresentationContract, Warning,
				TEXT("gp.UI.RunContextActionPresentationContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (bOk)
			{
				UE_LOG(LogGPContextActionPresentationContract, Log,
					TEXT("gp.UI.RunContextActionPresentationContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPContextActionPresentationContract, Error,
					TEXT("gp.UI.RunContextActionPresentationContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_ContextActionPresenter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_ContextActionPresenter::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) == nullptr,
			TEXT("A0_NoTickOnContextActionPresenter"));
		Expect(UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetContextActionPresentations")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetContextActionMode")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("RequestContextAction")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("BP_OnContextActionsChanged")) != nullptr,
			TEXT("A0_HUDRootExposesContextActionAPI"));
		Expect(AGP_PlayerController::StaticClass()->FindFunctionByName(TEXT("RequestStopSelectedUnits")) != nullptr
			&& AGP_PlayerController::StaticClass()->FindFunctionByName(
				TEXT("SelectionHasAttackMoveEligibleUnit")) != nullptr
			&& AGP_PlayerController::StaticClass()->FindFunctionByName(TEXT("EnterAttackMoveMode")) != nullptr
			&& AGP_PlayerController::StaticClass()->FindFunctionByName(TEXT("Server_RequestCommand")) != nullptr,
			TEXT("A0_StopAndAttackMoveUseExistingPCSeams"));

		NeutralizeAuthoredCombat(World);
		FlushAsyncLoading();

		UGameInstance* GameInstance = World->GetGameInstance();
		ULocalPlayer* LocalPlayer =
			GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
		AGP_PlayerController* PlayerController =
			LocalPlayer != nullptr
				? Cast<AGP_PlayerController>(LocalPlayer->GetPlayerController(World))
				: Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		UGP_HUDViewModelSubsystem* Subsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
		UGP_SelectionComponent* Selection =
			PlayerController != nullptr ? PlayerController->GetSelectionComponent() : nullptr;
		UGP_ContextActionPresenter* Presenter =
			Subsystem != nullptr ? Subsystem->GetContextActionPresenter() : nullptr;
		UGP_SelectionViewModel* SelectionVM =
			Subsystem != nullptr ? Subsystem->GetSelectionViewModel() : nullptr;

		Expect(IsValid(PlayerController) && IsValid(Selection) && IsValid(Subsystem)
			&& IsValid(Presenter) && Presenter->GetOuter() == Subsystem,
			TEXT("A1_LocalSelectionAndContextPresenterPresent"));
		if (!IsValid(PlayerController) || !IsValid(Selection) || !IsValid(Presenter))
		{
			UE_LOG(LogGPContextActionPresentationContract, Log,
				TEXT("gp.UI.RunContextActionPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		int32 LocalTeamId = 1;
		if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
		{
			LocalTeamId = PlayerState->GetTeamId();
		}
		if (LocalTeamId < 1)
		{
			LocalTeamId = 1;
		}

		Selection->ClearAllSelectionState();
		Expect(Presenter->GetMode() == EGP_ContextActionMode::None
			&& Presenter->GetActions().Num() == 0,
			TEXT("A_NoneNoSelectionNoActions"));

		AGP_SalvageWalker* WalkerA = SpawnOwned<AGP_SalvageWalker>(
			World, FVector(-48000.0f, -12000.0f, 100.0f), LocalTeamId);
		AGP_SalvageWalker* WalkerB = SpawnOwned<AGP_SalvageWalker>(
			World, FVector(-48100.0f, -12000.0f, 100.0f), LocalTeamId);
		AGP_Worker* Worker = SpawnOwned<AGP_Worker>(
			World, FVector(-48200.0f, -12000.0f, 100.0f), LocalTeamId);
		AGP_LogisticsHub* Hub = SpawnOwned<AGP_LogisticsHub>(
			World, FVector(-51000.0f, 6100.0f, 100.0f), LocalTeamId);
		AGP_MainBase* MainBase = SpawnOwned<AGP_MainBase>(
			World, FVector(-51000.0f, 7100.0f, 100.0f), LocalTeamId);
		FlushAsyncLoading();

		Expect(IsValid(WalkerA) && IsValid(WalkerB) && IsValid(Worker)
			&& IsValid(Hub) && IsValid(MainBase),
			TEXT("A2_SpawnedContractActors"));
		if (!IsValid(WalkerA) || !IsValid(WalkerB) || !IsValid(Worker)
			|| !IsValid(Hub) || !IsValid(MainBase))
		{
			UE_LOG(LogGPContextActionPresentationContract, Log,
				TEXT("gp.UI.RunContextActionPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		auto DestroySpawned = [&]()
		{
			Selection->ClearAllSelectionState();
			if (IsValid(WalkerA)) { WalkerA->Destroy(); }
			if (IsValid(WalkerB)) { WalkerB->Destroy(); }
			if (IsValid(Worker)) { Worker->Destroy(); }
			if (IsValid(Hub)) { Hub->Destroy(); }
			if (IsValid(MainBase)) { MainBase->Destroy(); }
		};

		Selection->ReplaceSelectionWithUnit(WalkerA);
		Expect(Presenter->GetMode() == EGP_ContextActionMode::Unit, TEXT("B_UnitMode"));
		Expect(HasAction(Presenter->GetActions(), EGP_ContextActionId::Stop, true)
			&& HasAction(Presenter->GetActions(), EGP_ContextActionId::AttackMove, true)
			&& HasAction(Presenter->GetActions(), EGP_ContextActionId::Patrol, false)
			&& HasAction(Presenter->GetActions(), EGP_ContextActionId::Move, false)
			&& HasNoAction(Presenter->GetActions(), EGP_ContextActionId::Purchase),
			TEXT("B_UnitActionsStopAttackMovePatrolNoPurchase"));
		Expect(PlayerController->SelectionHasAttackMoveEligibleUnit(),
			TEXT("B_AttackMoveEligibilityFromPCQuery"));

		Selection->ReplaceSelectionWithUnit(Worker);
		Expect(Presenter->GetMode() == EGP_ContextActionMode::Unit, TEXT("C_WorkerUnitMode"));
		Expect(HasNoAction(Presenter->GetActions(), EGP_ContextActionId::Purchase)
			&& HasAction(Presenter->GetActions(), EGP_ContextActionId::AttackMove, false)
			&& !PlayerController->SelectionHasAttackMoveEligibleUnit(),
			TEXT("C_WorkerAttackMoveFollowsCapabilityNotClassHardcode"));

		Selection->ReplaceSelectionWithUnit(WalkerA);
		Selection->AddUnitToSelection(WalkerB);
		Expect(Presenter->GetMode() == EGP_ContextActionMode::UnitGroup
			&& Presenter->GetActions().Num() == 4
			&& HasAction(Presenter->GetActions(), EGP_ContextActionId::Stop, true)
			&& HasAction(Presenter->GetActions(), EGP_ContextActionId::AttackMove, true)
			&& HasNoAction(Presenter->GetActions(), EGP_ContextActionId::Purchase),
			TEXT("D_UnitGroupActionsFactual"));

		Selection->ReplaceSelectionWithUnit(Hub);
		Expect(Presenter->GetMode() == EGP_ContextActionMode::Building
			&& Presenter->GetActions().Num() == 0
			&& HasNoAction(Presenter->GetActions(), EGP_ContextActionId::Purchase),
			TEXT("E_BuildingModePurchaseAbsent"));

		Selection->ReplaceSelectionWithUnit(MainBase);
		Expect(Presenter->GetMode() == EGP_ContextActionMode::MainBase
			&& HasAction(Presenter->GetActions(), EGP_ContextActionId::Purchase, true)
			&& HasNoAction(Presenter->GetActions(), EGP_ContextActionId::Stop)
			&& HasNoAction(Presenter->GetActions(), EGP_ContextActionId::AttackMove)
			&& HasNoAction(Presenter->GetActions(), EGP_ContextActionId::Move)
			&& HasNoAction(Presenter->GetActions(), EGP_ContextActionId::Patrol),
			TEXT("F_MainBasePurchaseOnly"));

		Selection->ReplaceSelectionWithUnit(WalkerA);
		Expect(Presenter->GetMode() == EGP_ContextActionMode::Unit, TEXT("G_UnitBeforeMainBase"));
		Selection->ReplaceSelectionWithUnit(MainBase);
		Expect(Presenter->GetMode() == EGP_ContextActionMode::MainBase
			&& HasAction(Presenter->GetActions(), EGP_ContextActionId::Purchase, true),
			TEXT("G_MainBaseAfterUnit"));
		Selection->ClearSelection();
		Expect(Presenter->GetMode() == EGP_ContextActionMode::None
			&& Presenter->GetActions().Num() == 0
			&& Presenter->GetPanelState() == EGP_ContextActionPanelState::Actions,
			TEXT("G_ClearReturnsNoneImmediately"));

		Selection->ReplaceSelectionWithUnit(WalkerA);
		const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
		FGP_CommandRequest MoveRequest;
		MoveRequest.CommandTag = GPTags.Command_Move;
		MoveRequest.IssuingUnits.Add(WalkerA);
		MoveRequest.TargetLocation = WalkerA->GetActorLocation() + FVector(800.0f, 0.0f, 0.0f);
		PlayerController->Server_RequestCommand(MoveRequest);
		UGP_UnitCommandComponent* WalkerCommand = WalkerA->GetUnitCommandComponent();
		Expect(WalkerCommand != nullptr && WalkerCommand->HasHeldCommand(),
			TEXT("H_MoveHeldViaCanonicalServerRequest"));
		Presenter->RequestContextAction(EGP_ContextActionId::Stop);
		Expect(WalkerCommand != nullptr && !WalkerCommand->HasHeldCommand(),
			TEXT("H_StopDispatchClearsHeldWithoutDirectExecutorCall"));

		Expect(!PlayerController->IsAttackMoveModeActive(), TEXT("I_AttackMoveInactiveBeforeRequest"));
		Presenter->RequestContextAction(EGP_ContextActionId::AttackMove);
		Expect(PlayerController->IsAttackMoveModeActive(),
			TEXT("I_AttackMoveRequestEntersExistingPCMode"));
		PlayerController->CancelAttackMoveMode();
		Expect(!PlayerController->IsAttackMoveModeActive(), TEXT("I_ExistingPCModeCancelled"));

		Selection->ReplaceSelectionWithUnit(Worker);
		AGP_Worker* WorkerB = SpawnOwned<AGP_Worker>(
			World, FVector(-48300.0f, -12000.0f, 100.0f), LocalTeamId);
		FlushAsyncLoading();
		Expect(IsValid(WorkerB), TEXT("J_SecondWorkerSpawned"));
		if (IsValid(WorkerB))
		{
			Selection->AddUnitToSelection(WorkerB);
			Expect(SelectionVM != nullptr
				&& SelectionVM->Mode == EGP_SelectionPresentationMode::Group
				&& SelectionVM->GetGroupRows().Num() == 2,
				TEXT("J_GroupPresentationReady"));

			int32 PresentationChanged = 0;
			FDelegateHandle PresentationHandle;
			if (SelectionVM != nullptr)
			{
				PresentationHandle = SelectionVM->OnSelectionPresentationChanged.AddLambda(
					[&PresentationChanged]()
					{
						++PresentationChanged;
					});
			}

			const float Row0Before = SelectionVM != nullptr && SelectionVM->GetGroupRows().Num() > 0
				? SelectionVM->GetGroupRows()[0].CurrentHealth
				: 0.0f;
			Expect(SetUnitHealth(WorkerB, 33.0f), TEXT("J_DamageGroupRow"));
			Expect(SelectionVM != nullptr
				&& SelectionVM->GetGroupRows().Num() == 2
				&& FMath::IsNearlyEqual(SelectionVM->GetGroupRows()[1].CurrentHealth, 33.0f, 0.05f)
				&& FMath::IsNearlyEqual(SelectionVM->GetGroupRows()[0].CurrentHealth, Row0Before, 0.05f)
				&& PresentationChanged > 0,
				TEXT("J_GroupRowHealthPushFiresPresentationChanged"));

			if (SelectionVM != nullptr && PresentationHandle.IsValid())
			{
				SelectionVM->OnSelectionPresentationChanged.Remove(PresentationHandle);
			}

			if (UGP_HUDRootWidget* HUD = Subsystem->GetProductionHUDWidget())
			{
				const TArray<FGP_SelectionGroupRow> HUDRows = HUD->GetSelectionGroupRows();
				Expect(HUDRows.Num() == 2
					&& FMath::IsNearlyEqual(HUDRows[1].CurrentHealth, 33.0f, 0.05f),
					TEXT("J_HUDRootGroupRowsMatchVM"));
			}

			WorkerB->Destroy();
		}

		Selection->ReplaceSelectionWithUnit(MainBase);
		Presenter->RequestContextAction(EGP_ContextActionId::Purchase);
		Expect(Presenter->GetPanelState() == EGP_ContextActionPanelState::PurchaseRoot,
			TEXT("F2_PurchaseOpensLocalPurchaseRoot"));

		Presenter->RequestContextAction(EGP_ContextActionId::Patrol);
		Expect(Presenter->GetMode() == EGP_ContextActionMode::MainBase,
			TEXT("PatrolDisabledIsNoOp"));

		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (IsValid(Unit))
			{
				Unit->SetTeamId(-1);
			}
		}

		const FVector MarqueeGround = ResolveMarqueeGround(PlayerController);
		AGP_Worker* MarqueeWorkerA = SpawnOwned<AGP_Worker>(
			World, MarqueeGround + FVector(180.0f, 0.0f, 0.0f), LocalTeamId);
		AGP_Worker* MarqueeWorkerB = SpawnOwned<AGP_Worker>(
			World, MarqueeGround + FVector(-180.0f, 0.0f, 0.0f), LocalTeamId);
		AGP_MainBase* MarqueeBase = SpawnOwned<AGP_MainBase>(
			World, MarqueeGround + FVector(0.0f, 220.0f, 0.0f), LocalTeamId);
		FlushAsyncLoading();
		Expect(IsValid(MarqueeWorkerA) && IsValid(MarqueeWorkerB) && IsValid(MarqueeBase),
			TEXT("K0_MarqueeClusterSpawned"));
		if (IsValid(MarqueeWorkerA) && IsValid(MarqueeWorkerB) && IsValid(MarqueeBase))
		{
			TArray<AActor*> MarqueeActors;
			MarqueeActors.Add(MarqueeWorkerA);
			MarqueeActors.Add(MarqueeWorkerB);
			MarqueeActors.Add(MarqueeBase);
			FVector2D MarqueeMin = FVector2D::ZeroVector;
			FVector2D MarqueeMax = FVector2D::ZeroVector;
			PrepareLocalView(PlayerController);
			const bool bMarqueeRect = MakeCoveringRect(
				PlayerController, MarqueeActors, MarqueeMin, MarqueeMax);
			Expect(bMarqueeRect, TEXT("K1_ProjectedUnitBuildingMarquee"));
			Selection->ClearAllSelectionState();
			if (bMarqueeRect)
			{
				PlayerController->ApplyMarqueeSelectionForContract(MarqueeMin, MarqueeMax);
			}
			Expect(Presenter->GetMode() == EGP_ContextActionMode::UnitGroup
				&& Selection->GetSelectionCount() == 2
				&& Selection->IsUnitSelected(MarqueeWorkerA)
				&& Selection->IsUnitSelected(MarqueeWorkerB)
				&& !Selection->IsUnitSelected(MarqueeBase),
				TEXT("K_MarqueeUnitsPlusBuildingIsUnitGroupNotMixedNone"));
			Selection->ClearAllSelectionState();
			MarqueeWorkerA->Destroy();
			MarqueeWorkerB->Destroy();
			MarqueeBase->Destroy();
		}

		DestroySpawned();
		UE_LOG(LogGPContextActionPresentationContract, Log,
			TEXT("gp.UI.RunContextActionPresentationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GContextActionPresentationContract(
		TEXT("gp.UI.RunContextActionPresentationContractTest"),
		TEXT("Run production HUD Context Action presenter contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunContextActionPresentationContractTest));
}

#endif
