// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GPGE_AddScore.generated.h"

/**
 * Instant C++ GE: Additive FerroniteScore via SetByCaller magnitude (GP-S30).
 * Spec magnitude key: UGP_GE_AddScore::GetMagnitudeDataName().
 */
UCLASS()
class GPGASRUNTIME_API UGP_GE_AddScore : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGP_GE_AddScore();

	static FName GetMagnitudeDataName();
};
