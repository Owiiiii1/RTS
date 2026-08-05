// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/GPMobileUnit.h"
#include "GPWorker.generated.h"

class UCapsuleComponent;
class UGP_CargoComponent;
class UGP_MiningComponent;
class AGP_ResourceNode;

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
	WaitingForStorage UMETA(DisplayName = "Waiting For Storage"),
	CommandFailed UMETA(DisplayName = "Command Failed")
};

/**
 * Production Worker: MobileUnit + Cargo + Mining (GP-S27) + haul via UnitCommand (GP-S28).
 * Mine/haul execution is orchestrated by UGP_UnitCommandComponent (serial-aware movement).
 * No auto-attack / CombatComponent.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_Worker : public AGP_MobileUnit
{
	GENERATED_BODY()

public:
	AGP_Worker();

	UFUNCTION(BlueprintPure, Category = "GP|Worker")
	UGP_CargoComponent* GetCargoComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker")
	UGP_MiningComponent* GetMiningComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Worker")
	UCapsuleComponent* GetCapsuleComponent() const;

	/** Derived orchestration view from held Mine + movement + MiningComponent. */
	UFUNCTION(BlueprintPure, Category = "GP|Worker")
	EGP_WorkerActivityState GetWorkerActivityState() const;

	bool ValidateWorkerContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Cargo", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_CargoComponent> CargoComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Mining", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_MiningComponent> MiningComponent;
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
