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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Combat", meta = (ClampMin = "0.05"))
	float AutoAcquireScanIntervalSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Combat", meta = (ClampMin = "0.0"))
	float AttackFacingRotationSpeedDegreesPerSecond = 360.0f;

	/** Unit-type move balance. Nav/repath/separation stay on UGP_MovementComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Movement", meta = (ClampMin = "0.0"))
	float MoveSpeedCmPerSecond = 0.0f;

	/**
	 * DATA ONLY in GP-S38D. GP-S39R owns behavior.
	 * 0 = retaliation pursuit disabled. >0 = max pursuit duration after reacting to attacker.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Behavior|Retaliation", meta = (ClampMin = "0.0"))
	float RetaliationPursuitSeconds = 5.0f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	static const TCHAR* PrimaryAssetTypeName();
};
