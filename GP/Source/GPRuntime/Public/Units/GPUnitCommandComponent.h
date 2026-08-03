// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Command/GPStoredUnitCommand.h"
#include "Misc/Optional.h"
#include "GPUnitCommandComponent.generated.h"

class UGP_MovementComponent;
enum class EGP_MovementCompletionResult : uint8;
struct FGP_UnitCommand;

/**
 * Server-authoritative held-command ownership on AGP_UnitBase (GP-S18–S22).
 * GP-S21: synchronizes Held Move / non-Move replace with UGP_MovementComponent.
 * GP-S22: serial-aware Held clear on Reached completion.
 * No tick, replication, RPC, queue execution, or failure propagation.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_UnitCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_UnitCommandComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Authority-only accept / replace / QueueDeferred. Syncs movement for non-queued changes. */
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
	void SynchronizeMovementWithHeldCommand(const TOptional<FGP_StoredUnitCommand>& PreviousCommand);
	void HandleMovementCompleted(uint32 CompletedSerial, EGP_MovementCompletionResult Result);

	TOptional<FGP_StoredUnitCommand> HeldCommand;
	uint32 NextCommandSerial = 1;

	FDelegateHandle MovementCompletionHandle;
	TWeakObjectPtr<UGP_MovementComponent> BoundMovementComponent;
};
