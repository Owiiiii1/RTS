// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Resources/GPMiningComponent.h"
#include "Units/GPMobileUnit.h"
#include "GPWorker.generated.h"

class UCapsuleComponent;
class USceneComponent;
class UGP_CargoComponent;
class AGP_ResourceNode;

/** Blueprint cargo presentation signal (GP-S28P1). SoT remains UGP_CargoComponent. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FGP_OnCargoVisualStateChanged,
	bool, bVisible,
	float, FillNormalized,
	float, CurrentAmount,
	float, Capacity);

/**
 * Blueprint Niagara mining effect signal (GP-S28P1).
 * bEffectActive is true only while MiningComponent state is Mining.
 * SoT remains UGP_MiningComponent (no replicated presentation bool).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FGP_OnMiningEffectStateChanged,
	bool, bEffectActive,
	EGP_MiningState, PreviousState,
	EGP_MiningState, NewState,
	EGP_MiningStopReason, Reason);

/** Worker orchestration view (GP-S27/S28). MiningComponent remains SoT for mining execution. */
UENUM(BlueprintType)
enum class EGP_WorkerActivityState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	MovingToMine UMETA(DisplayName = "Moving To Mine"),
	WaitingForMiningSlot UMETA(DisplayName = "Waiting For Mining Slot"),
	Mining UMETA(DisplayName = "Mining"),
	CargoFull UMETA(DisplayName = "Cargo Full"),
	DepositDepleted UMETA(DisplayName = "Deposit Depleted"),
	ReturningToBase UMETA(DisplayName = "Returning To Base"),
	DroppingOff UMETA(DisplayName = "Dropping Off"),
	ReturningToDeposit UMETA(DisplayName = "Returning To Deposit"),
	WaitingForDropOff UMETA(DisplayName = "Waiting For Drop Off"),
	WaitingForResource UMETA(DisplayName = "Waiting For Resource"),
	CommandFailed UMETA(DisplayName = "Command Failed")
};

/**
 * Production Worker: MobileUnit + Cargo + Mining (GP-S27) + haul via UnitCommand (GP-S28).
 * Mine/haul execution is orchestrated by UGP_UnitCommandComponent (serial-aware movement).
 * GP-S28P1: PresentationRoot + CargoVisualAnchor + MiningEffectAnchor (no C++ StaticMesh/Niagara asset).
 * No auto-attack / CombatComponent.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_Worker : public AGP_MobileUnit
{
	GENERATED_BODY()

public:
	AGP_Worker();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "GP|Worker")
	UGP_CargoComponent* GetCargoComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker")
	UGP_MiningComponent* GetMiningComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker")
	UCapsuleComponent* GetCapsuleComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker|Presentation")
	USceneComponent* GetPresentationRoot() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker|Presentation")
	USceneComponent* GetCargoVisualAnchor() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker|Presentation")
	USceneComponent* GetMiningEffectAnchor() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker|Presentation")
	float GetCargoFillNormalized() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker|Presentation")
	bool HasCargoForVisual() const;

	/** Derived orchestration view from held Mine + movement + MiningComponent. */
	UFUNCTION(BlueprintPure, Category = "GP|Worker")
	EGP_WorkerActivityState GetWorkerActivityState() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker|ResourceSearch")
	float GetResourceSearchRadiusCm() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker|ResourceSearch")
	float GetMaxResourcePathLengthCm() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker|ResourceSearch")
	bool GetAllowManualTargetOutsideAutoSearchRadius() const
	{
		return bAllowManualTargetOutsideAutoSearchRadius;
	}

	/**
	 * Cargo presentation signal.
	 * Operator BP usage: keep backpack/container mesh always visible; do not hide via bVisible.
	 * Drive material color from FillNormalized (0 white → partial white-yellow → ~1 green).
	 * Cargo still grants atomically after each full mining cycle — no gradual gameplay transfer.
	 */
	UPROPERTY(BlueprintAssignable, Category = "GP|Worker|Presentation")
	FGP_OnCargoVisualStateChanged OnCargoVisualStateChanged;

	/** Niagara activate/deactivate while MiningComponent is in Mining state. */
	UPROPERTY(BlueprintAssignable, Category = "GP|Worker|Presentation")
	FGP_OnMiningEffectStateChanged OnMiningEffectStateChanged;

	bool ValidateWorkerContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	/** BP mesh attach parent under Capsule. No StaticMesh in C++. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PresentationRoot;

	/** BP cargo prop attach point under PresentationRoot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> CargoVisualAnchor;

	/** Niagara / VFX attach point under PresentationRoot (no system asset in C++). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> MiningEffectAnchor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Cargo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_CargoComponent> CargoComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Mining", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_MiningComponent> MiningComponent;

	/** Manual Mine targets outside project ResourceSearchRadius remain allowed when true. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Worker|ResourceSearch",
		meta = (AllowPrivateAccess = "true"))
	bool bAllowManualTargetOutsideAutoSearchRadius = true;

private:
	UFUNCTION()
	void HandleCargoAmountChanged(float PreviousAmount, float NewAmount, float Capacity, float Delta);

	UFUNCTION()
	void HandleMiningStateChanged(
		EGP_MiningState PreviousState,
		EGP_MiningState NewState,
		EGP_MiningStopReason Reason);

	void BindCargoVisualEvents();
	void UnbindCargoVisualEvents();
	void SyncCargoVisualState();

	void BindMiningEffectEvents();
	void UnbindMiningEffectEvents();
	void SyncMiningEffectState();

	bool bCargoVisualEventsBound = false;
	bool bMiningEffectEventsBound = false;
};

/** Non-shipping presentation contract probe (binds Dynamic multicast). */
UCLASS()
class GPRUNTIME_API UGP_CargoVisualStateProbe : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleCargoVisualStateChanged(bool bVisible, float FillNormalized, float CurrentAmount, float Capacity);

	int32 EventCount = 0;
	bool bLastVisible = true;
	float LastFill = -1.0f;
	float LastAmount = -1.0f;
	float LastCapacity = -1.0f;
};

/** Non-shipping mining effect presentation probe. */
UCLASS()
class GPRUNTIME_API UGP_MiningEffectStateProbe : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleMiningEffectStateChanged(
		bool bEffectActive,
		EGP_MiningState PreviousState,
		EGP_MiningState NewState,
		EGP_MiningStopReason Reason);

	int32 EventCount = 0;
	bool bLastEffectActive = false;
	EGP_MiningState LastPreviousState = EGP_MiningState::Idle;
	EGP_MiningState LastNewState = EGP_MiningState::Idle;
	EGP_MiningStopReason LastReason = EGP_MiningStopReason::None;
};

/** Staged Worker contract test runner (debug console). */
UCLASS()
class GPRUNTIME_API UGP_WorkerContractTestRunner : public UObject
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
	void DestroyWeakWorker(TWeakObjectPtr<AGP_Worker>& Weak);
	AGP_ResourceNode* SpawnNode(const FVector& Loc) const;

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_ResourceNode> TestNodeWeak;
	TWeakObjectPtr<AGP_Worker> PrimaryWorkerWeak;
	TArray<TWeakObjectPtr<AGP_Worker>> FifoWorkersWeak;
	TWeakObjectPtr<AGP_Worker> WaitingWorkerWeak;
	float InteractionRangeCm = 200.0f;
	int32 MovementWaitTicks = 0;
	double MovementWaitStartTime = -1.0;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
	static constexpr float MovementWaitTimeoutSeconds = 20.0f;
};

class AGP_MainBase;

/** Staged Worker hauling contract test runner (GP-S28 debug console). */
UCLASS()
class GPRUNTIME_API UGP_WorkerHaulingContractTestRunner : public UObject
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
	void Cancel(const TCHAR* Reason);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();
	void DestroyWeakWorker(TWeakObjectPtr<AGP_Worker>& Weak);
	void DestroyWeakMainBase(TWeakObjectPtr<AGP_MainBase>& Weak);
	AGP_ResourceNode* SpawnNode(const FVector& Loc) const;
	AGP_MainBase* SpawnMainBase(const FVector& Loc, int32 TeamId) const;
	AGP_Worker* SpawnWorker(const FVector& Loc, int32 TeamId) const;

	/**
	 * Spawn a ResourceNode near the current scenario corridor (not absolute far coords).
	 * Validates NavMesh projection + approach path to MainBase before accepting the actor.
	 */
	AGP_ResourceNode* SpawnNavigableNodeNearScenario(
		AGP_MainBase* Base,
		const AActor* AnchorActor,
		FString& OutFailReason,
		float* OutDistanceToBaseCm = nullptr) const;

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_ResourceNode> TestNodeWeak;
	TWeakObjectPtr<AGP_MainBase> MainBaseWeak;
	TWeakObjectPtr<AGP_MainBase> EnemyBaseWeak;
	TWeakObjectPtr<AGP_Worker> PrimaryWorkerWeak;
	FVector ScenarioBaseLocation = FVector::ZeroVector;
	FVector ScenarioNodeLocation = FVector::ZeroVector;
	float InteractionRangeCm = 200.0f;
	float DropOffRangeCm = 400.0f;
	int32 ContractTeamId = 1;
	int32 MovementWaitTicks = 0;
	double MovementWaitStartTime = -1.0;
	uint32 StaleHaulSerial = 0;
	float ThreatBefore = 0.0f;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
	static constexpr float MovementWaitTimeoutSeconds = 30.0f;
	static constexpr float AssumedTravelSpeedCmPerSec = 400.0f;
};

/** Deterministic diagnostic scenario spawn/cleanup contract (GP-S28). */
UCLASS()
class GPRUNTIME_API UGP_DiagnosticScenarioContractTestRunner : public UObject
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
	void Cancel(const TCHAR* Reason);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_MainBase> MainBaseWeak;
	TWeakObjectPtr<AGP_MainBase> RejectedMainBaseWeak;
	TWeakObjectPtr<AGP_MainBase> ReplacementMainBaseWeak;
	TWeakObjectPtr<AGP_Worker> WorkerWeak;
	TWeakObjectPtr<AGP_ResourceNode> NodeWeak;
	int32 ContractTeamId = 1;
	bool bOperatorTeam1PresentAtStart = false;
	TWeakObjectPtr<AGP_MainBase> OperatorTeam1MainBaseWeak;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};

/** GP-S28P4 client-safe MainBase resolve + Planetary Ferronite HUD data-source contract. */
UCLASS()
class GPRUNTIME_API UGP_PlanetaryFerroniteHUDContractTestRunner : public UObject
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
	void CleanupActors();

	UFUNCTION()
	void HandleStorageChanged(float PreviousTotalStored, float NewTotalStored, float TotalCapacity);

	void HandleResolvedMainBaseChanged(int32 TeamId, AGP_MainBase* PreviousMainBase, AGP_MainBase* NewMainBase);

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_MainBase> Team1BaseWeak;
	TWeakObjectPtr<AGP_MainBase> Team2BaseWeak;
	TWeakObjectPtr<AGP_MainBase> ReplacementWeak;
	int32 ResolvedChangeCount = 0;
	int32 LastResolvedTeamId = -1;
	TWeakObjectPtr<AGP_MainBase> LastResolvedNew;
	int32 StorageEventCount = 0;
	float LastStorageNewTotal = -1.0f;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};

/** GP-S28P3 drop-off resilience / WaitingForDropOff contract (debug console). */
UCLASS()
class GPRUNTIME_API UGP_DropOffResilienceContractTestRunner : public UObject
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
	void CleanupActors();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_ResourceNode> NodeWeak;
	TWeakObjectPtr<AGP_Worker> WorkerWeak;
	TWeakObjectPtr<AGP_Worker> SecondaryWorkerWeak;
	TWeakObjectPtr<AGP_MainBase> MainBaseWeak;
	FVector ScenarioBaseLocation = FVector::ZeroVector;
	FVector ScenarioNodeLocation = FVector::ZeroVector;
	int32 ContractTeamId = 1;
	float ThreatBefore = 0.0f;
	float CargoAtWait = 0.0f;
	float NodeAmountAtStorageWait = 0.0f;
	int32 StableWaitTicks = 0;
	int32 MovementWaitTicks = 0;
	double MovementWaitStartTime = 0.0;
	float MovementWaitTimeoutSeconds = 45.0f;
	float SavedDropOffRetrySeconds = 3.0f;
	bool bSettingsOverridden = false;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};

/** GP-S28P2 depletion / registry / reassignment contract (debug console). */
UCLASS()
class GPRUNTIME_API UGP_DepletionReassignmentContractTestRunner : public UObject
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
	void CleanupActors();

	UFUNCTION()
	void HandleDepleted(AGP_ResourceNode* ResourceNode, int32 PreviousAmount);

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_ResourceNode> NodeAWeak;
	TWeakObjectPtr<AGP_ResourceNode> NodeBWeak;
	TWeakObjectPtr<AGP_ResourceNode> WakeInsideWeak;
	TWeakObjectPtr<AGP_ResourceNode> WakeOutsideWeak;
	TWeakObjectPtr<AGP_Worker> WorkerWeak;
	TWeakObjectPtr<AGP_Worker> SlotHolderWeak;
	TWeakObjectPtr<AGP_MainBase> MainBaseWeak;
	TWeakObjectPtr<AGP_ResourceNode> FifoNodeWeak;
	TArray<TWeakObjectPtr<AGP_Worker>> FifoWorkers;
	FVector AnchorClusterLocation = FVector::ZeroVector;
	FVector MainBaseLocation = FVector::ZeroVector;
	float TestSearchRadiusCm = 1000.0f;
	float TestMaxPathLengthCm = 6000.0f;
	float SavedSettingsSearchRadiusCm = 3000.0f;
	float SavedSettingsMaxPathLengthCm = 6000.0f;
	float SavedSettingsRetrySeconds = 3.0f;
	bool bSettingsOverridden = false;
	int32 MovementWaitTicks = 0;
	double MovementWaitStartTime = 0.0;
	float MovementWaitTimeoutSeconds = 45.0f;
	int32 DepletionEventCount = 0;
	int32 LastDepletionPreviousAmount = -1;
	float PartialThreatBefore = 0.0f;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};

/** Sequential GP-S28 PIE regression suite (waits for each contract token release). */
UCLASS()
class GPRUNTIME_API UGP_S28RegressionSuiteRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void Start(UWorld* InWorld);

private:
	void StartNext();
	void OnChildFinished(uint64 ExecutionId, int32 ChildFailures);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();

	int32 SuiteIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	uint64 WaitingForExecutionId = 0;
};

/** Coordinator isolation + ownership + null-safety contract stages. */
UCLASS()
class GPRUNTIME_API UGP_ContractIsolationContractTestRunner : public UObject
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

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	TWeakObjectPtr<AGP_MainBase> OperatorBaseWeak;
	TWeakObjectPtr<AGP_Worker> OperatorWorkerWeak;
	TWeakObjectPtr<AGP_ResourceNode> OperatorNodeWeak;
	TWeakObjectPtr<AGP_Worker> HaulWorkerWeak;
	int32 ContractTeamId = 2;
};

/** GP-S29R LOS fire-gate Attack contract. */
UCLASS()
class GPRUNTIME_API UGP_LOSFireGateContractTestRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void SetExecutionToken(uint64 InExecutionId, FName InOwnerTag) { ExecutionId = InExecutionId; OwnerTag = InOwnerTag; }
	void Start(UWorld* InWorld);

private:
	void ScheduleNext(float DelaySeconds = 0.05f);
	void AdvanceStage();
	bool Expect(bool bOk, const TCHAR* Label);
	void Abort(const TCHAR* Reason);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();
	void CleanupActors();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_Worker> AttackerWeak;
	TWeakObjectPtr<AGP_Worker> TargetWeak;
	TWeakObjectPtr<AGP_Worker> FriendlyWeak;
	TWeakObjectPtr<AActor> BlockerWeak;
	float HealthAfterClearHit = -1.0f;
	float HealthAtBlock = -1.0f;
	double NextHitTimeAtBlock = -1.0;
	uint32 AttackSerialAtBlock = 0;
	int32 PollTicks = 0;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};

/** GP-S29R health-bar presentation contract. */
UCLASS()
class GPRUNTIME_API UGP_HealthBarContractTestRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void SetExecutionToken(uint64 InExecutionId, FName InOwnerTag) { ExecutionId = InExecutionId; OwnerTag = InOwnerTag; }
	void Start(UWorld* InWorld);

private:
	void ScheduleNext(float DelaySeconds = 0.05f);
	void AdvanceStage();
	bool Expect(bool bOk, const TCHAR* Label);
	bool ValidateActorHealthBar(class AGP_UnitBase* Owner, const TCHAR* Prefix);
	void Abort(const TCHAR* Reason);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();
	void CleanupActors();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_Worker> UnitWeak;
	TWeakObjectPtr<AGP_MainBase> BaseWeak;
	float FrameDrawSizeX = 0.0f;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};

/** GP-S29R team-color presentation contract. */
UCLASS()
class GPRUNTIME_API UGP_TeamColorContractTestRunner : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void SetExecutionToken(uint64 InExecutionId, FName InOwnerTag) { ExecutionId = InExecutionId; OwnerTag = InOwnerTag; }
	void Start(UWorld* InWorld);

private:
	void ScheduleNext(float DelaySeconds = 0.05f);
	void AdvanceStage();
	bool Expect(bool bOk, const TCHAR* Label);
	void Abort(const TCHAR* Reason);
	void Finish();
	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void UnbindWorldCleanup();
	void CleanupActors();

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<AGP_Worker> UnitWeak;
	TWeakObjectPtr<AGP_MainBase> BaseWeak;
	int32 TeamIdBefore = -1;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
