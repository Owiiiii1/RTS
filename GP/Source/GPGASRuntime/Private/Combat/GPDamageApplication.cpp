// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/GPDamageApplication.h"

#include "AbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "GameplayEffect.h"

bool GPDamageApplication::ApplyDamageEffect(
	UAbilitySystemComponent* SourceASC,
	UAbilitySystemComponent* TargetASC,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	FGP_DamageApplicationResult& OutResult)
{
	OutResult = FGP_DamageApplicationResult();

	if (SourceASC == nullptr || TargetASC == nullptr)
	{
		OutResult.RejectReason = TEXT("MissingASC");
		return false;
	}

	if (*DamageEffectClass == nullptr)
	{
		OutResult.RejectReason = TEXT("MissingDamageEffectClass");
		return false;
	}

	const UGP_UnitAttributeSet* SourceAttrs = SourceASC->GetSet<UGP_UnitAttributeSet>();
	const UGP_UnitAttributeSet* TargetAttrs = TargetASC->GetSet<UGP_UnitAttributeSet>();
	if (SourceAttrs == nullptr || TargetAttrs == nullptr)
	{
		OutResult.RejectReason = TEXT("MissingUnitAttributeSet");
		return false;
	}

	OutResult.RawDamage = FMath::Max(0.0f, SourceAttrs->GetDamage());
	OutResult.HealthBefore = TargetAttrs->GetHealth();

	FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
	ContextHandle.AddSourceObject(SourceASC->GetAvatarActor());

	const FGameplayEffectSpecHandle SpecHandle =
		SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		OutResult.RejectReason = TEXT("InvalidOutgoingSpec");
		return false;
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	const float HealthAfter = TargetAttrs->GetHealth();
	OutResult.HealthAfter = HealthAfter;
	OutResult.FinalDamage = FMath::Max(0.0f, OutResult.HealthBefore - HealthAfter);
	OutResult.bApplied = true;
	return true;
}
