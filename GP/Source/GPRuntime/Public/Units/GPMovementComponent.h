// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GPMovementComponent.generated.h"

/** Plain C++ stop reason for UGP_MovementComponent (GP-S21). Not UENUM / not Blueprint. */
enum class EGP_MovementStopReason : uint8
{
	Manual,
	CommandReplaced,
	EndPlay
};

/** Plain C++ completion result (GP-S22). Broadcast only Reached today. Not UENUM / not Blueprint. */
enum class EGP_MovementCompletionResult : uint8
{
	Reached
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FGP_OnMovementCompleted,
	uint32 /*CompletedSerial*/,
	EGP_MovementCompletionResult /*Result*/);

/**
 * Server-authoritative straight-line movement backend (GP-S20–S22).
 * Authority mutates owner transform; component state is not replicated.
 * GP-S22: broadcasts Reached completion after local active state is cleared.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_MovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_MovementComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Authority-only. Returns true only when the move request is accepted. */
	bool RequestMove(const FVector& Destination, uint32 CommandSerial);

	/**
	 * Stops active movement without callbacks / completion broadcast.
	 * Authority-required except Reason=EndPlay (teardown-safe).
	 */
	void StopMove(EGP_MovementStopReason Reason = EGP_MovementStopReason::Manual);

	bool IsMoving() const;
	uint32 GetActiveMoveSerial() const;
	const FVector& GetMoveDestination() const;

	FGP_OnMovementCompleted& OnMovementCompleted();

#if !UE_BUILD_SHIPPING
	/** Synthetic Reached broadcast for stale-serial validation. Does not mutate movement state. */
	void DebugBroadcastCompletion(uint32 Serial);
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement")
	float AcceptanceRadius = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement")
	float RotationSpeed = 360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement")
	bool bRotateToMovement = true;

private:
	void ClearActiveMovementState();
	static const TCHAR* StopReasonToString(EGP_MovementStopReason Reason);

	FGP_OnMovementCompleted MovementCompletedDelegate;

	FVector MoveDestination = FVector::ZeroVector;
	uint32 ActiveMoveSerial = 0;
	bool bIsMoving = false;
};
