// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GPGE_UnitCap_Plus5.generated.h"

/**
 * Infinite C++ GE: Additive +5 MaxUnits (GP-S33C Logistics Hub bonus).
 * Native equivalent of GE_GP_UnitCap_Plus5. One active instance per living deployed Hub.
 */
UCLASS()
class GPGASRUNTIME_API UGP_GE_UnitCap_Plus5 : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGP_GE_UnitCap_Plus5();
};
