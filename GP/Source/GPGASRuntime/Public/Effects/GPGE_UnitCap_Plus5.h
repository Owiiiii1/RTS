// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GPGE_UnitCap_Plus5.generated.h"

/**
 * Infinite C++ GE: Additive MaxUnits via SetByCaller (GP-S39E).
 * Magnitude comes from UGP_BuildingDefinition.UnitCapBonus. Class name kept for compatibility.
 */
UCLASS()
class GPGASRUNTIME_API UGP_GE_UnitCap_Plus5 : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGP_GE_UnitCap_Plus5();

	static FName GetMagnitudeDataName();
};
