// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effects/GPGE_UnitCap_Base5.h"

#include "AttributeSets/GPPlayerAttributeSet.h"

UGP_GE_UnitCap_Base5::UGP_GE_UnitCap_Base5()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UGP_PlayerAttributeSet::GetMaxUnitsAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(5.0f);
	Modifiers.Add(Modifier);
}
