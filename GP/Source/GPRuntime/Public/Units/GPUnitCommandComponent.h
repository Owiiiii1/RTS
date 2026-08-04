// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Command/GPStoredUnitCommand.h"
#include "Misc/Optional.h"
#include "GPUnitCommandComponent.generated.h"

class AGP_UnitBase;
class UGP_MovementComponent;
enum class EGP_MovementResult : uint8;
enum class EGP_MovementResultReason : uint8;
struct FGP_UnitCommand;

/** Attack executor runtime state (GP-S24/S25B). Plain C++ — not UENUM / not Blueprint. */
enum class EGP_AttackExecutionState : uint8
{
	Idle,
	Approaching,
	Ready
};

/** Terminal Attack outcome. Ready is not terminal. */
enum class EGP_AttackTerminalResult : uint8
{
	Cancelled,
	Failed
};

/** Reason for Attack terminal / accept rejection. */
enum class EGP_AttackTerminalReason : uint8
{
	CommandReplaced,
	InvalidTarget,
	TargetDestroyed,
	TargetDied,
	MovementRejected,
	MovementCancelled,
	EndPlay
};

/** Effective Attack range resolution source (GP-S25B). */
enum class EGP_AttackRangeSource : uint8
{
	GAS,
	FallbackComponent,
	Invalid
};

/**
 * Server-authoritative held-command ownership on AGP_UnitBase (GP-S18–S25B).
 * GP-S21–S23: Held Move sync + serial-aware movement results.
 * GP-S24: authority Attack approach / Ready.
 * GP-S25B: Ready hit cadence + TargetDied binding via GP-S25A ApplyDamageFromUnit.
 * No replication, RPC, queue execution, or Blueprint Attack API.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_UnitCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_UnitCommandComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Authority-only accept / replace / QueueDeferred. Syncs movement for non-queued changes. */
	void HandleCommand(const FGP_UnitCommand& Command);

	bool HasHeldCommand() const;

	/**
	 * Read-only pointer to internal held command, or nullptr if empty.
	 * Caller must not store the pointer beyond immediate synchronous use.
	 */
	const FGP_StoredUnitCommand* GetHeldCommand() const;

	/**
	 * Authority-only owner death shutdown (GP-S25A/S25B).
	 * Unbinds target death, resets cadence/Attack, stops movement (OwnerDied), clears Held.
	 */
	void NotifyOwnerDied();

	EGP_AttackExecutionState GetAttackExecutionState() const;
	uint32 GetActiveAttackSerial() const;
	AGP_UnitBase* GetAttackTarget() const;

	/** Effective runtime Attack range (GAS if >0, else component fallback). */
	float GetAttackRange() const;

	bool IsAttackActive() const;
	double GetNextAttackHitTime() const;
	bool HasAttemptedFirstAttackHit() const;
	bool IsAttackTargetDeathBound() const;
	EGP_AttackRangeSource GetAttackRangeSource() const;
	const TCHAR* GetAttackRangeSourceLabel() const;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Attack")
	float AttackRange = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Attack")
	float AttackReissueDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Attack")
	float AttackReissueInterval = 0.25f;

private:
	void ClearHeldCommand();
	uint32 AllocateCommandSerial();

	/** Returns false when a Move RequestMove reject cleared Held. */
	bool SynchronizeMovementWithHeldCommand(const TOptional<FGP_StoredUnitCommand>& PreviousCommand);

	void HandleMovementResult(
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason);

	bool ValidateAttackTarget(
		AActor* Candidate,
		AGP_UnitBase*& OutTarget,
		EGP_AttackTerminalReason& OutReason) const;

	/** Returns false when accept-time reject cleared Held Attack. */
	bool StartAttackExecutor();
	void ResetAttackExecutor();
	void ResetAttackExecutorForReplacement(const TOptional<FGP_StoredUnitCommand>& PreviousCommand);
	void EvaluateAttack();
	void EnterAttackApproaching();
	void EnterAttackReady();
	void RequestOrRefreshAttackApproach(bool bForceIssue);
	void FinishAttack(
		EGP_AttackTerminalResult Result,
		EGP_AttackTerminalReason Reason);
	bool TryConsumeAttackMovementResult(
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason);

	void SetAttackTickEnabled(bool bEnabled);
	bool HasExactActiveHeldAttack() const;
	bool IsAttackConfigValid() const;

	bool TryResolveEffectiveAttackRange(
		float& OutRange,
		EGP_AttackRangeSource& OutSource) const;
	float ResolveSanitizedAttackCooldown(bool bLogSanitize) const;
	void ProcessReadyCadence();
	void AttemptAttackHit();

	void BindAttackTargetDeath(AGP_UnitBase* Target);
	void UnbindAttackTargetDeath();
	void HandleAttackTargetDied(AGP_UnitBase* DeadUnit);
	void ClearAttackCadenceState();

	/** Returns false when distance is unavailable (null/invalid target). OutDistance stays -1.f. */
	bool TryComputeAttackDistance2D(
		const AActor* Owner,
		const AActor* Target,
		float& OutDistance) const;

	FVector MakeApproachDestination(const AActor* Owner, const AActor* Target) const;
	UGP_MovementComponent* ResolveMovementComponent() const;

	static const TCHAR* AttackStateToString(EGP_AttackExecutionState State);
	static const TCHAR* AttackTerminalResultToString(EGP_AttackTerminalResult Result);
	static const TCHAR* AttackTerminalReasonToString(EGP_AttackTerminalReason Reason);
	static const TCHAR* AttackRangeSourceToString(EGP_AttackRangeSource Source);

	TOptional<FGP_StoredUnitCommand> HeldCommand;
	uint32 NextCommandSerial = 1;

	FDelegateHandle MovementResultHandle;
	TWeakObjectPtr<UGP_MovementComponent> BoundMovementComponent;

	EGP_AttackExecutionState AttackState = EGP_AttackExecutionState::Idle;
	uint32 ActiveAttackSerial = 0;
	TWeakObjectPtr<AGP_UnitBase> AttackTarget;
	FVector LastApproachDestination = FVector::ZeroVector;
	double LastApproachIssueTime = -1.0;
	bool bExpectRangeEntryStop = false;
	bool bFinishingAttack = false;

	/** Internal FinishAttack StopMove(Manual) cleanup — not range-entry. */
	bool bExpectAttackCleanupStopResult = false;
	uint32 PendingAttackCleanupMovementSerial = 0;

	/** GP-S25B cadence / target-death binding. */
	FDelegateHandle TargetDiedHandle;
	TWeakObjectPtr<AGP_UnitBase> BoundDeathTarget;
	double NextAttackHitTime = -1.0;
	bool bHasAttemptedFirstHit = false;
	bool bAttackHitInProgress = false;
};
