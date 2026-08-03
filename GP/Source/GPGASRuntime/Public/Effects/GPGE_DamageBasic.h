// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GPGE_DamageBasic.generated.h"

/**
 * Instant C++ damage GE (GP-S25A).
 * Health Additive magnitude from UGP_DamageCalculation MMC.
 * No Blueprint asset required.
 */
UCLASS()
class GPGASRUNTIME_API UGP_GE_Damage_Basic : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGP_GE_Damage_Basic();
};
