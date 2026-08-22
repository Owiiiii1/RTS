// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPMainBase.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerState.h"
#include "Resources/GPStorageComponent.h"
#include "ViewModels/GPResourceViewModel.h"
#include "ViewModels/GPResourceViewModelAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPPlanetFerronitePresentationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPPlanetFerronitePresentationContractPrivate
{
	static void RunPlanetFerronitePresentationContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPPlanetFerronitePresentationContract, Warning,
				TEXT("gp.UI.RunPlanetFerronitePresentationContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPPlanetFerronitePresentationContract, Log,
					TEXT("gp.UI.RunPlanetFerronitePresentationContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPPlanetFerronitePresentationContract, Error,
					TEXT("gp.UI.RunPlanetFerronitePresentationContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_ResourceViewModelAdapter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_ResourceViewModelAdapter::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) == nullptr
			&& UGP_ResourceViewModel::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr,
			TEXT("A_NoTickOnPlanetFerronitePresentationPath"));

		AGP_GameState* GameState = World->GetGameState<AGP_GameState>();
		APlayerController* PlayerController = World->GetFirstPlayerController();
		AGP_PlayerState* LocalPlayerState =
			PlayerController != nullptr ? PlayerController->GetPlayerState<AGP_PlayerState>() : nullptr;
		Expect(GameState != nullptr && IsValid(LocalPlayerState), TEXT("B_GameStateAndLocalPlayerStatePresent"));
		if (GameState == nullptr || !IsValid(LocalPlayerState))
		{
			UE_LOG(LogGPPlanetFerronitePresentationContract, Log,
				TEXT("gp.UI.RunPlanetFerronitePresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		constexpr int32 LocalTeamId = 93;
		constexpr int32 OtherTeamId = 94;

		UGP_ResourceViewModel* ResourceVM = NewObject<UGP_ResourceViewModel>(GetTransientPackage());
		UGP_ResourceViewModelAdapter* Adapter =
			NewObject<UGP_ResourceViewModelAdapter>(GetTransientPackage());
		Expect(ResourceVM != nullptr && Adapter != nullptr, TEXT("C_ViewModelAndAdapterCreated"));
		if (ResourceVM == nullptr || Adapter == nullptr)
		{
			UE_LOG(LogGPPlanetFerronitePresentationContract, Log,
				TEXT("gp.UI.RunPlanetFerronitePresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		ResourceVM->SetPlanetFerronite(99.0f);
		const bool bInitializedWithoutBase = Adapter->Initialize(
			ResourceVM, LocalPlayerState, nullptr, GameState, LocalTeamId);
		Expect(bInitializedWithoutBase
			&& FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, 0.0f),
			TEXT("D_NoMainBasePlanetFerroniteZero"));

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* LocalBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-49000.0f, 4100.0f, 100.0f), FRotator::ZeroRotator, Params);
		AGP_MainBase* OtherBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-49000.0f, 5100.0f, 100.0f), FRotator::ZeroRotator, Params);
		if (!IsValid(LocalBase) || !IsValid(OtherBase))
		{
			Expect(false, TEXT("E_SpawnedTeamMainBases"));
			Adapter->Shutdown();
			UE_LOG(LogGPPlanetFerronitePresentationContract, Log,
				TEXT("gp.UI.RunPlanetFerronitePresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}
		Expect(true, TEXT("E_SpawnedTeamMainBases"));

		LocalBase->SetTeamId(LocalTeamId);
		OtherBase->SetTeamId(OtherTeamId);
		GameState->RegisterMainBase(LocalBase);
		GameState->RegisterMainBase(OtherBase);

		UGP_StorageComponent* LocalStorage = LocalBase->GetStorageComponent();
		UGP_StorageComponent* OtherStorage = OtherBase->GetStorageComponent();
		Expect(IsValid(LocalStorage) && IsValid(OtherStorage), TEXT("F_StorageComponentsPresent"));
		if (IsValid(LocalStorage))
		{
			LocalStorage->ConfigureFromDefinition(100.0f, 5);
		}
		if (IsValid(OtherStorage))
		{
			OtherStorage->ConfigureFromDefinition(100.0f, 5);
		}

		Expect(GameState->FindMainBaseForTeamClientSafe(LocalTeamId) == LocalBase
			&& GameState->FindMainBaseForTeamClientSafe(OtherTeamId) == OtherBase,
			TEXT("G_ClientSafeMainBaseIsTeamScoped"));
		Expect(FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, 0.0f),
			TEXT("H_ResolvedEmptyStorageProjectsZero"));

		if (IsValid(LocalStorage))
		{
			LocalStorage->AddPlanetaryFerronite(250.0f);
		}
		const float LocalExact = IsValid(LocalStorage) ? LocalStorage->GetTotalStored() : -1.0f;
		Expect(FMath::IsNearlyEqual(LocalExact, 250.0f, 0.05f)
			&& FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, LocalExact, 0.05f),
			TEXT("I_LocalStorageExactTotalIsProjected"));

		if (IsValid(LocalStorage))
		{
			LocalStorage->AddPlanetaryFerronite(40.0f);
		}
		const float LocalAfterChange = IsValid(LocalStorage) ? LocalStorage->GetTotalStored() : -1.0f;
		Expect(FMath::IsNearlyEqual(LocalAfterChange, 290.0f, 0.05f)
			&& FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, LocalAfterChange, 0.05f),
			TEXT("J_StorageChangeUpdatesPlanetFerronite"));

		const float LocalBeforeOther = ResourceVM->PlanetFerronite;
		if (IsValid(OtherStorage))
		{
			OtherStorage->AddPlanetaryFerronite(180.0f);
		}
		Expect(FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, LocalBeforeOther, 0.05f)
			&& IsValid(OtherStorage)
			&& FMath::IsNearlyEqual(OtherStorage->GetTotalStored(), 180.0f, 0.05f),
			TEXT("K_OtherTeamStorageDoesNotUpdateLocalPlanetFerronite"));

		AGP_MainBase* ReplacementBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-49000.0f, 6100.0f, 100.0f), FRotator::ZeroRotator, Params);
		Expect(IsValid(ReplacementBase), TEXT("L_SpawnedReplacementMainBase"));
		UGP_StorageComponent* ReplacementStorage =
			IsValid(ReplacementBase) ? ReplacementBase->GetStorageComponent() : nullptr;
		GameState->UnregisterMainBase(LocalBase);
		Expect(FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, 0.0f),
			TEXT("M_LocalUnregisterClearsPlanetFerronite"));

		if (IsValid(ReplacementBase) && IsValid(ReplacementStorage))
		{
			ReplacementBase->SetTeamId(LocalTeamId);
			ReplacementStorage->ConfigureFromDefinition(100.0f, 5);
			ReplacementStorage->AddPlanetaryFerronite(80.0f);
			GameState->RegisterMainBase(ReplacementBase);
		}
		const float ReplacementExact =
			IsValid(ReplacementStorage) ? ReplacementStorage->GetTotalStored() : -1.0f;
		Expect(FMath::IsNearlyEqual(ReplacementExact, 80.0f, 0.05f)
			&& FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, ReplacementExact, 0.05f),
			TEXT("N_ReplacementMainBaseRebindsToNewStorage"));

		if (IsValid(LocalStorage))
		{
			LocalStorage->AddPlanetaryFerronite(25.0f);
		}
		Expect(FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, ReplacementExact, 0.05f),
			TEXT("O_OldStorageNoLongerAffectsViewModel"));

		const int32 BoundBeforeRebind = Adapter->GetBoundDelegateCount();
		Adapter->Initialize(ResourceVM, LocalPlayerState, nullptr, GameState, LocalTeamId);
		Expect(Adapter->GetBoundDelegateCount() == BoundBeforeRebind
			&& FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, ReplacementExact, 0.05f),
			TEXT("P_RepeatedInitializeDoesNotDuplicateDelegates"));

		Adapter->Shutdown();
		Adapter->Shutdown();
		Expect(Adapter->GetBoundDelegateCount() == 0
			&& FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, 0.0f),
			TEXT("Q_ShutdownResetClearsPresentationValue"));

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

		UE_LOG(LogGPPlanetFerronitePresentationContract, Log,
			TEXT("gp.UI.RunPlanetFerronitePresentationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GPlanetFerronitePresentationContract(
		TEXT("gp.UI.RunPlanetFerronitePresentationContractTest"),
		TEXT("Run production HUD PlanetFerronite presentation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunPlanetFerronitePresentationContractTest));
}

#endif
