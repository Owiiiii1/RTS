// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "GPDamageCalculation.generated.h"

/**
 * MMC for GE_GP_Damage_Basic (Combat slice):
 * EffectiveDamage = max(0, Damage - Armor) * (1 - clamp(DamageResistance, 0, 1))
 * Returns -EffectiveDamage for a Health Additive modifier.
 *
 * UE 5.8: override CalculateBaseMagnitude_Implementation (BlueprintNativeEvent).
 */
UCLASS()
class GPGASRUNTIME_API UGP_DamageCalculation : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UGP_DamageCalculation();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};
