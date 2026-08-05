// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AGP_MainBase;
class AGP_ResourceNode;
class AGP_Worker;
class UWorld;

#if !UE_BUILD_SHIPPING

/** Shared GP-S28 resource-loop diagnostic spawn helpers (console / contract tests). */
namespace GPResourceLoopDiagnostics
{
	inline const FName TagScenario(TEXT("GP_DiagScenario"));
	/** Legacy marker — prefer exact OwnerTag from GPContractTestCoordinator. */
	inline const FName TagOwnedByContract(TEXT("GP_DiagScenario_OwnedByContract"));

	FName MakeTeamScenarioTag(int32 TeamId);

	struct FGP_DiagnosticScenarioActors
	{
		AGP_MainBase* MainBase = nullptr;
		AGP_Worker* Worker = nullptr;
		AGP_ResourceNode* ResourceNode = nullptr;
		int32 TeamId = 1;
		FName OwnerTag;
		bool bOk = false;
		bool bCreatedMainBase = false;
		bool bCreatedWorker = false;
		bool bCreatedResourceNode = false;

		bool bNavSystemPresent = false;
		bool bAnchorProjected = false;
		bool bWorkerProjected = false;
		bool bNodeApproachProjected = false;
		bool bBaseDropOffProjected = false;
		bool bNavWorkerToNode = false;
		bool bNavNodeToBase = false;
		bool bNavBaseToNode = false;
		bool bReadyForHaulingTest = false;

		FVector AnchorLocation = FVector::ZeroVector;
		FVector MainBaseLocation = FVector::ZeroVector;
		FVector ResourceNodeLocation = FVector::ZeroVector;
		FVector WorkerLocation = FVector::ZeroVector;
		FVector NodeApproachLocation = FVector::ZeroVector;
		FVector BaseDropOffLocation = FVector::ZeroVector;

		FString Error;
		FString PathFailureReason;
	};

	struct FGP_ScenarioValidation
	{
		bool bPlayableTeamValid = false;
		bool bWorkerHasMainBase = false;
		bool bWorkerHasResourceNode = false;
		bool bMainBaseRegisteredForTeam = false;
		bool bWorkerAndBaseSameTeam = false;
		bool bNodeMineable = false;
		bool bNavSystemPresent = false;
		bool bWorkerProjected = false;
		bool bNodeApproachProjected = false;
		bool bBaseDropOffProjected = false;
		bool bNavReachableWorkerToNode = false;
		bool bNavReachableNodeToBase = false;
		bool bNavReachableBaseToNode = false;
		int32 MainBaseCountForWorkerTeam = 0;
		bool bRegistryUniqueForTeam = false;
		bool bResolvedMainBaseMatchesListedBase = false;
		bool bReadyForHaulingTest = false;
		int32 Errors = 0;
		int32 Warnings = 0;
		FString PathFailureReason;
		FString SuggestedCommand;
	};

	/** First playable TeamId in [1..8] with no registered MainBase, or INDEX_NONE. */
	int32 FindFreePlayableTeamId(UWorld* World);

	bool IsNavPointProjected(UWorld* World, const FVector& Location, FVector* OutProjected = nullptr, float ExtentXY = 800.0f, float ExtentZ = 800.0f);
	bool IsNavReachable(UWorld* World, const FVector& From, const FVector& To, FString* OutFailReason = nullptr);

	AGP_MainBase* SpawnMainBaseDeferred(UWorld* World, const FVector& Location, int32 TeamId, FName OwnerTag);
	AGP_Worker* SpawnWorkerDeferred(UWorld* World, const FVector& Location, int32 TeamId, FName OwnerTag);
	AGP_ResourceNode* SpawnResourceNodeTransient(UWorld* World, const FVector& Location, FName OwnerTag);

	/** Destroy actors that carry the exact OwnerTag (any team). Never touches other owners. */
	void CleanupScenarioByOwnerTag(UWorld* World, FName OwnerTag);

	/**
	 * Operator re-spawn helper: destroy prior operator-owned scenario for TeamId only.
	 * Does not destroy contract-owned or authored/production actors.
	 */
	void CleanupOperatorScenarioForTeam(UWorld* World, int32 TeamId);

	/**
	 * Authority: discover navigable layout, validate paths, then spawn coherent Team scenario.
	 * OwnerTag selects cleanup/ownership scope. Contract owners never cleanup other owners' teams.
	 * On nav failure: Ok=false and no leftover actors (atomic).
	 */
	FGP_DiagnosticScenarioActors SpawnDiagnosticScenario(UWorld* World, int32 TeamId, FName OwnerTag);

	FGP_ScenarioValidation ValidateHaulingScenario(UWorld* World, int32 TeamId, AGP_Worker* WorkerHint = nullptr);

	void DestroyDiagnosticScenarioActors(AGP_MainBase* MainBase, AGP_Worker* Worker, AGP_ResourceNode* Node);

	bool ActorHasOwnerTag(const AActor* Actor, FName OwnerTag);
}

#endif // !UE_BUILD_SHIPPING
