// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UClass;

/**
 * Ground placement for capsule-rooted units (GP-S31R).
 * Actor origin is capsule center — spawn Z must be GroundZ + scaled half-height
 * so the capsule bottom rests on the landing plane. Uses gameplay capsule, not mesh bounds.
 */
namespace GPUnitGroundPlacement
{
	/**
	 * Vertical offset to add to ground Z when spawning UnitClass.
	 * Reads CDO root / capsule; returns 0 if no capsule found.
	 */
	float GetGroundSpawnOffsetZForUnitClass(UClass* UnitClass);
}
