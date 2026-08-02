// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GPUnitBase.generated.h"

/**
 * Abstract replicated unit ancestor (scaffold only).
 * Exists so reflected selection storage can use TWeakObjectPtr<AGP_UnitBase>.
 * Full gameplay UnitBase belongs to GP-S18 — not this prerequisite.
 */
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_UnitBase : public APawn
{
	GENERATED_BODY()

public:
	AGP_UnitBase();
};
