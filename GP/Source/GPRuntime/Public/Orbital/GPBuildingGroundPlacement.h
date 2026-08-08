// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UClass;

/**
 * Ground placement for capsule-rooted buildings (GP-S32R).
 * Actor origin is capsule center — spawn Z must be GroundZ + scaled half-height.
 */
namespace GPBuildingGroundPlacement
{
	float GetGroundSpawnOffsetZForBuildingClass(UClass* BuildingClass);
}
