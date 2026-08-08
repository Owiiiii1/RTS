// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GPGE_SpendOrbital.generated.h"

/**
 * Instant C++ GE: Additive OrbitalFerronite via SetByCaller (GP-S31R).
 * Apply with negative magnitude to spend. Spec key: GetMagnitudeDataName().
 */
UCLASS()
class GPGASRUNTIME_API UGP_GE_SpendOrbital : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGP_GE_SpendOrbital();

	static FName GetMagnitudeDataName();
};
