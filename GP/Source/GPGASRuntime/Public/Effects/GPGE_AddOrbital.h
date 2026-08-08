// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GPGE_AddOrbital.generated.h"

/**
 * Instant C++ GE: Additive OrbitalFerronite via SetByCaller magnitude (GP-S30).
 * Spec magnitude key: UGP_GE_AddOrbital::GetMagnitudeDataName().
 */
UCLASS()
class GPGASRUNTIME_API UGP_GE_AddOrbital : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGP_GE_AddOrbital();

	static FName GetMagnitudeDataName();
};
