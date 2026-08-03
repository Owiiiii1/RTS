// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GPDeathSink.generated.h"

struct FGameplayEffectModCallbackData;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGP_DeathSink : public UInterface
{
	GENERATED_BODY()
};

/**
 * GPGASRuntime-owned death notify sink.
 * Implemented by unit hosts (GPRuntime). AttributeSet must not include GPRuntime types.
 */
class GPGASRUNTIME_API IGP_DeathSink
{
	GENERATED_BODY()

public:
	virtual void HandleGASDeath(const FGameplayEffectModCallbackData& Data) = 0;
};
