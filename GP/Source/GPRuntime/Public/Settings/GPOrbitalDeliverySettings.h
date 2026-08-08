// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GPOrbitalDeliverySettings.generated.h"

class AGP_DropPod;
class AGP_Worker;
class AGP_SalvageWalker;

/**
 * Project Settings → Game → GP Orbital Delivery (GP-S31R).
 * TEMP operator-test tuning — not final balance. Config=Game → DefaultGame.ini.
 *
 * Capacity / costs / descent stay here (gameplay SoT). Authored payload + DropPod classes
 * are soft refs — no hardcoded /Game paths in C++. Future UGP_DropPodDefinition optional.
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
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Slots", meta = (ClampMin = "1"))
	int32 PodTransportSlotCapacity = 4;

	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Slots", meta = (ClampMin = "1"))
	int32 WorkerTransportSlotCost = 1;

	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Slots", meta = (ClampMin = "1"))
	int32 SalvageWalkerTransportSlotCost = 2;

	/** TEMP Orbital costs — one Ready container launch (100) should afford a meaningful order. */
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Cost", meta = (ClampMin = "0.0"))
	float WorkerOrbitalDropCost = 25.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Cost", meta = (ClampMin = "0.0"))
	float SalvageWalkerOrbitalDropCost = 50.0f;

	/**
	 * Authored Worker BP (must derive from AGP_Worker). Empty → native AGP_Worker fallback.
	 * Owner assigns e.g. BP_Worker in Project Settings — no C++ /Game path.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Payload", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AGP_Worker> WorkerPayloadClass;

	/**
	 * Authored Salvage Walker BP (must derive from AGP_SalvageWalker). Empty → native fallback.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Unit Drop|Payload", meta = (AllowAbstract = "false"))
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

	/** Resolve Worker payload: soft class if valid subclass, else native. */
	TSubclassOf<AGP_Worker> ResolveWorkerPayloadClass(bool* bOutUsedAuthored = nullptr) const;

	/** Resolve Salvage Walker payload: soft class if valid subclass, else native. */
	TSubclassOf<AGP_SalvageWalker> ResolveSalvageWalkerPayloadClass(bool* bOutUsedAuthored = nullptr) const;

	/** Resolve DropPod class: soft class if valid subclass, else native. */
	TSubclassOf<AGP_DropPod> ResolveUnitDropPodClass(bool* bOutUsedAuthored = nullptr) const;

	/** True if soft ref is set but fails base-class / load checks (does not use invalid class). */
	bool IsWorkerPayloadClassConfigInvalid() const;
	bool IsSalvageWalkerPayloadClassConfigInvalid() const;
	bool IsUnitDropPodClassConfigInvalid() const;
};
