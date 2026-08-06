// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Command/GPStoredUnitCommand.h"
#include "Misc/Optional.h"
#include "GPUnitCommandComponent.generated.h"

class AGP_UnitBase;
class AGP_Worker;
class AGP_ResourceNode;
class AGP_MainBase;
class UGP_MovementComponent;
class UGP_MiningComponent;
enum class EGP_MovementResult : uint8;
enum class EGP_MovementResultReason : uint8;
enum class EGP_MiningState : uint8;
enum class EGP_MiningStopReason : uint8;
struct FGP_UnitCommand;

/** Attack executor runtime state (GP-S24/S25B). Plain C++ — not UENUM / not Blueprint. */
enum class EGP_AttackExecutionState : uint8
{
	Idle,
	Approaching,
	Ready
};

/** Mine approach / active orchestration (GP-S27/S28P2). Plain C++ — not UENUM / not Blueprint. */
enum class EGP_MineExecutionState : uint8
{
	Idle,
	Approaching,
	Active,
	/** Cargo not full; no reachable compatible ResourceNode candidate. */
	WaitingForResource
};

/** Haul / return-to-base orchestration (GP-S28). Plain C++ — not UENUM / not Blueprint. */
enum class EGP_HaulExecutionState : uint8
{
	Idle,
	ReturningToBase,
	DroppingOff,
	ReturningToDeposit,
	WaitingForStorage,
	Failed
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
	RangeUnreachable,
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
 * GP-S27: authority Mine approach / BeginMining for AGP_Worker (serial-aware).
 * GP-S28: authority haul / drop-off / return-to-deposit chain (serial-aware).
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

	EGP_MineExecutionState GetMineExecutionState() const;
	uint32 GetActiveMineSerial() const;
	AGP_ResourceNode* GetMineTarget() const;
	bool IsMineApproachActive() const;

	/** GP-S27 approach diagnostics (server orchestration; not replicated). */
	FVector GetMineApproachDestination() const;
	float GetMineApproachDesiredNodeDistance() const;
	float GetMineApproachSafetyMarginCm() const;
	int32 GetMineApproachAttempt() const;
	float GetMinePredictedWorstCaseDistance() const;
	float GetMineLastArrivalDistance() const;
	float GetMineLastArrivalRangeError() const;

	EGP_HaulExecutionState GetHaulExecutionState() const;
	uint32 GetActiveHaulSerial() const;
	AGP_ResourceNode* GetLastHaulDeposit() const;
	AGP_MainBase* GetHaulMainBase() const;
	bool IsHaulActive() const;
	bool ShouldReturnToDepositAfterHaul() const;
	float GetLastHaulAcceptedAmount() const;
	float GetLastHaulRejectedAmount() const;
	float GetLastHaulThreatDelta() const;
	FVector GetHaulApproachDestination() const;
	float GetHaulApproachDesiredDistance() const;
	int32 GetHaulApproachAttempt() const;
	float GetHaulLastArrivalDistance() const;
	float GetHaulDropOffRangeCm() const;

#if !UE_BUILD_SHIPPING
	/** Next arrival distance check treats Worker as slightly OOR once (contract test). */
	void DebugForceNextMineArrivalOutOfRangeOnce();
	void DebugForceNextHaulArrivalOutOfRangeOnce();

	/** Mine resource-search anchor diagnostics (GP-S28P2 contract). */
	bool DebugHasMineSearchAnchor() const { return bHasMineSearchAnchor; }
	FVector DebugGetMineSearchAnchorLocation() const { return MineSearchAnchorLocation; }
#endif

	UPROPERTY(EditDefaultsOnly, Category = "GP|Attack")
	float AttackRange = 250.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Attack")
	float AttackReissueDistance = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Attack")
	float AttackReissueInterval = 0.25f;

private:
	/**
	 * Ready→Approaching only when Distance > EffectiveRange + this tolerance.
	 * Entry into Ready remains Distance <= EffectiveRange (no early hits).
	 */
	static constexpr float AttackReadyExitTolerance = 20.0f;

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

	bool TryAcceptIdempotentMineCommand(const FGP_UnitCommand& Command) const;
	bool TryRejectMineCommandBeforeAccept(const FGP_UnitCommand& Command) const;
	bool StartMineExecutor();
	void ResetMineExecutor();
	void ResetMineExecutorForReplacement(const TOptional<FGP_StoredUnitCommand>& PreviousCommand);
	bool TryConsumeMineMovementResult(
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason);
	void BeginMiningAtHeldTarget(uint32 MineSerial);
	void BindMiningStateEvents(UGP_MiningComponent* Mining);
	void UnbindMiningStateEvents();
	UFUNCTION()
	void HandleMiningStateChanged(
		EGP_MiningState PreviousState,
		EGP_MiningState NewState,
		EGP_MiningStopReason Reason);
	float ResolveMineInteractionRangeCm(const UGP_MiningComponent* Mining, const AGP_ResourceNode* Node) const;

	/** GP-S28P2 path-aware reassignment / WaitingForResource. */
	bool TryRetargetMineToNode(AGP_ResourceNode* NewNode, uint32 MineSerial, bool bStartApproach);
	AGP_ResourceNode* FindAutoResourceCandidate(
		AGP_Worker* Worker,
		AGP_ResourceNode* ExcludeNode,
		bool bRequireFreeSlot,
		FName SearchReason) const;
	bool TryAutoReassignMine(
		uint32 MineSerial,
		AGP_ResourceNode* PreferredOrFailedNode,
		bool bPreferFreeSlotFirst,
		FName SearchReason);
	void EnterWaitingForResource(uint32 MineSerial);
	void BindResourceRegistryWake();
	void UnbindResourceRegistryWake();
	void HandleResourceNodeRegisteredWake(AGP_ResourceNode* Node);
	void HandleWaitingForResourceSafetyRetry();
	void SetMineSearchAnchorFromNode(const AGP_ResourceNode* Node);
	void ClearMineSearchAnchor();

	/**
	 * Computes approach destination so even AcceptanceRadius edge completion stays
	 * strictly inside InteractionRange in 3D (accounts for DeltaZ + safety margin).
	 */
	bool TryMakeRangeApproachDestination(
		const AActor* Owner,
		const AActor* Target,
		float InteractionRangeCm,
		float AcceptanceRadius,
		float ExtraInwardMarginCm,
		FVector& OutDestination,
		float& OutDesiredHorizontalDistance,
		float& OutPredictedWorstCaseDistance) const;

	bool TryMakeMineApproachDestination(
		const AActor* Owner,
		const AGP_ResourceNode* Node,
		float InteractionRangeCm,
		float AcceptanceRadius,
		float ExtraInwardMarginCm,
		FVector& OutDestination,
		float& OutDesiredHorizontalDistance,
		float& OutPredictedWorstCaseDistance) const;

	bool RequestMineApproachMove(
		AActor* Owner,
		AGP_ResourceNode* Node,
		uint32 MineSerial,
		float ExtraInwardMarginCm,
		const TCHAR* LogLabel);

	bool IsActiveHaulChainForDeposit(const AGP_ResourceNode* Node) const;
	void ResetHaulExecutor();
	void ResetHaulExecutorForReplacement(const TOptional<FGP_StoredUnitCommand>& PreviousCommand);
	void StartHaulReturnToBase(uint32 ChainSerial, AGP_ResourceNode* Deposit, bool bReturnToDeposit);
	bool RequestHaulApproachMove(
		AActor* Owner,
		AGP_MainBase* MainBase,
		uint32 HaulSerial,
		float ExtraInwardMarginCm,
		const TCHAR* LogLabel);
	bool TryConsumeHaulMovementResult(
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason);
	void BeginDropOffAtMainBase(uint32 HaulSerial);
	void FinishHaulChain(bool bClearHeld);
	void ContinueMineAfterSuccessfulHaul(uint32 ChainSerial);

	static const TCHAR* MineStateToString(EGP_MineExecutionState State);
	static const TCHAR* HaulStateToString(EGP_HaulExecutionState State);

	/** Inward margin beyond AcceptanceRadius so worst-case arrival stays < InteractionRange. */
	static constexpr float WorkerMineApproachSafetyMarginCm = 25.0f;

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
	void ClearApproachProgressState();

	/**
	 * Movement Reached while still outside effective range.
	 * Returns true when Attack was terminated (unreachable / no progress).
	 */
	bool HandleReachedStillOutOfRange(
		AActor* Owner,
		AGP_UnitBase* Target,
		float Distance,
		float EffectiveRange,
		EGP_AttackRangeSource RangeSource);

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

	/** GP-S25B unreachable-range / no-progress approach tracking. */
	bool bHasReachedOutOfRangeSample = false;
	float LastReachedOutOfRangeDistance = -1.0f;
	FVector LastReachedOutOfRangeLocation = FVector::ZeroVector;
	int32 ConsecutiveNoProgressApproachCount = 0;

	/** GP-S27 Mine orchestration (Worker only). */
	EGP_MineExecutionState MineState = EGP_MineExecutionState::Idle;
	uint32 ActiveMineSerial = 0;
	TWeakObjectPtr<AGP_ResourceNode> MineTarget;
	TWeakObjectPtr<UGP_MiningComponent> BoundMiningComponent;
	bool bFinishingMine = false;

	FVector MineApproachDestination = FVector::ZeroVector;
	float MineApproachDesiredNodeDistance = -1.0f;
	float MinePredictedWorstCaseDistance = -1.0f;
	float MineLastArrivalDistance = -1.0f;
	float MineLastArrivalRangeError = -1.0f;
	int32 MineApproachAttempt = 0;

	FDelegateHandle ResourceNodeRegisteredHandle;
	FTimerHandle WaitingForResourceRetryTimerHandle;
	bool bResourceRegistryWakeBound = false;

	/**
	 * Persistent resource-cluster anchor for Mine intent (GP-S28P2).
	 * SearchCenter for auto-reassignment; survives target Destroy / haul to MainBase.
	 * Cleared on command replace / Mine cancel / EndPlay via ResetMineExecutor.
	 */
	FVector MineSearchAnchorLocation = FVector::ZeroVector;
	bool bHasMineSearchAnchor = false;

	/** GP-S28 Haul orchestration (Worker only; shares Mine command serial as chain id). */
	EGP_HaulExecutionState HaulState = EGP_HaulExecutionState::Idle;
	uint32 ActiveHaulSerial = 0;
	TWeakObjectPtr<AGP_ResourceNode> LastHaulDeposit;
	TWeakObjectPtr<AGP_MainBase> HaulMainBase;
	bool bShouldReturnToDepositAfterHaul = false;
	bool bFinishingHaul = false;
	float LastHaulAcceptedAmount = 0.0f;
	float LastHaulRejectedAmount = 0.0f;
	float LastHaulThreatDelta = 0.0f;
	float HaulDropOffRangeCm = 400.0f;

	FVector HaulApproachDestination = FVector::ZeroVector;
	float HaulApproachDesiredDistance = -1.0f;
	float HaulPredictedWorstCaseDistance = -1.0f;
	float HaulLastArrivalDistance = -1.0f;
	float HaulLastArrivalRangeError = -1.0f;
	int32 HaulApproachAttempt = 0;

#if !UE_BUILD_SHIPPING
	bool bDebugForceNextMineArrivalOutOfRange = false;
	bool bDebugForceNextHaulArrivalOutOfRange = false;
#endif
};
