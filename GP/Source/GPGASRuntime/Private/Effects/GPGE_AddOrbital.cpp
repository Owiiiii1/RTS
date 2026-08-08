// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effects/GPGE_AddOrbital.h"

#include "AttributeSets/GPPlayerAttributeSet.h"

FName UGP_GE_AddOrbital::GetMagnitudeDataName()
{
	static const FName Name(TEXT("GP.Launch.OrbitalMagnitude"));
	return Name;
}

UGP_GE_AddOrbital::UGP_GE_AddOrbital()
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
