// Copyright Epic Games, Inc. All Rights Reserved.

#include "Effects/GPGE_AddScore.h"

#include "AttributeSets/GPPlayerAttributeSet.h"

FName UGP_GE_AddScore::GetMagnitudeDataName()
{
	static const FName Name(TEXT("GP.Launch.ScoreMagnitude"));
	return Name;
}

UGP_GE_AddScore::UGP_GE_AddScore()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UGP_PlayerAttributeSet::GetFerroniteScoreAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;

	FSetByCallerFloat SetByCaller;
	SetByCaller.DataName = GetMagnitudeDataName();
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);

	Modifiers.Add(Modifier);
}
