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

/** Terminal result for a movement serial that was accepted/active (GP-S23 / GP-S33M). */
enum class EGP_MovementResult : uint8
{
	Reached,
	Cancelled,
	Failed
};

/** Reason accompanying a terminal movement result. Reached uses None. */
enum class EGP_MovementResultReason : uint8
{
	None,
	Superseded,
	CommandReplaced,
	Manual,
	PathNotFound,
	PathInvalid,
	DestinationOffNav,
	Blocked
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
	InvalidAcceptanceRadius,
	PathNotFound,
	DestinationOffNav
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
 * Server-authoritative movement backend (GP-S20–S23 + GP-S33M).
 * NavMesh path following + lightweight separation; component state is not replicated.
 * Public API remains RequestMove / StopMove / OnMovementResult for all command consumers.
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

	/** GP-S33M diagnostic: current nav path point count (0 if idle / straight fallback). */
	int32 GetActivePathPointCount() const { return PathPoints.Num(); }

	/** GP-S33M diagnostic: true when active path used NavigationSystem (not straight fallback). */
	bool IsActivePathFromNavigation() const { return bActivePathFromNavigation; }

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

	/** Nav projection half-extent (cm) for start/end points. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement|Navigation", meta = (ClampMin = "1.0"))
	float NavProjectionExtentXY = 250.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement|Navigation", meta = (ClampMin = "1.0"))
	float NavProjectionExtentZ = 400.0f;

	/** Minimum seconds between mid-path repath attempts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement|Navigation", meta = (ClampMin = "0.1"))
	float RepathIntervalSeconds = 0.75f;

	/** If blocked / no progress longer than this, Fail with Blocked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement|Navigation", meta = (ClampMin = "0.5"))
	float BlockedFailSeconds = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement|Separation", meta = (ClampMin = "0.0"))
	float SeparationRadius = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement|Separation", meta = (ClampMin = "0.0"))
	float SeparationStrength = 1.25f;

	/** Max fraction of MoveSpeed contributed by separation steering (0–1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement|Separation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxSteeringContribution = 0.55f;

	/**
	 * When true and the unit is on NavMesh, unreachable/off-nav destinations reject RequestMove.
	 * When NavMesh is missing OR the unit is outside nav coverage, uses straight-line fallback.
	 * Operator must add NavMeshBoundsVolume on playable maps for production pathfinding.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement|Navigation")
	bool bRequireNavigationWhenAvailable = true;

private:
	void ClearActiveMovementState();
	void BroadcastMovementResult(
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason,
		const FVector& DestinationForLog);

	bool TryBuildNavigationPath(const FVector& Start, const FVector& Dest, TArray<FVector>& OutPoints, bool& bOutUsedNav);
	FVector ComputeSteeringOffset(const FVector& OwnerLocation, const FVector& DesiredDir2D) const;
	void FinishMoveReached(AActor* Owner, const FVector& FinalLocation);
	void FinishMoveFailed(AActor* Owner, EGP_MovementResultReason Reason);

	static const TCHAR* StopReasonToString(EGP_MovementStopReason Reason);
	static const TCHAR* MovementResultToString(EGP_MovementResult Result);
	static const TCHAR* MovementResultReasonToString(EGP_MovementResultReason Reason);
	static const TCHAR* RejectReasonToString(EGP_MovementRejectReason Reason);
	static const TCHAR* RequestStatusToString(EGP_MovementRequestStatus Status);

	FGP_OnMovementResult MovementResultDelegate;

	FVector MoveDestination = FVector::ZeroVector;
	uint32 ActiveMoveSerial = 0;
	bool bIsMoving = false;

	TArray<FVector> PathPoints;
	int32 PathIndex = 0;
	bool bActivePathFromNavigation = false;
	double LastRepathWorldTime = -1.0;
	double LastProgressWorldTime = -1.0;
	FVector LastProgressLocation = FVector::ZeroVector;
};
