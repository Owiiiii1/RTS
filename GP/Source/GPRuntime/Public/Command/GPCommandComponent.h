// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GPCommandComponent.generated.h"

struct FGP_CommandRequest;

/** Internal server command validation reject reasons (Phase D). Not UENUM / not replicated. */
enum class EGP_CommandRejectReason : uint8
{
	None,
	InvalidController,
	InvalidPlayerState,
	InvalidRequestingTeam,
	InvalidCommandTag,
	UnsupportedCommandTag,
	NoCommandableUnits,
	InvalidTarget,
	FriendlyAttackTarget,
	InvalidResourceTarget,
	InvalidTargetLocation
};

DECLARE_LOG_CATEGORY_EXTERN(LogGPCommandServer, Log, All);

/**
 * Local command orchestration.
 * Phase B: BuildSmartCommand builds speculative FGP_CommandRequest from selection.
 * Phase D: ValidateAndNormalizeCommand authoritative server normalize (no execution).
 * No tick, replication, input, or command execution.
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

	/**
	 * Authoritative validate/normalize of a client candidate request (Phase D).
	 * Owner must be AGP_PlayerController. Does not dispatch or execute.
	 * On failure, OutValidatedRequest is default; OutRejectReason is set.
	 */
	bool ValidateAndNormalizeCommand(
		const FGP_CommandRequest& ClientRequest,
		FGP_CommandRequest& OutValidatedRequest,
		EGP_CommandRejectReason& OutRejectReason) const;
};
