// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Resources/GPResourceNode.h"
#include "GPMiningComponent.generated.h"

class AGP_ResourceNode;
class UGP_CargoComponent;
class UGP_ResourceDefinition;
class USceneComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogGPMining, Log, All);

UENUM(BlueprintType)
enum class EGP_MiningState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	WaitingForSlot UMETA(DisplayName = "Waiting For Slot"),
	Mining UMETA(DisplayName = "Mining"),
	CargoFull UMETA(DisplayName = "Cargo Full"),
	DepositDepleted UMETA(DisplayName = "Deposit Depleted"),
	OutOfRange UMETA(DisplayName = "Out Of Range"),
	Invalid UMETA(DisplayName = "Invalid")
};

UENUM(BlueprintType)
enum class EGP_BeginMiningResult : uint8
{
	Started UMETA(DisplayName = "Started"),
	WaitingForSlot UMETA(DisplayName = "Waiting For Slot"),
	AlreadyMiningTarget UMETA(DisplayName = "Already Mining Target"),
	RejectedNoAuthority UMETA(DisplayName = "Rejected No Authority"),
	RejectedInvalidOwner UMETA(DisplayName = "Rejected Invalid Owner"),
	RejectedMissingCargo UMETA(DisplayName = "Rejected Missing Cargo"),
	RejectedInvalidNode UMETA(DisplayName = "Rejected Invalid Node"),
	RejectedDepleted UMETA(DisplayName = "Rejected Depleted"),
	RejectedCargoFull UMETA(DisplayName = "Rejected Cargo Full"),
	RejectedOutOfRange UMETA(DisplayName = "Rejected Out Of Range"),
	RejectedResourceMismatch UMETA(DisplayName = "Rejected Resource Mismatch")
};

UENUM(BlueprintType)
enum class EGP_MiningStopReason : uint8
{
	None UMETA(DisplayName = "None"),
	ManualStop UMETA(DisplayName = "Manual Stop"),
	CargoFull UMETA(DisplayName = "Cargo Full"),
	DepositDepleted UMETA(DisplayName = "Deposit Depleted"),
	OutOfRange UMETA(DisplayName = "Out Of Range"),
	InvalidTarget UMETA(DisplayName = "Invalid Target"),
	MissingCargo UMETA(DisplayName = "Missing Cargo"),
	InvariantFailure UMETA(DisplayName = "Invariant Failure"),
	OwnerEndPlay UMETA(DisplayName = "Owner End Play"),
	ComponentEndPlay UMETA(DisplayName = "Component End Play"),
	TargetEndPlay UMETA(DisplayName = "Target End Play"),
	ResourceMismatch UMETA(DisplayName = "Resource Mismatch")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FGP_OnMiningStateChanged,
	EGP_MiningState, PreviousState,
	EGP_MiningState, NewState,
	EGP_MiningStopReason, Reason);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(
	FGP_OnMiningCycleCompleted,
	AGP_ResourceNode*, ResourceNode,
	int32, RequestedAmount,
	int32, ConsumedAmount,
	float, CargoAcceptedAmount,
	int32, NodeAmountAfter,
	float, CargoAmountAfter);

/**
 * Authority mining loop: ResourceNode → CargoComponent (GP-S26).
 * Timer-driven cycles; occupancy-aware; no movement/Worker/Storage.
 * First cycle fires after a full MiningCycleDurationSeconds (no instant transfer on Begin).
 */
UCLASS(BlueprintType, ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_MiningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_MiningComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "GP|Mining")
	EGP_BeginMiningResult BeginMining(AGP_ResourceNode* ResourceNode);

	UFUNCTION(BlueprintCallable, Category = "GP|Mining")
	void StopMining(EGP_MiningStopReason Reason = EGP_MiningStopReason::ManualStop);

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	bool IsMining() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	bool IsWaitingForSlot() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	EGP_MiningState GetMiningState() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	EGP_MiningStopReason GetLastStopReason() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	AGP_ResourceNode* GetCurrentResourceNode() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	UGP_CargoComponent* GetCargoComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	float GetMiningCycleDuration() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	float GetAmountPerMiningCycle() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	float GetInteractionRangeCm() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	float GetDistanceToCurrentNode() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	bool IsInRangeOfCurrentNode() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	bool IsMiningTimerActive() const;

	/** True while StopMining is on the stack (ManualStop remine Idle is broadcast under this). */
	bool IsStopInProgress() const { return bIsStoppingMining; }

	bool ValidateMiningContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if !UE_BUILD_SHIPPING
	/** Debug-only: runs the same production cycle function without waiting for timer. */
	void DebugForceExecuteMiningCycle();
#endif

	UPROPERTY(BlueprintAssignable, Category = "GP|Mining|Events")
	FGP_OnMiningStateChanged OnMiningStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "GP|Mining|Events")
	FGP_OnMiningCycleCompleted OnMiningCycleCompleted;

protected:
	UFUNCTION()
	void OnRep_CurrentMiningState(EGP_MiningState PreviousState);

	void ExecuteMiningCycle();
	void SetMiningState(EGP_MiningState NewState, EGP_MiningStopReason Reason);
	void ClearMiningTimer();
	void StartMiningTimer();
	void BindOccupancyEvents(AGP_ResourceNode* Node);
	void UnbindOccupancyEvents();
	void HandleMinerSlotStateChanged(AActor* Miner, EGP_MinerOccupancyState OldState, EGP_MinerOccupancyState NewState);
	void ReleaseSlotOnCurrentNode();
	void ClearTargetReferences();
	bool ResolveMiningTunables(bool bAllowSyncLoad);
	bool AreResourceIdentitiesCompatible(const AGP_ResourceNode* Node, const UGP_CargoComponent* Cargo, bool bAllowSyncLoad) const;
	UGP_CargoComponent* FindOwnerCargoComponent() const;
	bool HasAuthorityOwner() const;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentMiningState, Category = "GP|Mining")
	EGP_MiningState CurrentMiningState = EGP_MiningState::Idle;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "GP|Mining")
	TObjectPtr<AGP_ResourceNode> CurrentResourceNode = nullptr;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "GP|Mining")
	EGP_MiningStopReason LastStopReason = EGP_MiningStopReason::None;

private:
	FTimerHandle MiningCycleTimerHandle;
	FDelegateHandle OccupancyDelegateHandle;
	TWeakObjectPtr<UGP_CargoComponent> CachedCargoComponent;
	TWeakObjectPtr<AGP_ResourceNode> BoundOccupancyNode;

	/** Guards StopMining against occupancy-delegate reentrancy (Release→Broadcast→Stop). */
	bool bIsStoppingMining = false;

	/**
	 * True while ExecuteMiningCycle is inside ConsumeResource→AddCargo.
	 * Depletion ClearOccupancy must not StopMining before cargo is credited.
	 */
	bool bExecutingMiningCycle = false;

	float CachedAmountPerMiningCycle = 0.0f;
	float CachedMiningCycleDurationSeconds = 0.0f;
	float CachedInteractionRangeCm = 0.0f;
};

/**
 * Transient host for mining diagnostics (Cargo + Mining). Not a Worker/Unit. Do not save to maps.
 * Owns a USceneComponent root so spawn/set transforms apply to actor location.
 */
UCLASS(NotPlaceable, Transient)
class GPRUNTIME_API AGP_MiningDiagnosticHost : public AActor
{
	GENERATED_BODY()

public:
	AGP_MiningDiagnosticHost();

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	UGP_CargoComponent* GetCargoComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	UGP_MiningComponent* GetMiningComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	USceneComponent* GetSceneRoot() const;

protected:
	/** Root for spawn transform / GetActorLocation. No collision, not nav-relevant. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Mining")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Mining")
	TObjectPtr<UGP_CargoComponent> CargoComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Mining")
	TObjectPtr<UGP_MiningComponent> MiningComponent;
};

/**
 * Debug-only host with MiningComponent but no CargoComponent (missing-cargo rejection tests).
 * Does not destroy default subobjects at runtime.
 */
UCLASS(NotPlaceable, Transient)
class GPRUNTIME_API AGP_MiningNoCargoDiagnosticHost : public AActor
{
	GENERATED_BODY()

public:
	AGP_MiningNoCargoDiagnosticHost();

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	UGP_MiningComponent* GetMiningComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Mining")
	USceneComponent* GetSceneRoot() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Mining")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Mining")
	TObjectPtr<UGP_MiningComponent> MiningComponent;
};

/** Staged contract test runner (debug console). Next-tick stages; weak refs; reentrancy guard. */
UCLASS()
class GPRUNTIME_API UGP_MiningContractTestRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	void SetExecutionToken(uint64 InExecutionId, FName InOwnerTag) { ExecutionId = InExecutionId; OwnerTag = InOwnerTag; }
	void Start(UWorld* InWorld);
	void Abort(const TCHAR* Reason);

private:
	void ScheduleNext();
	void AdvanceStage();
	bool Expect(bool bOk, const TCHAR* Label);
	void LogStage(const TCHAR* StageName) const;
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();
	AGP_MiningDiagnosticHost* SpawnHostNear(AGP_ResourceNode* Node, float RangeCm) const;
	AGP_ResourceNode* SpawnTransientNode(const FVector& Location) const;
	void SafeStopAndDestroyHost(TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak);

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_ResourceNode> TestNodeWeak;
	TWeakObjectPtr<AGP_MiningDiagnosticHost> PrimaryHostWeak;
	TArray<TWeakObjectPtr<AGP_MiningDiagnosticHost>> FifoHostsWeak;
	TWeakObjectPtr<AGP_MiningDiagnosticHost> WaitingHostWeak;
	int32 NodeAmountBeforeCycles = 0;
	float InteractionRangeCm = 200.0f;
	FTimerHandle StageTimerHandle;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};

/**
 * ResourceNode EndPlay occupancy teardown contract (GP-S28).
 * Verifies snapshot/clear/guard path: no live-array mutation during listener cleanup.
 */
UCLASS()
class GPRUNTIME_API UGP_ResourceNodeEndPlayContractTestRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void SetExecutionToken(uint64 InExecutionId, FName InOwnerTag) { ExecutionId = InExecutionId; OwnerTag = InOwnerTag; }
	void Start(UWorld* InWorld);

private:
	void ScheduleNext();
	void AdvanceStage();
	bool Expect(bool bOk, const TCHAR* Label);
	void Abort(const TCHAR* Reason);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();
	AGP_ResourceNode* SpawnTransientNode(const FVector& Location) const;
	AGP_MiningDiagnosticHost* SpawnHostNear(AGP_ResourceNode* Node, float RangeCm) const;
	void SafeStopAndDestroyHost(TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak);

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_ResourceNode> TestNodeWeak;
	TArray<TWeakObjectPtr<AGP_MiningDiagnosticHost>> OccupancyHostsWeak;
	TWeakObjectPtr<AGP_MiningDiagnosticHost> WaitingHostWeak;
	TWeakObjectPtr<AGP_MiningDiagnosticHost> HaulHostWeak;
	TWeakObjectPtr<class AGP_Worker> HaulWorkerWeak;
	TWeakObjectPtr<class AGP_MainBase> HaulMainBaseWeak;
	float ThreatBeforeNodeDestroy = 0.0f;
	float InteractionRangeCm = 200.0f;
	int32 TerminalNoneCount = 0;
	int32 PromotionCount = 0;
	FDelegateHandle OccupancyObserveHandle;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
