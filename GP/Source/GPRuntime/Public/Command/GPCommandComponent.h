// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GPCommandComponent.generated.h"

/**
 * Phase A ownership shell for future smart-command orchestration.
 * Owned by AGP_PlayerController. No tick, replication, RPC, request types,
 * selection state, input, or command execution.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_CommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_CommandComponent();
};
