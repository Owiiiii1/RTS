// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GPVisualProfileSeedCommandlet.generated.h"

/**
 * Seeds default editable primitive visual profiles (GP-S26B2A):
 * DA_Visual_InfantryMelee, DA_Visual_Ore under /Game/GrimProtocol/VisualProfiles.
 */
UCLASS()
class UGPVisualProfileSeedCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGPVisualProfileSeedCommandlet();

	virtual int32 Main(const FString& Params) override;
};
