// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Orbital/GPOrbitalBuildingType.h"

class AGP_DropPod;
class AGP_PlayerState;
class UWorld;

UENUM(BlueprintType)
enum class EGP_BuildingDropRejectReason : uint8
{
	None = 0,
	InvalidType,
	InsufficientOrbital,
	SpendFailed,
	MissingMainBase,
	NoReadyInventory,
	InvalidTransform,
	OutOfDeployRadius,
	PlacementOverlap,
	SpawnFailed,
	MissingPlayerState
};

/**
 * Authority building purchase + deploy (GP-S32R).
 * Purchase spends Orbital once → READY++. Deploy consumes READY after pod spawn — no second spend.
 */
namespace GPBuildingDropAuthority
{
	struct FPurchaseResult
	{
		bool bAccepted = false;
		EGP_BuildingDropRejectReason RejectReason = EGP_BuildingDropRejectReason::None;
		float OrbitalCost = 0.0f;
		int32 ReadyAfter = 0;
	};

	struct FDeployResult
	{
		bool bAccepted = false;
		EGP_BuildingDropRejectReason RejectReason = EGP_BuildingDropRejectReason::None;
		int32 ReadyAfter = 0;
		TWeakObjectPtr<AGP_DropPod> SpawnedPod;
	};

	float GetPurchaseCostForType(EGP_OrbitalBuildingType BuildingType);

	/** Interim placement validation: finite transform, radius from MainBase, overlap vs buildings. */
	bool ValidateInterimPlacement(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		EGP_OrbitalBuildingType BuildingType,
		const FTransform& WorldTransform,
		EGP_BuildingDropRejectReason& OutReject);

	FPurchaseResult AuthorityPurchaseBuilding(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		EGP_OrbitalBuildingType BuildingType);

	FDeployResult AuthorityDeployBuilding(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		EGP_OrbitalBuildingType BuildingType,
		const FTransform& WorldTransform);
}
