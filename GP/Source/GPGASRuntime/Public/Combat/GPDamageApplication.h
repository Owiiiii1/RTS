// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Templates/SubclassOf.h"

class UAbilitySystemComponent;

/** Result of an authoritative unit damage application attempt (plain C++). */
struct GPGASRUNTIME_API FGP_DamageApplicationResult
{
	bool bApplied = false;
	float HealthBefore = 0.0f;
	float HealthAfter = 0.0f;
	float RawDamage = 0.0f;
	float FinalDamage = 0.0f;
	FString RejectReason;
};

/**
 * Apply Instant UGP_GE_Damage_Basic from SourceASC to TargetASC.
 * Caller owns validation (authority, teams, dead state). Returns false when Apply is rejected.
 */
namespace GPDamageApplication
{
	GPGASRUNTIME_API bool ApplyDamageEffect(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		TSubclassOf<UGameplayEffect> DamageEffectClass,
		FGP_DamageApplicationResult& OutResult);
}
