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
class UGP_StorageComponent;
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
	/** Cargo held; MainBase missing / destroyed / unreachable / storage full (GP-S28P3 + GP-S30). */
	WaitingForDropOff,
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

/** AutoAcquire candidate policy (GP-S37T). Not a generic targeting framework. */
enum class EGP_AutoAcquireMode : uint8
{
	LegacyUnitIdle,
	DefensiveTurretIdle,
	AttackMove
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

	/**
	 * GP-S40R: after successful hostile ApplyDamageFromUnit.
	 * Autonomous retaliation — not a player-visible Held command.
	 */
	void NotifyHostileDamageReceived(AGP_UnitBase* SourceUnit);

	bool IsRetaliationActive() const { return bRetaliationActive; }
	AGP_UnitBase* GetRetaliationTarget() const { return RetaliationTarget.Get(); }

	EGP_AttackExecutionState GetAttackExecutionState() const;
	uint32 GetActiveAttackSerial() const;
	AGP_UnitBase* GetAttackTarget() const;

	/** Effective runtime Attack range (GAS if >0, else component fallback). Fire/engage range. */
	float GetAttackRange() const;

	/**
	 * GP-S30R: effective auto-acquire / sight range (cm).
	 * Runtime: max(configured AutoAcquireSightRangeCm, AttackRange) so sight never under-fires acquire.
	 */
	float GetEffectiveAutoAcquireRange() const;

	bool IsAttackActive() const;
	double GetNextAttackHitTime() const;
	bool HasAttemptedFirstAttackHit() const;
	bool IsAttackTargetDeathBound() const;
	EGP_AttackRangeSource GetAttackRangeSource() const;
	const TCHAR* GetAttackRangeSourceLabel() const;

	/** GP-S30R: true when Idle and eligible for combat auto-acquire scan. */
	bool IsEligibleForCombatAutoAcquire() const;

	/**
	 * GP-S32A: true when Held AttackMove may scan while travelling (not Idle Move).
	 * Pure Move remains suppressed via IsEligibleForCombatAutoAcquire.
	 */
	bool IsEligibleForAttackMoveAcquire() const;

	/** Combat-capable unit with Held Patrol may acquire without replacing Patrol. */
	bool IsEligibleForPatrolAcquire() const;

	/** GP-S32A: Held command is AttackMove (destination travel or temporary engage). */
	bool IsAttackMoveActive() const;

	/** GP-S32A: Attack FSM currently running under AttackMove ownership. */
	bool IsAttackMoveEngaging() const;

	/** GP-S32A: original AttackMove destination (Held TargetLocation while active). */
	FVector GetAttackMoveDestination() const;

	bool IsPatrolActive() const;
	FVector GetPatrolAnchorA() const;
	FVector GetPatrolAnchorB() const;
	bool IsPatrolHeadingToB() const;
	bool IsPatrolEngaging() const;
	FVector GetPendingPatrolDestination() const;

	/** GP-S30R diagnostic: last auto-acquire scan found a candidate (not replicated). */
	AGP_UnitBase* DebugGetLastAutoAcquireCandidate() const { return LastAutoAcquireCandidate.Get(); }

	/**
	 * GP-S30R: server Idle scan interval for combat auto-acquire (seconds).
	 * Component tuning seam — not a new settings system.
	 */
	UPROPERTY(EditAnywhere, Category = "GP|Combat|AutoAcquire", meta = (ClampMin = "0.05"))
	float AutoAcquireScanIntervalSeconds = 0.35f;

	/**
	 * GP-S30R: sight / auto-acquire range (cm). Not fire range.
	 * Effective acquire uses max(this, AttackRange). SW default tuned to 900 with AttackRange 600.
	 */
	UPROPERTY(EditAnywhere, Category = "GP|Combat|AutoAcquire", meta = (ClampMin = "0.0"))
	float AutoAcquireSightRangeCm = 900.0f;

	/**
	 * GP-S30R: yaw-only facing toward Attack target while Ready (deg/sec).
	 * Approaching facing remains movement-driven (UGP_MovementComponent::RotationSpeed).
	 */
	UPROPERTY(EditAnywhere, Category = "GP|Combat|Facing", meta = (ClampMin = "0.0"))
	float AttackFacingRotationSpeedDegreesPerSecond = 360.0f;

	/** Authority helper: apply interval and restart scan timer (contracts / tuning). */
	void RefreshCombatAutoAcquireTimer();

	/**
	 * Diagnostic: last observed Ready LOS fire-gate result for the active Attack.
	 * True after CLEAR→BLOCKED until BLOCKED→CLEAR or Attack reset. Not replicated.
	 */
	bool IsAttackLOSBlocked() const;

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
	/** Next haul approach request fails once (unreachable / PathRejected harness). */
	void DebugForceNextHaulApproachRejectOnce();

	/** Mine resource-search anchor diagnostics (GP-S28P2 contract). */
	bool DebugHasMineSearchAnchor() const { return bHasMineSearchAnchor; }
	FVector DebugGetMineSearchAnchorLocation() const { return MineSearchAnchorLocation; }

	/** Drop-off wait subscription / wake diagnostics (GP-S28P3). */
	int32 DebugGetDropOffWakeCount() const { return DebugDropOffWakeCount; }
	bool DebugIsWaitingRegisterBound() const { return bMainBaseRegisteredDropOffBound; }
	bool DebugIsDropOffStorageWakeBound() const { return bDropOffStorageWakeBound; }
	bool DebugIsActiveHaulUnregisterBound() const { return bMainBaseUnregisteredHaulBound; }
	bool DebugIsDropOffRetryArmed() const { return DropOffRetryTimerHandle.IsValid(); }

	/** FIFO / re-entry watchdog counters for contract tests (not Tick). */
	void DebugResetFifoWatchdogCounters();
	int32 DebugGetMineBeginCallsThisTransition() const { return DebugMineBeginCallsThisTransition; }
	int32 DebugGetReassignmentAttemptsThisTransition() const { return DebugReassignmentAttemptsThisTransition; }
	int32 DebugGetSameTargetRetargetAttempts() const { return DebugSameTargetRetargetAttempts; }

	/** True when UnitCommand is subscribed to MiningComponent::OnMiningStateChanged. */
	bool DebugIsMiningStateEventBound() const { return BoundMiningComponent.IsValid(); }

	/** Bitmask of haul/mine approach candidate indices to skip (contract harness). */
	void DebugSetApproachSkipCandidateMask(int32 Mask) { DebugApproachSkipCandidateMask = Mask; }
	int32 DebugGetApproachSkipCandidateMask() const { return DebugApproachSkipCandidateMask; }
	int32 DebugGetLastApproachCandidateIndex() const { return DebugLastApproachCandidateIndex; }
	int32 DebugGetLastApproachCandidateCount() const { return DebugLastApproachCandidateCount; }

	uint32 DebugGetRetaliationMovementSerial() const { return RetaliationMovementSerial; }
	uint32 DebugGetLastRetaliationMovementSerial() const { return LastRetaliationMovementSerial; }
	float DebugGetRetaliationRemainingSeconds() const;
#endif

	/**
	 * Direct terminal notify from MiningComponent after multicast broadcast.
	 * Safety net when UnitCommand was unbound but a Mine chain still owns the cargo terminal.
	 */
	void NotifyMiningComponentTerminal(
		EGP_MiningState PreviousState,
		EGP_MiningState NewState,
		EGP_MiningStopReason Reason);

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

	void ResetPatrolExecutor();
	void BeginPatrolExecutor();
	bool TryConsumePatrolMovementResult(
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason);
	bool ResumePatrolTravelAfterEngagement();

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
		FName SearchReason);
	bool TryAutoReassignMine(
		uint32 MineSerial,
		AGP_ResourceNode* PreferredOrFailedNode,
		bool bPreferFreeSlotFirst,
		FName SearchReason);
	void EnterWaitingForResource(uint32 MineSerial);
	/**
	 * If Worker still carries cargo, haul to MainBase instead of WaitingForResource.
	 * Returns true when haul was started (or redirect attempted with valid MainBase).
	 * Non-shipping: Error log on stranded-cargo invariant. No recursive re-enter.
	 */
	bool TryHaulPartialCargoBeforeWaiting(uint32 MineSerial, AGP_ResourceNode* DepositHint);
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

	/**
	 * Nav-aware interaction approach: several deterministic candidates around Target,
	 * project + complete FindPathSync, pick shortest reachable path inside InteractionRange
	 * and outside NavigationObstacle / collision footprint when authored.
	 */
	bool TryFindReachableRangeApproachDestination(
		const AActor* Owner,
		const AActor* Target,
		float InteractionRangeCm,
		float AcceptanceRadius,
		float ExtraInwardMarginCm,
		uint32 LogSerial,
		FVector& OutDestination,
		float& OutDesiredHorizontalDistance,
		float& OutPredictedWorstCaseDistance,
		float* OutPathLengthCm = nullptr,
		int32* OutCandidateIndex = nullptr) const;

	float ResolveTargetApproachClearanceHalfXY(const AActor* Target) const;

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
	void StartHaulReturnToBase(
		uint32 ChainSerial,
		AGP_ResourceNode* Deposit,
		bool bReturnToDeposit,
		AGP_MainBase* PreferredMainBase = nullptr);
	/** Explicit friendly MainBase Move: one-shot deposit, no mining-cycle resume. */
	bool TryStartOneShotMainBaseDeposit();
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

	/** GP-S28P3 WaitingForDropOff — MainBase missing/destroyed/unreachable + storage-full recovery. */
	void EnterWaitingForDropOff(FName Reason);
	void ClearDropOffSubscriptionsAndTimer();
	void BindActiveHaulMainBaseUnregister();
	void UnbindActiveHaulMainBaseUnregister();
	void BindDropOffWaitingRegisterWake();
	void UnbindDropOffWaitingRegisterWake();
	void BindDropOffWaitingStorageWake();
	void UnbindDropOffWaitingStorageWake();
	void ArmDropOffRetryTimer();
	void ClearDropOffRetryTimer();
	void HandleMainBaseUnregisteredActiveHaul(AGP_MainBase* MainBase);
	void HandleMainBaseRegisteredDropOffWake(AGP_MainBase* MainBase);
	UFUNCTION()
	void HandleDropOffWaitingStorageChanged(
		float PreviousTotalStored,
		float NewTotalStored,
		float TotalCapacity);
	void HandleDropOffSafetyRetry();
	void TryResumeHaulFromDropOffWait(FName WakeReason);
	void ExecuteScheduledDropOffHaulResume();
	bool WorkerHasHaulCargo() const;
	bool TeamMainBaseHasStorageRoom() const;

	static const TCHAR* MineStateToString(EGP_MineExecutionState State);
	static const TCHAR* HaulStateToString(EGP_HaulExecutionState State);

	float ResolveApproachSafetyMarginCm() const;

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

	/** GP-S30R Idle auto-acquire (authority, rate-limited). */
	void StartCombatAutoAcquireTimer();
	void StopCombatAutoAcquireTimer();
	void OnCombatAutoAcquireScan();
	bool IsCombatCapableForAutoAcquire(const AGP_UnitBase* Unit) const;
	EGP_AutoAcquireMode ResolveIdleAutoAcquireMode(const AGP_UnitBase* OwnerUnit) const;
	bool IsEligibleAutoAcquireTarget(
		const AGP_UnitBase* OwnerUnit,
		const AGP_UnitBase* Candidate,
		EGP_AutoAcquireMode Mode) const;
	AGP_UnitBase* FindNearestAutoAcquireTarget(float MaxRangeCm, EGP_AutoAcquireMode Mode) const;
	void TryIssueAutoAcquireAttack(AGP_UnitBase* Target);
	void UpdateAttackFacingTowardTarget(float DeltaTime);

	/** GP-S32A: engage under Held AttackMove without replacing destination ownership. */
	bool StartAttackMoveEngagement(AGP_UnitBase* Target);
	void TryIssueAttackMoveAcquire(AGP_UnitBase* Target);
	bool ResumeAttackMoveTravelAfterEngagement();
	bool IsHeldAttackMove(uint32 Serial) const;

	/** GP-S40R: autonomous retaliation pursuit (not a Held player command). */
	bool IsMobileCombatUnitForRetaliation(const AGP_UnitBase* Unit) const;
	bool HasCommandThatBlocksRetaliationStart() const;
	bool IsRetaliationAttackerValid(const AGP_UnitBase* Attacker) const;
	bool CanEngageRetaliationTarget(AGP_UnitBase* Attacker) const;
	void StartOrRefreshRetaliation(AGP_UnitBase* Attacker);
	void CancelRetaliation(const TCHAR* Reason, bool bStopRetaliationMovement);
	void BindRetaliationAttackerDeath(AGP_UnitBase* Attacker);
	void UnbindRetaliationAttackerDeath();
	void HandleRetaliationAttackerDied(AGP_UnitBase* DeadUnit);
	void ArmRetaliationTimeout(float DurationSeconds);
	void ClearRetaliationTimers();
	void OnRetaliationTimeout();
	void OnRetaliationEvaluate();
	void RequestRetaliationPursuitMove(AGP_UnitBase* Attacker, bool bForceIssue);
	void TryEngageRetaliationTarget(AGP_UnitBase* Attacker);
	bool TryConsumeRetaliationMovementResult(
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason);

	static const TCHAR* AttackStateToString(EGP_AttackExecutionState State);
	static const TCHAR* AttackTerminalResultToString(EGP_AttackTerminalResult Result);
	static const TCHAR* AttackTerminalReasonToString(EGP_AttackTerminalReason Reason);
	static const TCHAR* AttackRangeSourceToString(EGP_AttackRangeSource Source);

	TOptional<FGP_StoredUnitCommand> HeldCommand;
	uint32 NextCommandSerial = 1;

	bool bPatrolActive = false;
	FVector PatrolAnchorA = FVector::ZeroVector;
	FVector PatrolAnchorB = FVector::ZeroVector;
	bool bPatrolHeadingToB = true;

	FDelegateHandle MovementResultHandle;
	TWeakObjectPtr<UGP_MovementComponent> BoundMovementComponent;

	EGP_AttackExecutionState AttackState = EGP_AttackExecutionState::Idle;
	uint32 ActiveAttackSerial = 0;
	TWeakObjectPtr<AGP_UnitBase> AttackTarget;
	FVector LastApproachDestination = FVector::ZeroVector;

	FTimerHandle AutoAcquireTimerHandle;
	TWeakObjectPtr<AGP_UnitBase> LastAutoAcquireCandidate;

	bool bRetaliationActive = false;
	bool bIssuingRetaliationEngageCommand = false;
	TWeakObjectPtr<AGP_UnitBase> RetaliationTarget;
	FTimerHandle RetaliationTimeoutHandle;
	FTimerHandle RetaliationEvaluateHandle;
	FDelegateHandle RetaliationAttackerDiedHandle;
	TWeakObjectPtr<AGP_UnitBase> BoundRetaliationAttacker;
	uint32 RetaliationMovementSerial = 0;
	uint32 LastRetaliationMovementSerial = 0;
	FVector LastRetaliationDestination = FVector::ZeroVector;
	double LastRetaliationIssueTime = -1.0;
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

	/** Diagnostic LOS fire-gate latched state for active Attack (spam-safe transition logs). */
	bool bAttackLOSBlocked = false;

	/** GP-S25B unreachable-range / no-progress approach tracking. */
	bool bHasReachedOutOfRangeSample = false;
	float LastReachedOutOfRangeDistance = -1.0f;
	FVector LastReachedOutOfRangeLocation = FVector::ZeroVector;
	int32 ConsecutiveNoProgressApproachCount = 0;

	/** GP-S27 Mine orchestration (Worker only). */
	EGP_MineExecutionState MineState = EGP_MineExecutionState::Idle;
	uint32 ActiveMineSerial = 0;

	/**
	 * Last deposit chosen for the active Mine chain (original or reassigned).
	 * Survives MineTarget.Reset() during terminal handling so CargoFull can resolve haul deposit.
	 */
	TWeakObjectPtr<AGP_ResourceNode> LastMineDepositForHaul;
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

	/** Prevents recursive BeginMiningAtHeldTarget ↔ Retarget chains in one stack. */
	bool bBeginMiningAtHeldTargetInProgress = false;

	/** Prevents EnterWaitingForResource ↔ stranded-cargo haul redirect recursion. */
	bool bRedirectingStrandedCargoHaul = false;

#if !UE_BUILD_SHIPPING
	/** Suppress identical WaitingForResource no-candidate summaries. */
	int32 LastWaitingNoCandidateRegistryCount = -1;
	FName LastWaitingNoCandidateReason = NAME_None;
	FVector LastWaitingNoCandidateAnchor = FVector::ZeroVector;
	bool bHasLastWaitingNoCandidate = false;

	bool bLoggedSameTargetRetargetSkip = false;
	int32 DebugMineBeginCallsThisTransition = 0;
	int32 DebugReassignmentAttemptsThisTransition = 0;
	int32 DebugSameTargetRetargetAttempts = 0;
	int32 DebugApproachSkipCandidateMask = 0;
	mutable int32 DebugLastApproachCandidateIndex = -1;
	mutable int32 DebugLastApproachCandidateCount = 0;
#endif

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

	/** Active-haul current-target invalidation (ReturningToBase / DroppingOff). */
	FDelegateHandle MainBaseUnregisteredHaulHandle;
	bool bMainBaseUnregisteredHaulBound = false;

	/** WaitingForDropOff register wake. */
	FDelegateHandle MainBaseRegisteredDropOffHandle;
	bool bMainBaseRegisteredDropOffBound = false;
	/** WaitingForDropOff storage-capacity wake (OnStorageChanged; no Tick). */
	TWeakObjectPtr<UGP_StorageComponent> BoundDropOffWaitStorage;
	bool bDropOffStorageWakeBound = false;
	FTimerHandle DropOffRetryTimerHandle;
	bool bEnteringDropOffWait = false;
	bool bDropOffWakeInProgress = false;
	bool bDropOffResumeScheduled = false;
	uint32 PendingDropOffResumeSerial = 0;
	TWeakObjectPtr<AGP_ResourceNode> PendingDropOffResumeDeposit;
	bool bPendingDropOffResumeReturnToDeposit = false;
	FName LastDropOffWaitReason = NAME_None;
	FName LastDropOffRetryLogReason = NAME_None;

#if !UE_BUILD_SHIPPING
	bool bDebugForceNextMineArrivalOutOfRange = false;
	bool bDebugForceNextHaulArrivalOutOfRange = false;
	bool bDebugForceNextHaulApproachReject = false;
	int32 DebugDropOffWakeCount = 0;
#endif
};
