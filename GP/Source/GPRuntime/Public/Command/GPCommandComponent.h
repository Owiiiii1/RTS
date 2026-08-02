// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GPCommandComponent.generated.h"

struct FGP_CommandRequest;

/**
 * Local command orchestration shell.
 * Phase B: BuildSmartCommand builds speculative FGP_CommandRequest from selection.
 * No tick, replication, RPC, input, or command execution.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_CommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_CommandComponent();

	/**
	 * Build a local speculative command request from current selection + target context.
	 * Does not send RPC, mutate selection, or execute commands.
	 * On failure, OutRequest is left default-constructed.
	 */
	bool BuildSmartCommand(
		AActor* TargetActor,
		const FVector& TargetLocation,
		bool bQueue,
		FGP_CommandRequest& OutRequest) const;
};
