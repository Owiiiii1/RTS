// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;

/**
 * Per-unit command delivery payload (GP-S17 Phase E).
 * Plain C++ — not USTRUCT, not replicated, not Blueprint.
 * TargetActor is a synchronous raw pointer; receivers must not store it.
 */
struct GPRUNTIME_API FGP_UnitCommand
{
	FGameplayTag CommandTag;
	FVector TargetLocation = FVector::ZeroVector;
	AActor* TargetActor = nullptr;
	bool bQueue = false;
};
