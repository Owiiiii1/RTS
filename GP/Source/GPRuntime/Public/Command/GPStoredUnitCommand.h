// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;

/**
 * Per-unit held command payload (GP-S18).
 * Plain C++ — not USTRUCT, not replicated, not Blueprint.
 * TargetActor is lifetime-safe; delivery FGP_UnitCommand remains sync AActor*.
 */
struct GPRUNTIME_API FGP_StoredUnitCommand
{
	FGameplayTag CommandTag;
	FVector TargetLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> TargetActor;
	bool bQueue = false;
	uint32 CommandSerial = 0;
};
