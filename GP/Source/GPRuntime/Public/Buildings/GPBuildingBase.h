// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/GPUnitBase.h"
#include "GPBuildingBase.generated.h"

/**
 * Minimal static unit ancestor for buildings (GP-S28 adaptation ahead of full GP-S34).
 * No Production / Construction component. No permanent Tick.
 */
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_BuildingBase : public AGP_UnitBase
{
	GENERATED_BODY()

public:
	AGP_BuildingBase();
};
