// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Orbital/GPWallPackageDefinition.h"
#include "GPOrbitalDeliverySettings.generated.h"

class AGP_BuildingBase;
class AGP_DropPod;
class UGP_OrbitalDropDefinition;
class UGP_OrbitalUnitDropDefinition;

/**
 * Project Settings → Game → GP Orbital Delivery (GP-S31R).
 * TEMP operator-test tuning — not final balance. Config=Game → DefaultGame.ini.
 *
 * Project Settings own:
 * - DataAsset / catalog references
 * - true global transport and world-system tunables (pod class, altitude, spacing, cleanup, deploy radius)
 * - retained legacy / fallback Config compatibility (not canonical product balance)
 *
 * Product DataAssets own:
 * - product cost, transport slot cost, payload
 * - product-specific descent / payload-deploy timing
 *
 * Wall Package owns its own descent / deploy timing on UGP_WallPackageDefinition.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GP Orbital Delivery"))
class GPRUNTIME_API UGP_OrbitalDeliverySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGP_OrbitalDeliverySettings();

	virtual FName GetCategoryName() const override;

	static const UGP_OrbitalDeliverySettings* Get();

	/**
	 * Designer-selected canonical Worker acquisition DataAsset.
	 * Empty = native bootstrap. Contains no balance values itself.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Definitions")
	TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> WorkerDropDefinition;

	/**
	 * Designer-selected canonical Salvage Walker acquisition DataAsset.
	 * Empty = native bootstrap. Contains no balance values itself.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Definitions")
	TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> SalvageWalkerDropDefinition;

	/**
	 * Designer-selected canonical Logistics Hub acquisition DataAsset.
	 * Empty = native bootstrap. Contains no balance values itself.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Definitions")
	TSoftObjectPtr<UGP_OrbitalDropDefinition> LogisticsHubDropDefinition;

	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Definitions")
	TSoftObjectPtr<UGP_OrbitalDropDefinition> DefensiveTurretDropDefinition;

	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Definitions")
	TSoftObjectPtr<UGP_OrbitalDropDefinition> WallDropDefinition;

	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Definitions")
	TSoftObjectPtr<UGP_OrbitalDropDefinition> WallTurretDropDefinition;

	/**
	 * Designer-selected Wall Package DataAsset (GP-S42A).
	 * Empty = native bootstrap. Authored configured definition wins.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Wall Package|Definitions")
	TSoftObjectPtr<UGP_WallPackageDefinition> WallPackageDefinition;

	/** Transport slots per unit DropPod (MVP tuning example 4). */
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Slots", meta = (ClampMin = "1"))
	int32 PodTransportSlotCapacity = 4;

	/**
	 * Authored DropPod presentation BP (must derive from AGP_DropPod). Empty → native AGP_DropPod.
	 * Owner assigns BP_DropPod_MVP here after creating the Blueprint child.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod|Class", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AGP_DropPod> UnitDropPodClass;

	/**
	 * Fallback seed for unit DropPod descent. Canonical timing is
	 * UGP_OrbitalUnitDropDefinition.DeliveryDescentSeconds and normally overwrites this.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Fallback Defaults|Unit Product Timing",
		meta = (ClampMin = "0.05", DisplayName = "Unit Descent Seconds (Fallback Seed)",
			ToolTip = "Fallback seed used when resolving unit delivery timing. Canonical per-product timing lives on UGP_OrbitalUnitDropDefinition and normally overwrites this. Wall Package uses its own definition timing."))
	float UnitDropDescentDurationSeconds = 2.5f;

	/** Spawn altitude above Unit Drop Zone (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "100.0"))
	float UnitDropSpawnAltitudeCm = 2500.0f;

	/** Horizontal spacing between multi-unit spawn offsets (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "50.0"))
	float UnitDropSpawnSpacingCm = 180.0f;

	/**
	 * Fallback seed for unit payload deploy delay after Impact.
	 * Canonical timing is UGP_OrbitalUnitDropDefinition.PayloadDeployDelaySeconds and normally overwrites this.
	 * Zero = Impact → immediate payload. Separate from descent / cleanup.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Fallback Defaults|Unit Product Timing",
		meta = (ClampMin = "0.0", DisplayName = "Unit Payload Deploy Delay Seconds (Fallback Seed)",
			ToolTip = "Fallback seed used when resolving unit payload deploy delay. Canonical per-product timing lives on UGP_OrbitalUnitDropDefinition and normally overwrites this. Wall Package uses its own definition timing."))
	float UnitDropPayloadDeployDelaySeconds = 1.25f;

	/** Delay after payload deploy before DropPod destroy (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "0.0"))
	float UnitDropCleanupDelaySeconds = 0.35f;

	/**
	 * DEPRECATED compatibility bridge. Canonical cost is UGP_OrbitalDropDefinition.Cost.
	 * Retained as operator DefaultGame.ini compatibility for the native Logistics Hub catalog entry.
	 * Not a designer-authoritative Project Settings control.
	 */
	UPROPERTY(Config, meta = (ClampMin = "0.0", DeprecatedProperty,
		DeprecationMessage = "Canonical cost is UGP_OrbitalDropDefinition.Cost. Kept as Logistics Hub compatibility bridge."))
	float BuildingOrbitalPurchaseCost = 100.0f;

	/**
	 * DEPRECATED compatibility bridge. Canonical class is UGP_BuildingDefinition.SpawnedClass.
	 * Retained as operator DefaultGame.ini compatibility for Logistics Hub.
	 * Not a designer-authoritative Project Settings control.
	 */
	UPROPERTY(Config, meta = (AllowAbstract = "false", DeprecatedProperty,
		DeprecationMessage = "Canonical payload is UGP_BuildingDefinition.SpawnedClass. Kept as Logistics Hub compatibility bridge."))
	TSoftClassPtr<AGP_BuildingBase> BuildingPayloadClass;

	/**
	 * LEGACY compatibility override. When set, currently outranks BuildingDefinition.SpawnedClass.
	 * Not the desired future source of truth. Keep editable until BuildingDefinition payload migration.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|LEGACY Compatibility Override",
		meta = (AllowAbstract = "false",
			DisplayName = "Defensive Turret Payload Class (LEGACY Override)",
			ToolTip = "LEGACY compatibility override. Currently outranks BuildingDefinition.SpawnedClass when set. Not the desired future source of truth. Keep until BuildingDefinition payload migration."))
	TSoftClassPtr<AGP_BuildingBase> DefensiveTurretPayloadClass;

	/**
	 * Fallback seed for building DropPod descent. Canonical timing is
	 * UGP_OrbitalDropDefinition.DeliveryDescentSeconds and normally overwrites this.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Fallback Defaults|Building Product Timing",
		meta = (ClampMin = "0.05", DisplayName = "Building Descent Seconds (Fallback Seed)",
			ToolTip = "Fallback seed used when resolving building delivery timing. Canonical per-product timing lives on UGP_OrbitalDropDefinition and normally overwrites this. Wall Package uses its own definition timing."))
	float BuildingDropDescentDurationSeconds = 2.5f;

	/** Spawn altitude above building landing point (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|DropPod", meta = (ClampMin = "100.0"))
	float BuildingDropSpawnAltitudeCm = 2500.0f;

	/**
	 * Fallback seed for building payload deploy delay after Impact.
	 * Canonical timing is UGP_OrbitalDropDefinition.PayloadDeployDelaySeconds and normally overwrites this.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Fallback Defaults|Building Product Timing",
		meta = (ClampMin = "0.0", DisplayName = "Building Payload Deploy Delay Seconds (Fallback Seed)",
			ToolTip = "Fallback seed used when resolving building payload deploy delay. Canonical per-product timing lives on UGP_OrbitalDropDefinition and normally overwrites this. Wall Package uses its own definition timing."))
	float BuildingDropPayloadDeployDelaySeconds = 2.0f;

	/** Delay after building deploy before DropPod destroy (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|DropPod", meta = (ClampMin = "0.0"))
	float BuildingDropCleanupDelaySeconds = 0.5f;

	/** Max horizontal deploy distance from owning MainBase (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Placement", meta = (ClampMin = "100.0"))
	float BuildingMaxDeployRadiusFromMainBaseCm = 5000.0f;

	/** Resolve DropPod class: soft class if valid subclass, else native. */
	TSubclassOf<AGP_DropPod> ResolveUnitDropPodClass(bool* bOutUsedAuthored = nullptr) const;

	/** Resolve building payload: soft class if valid subclass, else native AGP_LogisticsHub. */
	TSubclassOf<AGP_BuildingBase> ResolveBuildingPayloadClass(bool* bOutUsedAuthored = nullptr) const;

	/** Resolve Defensive Turret payload: authored subclass if valid, else native AGP_DefensiveTurret. */
	TSubclassOf<AGP_BuildingBase> ResolveDefensiveTurretPayloadClass(bool* bOutUsedAuthored = nullptr) const;

	/** Building pods reuse UnitDropPodClass / native AGP_DropPod fallback (GP-S32R). */
	TSubclassOf<AGP_DropPod> ResolveBuildingDropPodClass(bool* bOutUsedAuthored = nullptr) const
	{
		return ResolveUnitDropPodClass(bOutUsedAuthored);
	}

	/** True if soft ref is set but fails base-class / load checks (does not use invalid class). */
	bool IsUnitDropPodClassConfigInvalid() const;
	bool IsBuildingPayloadClassConfigInvalid() const;
	bool IsDefensiveTurretPayloadClassConfigInvalid() const;
};
