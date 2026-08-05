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
	struct FGP_DiagnosticScenarioActors
	{
		AGP_MainBase* MainBase = nullptr;
		AGP_Worker* Worker = nullptr;
		AGP_ResourceNode* ResourceNode = nullptr;
		int32 TeamId = 1;
		bool bOk = false;
		bool bCreatedMainBase = false;
		bool bCreatedWorker = false;
		bool bCreatedResourceNode = false;
		bool bNavWorkerToNode = false;
		bool bNavNodeToBase = false;
		FString Error;
	};

	struct FGP_ScenarioValidation
	{
		bool bPlayableTeamValid = false;
		bool bWorkerHasMainBase = false;
		bool bWorkerHasResourceNode = false;
		bool bMainBaseRegisteredForTeam = false;
		bool bWorkerAndBaseSameTeam = false;
		bool bNodeMineable = false;
		bool bNavReachableWorkerToNode = false;
		bool bNavReachableNodeToBase = false;
		bool bReadyForHaulingTest = false;
		int32 Errors = 0;
		int32 Warnings = 0;
	};

	/** Canonical diagnostic layout in the existing PrototypeArena-style west strip. */
	FVector GetDiagnosticMainBaseLocation();
	FVector GetDiagnosticResourceNodeLocation();
	FVector GetDiagnosticWorkerLocation();

	bool IsNavPointProjected(UWorld* World, const FVector& Location, FVector* OutProjected = nullptr);
	bool IsNavReachable(UWorld* World, const FVector& From, const FVector& To);

	AGP_MainBase* SpawnMainBaseDeferred(UWorld* World, const FVector& Location, int32 TeamId);
	AGP_Worker* SpawnWorkerDeferred(UWorld* World, const FVector& Location, int32 TeamId);
	AGP_ResourceNode* SpawnResourceNodeTransient(UWorld* World, const FVector& Location);

	/**
	 * Authority: spawn or complete a coherent Team scenario
	 * (MainBase + Storage + ResourceNode + Worker).
	 */
	FGP_DiagnosticScenarioActors SpawnDiagnosticScenario(UWorld* World, int32 TeamId);

	FGP_ScenarioValidation ValidateHaulingScenario(UWorld* World, int32 TeamId, AGP_Worker* WorkerHint = nullptr);

	void DestroyDiagnosticScenarioActors(AGP_MainBase* MainBase, AGP_Worker* Worker, AGP_ResourceNode* Node);
}

#endif // !UE_BUILD_SHIPPING
