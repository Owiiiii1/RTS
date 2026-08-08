// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GPOrbitalDeliverySettings.generated.h"

/**
 * Project Settings → Game → GP Orbital Delivery (GP-S31R).
 * TEMP operator-test tuning — not final balance. Config=Game → DefaultGame.ini.
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

	/** DropPod descent telegraph (GDD 2–3 s). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "0.05"))
	float UnitDropDescentDurationSeconds = 2.5f;

	/** Spawn altitude above Unit Drop Zone (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "100.0"))
	float UnitDropSpawnAltitudeCm = 2500.0f;

	/** Horizontal spacing between multi-unit spawn offsets (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "50.0"))
	float UnitDropSpawnSpacingCm = 180.0f;

	/** Delay after landing before DropPod destroy (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "DropPod", meta = (ClampMin = "0.0"))
	float UnitDropCleanupDelaySeconds = 0.35f;
};
