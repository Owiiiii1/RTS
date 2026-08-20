// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GPUnitDefinition.generated.h"

/**
 * Intrinsic unit/building gameplay stats (GP-S38D).
 * Canonical initial/base-value source. Does not replace GAS runtime state.
 * Acquisition cost / transport slots live on orbital drop definitions, not here.
 */
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_UnitDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGP_UnitDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Vitals", meta = (ClampMin = "0.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Vitals", meta = (ClampMin = "0.0"))
	float InitialHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Vitals", meta = (ClampMin = "0.0"))
	float Armor = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Vitals", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageResistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Combat", meta = (ClampMin = "0.0"))
	float Damage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Combat", meta = (ClampMin = "0.0"))
	float AttackRangeCm = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Combat", meta = (ClampMin = "0.0"))
	float AttackCooldownSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Combat", meta = (ClampMin = "0.0"))
	float SightRangeCm = 900.0f;

	/**
	 * Per-team Fog of War reveal radius. Separate from combat SightRangeCm so scouting and
	 * auto-acquire balance can evolve independently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Vision", meta = (ClampMin = "0.0"))
	float FogOfWarSightRadiusCm = 900.0f;

	/** Whether a live actor using this definition contributes authoritative Fog of War vision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Vision")
	bool bGrantsFogOfWarVision = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Combat", meta = (ClampMin = "0.05"))
	float AutoAcquireScanIntervalSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Combat", meta = (ClampMin = "0.0"))
	float AttackFacingRotationSpeedDegreesPerSecond = 360.0f;

	/** Unit-type move balance. Nav/repath/separation stay on UGP_MovementComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeedCmPerSecond = 0.0f;

	/**
	 * GP-S40R: max seconds a mobile combat unit may pursue its current retaliation attacker.
	 * 0 = retaliation pursuit disabled. Canonical via AGP_UnitBase::GetRetaliationPursuitSeconds().
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Behavior|Retaliation", meta = (ClampMin = "0.0"))
	float RetaliationPursuitSeconds = 5.0f;

	/**
	 * Unit-owned cargo capacity. 0 = unit has no cargo / does not use cargo.
	 * Worker baseline 50. Salvage Walker and buildings 0.
	 * Runtime cargo state stays on UGP_CargoComponent.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Logistics|Cargo", meta = (ClampMin = "0.0"))
	float CargoCapacity = 0.0f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	static const TCHAR* PrimaryAssetTypeName();
};
