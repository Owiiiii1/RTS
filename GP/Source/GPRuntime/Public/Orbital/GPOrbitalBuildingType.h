// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPOrbitalBuildingType.generated.h"

/** MVP orbital building catalog entry (GP-S32R). */
UENUM(BlueprintType)
enum class EGP_OrbitalBuildingType : uint8
{
	None = 0,
	LogisticsHub
};
