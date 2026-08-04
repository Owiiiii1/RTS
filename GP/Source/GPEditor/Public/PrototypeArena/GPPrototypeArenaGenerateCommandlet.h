// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GPPrototypeArenaGenerateCommandlet.generated.h"

/** Headless editor automation entry that calls FGPPrototypeArenaGenerator. */
UCLASS()
class UGPPrototypeArenaGenerateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGPPrototypeArenaGenerateCommandlet();

	virtual int32 Main(const FString& Params) override;
};
