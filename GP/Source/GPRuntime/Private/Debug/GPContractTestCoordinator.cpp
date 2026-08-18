// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/GPContractTestCoordinator.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPDefensiveTurret.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPOrbitalUnitDropCatalog.h"
#include "Units/GPUnitCommandComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPContractTest, Log, All);

namespace GPContractTestCoordinator
{
	namespace Private
	{
		static uint64 GNextExecutionId = 1;
		static FExecutionToken GActive;
		static bool GBusy = false;
		static FOnContractFinished GFinishCallback;
	}

	FName MakeOwnerTag(const TCHAR* ContractKind, uint64 ExecutionId)
	{
		return FName(*FString::Printf(TEXT("GP_DiagOwner_%s_%llu"), ContractKind, ExecutionId));
	}

	bool IsBusy()
	{
		return Private::GBusy;
	}

	FName GetActiveTestName()
	{
		return Private::GBusy ? Private::GActive.TestName : NAME_None;
	}

	uint64 GetActiveExecutionId()
	{
		return Private::GBusy ? Private::GActive.ExecutionId : 0;
	}

	bool IsWorldTearingDown(const UWorld* World)
	{
		return World == nullptr || !IsValid(World) || World->bIsTearingDown;
	}

	bool TryAcquire(UWorld* World, FName TestName, const TCHAR* ContractKind, FExecutionToken& OutToken)
	{
		OutToken = FExecutionToken();
		if (IsWorldTearingDown(World))
		{
			UE_LOG(LogGPContractTest, Warning,
				TEXT("ContractTestRejected: Requested=%s Active=%s Reason=WorldEndPlay"),
				*TestName.ToString(),
				*GetActiveTestName().ToString());
			return false;
		}

		if (Private::GBusy)
		{
			UE_LOG(LogGPContractTest, Warning,
				TEXT("ContractTestRejected: Requested=%s Active=%s Reason=AnotherContractTestRunning ContractTestRejected=true"),
				*TestName.ToString(),
				*Private::GActive.TestName.ToString());
			return false;
		}

		Private::GBusy = true;
		Private::GActive.ExecutionId = Private::GNextExecutionId++;
		Private::GActive.TestName = TestName;
		Private::GActive.OwnerTag = MakeOwnerTag(ContractKind, Private::GActive.ExecutionId);
		Private::GActive.WorldWeak = World;
		OutToken = Private::GActive;

		// Operator-authored drop DataAssets must not shadow native bootstrap during contracts.
		UGP_OrbitalUnitDropCatalog::Get().DebugBeginContractIsolation();
		UGP_BuildingDropCatalog::Get().DebugBeginContractIsolation();

		// Operator-placed arena turrets must not fire during contracts (headless isolation).
		for (TActorIterator<AGP_DefensiveTurret> It(World); It; ++It)
		{
			AGP_DefensiveTurret* Turret = *It;
			if (!IsValid(Turret) || Turret->IsDead())
			{
				continue;
			}
			Turret->SetTeamId(-1);
			if (UGP_UnitCommandComponent* Cmd = Turret->GetUnitCommandComponent())
			{
				Cmd->RefreshCombatAutoAcquireTimer();
			}
		}

		UE_LOG(LogGPContractTest, Log,
			TEXT("ContractTestStart: Name=%s ExecutionId=%llu TeamId=-1 OwnerTag=%s"),
			*OutToken.TestName.ToString(),
			OutToken.ExecutionId,
			*OutToken.OwnerTag.ToString());
		return true;
	}

	bool IsTokenActive(uint64 ExecutionId)
	{
		return Private::GBusy && Private::GActive.ExecutionId == ExecutionId;
	}

	void Release(uint64 ExecutionId, int32 Failures, bool bCancelled, const TCHAR* CancelReason)
	{
		if (!Private::GBusy || Private::GActive.ExecutionId != ExecutionId)
		{
			return;
		}

		const FName FinishedName = Private::GActive.TestName;
		const uint64 FinishedId = Private::GActive.ExecutionId;
		UGP_OrbitalUnitDropCatalog::Get().DebugEndContractIsolation();
		UGP_BuildingDropCatalog::Get().DebugEndContractIsolation();
		Private::GBusy = false;
		Private::GActive = FExecutionToken();

		UE_LOG(LogGPContractTest, Log,
			TEXT("ContractTestComplete: Name=%s ExecutionId=%llu Failures=%d Cancelled=%s CancelReason=%s"),
			*FinishedName.ToString(),
			FinishedId,
			Failures,
			bCancelled ? TEXT("true") : TEXT("false"),
			CancelReason != nullptr ? CancelReason : TEXT("None"));

		const FOnContractFinished Callback = Private::GFinishCallback;
		if (Callback.IsBound())
		{
			Callback.ExecuteIfBound(FinishedId, Failures);
		}
	}

	void SetFinishCallback(const FOnContractFinished& Callback)
	{
		Private::GFinishCallback = Callback;
	}

	void ClearFinishCallback()
	{
		Private::GFinishCallback.Unbind();
	}
}

#endif // !UE_BUILD_SHIPPING
