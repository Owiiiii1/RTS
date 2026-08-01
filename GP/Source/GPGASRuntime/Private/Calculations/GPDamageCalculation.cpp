// Copyright Epic Games, Inc. All Rights Reserved.

#include "Calculations/GPDamageCalculation.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "GameplayEffectExecutionCalculation.h"

struct FGPDamageStatics
{
	DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);
	DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
	DECLARE_ATTRIBUTE_CAPTUREDEF(DamageResistance);

	FGPDamageStatics()
	{
		// Source/Target = capture side; false = live (non-snapshot) at apply/eval time.
		DEFINE_ATTRIBUTE_CAPTUREDEF(UGP_UnitAttributeSet, Damage, Source, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UGP_UnitAttributeSet, Armor, Target, false);
		DEFINE_ATTRIBUTE_CAPTUREDEF(UGP_UnitAttributeSet, DamageResistance, Target, false);
	}
};

static const FGPDamageStatics& DamageStatics()
{
	static FGPDamageStatics Statics;
	return Statics;
}

UGP_DamageCalculation::UGP_DamageCalculation()
{
	RelevantAttributesToCapture.Add(DamageStatics().DamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
	RelevantAttributesToCapture.Add(DamageStatics().DamageResistanceDef);
}

float UGP_DamageCalculation::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvaluationParameters.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	float CapturedDamage = 0.0f;
	float CapturedArmor = 0.0f;
	float CapturedDamageResistance = 0.0f;

	// MMC API is GetCapturedAttributeMagnitude (internally uses AttemptCalculateAttributeMagnitude).
	// FGameplayEffectCustomExecutionParameters::AttemptCalculateCapturedAttributeMagnitude is Execution-only.
	if (!GetCapturedAttributeMagnitude(DamageStatics().DamageDef, Spec, EvaluationParameters, CapturedDamage))
	{
		UE_LOG(LogTemp, Warning, TEXT("UGP_DamageCalculation: failed to capture Source Damage; using 0."));
		CapturedDamage = 0.0f;
	}

	if (!GetCapturedAttributeMagnitude(DamageStatics().ArmorDef, Spec, EvaluationParameters, CapturedArmor))
	{
		UE_LOG(LogTemp, Warning, TEXT("UGP_DamageCalculation: failed to capture Target Armor; using 0."));
		CapturedArmor = 0.0f;
	}

	if (!GetCapturedAttributeMagnitude(DamageStatics().DamageResistanceDef, Spec, EvaluationParameters, CapturedDamageResistance))
	{
		UE_LOG(LogTemp, Warning, TEXT("UGP_DamageCalculation: failed to capture Target DamageResistance; using 0."));
		CapturedDamageResistance = 0.0f;
	}

	const float RawDamage = FMath::Max(0.0f, CapturedDamage);
	const float EffectiveArmor = FMath::Max(0.0f, CapturedArmor);
	const float Resistance = FMath::Clamp(CapturedDamageResistance, 0.0f, 1.0f);

	const float DamageAfterArmor = FMath::Max(0.0f, RawDamage - EffectiveArmor);
	const float EffectiveDamage = DamageAfterArmor * (1.0f - Resistance);
	const float HealthDelta = -EffectiveDamage;

	UE_LOG(LogTemp, Verbose,
		TEXT("UGP_DamageCalculation: Damage=%.2f Armor=%.2f Resistance=%.2f -> AfterArmor=%.2f Effective=%.2f Return=%.2f"),
		RawDamage, EffectiveArmor, Resistance, DamageAfterArmor, EffectiveDamage, HealthDelta);

	return HealthDelta;
}
