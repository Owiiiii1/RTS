// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effects/GPGE_DamageBasic.h"

#include "AttributeSets/GPUnitAttributeSet.h"
#include "Calculations/GPDamageCalculation.h"

UGP_GE_Damage_Basic::UGP_GE_Damage_Basic()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo HealthModifier;
	HealthModifier.Attribute = UGP_UnitAttributeSet::GetHealthAttribute();
	HealthModifier.ModifierOp = EGameplayModOp::Additive;

	FCustomCalculationBasedFloat CustomCalc;
	CustomCalc.CalculationClassMagnitude = UGP_DamageCalculation::StaticClass();
	HealthModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(CustomCalc);

	Modifiers.Add(HealthModifier);
}
