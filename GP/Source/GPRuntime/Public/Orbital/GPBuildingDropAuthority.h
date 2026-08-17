// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Orbital/GPOrbitalBuildingType.h"

class AGP_BuildingBase;
class AGP_DropPod;
class AGP_PlayerState;
class UGP_OrbitalDropDefinition;
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
	MissingPlayerState,
	MatchFinished,
	InvalidDefinition,
	MissingBuildingDefinition,
	MissingSpawnedClass
};

/**
 * Authority building purchase + deploy (GP-S32R / GP-S35B).
 * Purchase spends Orbital once → READY[DropDef]++. Deploy consumes READY after pod spawn — no second spend.
 * Canonical identity is UGP_OrbitalDropDefinition / FPrimaryAssetId. Enum is compatibility glue only.
 */
namespace GPBuildingDropAuthority
{
	struct FPurchaseResult
	{
		bool bAccepted = false;
		EGP_BuildingDropRejectReason RejectReason = EGP_BuildingDropRejectReason::None;
		float OrbitalCost = 0.0f;
		int32 ReadyAfter = 0;
		FPrimaryAssetId DropDefinitionId;
	};

	struct FDeployResult
	{
		bool bAccepted = false;
		EGP_BuildingDropRejectReason RejectReason = EGP_BuildingDropRejectReason::None;
		int32 ReadyAfter = 0;
		TWeakObjectPtr<AGP_DropPod> SpawnedPod;
		FPrimaryAssetId DropDefinitionId;
		TSubclassOf<::AGP_BuildingBase> PayloadClass;
	};

	float GetPurchaseCost(const UGP_OrbitalDropDefinition* DropDefinition);
	float GetPurchaseCostForType(EGP_OrbitalBuildingType BuildingType);

	bool ValidateInterimPlacement(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		const UGP_OrbitalDropDefinition* DropDefinition,
		const FTransform& WorldTransform,
		EGP_BuildingDropRejectReason& OutReject);

	bool ValidateInterimPlacement(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		EGP_OrbitalBuildingType BuildingType,
		const FTransform& WorldTransform,
		EGP_BuildingDropRejectReason& OutReject);

	FPurchaseResult AuthorityPurchaseBuilding(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		const UGP_OrbitalDropDefinition* DropDefinition);

	FPurchaseResult AuthorityPurchaseBuilding(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		EGP_OrbitalBuildingType BuildingType);

	FDeployResult AuthorityDeployBuilding(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		const UGP_OrbitalDropDefinition* DropDefinition,
		const FTransform& WorldTransform);

	FDeployResult AuthorityDeployBuilding(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		EGP_OrbitalBuildingType BuildingType,
		const FTransform& WorldTransform);
}
