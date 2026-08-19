// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Orbital/GPWallPackageDefinition.h"
#include "GPOrbitalDeliverySettings.generated.h"

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
 *
 * Product DataAssets own:
 * - product cost, transport slot cost, payload
 * - product-specific descent / payload-deploy timing
 *
 * Native bootstrap products own native delivery timing and native Hub/Turret payload classes.
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

	/** Spawn altitude above Unit Drop Zone (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "100.0"))
	float UnitDropSpawnAltitudeCm = 2500.0f;

	/** Horizontal spacing between multi-unit spawn offsets (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "50.0"))
	float UnitDropSpawnSpacingCm = 180.0f;

	/** Delay after payload deploy before DropPod destroy (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "0.0"))
	float UnitDropCleanupDelaySeconds = 0.35f;

	/** Spawn altitude above building landing point (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|DropPod", meta = (ClampMin = "100.0"))
	float BuildingDropSpawnAltitudeCm = 2500.0f;

	/** Delay after building deploy before DropPod destroy (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|DropPod", meta = (ClampMin = "0.0"))
	float BuildingDropCleanupDelaySeconds = 0.5f;

	/** Max horizontal deploy distance from owning MainBase (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Placement", meta = (ClampMin = "100.0"))
	float BuildingMaxDeployRadiusFromMainBaseCm = 5000.0f;

	/** Resolve DropPod class: soft class if valid subclass, else native. */
	TSubclassOf<AGP_DropPod> ResolveUnitDropPodClass(bool* bOutUsedAuthored = nullptr) const;

	/** Building pods reuse UnitDropPodClass / native AGP_DropPod fallback (GP-S32R). */
	TSubclassOf<AGP_DropPod> ResolveBuildingDropPodClass(bool* bOutUsedAuthored = nullptr) const
	{
		return ResolveUnitDropPodClass(bOutUsedAuthored);
	}

	/** True if soft ref is set but fails base-class / load checks (does not use invalid class). */
	bool IsUnitDropPodClassConfigInvalid() const;
};
