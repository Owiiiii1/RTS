// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GPResourceDefinitionSeedCommandlet.generated.h"

/**
 * Seeds / verifies DA_GP_Resource_Ferronite (GP-S23R).
 * Usage: -run=GPResourceDefinitionSeed [-VerifyOnly]
 */
UCLASS()
class UGPResourceDefinitionSeedCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGPResourceDefinitionSeedCommandlet();

	virtual int32 Main(const FString& Params) override;
};
