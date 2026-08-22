// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPMainBase.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPStorageComponent.h"
#include "ViewModels/GPMatchViewModel.h"
#include "ViewModels/GPMatchViewModelAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPThreatPresentationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPThreatPresentationContractPrivate
{
	static void RunThreatPresentationContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPThreatPresentationContract, Warning,
				TEXT("gp.UI.RunThreatPresentationContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPThreatPresentationContract, Log,
					TEXT("gp.UI.RunThreatPresentationContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPThreatPresentationContract, Error,
					TEXT("gp.UI.RunThreatPresentationContractTest FAIL: %s"), Label);
			}
		};

		Expect(FMath::IsNearlyEqual(
				UGP_MatchViewModelAdapter::ComputeFerroniteThreatNormalized(0.0f, 500.0f, 0.5f),
				0.0f),
			TEXT("A_ThreatZeroNormalizedZero"));
		Expect(FMath::IsNearlyEqual(
				UGP_MatchViewModelAdapter::ComputeFerroniteThreatNormalized(125.0f, 500.0f, 0.5f),
				0.5f),
			TEXT("B_HalfDerivedMaxNormalizedHalf"));
		Expect(FMath::IsNearlyEqual(
				UGP_MatchViewModelAdapter::ComputeFerroniteThreatNormalized(250.0f, 500.0f, 0.5f),
				1.0f),
			TEXT("C_AtDerivedMaxNormalizedOne"));
		Expect(FMath::IsNearlyEqual(
				UGP_MatchViewModelAdapter::ComputeFerroniteThreatNormalized(400.0f, 500.0f, 0.5f),
				1.0f),
			TEXT("D_AboveMaxClampedToOne"));
		Expect(FMath::IsNearlyEqual(
				UGP_MatchViewModelAdapter::ComputeFerroniteThreatNormalized(100.0f, 0.0f, 0.5f),
				0.0f)
			&& FMath::IsNearlyEqual(
				UGP_MatchViewModelAdapter::ComputeFerroniteThreatNormalized(100.0f, -10.0f, 0.5f),
				0.0f),
			TEXT("E_InvalidCapacityNormalizedZero"));
		Expect(FMath::IsNearlyEqual(
				UGP_MatchViewModelAdapter::ComputeFerroniteThreatNormalized(100.0f, 100.0f, 1.0f),
				1.0f)
			&& FMath::IsNearlyEqual(
				UGP_MatchViewModelAdapter::ComputeFerroniteThreatNormalized(100.0f, 100.0f, 2.0f),
				0.5f)
			&& FMath::IsNearlyEqual(
				UGP_MatchViewModelAdapter::ComputeThreatPresentationMax(100.0f, 2.0f),
				200.0f),
			TEXT("F_ThreatPerStoredUnitParticipatesInDenominator"));

		Expect(UGP_MatchViewModelAdapter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_MatchViewModelAdapter::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) == nullptr
			&& UGP_MatchViewModel::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr,
			TEXT("G_NoTickOnThreatPresentationPath"));

		AGP_GameState* GameState = World->GetGameState<AGP_GameState>();
		Expect(GameState != nullptr, TEXT("H_GameStatePresent"));
		if (GameState == nullptr)
		{
			UE_LOG(LogGPThreatPresentationContract, Log,
				TEXT("gp.UI.RunThreatPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* LocalBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-48000.0f, 4100.0f, 100.0f), FRotator::ZeroRotator, Params);
		AGP_MainBase* OtherBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FVector(-48000.0f, 5100.0f, 100.0f), FRotator::ZeroRotator, Params);
		if (!IsValid(LocalBase) || !IsValid(OtherBase))
		{
			Expect(false, TEXT("I_SpawnedTeamMainBases"));
			UE_LOG(LogGPThreatPresentationContract, Log,
				TEXT("gp.UI.RunThreatPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}
		Expect(true, TEXT("I_SpawnedTeamMainBases"));

		constexpr int32 LocalTeamId = 91;
		constexpr int32 OtherTeamId = 92;
		LocalBase->SetTeamId(LocalTeamId);
		OtherBase->SetTeamId(OtherTeamId);
		GameState->RegisterMainBase(LocalBase);
		GameState->RegisterMainBase(OtherBase);

		UGP_StorageComponent* LocalStorage = LocalBase->GetStorageComponent();
		UGP_StorageComponent* OtherStorage = OtherBase->GetStorageComponent();
		Expect(IsValid(LocalStorage) && IsValid(OtherStorage), TEXT("J_StorageComponentsPresent"));
		if (IsValid(LocalStorage))
		{
			LocalStorage->ConfigureFromDefinition(100.0f, 5);
		}
		if (IsValid(OtherStorage))
		{
			OtherStorage->ConfigureFromDefinition(100.0f, 20);
		}

		Expect(GameState->FindMainBaseForTeamClientSafe(LocalTeamId) == LocalBase
			&& GameState->FindMainBaseForTeamClientSafe(OtherTeamId) == OtherBase,
			TEXT("K_ClientSafeMainBaseIsTeamScoped"));

		UGP_MatchViewModel* MatchVM = NewObject<UGP_MatchViewModel>(GetTransientPackage());
		UGP_MatchViewModelAdapter* Adapter = NewObject<UGP_MatchViewModelAdapter>(GetTransientPackage());
		const bool bInitialized = Adapter != nullptr && MatchVM != nullptr
			&& Adapter->Initialize(MatchVM, GameState, LocalTeamId);
		Expect(bInitialized, TEXT("L_AdapterInitialize"));

		const float LocalRate = IsValid(LocalStorage) ? LocalStorage->GetThreatPerStoredUnit() : 0.0f;
		const float LocalCapacity = IsValid(LocalStorage) ? LocalStorage->GetTotalCapacity() : 0.0f;
		const float LocalMax = LocalCapacity * LocalRate;
		const float OtherMax = (IsValid(OtherStorage) ? OtherStorage->GetTotalCapacity() : 0.0f)
			* (IsValid(OtherStorage) ? OtherStorage->GetThreatPerStoredUnit() : 0.0f);
		Expect(LocalMax > 0.0f && OtherMax > LocalMax,
			TEXT("M_LocalAndOtherPresentationMaxDiffer"));
		Expect(FMath::IsNearlyEqual(Adapter->GetThreatPresentationMax(), LocalMax),
			TEXT("N_UsesLocalTeamMainBaseNotOther"));

		GameState->SetFerroniteThreatValueForTeam(LocalTeamId, 0.0f);
		Expect(FMath::IsNearlyEqual(MatchVM->FerroniteThreatValue, 0.0f)
			&& FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, 0.0f),
			TEXT("O_LiveThreatZeroNormalizedZero"));

		GameState->SetFerroniteThreatValueForTeam(LocalTeamId, LocalMax * 0.5f);
		Expect(FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, 0.5f, 0.01f),
			TEXT("P_LiveHalfMaxNormalizedHalf"));

		GameState->SetFerroniteThreatValueForTeam(LocalTeamId, LocalMax);
		Expect(FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, 1.0f, 0.01f),
			TEXT("Q_LiveAtMaxNormalizedOne"));

		GameState->SetFerroniteThreatValueForTeam(LocalTeamId, LocalMax * 2.0f);
		Expect(FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, 1.0f, 0.01f),
			TEXT("R_LiveAboveMaxClamped"));

		const float NormalizedBeforeOtherThreat = MatchVM->FerroniteThreatNormalized;
		GameState->SetFerroniteThreatValueForTeam(OtherTeamId, OtherMax);
		Expect(FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, NormalizedBeforeOtherThreat)
			&& FMath::IsNearlyEqual(Adapter->GetThreatPresentationMax(), LocalMax),
			TEXT("S_OtherTeamThreatDoesNotDriveLocalNormalized"));

		GameState->SetFerroniteThreatValueForTeam(LocalTeamId, LocalMax);
		const int32 BoundBefore = Adapter->GetBoundDelegateCount();
		if (IsValid(LocalStorage))
		{
			LocalStorage->ConfigureFromDefinition(100.0f, 10);
		}
		const float DoubledMax = IsValid(LocalStorage)
			? LocalStorage->GetTotalCapacity() * LocalStorage->GetThreatPerStoredUnit()
			: 0.0f;
		Expect(DoubledMax > LocalMax
			&& FMath::IsNearlyEqual(Adapter->GetThreatPresentationMax(), DoubledMax)
			&& FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, 0.5f, 0.01f),
			TEXT("T_StorageEventRefreshUpdatesNormalized"));

		Adapter->Initialize(MatchVM, GameState, LocalTeamId);
		Expect(Adapter->GetBoundDelegateCount() == BoundBefore
			&& Adapter->GetBoundDelegateCount() == Adapter->GetBoundDelegateCount(),
			TEXT("U_RebindDoesNotDuplicateDelegates"));

		if (IsValid(LocalStorage))
		{
			LocalStorage->ConfigureFromDefinition(0.0f, 0);
		}
		Expect(FMath::IsNearlyEqual(Adapter->GetThreatPresentationMax(), 0.0f)
			&& FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, 0.0f),
			TEXT("V_ZeroCapacityNormalizedZero"));

		MatchVM->SetFerroniteThreatNormalized(0.8f);
		Adapter->Initialize(MatchVM, GameState, LocalTeamId);
		Expect(FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, 0.0f),
			TEXT("W_RebindResetClearsNormalizedWhenCapacityInvalid"));

		Adapter->Shutdown();
		Adapter->Shutdown();
		Expect(Adapter->GetBoundDelegateCount() == 0, TEXT("X_ShutdownClearsDelegatesIdempotent"));

		GameState->UnregisterMainBase(LocalBase);
		GameState->UnregisterMainBase(OtherBase);
		LocalBase->Destroy();
		OtherBase->Destroy();

		UE_LOG(LogGPThreatPresentationContract, Log,
			TEXT("gp.UI.RunThreatPresentationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GThreatPresentationContract(
		TEXT("gp.UI.RunThreatPresentationContractTest"),
		TEXT("Run production HUD FerroniteThreatNormalized presentation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunThreatPresentationContractTest));
}

#endif
