// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GPOrbitalDeliverySettings.generated.h"

class AGP_BuildingBase;
class AGP_DropPod;
class AGP_Worker;
class AGP_SalvageWalker;
class UGP_OrbitalDropDefinition;
class UGP_OrbitalUnitDropDefinition;

/**
 * Project Settings → Game → GP Orbital Delivery (GP-S31R).
 * TEMP operator-test tuning — not final balance. Config=Game → DefaultGame.ini.
 *
 * Global transport / world-system tunables stay here (pod class, altitude, spacing, cleanup,
 * deploy radius, overlap). Per-purchase unit/building cost, slots, payload and delivery timing
 * live on orbital drop definitions. Deprecated unit-specific fields remain ini compatibility.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GP Orbital Delivery"))
class GPRUNTIME_API UGP_OrbitalDeliverySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGP_OrbitalDeliverySettings();

	virtual FName GetCategoryName() const override;

	static const UGP_OrbitalDeliverySettings* Get();

	/** Transport slots per unit DropPod (MVP tuning example 4). */
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

	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Slots", meta = (ClampMin = "1"))
	int32 PodTransportSlotCapacity = 4;

	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Slots", meta = (ClampMin = "1", DeprecatedProperty,
		DeprecationMessage = "Canonical slot cost is UGP_OrbitalUnitDropDefinition.TransportSlotCost."))
	int32 WorkerTransportSlotCost = 1;

	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Slots", meta = (ClampMin = "1", DeprecatedProperty,
		DeprecationMessage = "Canonical slot cost is UGP_OrbitalUnitDropDefinition.TransportSlotCost."))
	int32 SalvageWalkerTransportSlotCost = 2;

	/** Compatibility fallback. Canonical cost is UGP_OrbitalUnitDropDefinition.Cost. */
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Cost", meta = (ClampMin = "0.0", DeprecatedProperty,
		DeprecationMessage = "Canonical cost is UGP_OrbitalUnitDropDefinition.Cost."))
	float WorkerOrbitalDropCost = 25.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Cost", meta = (ClampMin = "0.0", DeprecatedProperty,
		DeprecationMessage = "Canonical cost is UGP_OrbitalUnitDropDefinition.Cost."))
	float SalvageWalkerOrbitalDropCost = 50.0f;

	/**
	 * Authored Worker BP (must derive from AGP_Worker). Empty → native AGP_Worker fallback.
	 * Owner assigns e.g. BP_Worker in Project Settings — no C++ /Game path.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Payload", meta = (AllowAbstract = "false", DeprecatedProperty,
		DeprecationMessage = "Canonical payload is UGP_OrbitalUnitDropDefinition.PayloadClass. Kept as operator BP bridge."))
	TSoftClassPtr<AGP_Worker> WorkerPayloadClass;

	/**
	 * Authored Salvage Walker BP (must derive from AGP_SalvageWalker). Empty → native fallback.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Payload", meta = (AllowAbstract = "false", DeprecatedProperty,
		DeprecationMessage = "Canonical payload is UGP_OrbitalUnitDropDefinition.PayloadClass. Kept as operator BP bridge."))
	TSoftClassPtr<AGP_SalvageWalker> SalvageWalkerPayloadClass;

	/**
	 * Authored DropPod presentation BP (must derive from AGP_DropPod). Empty → native AGP_DropPod.
	 * Owner assigns BP_DropPod_MVP here after creating the Blueprint child.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod|Class", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AGP_DropPod> UnitDropPodClass;

	/** DropPod descent telegraph (GDD 2–3 s). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "0.05"))
	float UnitDropDescentDurationSeconds = 2.5f;

	/** Spawn altitude above Unit Drop Zone (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "100.0"))
	float UnitDropSpawnAltitudeCm = 2500.0f;

	/** Horizontal spacing between multi-unit spawn offsets (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "50.0"))
	float UnitDropSpawnSpacingCm = 180.0f;

	/**
	 * After Impact: wait before authority payload spawn (TEMP ~1.0–1.5s for units).
	 * Zero = Impact → immediate payload. Separate from descent / cleanup.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "0.0"))
	float UnitDropPayloadDeployDelaySeconds = 1.25f;

	/** Delay after payload deploy before DropPod destroy (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "0.0"))
	float UnitDropCleanupDelaySeconds = 0.35f;

	/** TEMP Orbital purchase cost for Logistics Hub. Deprecated SoT — GP-S35B uses UGP_OrbitalDropDefinition.Cost.
	 * Retained as operator DefaultGame.ini compatibility bridge for the native Logistics Hub catalog entry. */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Cost", meta = (ClampMin = "0.0", DeprecatedProperty,
		DeprecationMessage = "Canonical cost is UGP_OrbitalDropDefinition.Cost. Kept as Logistics Hub compatibility bridge."))
	float BuildingOrbitalPurchaseCost = 100.0f;

	/**
	 * Authored building payload BP (must derive from AGP_BuildingBase). Empty → native AGP_LogisticsHub.
	 * Deprecated SoT — canonical class is UGP_BuildingDefinition.SpawnedClass.
	 * Retained as operator DefaultGame.ini compatibility bridge for Logistics Hub.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Payload", meta = (AllowAbstract = "false", DeprecatedProperty,
		DeprecationMessage = "Canonical payload is UGP_BuildingDefinition.SpawnedClass. Kept as Logistics Hub compatibility bridge."))
	TSoftClassPtr<AGP_BuildingBase> BuildingPayloadClass;

	/**
	 * Authored Defensive Turret BP (must derive from AGP_DefensiveTurret). Empty → native class.
	 * Canonical payload is still UGP_BuildingDefinition.SpawnedClass; this is the Hub-style override seam.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Payload", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AGP_BuildingBase> DefensiveTurretPayloadClass;

	/** Building DropPod descent telegraph (GDD 2–3 s). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|DropPod", meta = (ClampMin = "0.05"))
	float BuildingDropDescentDurationSeconds = 2.5f;

	/** Spawn altitude above building landing point (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|DropPod", meta = (ClampMin = "100.0"))
	float BuildingDropSpawnAltitudeCm = 2500.0f;

	/** After Impact: wait before authority building spawn (TEMP ~2.0 s — longer than units). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|DropPod", meta = (ClampMin = "0.0"))
	float BuildingDropPayloadDeployDelaySeconds = 2.0f;

	/** Delay after building deploy before DropPod destroy (seconds). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|DropPod", meta = (ClampMin = "0.0"))
	float BuildingDropCleanupDelaySeconds = 0.5f;

	/** Max horizontal deploy distance from owning MainBase (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Placement", meta = (ClampMin = "100.0"))
	float BuildingMaxDeployRadiusFromMainBaseCm = 5000.0f;

	/** Extra overlap margin when validating building placement (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Building Drop|Placement", meta = (ClampMin = "0.0"))
	float BuildingPlacementOverlapMarginCm = 25.0f;

	/** Resolve Worker payload: soft class if valid subclass, else native. */
	TSubclassOf<AGP_Worker> ResolveWorkerPayloadClass(bool* bOutUsedAuthored = nullptr) const;

	/** Resolve Salvage Walker payload: soft class if valid subclass, else native. */
	TSubclassOf<AGP_SalvageWalker> ResolveSalvageWalkerPayloadClass(bool* bOutUsedAuthored = nullptr) const;

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
	bool IsWorkerPayloadClassConfigInvalid() const;
	bool IsSalvageWalkerPayloadClassConfigInvalid() const;
	bool IsUnitDropPodClassConfigInvalid() const;
	bool IsBuildingPayloadClassConfigInvalid() const;
	bool IsDefensiveTurretPayloadClassConfigInvalid() const;
};
