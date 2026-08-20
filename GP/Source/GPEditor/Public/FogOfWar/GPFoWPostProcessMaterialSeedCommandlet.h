// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GPFoWPostProcessMaterialSeedCommandlet.generated.h"

/**
 * Seeds /Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess.
 * Usage: -run=GPFoWPostProcessMaterialSeed
 */
UCLASS()
class UGPFoWPostProcessMaterialSeedCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGPFoWPostProcessMaterialSeedCommandlet();

	virtual int32 Main(const FString& Params) override;
};
