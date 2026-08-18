// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effects/GPGE_UnitCap_Plus5.h"

#include "AttributeSets/GPPlayerAttributeSet.h"

FName UGP_GE_UnitCap_Plus5::GetMagnitudeDataName()
{
	static const FName Name(TEXT("GP.UnitCap.BonusMagnitude"));
	return Name;
}

UGP_GE_UnitCap_Plus5::UGP_GE_UnitCap_Plus5()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UGP_PlayerAttributeSet::GetMaxUnitsAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataName = GetMagnitudeDataName();
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Modifiers.Add(Modifier);
}
