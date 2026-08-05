// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

class UWorld;

/**
 * Narrow PIE console contract execution lock (GP-S28).
 * Exactly one actor-mutating async contract runner may hold the token at a time.
 * Not a general automation framework.
 */
namespace GPContractTestCoordinator
{
	struct FExecutionToken
	{
		uint64 ExecutionId = 0;
		FName TestName;
		FName OwnerTag;
		TWeakObjectPtr<UWorld> WorldWeak;
	};

	DECLARE_DELEGATE_TwoParams(FOnContractFinished, uint64 /*ExecutionId*/, int32 /*Failures*/);

	/** Operator diagnostic scenario ownership (never cleaned by contracts). */
	inline const FName OwnerTagOperator(TEXT("GP_DiagOwner_Operator"));

	FName MakeOwnerTag(const TCHAR* ContractKind, uint64 ExecutionId);

	bool IsBusy();
	FName GetActiveTestName();
	uint64 GetActiveExecutionId();

	/**
	 * Try to acquire the global execution token.
	 * On failure logs ContractTestRejected and returns false.
	 */
	bool TryAcquire(UWorld* World, FName TestName, const TCHAR* ContractKind, FExecutionToken& OutToken);

	/** Release token if ExecutionId matches the active holder. */
	void Release(uint64 ExecutionId, int32 Failures, bool bCancelled, const TCHAR* CancelReason);

	bool IsTokenActive(uint64 ExecutionId);
	bool IsWorldTearingDown(const UWorld* World);

	/** Optional finish callback for sequential suite chaining. */
	void SetFinishCallback(const FOnContractFinished& Callback);
	void ClearFinishCallback();
}

#endif // !UE_BUILD_SHIPPING
