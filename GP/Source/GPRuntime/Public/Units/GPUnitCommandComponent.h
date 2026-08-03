// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Command/GPStoredUnitCommand.h"
#include "Misc/Optional.h"
#include "GPUnitCommandComponent.generated.h"

struct FGP_UnitCommand;

/**
 * Server-authoritative held-command ownership on AGP_UnitBase (GP-S18).
 * Accepts delivery payloads, stores one Held Command, apply replace / QueueDeferred.
 * No tick, replication, RPC, routing, or gameplay execution.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_UnitCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_UnitCommandComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Authority-only accept / replace / QueueDeferred. No execution. */
	void HandleCommand(const FGP_UnitCommand& Command);

	bool HasHeldCommand() const;

	/**
	 * Read-only pointer to internal held command, or nullptr if empty.
	 * Caller must not store the pointer beyond immediate synchronous use.
	 */
	const FGP_StoredUnitCommand* GetHeldCommand() const;

private:
	void ClearHeldCommand();
	uint32 AllocateCommandSerial();

	TOptional<FGP_StoredUnitCommand> HeldCommand;
	uint32 NextCommandSerial = 1;
};
