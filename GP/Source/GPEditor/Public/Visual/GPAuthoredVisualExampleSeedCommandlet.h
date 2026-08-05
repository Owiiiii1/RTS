// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GPAuthoredVisualExampleSeedCommandlet.generated.h"

/**
 * Seeds minimal Blueprint authored visual examples (GP-S26B2A):
 * BP_Unit_AuthoredExample, BP_ResourceNode_AuthoredExample.
 */
UCLASS()
class UGPAuthoredVisualExampleSeedCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGPAuthoredVisualExampleSeedCommandlet();

	virtual int32 Main(const FString& Params) override;
};
