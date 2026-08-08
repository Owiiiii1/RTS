// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effects/GPGE_SpendOrbital.h"

#include "AttributeSets/GPPlayerAttributeSet.h"

FName UGP_GE_SpendOrbital::GetMagnitudeDataName()
{
	static const FName Name(TEXT("GP.Drop.OrbitalSpendMagnitude"));
	return Name;
}

UGP_GE_SpendOrbital::UGP_GE_SpendOrbital()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataName = GetMagnitudeDataName();
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(Modifier);
}
