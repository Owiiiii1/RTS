// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AGP_DropPod;
class AGP_PlayerState;
class UGP_WallPackageDefinition;
class UWorld;

enum class EGP_WallPackageRejectReason : uint8
{
	None = 0,
	MissingPlayerState,
	MatchFinished,
	MissingMainBase,
	InvalidDefinition,
	DefinitionNotReady,
	InventoryFull,
	PackagePending,
	InsufficientOrbital,
	SpendFailed,
	SpawnFailed,
	InvalidInventory
};

/**
 * Authority Wall Package purchase → one DropPod to MainBase (GP-S42A).
 * Not READY. Not placement. Not AGP_Wall spawn.
 */
namespace GPWallPackageAuthority
{
	struct FPurchaseResult
	{
		bool bAccepted = false;
		EGP_WallPackageRejectReason RejectReason = EGP_WallPackageRejectReason::None;
		float OrbitalCost = 0.0f;
		int32 StockAfter = 0;
		bool bPending = false;
		TWeakObjectPtr<AGP_DropPod> SpawnedPod;
	};

	FPurchaseResult AuthorityPurchaseWallPackage(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		const UGP_WallPackageDefinition* PackageDefinition = nullptr);

#if !UE_BUILD_SHIPPING
	void DebugForceNextPodSpawnFailure(bool bForce);
	bool DebugConsumeForcedPodSpawnFailure();
#endif
}
