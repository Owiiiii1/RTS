// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/UserWidget.h"
#include "Buildings/GPMainBase.h"
#include "Debug/GPHUDRootWidgetContractStub.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Resources/GPStorageComponent.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPLaunchMenuPresenter.h"
#include "Widgets/GPHUDRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPLaunchMenuPresentationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPLaunchMenuPresentationContractPrivate
{
	static bool RowMatches(
		const FGP_LaunchContainerRow& Row,
		int32 Index,
		float Stored,
		float Capacity,
		bool bReady)
	{
		const float ExpectedFill = Capacity > KINDA_SMALL_NUMBER
			? FMath::Clamp(Stored / Capacity, 0.0f, 1.0f)
			: 0.0f;
		return Row.Index == Index
			&& FMath::IsNearlyEqual(Row.StoredAmount, Stored, 0.05f)
			&& FMath::IsNearlyEqual(Row.Capacity, Capacity, 0.05f)
			&& FMath::IsNearlyEqual(Row.FillNormalized, ExpectedFill, 0.05f)
			&& Row.bIsReadyForLaunch == bReady
			&& Row.FillNormalized >= 0.0f
			&& Row.FillNormalized <= 1.0f;
	}

	static void RunLaunchMenuPresentationContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPLaunchMenuPresentationContract, Warning,
				TEXT("gp.UI.RunLaunchMenuPresentationContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPLaunchMenuPresentationContract, Log,
					TEXT("gp.UI.RunLaunchMenuPresentationContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPLaunchMenuPresentationContract, Error,
					TEXT("gp.UI.RunLaunchMenuPresentationContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_LaunchMenuPresenter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_LaunchMenuPresenter::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) == nullptr
			&& UGP_HUDViewModelSubsystem::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr,
			TEXT("A_NoTickOnLaunchMenuPresentationPath"));
		Expect(FindObject<UClass>(nullptr, TEXT("/Script/GPRuntime.GP_TEMP_S28P_PlanetaryFerroniteHUD")) == nullptr
			&& AGP_PlayerController::StaticClass()->FindPropertyByName(TEXT("PlanetaryFerroniteHUD")) == nullptr,
			TEXT("B_TEMPHUDRemainsRetired"));
		Expect(UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetLaunchContainerRows")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetLaunchContainerPresentations")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("CanLaunchReadyContainer")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetReadyLaunchContainerCount")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("RequestLaunchReadyContainer")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("BP_OnLaunchMenuChanged")) != nullptr,
			TEXT("C_HUDRootExposesLaunchMenuAPI"));
		Expect(AGP_PlayerController::StaticClass()->FindFunctionByName(
				TEXT("Server_RequestLaunchReadyContainer")) != nullptr,
			TEXT("D_LaunchRequestForwardsToExistingGameplayRPC"));

		AGP_GameState* GameState = World->GetGameState<AGP_GameState>();
		APlayerController* PlayerController = World->GetFirstPlayerController();
		Expect(GameState != nullptr && IsValid(PlayerController),
			TEXT("E_GameStateAndPlayerControllerPresent"));
		if (GameState == nullptr || !IsValid(PlayerController))
		{
			UE_LOG(LogGPLaunchMenuPresentationContract, Log,
				TEXT("gp.UI.RunLaunchMenuPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		constexpr int32 LocalTeamId = 97;
		constexpr int32 OtherTeamId = 98;

		UGP_LaunchMenuPresenter* Presenter =
			NewObject<UGP_LaunchMenuPresenter>(GetTransientPackage());
		Expect(Presenter != nullptr, TEXT("F_PresenterCreated"));
		if (Presenter == nullptr)
		{
			UE_LOG(LogGPLaunchMenuPresentationContract, Log,
				TEXT("gp.UI.RunLaunchMenuPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		int32 PresentationChangedCount = 0;
		Presenter->OnLaunchMenuPresentationChanged.AddLambda(
			[&PresentationChangedCount]()
			{
				++PresentationChangedCount;
			});

		const bool bInitializedWithoutBase = Presenter->Initialize(GameState, LocalTeamId);
		Expect(bInitializedWithoutBase
			&& Presenter->GetRows().Num() == 0
			&& Presenter->GetReadyLaunchContainerCount() == 0
			&& !Presenter->CanLaunchReadyContainer(),
			TEXT("G_NoMainBaseEmptyLaunchMenu"));

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* LocalBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-51000.0f, 4100.0f, 100.0f), FRotator::ZeroRotator, Params);
		AGP_MainBase* OtherBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-51000.0f, 5100.0f, 100.0f), FRotator::ZeroRotator, Params);
		if (!IsValid(LocalBase) || !IsValid(OtherBase))
		{
			Expect(false, TEXT("H_SpawnedTeamMainBases"));
			Presenter->Shutdown();
			UE_LOG(LogGPLaunchMenuPresentationContract, Log,
				TEXT("gp.UI.RunLaunchMenuPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}
		Expect(true, TEXT("H_SpawnedTeamMainBases"));

		LocalBase->SetTeamId(LocalTeamId);
		OtherBase->SetTeamId(OtherTeamId);
		GameState->RegisterMainBase(LocalBase);
		GameState->RegisterMainBase(OtherBase);

		UGP_StorageComponent* LocalStorage = LocalBase->GetStorageComponent();
		UGP_StorageComponent* OtherStorage = OtherBase->GetStorageComponent();
		Expect(IsValid(LocalStorage) && IsValid(OtherStorage), TEXT("I_StorageComponentsPresent"));
		if (IsValid(LocalStorage))
		{
			LocalStorage->ConfigureFromDefinition(100.0f, 5);
		}
		if (IsValid(OtherStorage))
		{
			OtherStorage->ConfigureFromDefinition(100.0f, 5);
		}

		Expect(Presenter->GetRows().Num() == 5, TEXT("J_LocalStorageProducesFiveRows"));
		if (Presenter->GetRows().Num() == 5)
		{
			Expect(RowMatches(Presenter->GetRows()[0], 0, 0.0f, 100.0f, false)
				&& RowMatches(Presenter->GetRows()[4], 4, 0.0f, 100.0f, false)
				&& Presenter->GetReadyLaunchContainerCount() == 0
				&& !Presenter->CanLaunchReadyContainer(),
				TEXT("K_EmptyContainersAreNotReady"));
		}

		if (IsValid(LocalStorage))
		{
			LocalStorage->AddPlanetaryFerronite(250.0f);
		}
		Expect(Presenter->GetRows().Num() == 5
			&& Presenter->GetReadyLaunchContainerCount() == 2
			&& Presenter->CanLaunchReadyContainer(),
			TEXT("L_PartialAndReadyStateProjected"));
		if (Presenter->GetRows().Num() == 5)
		{
			Expect(RowMatches(Presenter->GetRows()[0], 0, 100.0f, 100.0f, true)
				&& RowMatches(Presenter->GetRows()[1], 1, 100.0f, 100.0f, true)
				&& RowMatches(Presenter->GetRows()[2], 2, 50.0f, 100.0f, false)
				&& Presenter->GetRows()[0].FillNormalized >= 0.99f
				&& Presenter->GetRows()[2].FillNormalized > 0.49f
				&& Presenter->GetRows()[2].FillNormalized < 0.51f,
				TEXT("M_FillNormalizedClampedAndReadyFlagsMatchStorage"));
		}

		const int32 LocalReadyBeforeOther = Presenter->GetReadyLaunchContainerCount();
		const int32 ChangedBeforeOther = PresentationChangedCount;
		if (IsValid(OtherStorage))
		{
			OtherStorage->AddPlanetaryFerronite(180.0f);
		}
		Expect(Presenter->GetReadyLaunchContainerCount() == LocalReadyBeforeOther
			&& PresentationChangedCount == ChangedBeforeOther
			&& IsValid(OtherStorage)
			&& OtherStorage->GetReadyCount() >= 1,
			TEXT("N_OtherTeamStorageDoesNotAffectLocalLaunchMenu"));

		AGP_MainBase* ReplacementBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-51000.0f, 6100.0f, 100.0f), FRotator::ZeroRotator, Params);
		Expect(IsValid(ReplacementBase), TEXT("O_SpawnedReplacementMainBase"));
		UGP_StorageComponent* ReplacementStorage =
			IsValid(ReplacementBase) ? ReplacementBase->GetStorageComponent() : nullptr;
		GameState->UnregisterMainBase(LocalBase);
		Expect(Presenter->GetRows().Num() == 0
			&& Presenter->GetReadyLaunchContainerCount() == 0
			&& !Presenter->CanLaunchReadyContainer(),
			TEXT("P_LocalUnregisterClearsLaunchMenu"));

		if (IsValid(ReplacementBase) && IsValid(ReplacementStorage))
		{
			ReplacementBase->SetTeamId(LocalTeamId);
			ReplacementStorage->ConfigureFromDefinition(100.0f, 5);
			ReplacementStorage->AddPlanetaryFerronite(80.0f);
			GameState->RegisterMainBase(ReplacementBase);
		}
		Expect(Presenter->GetRows().Num() == 5
			&& Presenter->GetReadyLaunchContainerCount() == 0
			&& !Presenter->CanLaunchReadyContainer()
			&& Presenter->GetRows().Num() > 0
			&& RowMatches(Presenter->GetRows()[0], 0, 80.0f, 100.0f, false),
			TEXT("Q_ReplacementMainBaseRebindsToNewStorage"));

		if (IsValid(LocalStorage))
		{
			LocalStorage->AddPlanetaryFerronite(25.0f);
		}
		Expect(Presenter->GetRows().Num() == 5
			&& Presenter->GetReadyLaunchContainerCount() == 0
			&& RowMatches(Presenter->GetRows()[0], 0, 80.0f, 100.0f, false),
			TEXT("R_OldStorageNoLongerUpdatesPresentation"));

		const int32 BoundBeforeRebind = Presenter->GetBoundDelegateCount();
		Presenter->Initialize(GameState, LocalTeamId);
		Expect(Presenter->GetBoundDelegateCount() == BoundBeforeRebind
			&& Presenter->GetRows().Num() == 5,
			TEXT("S_RepeatedInitializeDoesNotDuplicateDelegates"));

		UGP_HUDRootWidget* HUD = CreateWidget<UGP_HUDRootWidget>(
			PlayerController, UGP_HUDRootWidgetContractStub::StaticClass());
		Expect(IsValid(HUD), TEXT("T_HUDRootStubCreated"));
		if (IsValid(HUD))
		{
			HUD->RequestLaunchReadyContainer();
			Expect(true, TEXT("U_HUDRequestLaunchReadyContainerForwardsWithoutCrash"));
		}

		Presenter->Shutdown();
		Presenter->Shutdown();
		Expect(Presenter->GetBoundDelegateCount() == 0
			&& Presenter->GetRows().Num() == 0
			&& Presenter->GetReadyLaunchContainerCount() == 0
			&& !Presenter->CanLaunchReadyContainer(),
			TEXT("V_ShutdownResetClearsPresentation"));

		GameState->UnregisterMainBase(ReplacementBase);
		GameState->UnregisterMainBase(OtherBase);
		if (IsValid(LocalBase))
		{
			LocalBase->Destroy();
		}
		if (IsValid(OtherBase))
		{
			OtherBase->Destroy();
		}
		if (IsValid(ReplacementBase))
		{
			ReplacementBase->Destroy();
		}

		UE_LOG(LogGPLaunchMenuPresentationContract, Log,
			TEXT("gp.UI.RunLaunchMenuPresentationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GLaunchMenuPresentationContract(
		TEXT("gp.UI.RunLaunchMenuPresentationContractTest"),
		TEXT("Run production HUD right-side launch menu presentation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunLaunchMenuPresentationContractTest));
}

#endif
