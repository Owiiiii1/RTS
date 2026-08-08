// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Orbital/GPUnitDropManifest.h"

class AGP_DropPod;
class AGP_MainBase;
class AGP_PlayerState;
class UWorld;

/**
 * Authority unit-drop validate → spend → spawn one DropPod (GP-S31R).
 * No subsystem — called from PlayerController RPC / contracts.
 */
namespace GPUnitDropAuthority
{
	struct FEvalResult
	{
		bool bAccepted = false;
		EGP_UnitDropRejectReason RejectReason = EGP_UnitDropRejectReason::None;
		int32 SlotCost = 0;
		float OrbitalCost = 0.0f;
		int32 UnitCount = 0;
		TWeakObjectPtr<AGP_DropPod> SpawnedPod;
	};

	/** Compute slots/cost from settings (no mutation). */
	bool ComputeManifestCosts(
		const FGP_UnitDropManifest& Manifest,
		int32& OutSlotCost,
		float& OutOrbitalCost,
		int32& OutUnitCount,
		EGP_UnitDropRejectReason& OutReject);

	/**
	 * Full authority order: validate → Instant SpendOrbital once → spawn one DropPod.
	 * Rejects never spend. Client cannot choose payload class/TeamId.
	 */
	FEvalResult AuthorityRequestUnitDrop(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		const FGP_UnitDropManifest& Manifest);
}
