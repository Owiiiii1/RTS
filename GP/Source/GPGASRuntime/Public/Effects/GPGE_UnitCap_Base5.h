// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GPGE_UnitCap_Base5.generated.h"

/**
 * Infinite C++ GE: Additive +5 MaxUnits (GP-S33C base player capacity).
 * Applied once per PlayerState ASC on authority.
 */
UCLASS()
class GPGASRUNTIME_API UGP_GE_UnitCap_Base5 : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGP_GE_UnitCap_Base5();
};
