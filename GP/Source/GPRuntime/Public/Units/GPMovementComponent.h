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
	EndPlay,
	OwnerDied
};

/** Terminal result for a movement serial that was accepted/active (GP-S23). Not UENUM / not Blueprint. */
enum class EGP_MovementResult : uint8
{
	Reached,
	Cancelled
};

/** Reason accompanying a terminal movement result. Reached uses None. */
enum class EGP_MovementResultReason : uint8
{
	None,
	Superseded,
	CommandReplaced,
	Manual
};

/** Synchronous RequestMove outcome status. Not delivered via delegate. */
enum class EGP_MovementRequestStatus : uint8
{
	Accepted,
	Rejected
};

/** Synchronous RequestMove rejection reason. */
enum class EGP_MovementRejectReason : uint8
{
	None,
	MissingOwner,
	NoAuthority,
	InvalidSerial,
	InvalidDestination,
	InvalidMoveSpeed,
	InvalidAcceptanceRadius
};

/** Structured immediate RequestMove outcome (GP-S23). Plain C++ — not USTRUCT. */
struct FGP_MovementRequestOutcome
{
	EGP_MovementRequestStatus Status = EGP_MovementRequestStatus::Rejected;
	EGP_MovementRejectReason RejectReason = EGP_MovementRejectReason::None;

	bool IsAccepted() const
	{
		return Status == EGP_MovementRequestStatus::Accepted;
	}
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FGP_OnMovementResult,
	uint32 /*Serial*/,
	EGP_MovementResult /*Result*/,
	EGP_MovementResultReason /*Reason*/);

/**
 * Server-authoritative straight-line movement backend (GP-S20–S23).
 * Authority mutates owner transform; component state is not replicated.
 * GP-S23: terminal OnMovementResult for Reached/Cancelled; sync RequestMove outcome for rejects.
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

	/** Authority-only. Returns structured Accepted/Rejected outcome; never broadcasts rejection. */
	FGP_MovementRequestOutcome RequestMove(const FVector& Destination, uint32 CommandSerial);

	/**
	 * Stops active movement.
	 * Manual / CommandReplaced broadcast Cancelled when an active move is stopped.
	 * EndPlay clears silently (no terminal broadcast).
	 * Authority-required except Reason=EndPlay (teardown-safe).
	 */
	void StopMove(EGP_MovementStopReason Reason = EGP_MovementStopReason::Manual);

	bool IsMoving() const;
	uint32 GetActiveMoveSerial() const;
	const FVector& GetMoveDestination() const;

	FGP_OnMovementResult& OnMovementResult();

#if !UE_BUILD_SHIPPING
	/** Synthetic terminal broadcast for stale-serial validation. Does not mutate movement state. */
	void DebugBroadcastResult(
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason);
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
	void BroadcastMovementResult(
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason,
		const FVector& DestinationForLog);

	static const TCHAR* StopReasonToString(EGP_MovementStopReason Reason);
	static const TCHAR* MovementResultToString(EGP_MovementResult Result);
	static const TCHAR* MovementResultReasonToString(EGP_MovementResultReason Reason);
	static const TCHAR* RejectReasonToString(EGP_MovementRejectReason Reason);
	static const TCHAR* RequestStatusToString(EGP_MovementRequestStatus Status);

	FGP_OnMovementResult MovementResultDelegate;

	FVector MoveDestination = FVector::ZeroVector;
	uint32 ActiveMoveSerial = 0;
	bool bIsMoving = false;
};
