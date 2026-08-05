// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GPStorageComponent.generated.h"

class UGP_ResourceDefinition;

DECLARE_LOG_CATEGORY_EXTERN(LogGPStorage, Log, All);

UENUM(BlueprintType)
enum class EGP_StorageContainerState : uint8
{
	Empty UMETA(DisplayName = "Empty"),
	Filling UMETA(DisplayName = "Filling"),
	Ready UMETA(DisplayName = "Ready"),
	/** Scaffold only in GP-S28 — launch mutation belongs to GP-S36. */
	Launching UMETA(DisplayName = "Launching")
};

USTRUCT(BlueprintType)
struct FGP_StorageContainer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage")
	float CurrentAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage")
	EGP_StorageContainerState State = EGP_StorageContainerState::Empty;
};

USTRUCT(BlueprintType)
struct FGP_StorageAddResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage")
	float Requested = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage")
	float Accepted = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage")
	float Rejected = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage")
	int32 ContainersTouched = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage")
	bool bStorageFullAfter = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage")
	bool bReadyContainerCreated = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage")
	bool bRejectedInvalidInput = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FGP_OnStorageChanged,
	float, PreviousTotalStored,
	float, NewTotalStored,
	float, TotalCapacity);

/**
 * MainBase Planetary Ferronite containers (GP-S28).
 * Sole writable SoT for raw Planetary Ferronite at base.
 * No permanent Tick. Launch / Orbital / Score are out of S28 scope.
 */
UCLASS(BlueprintType, ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_StorageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_StorageComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "GP|Storage")
	float GetContainerCapacity() const { return ContainerCapacity; }

	UFUNCTION(BlueprintPure, Category = "GP|Storage")
	int32 GetContainerCount() const { return ContainerCount; }

	UFUNCTION(BlueprintPure, Category = "GP|Storage")
	const TArray<FGP_StorageContainer>& GetContainers() const { return Containers; }

	UFUNCTION(BlueprintPure, Category = "GP|Storage")
	float GetTotalStored() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage")
	float GetTotalCapacity() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage")
	float GetTotalRemaining() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage")
	int32 GetReadyCount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage")
	int32 GetLaunchingCount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage")
	bool IsStorageFull() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage|Definition")
	TSoftObjectPtr<UGP_ResourceDefinition> GetResourceDefinitionSoft() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage|Definition")
	UGP_ResourceDefinition* GetResolvedResourceDefinition() const;

	UGP_ResourceDefinition* ResolveResourceDefinition(bool bAllowSynchronousLoad) const;

	/** ThreatPerStoredUnit from ResourceDefinition; fallback 0.5 if unresolved. */
	UFUNCTION(BlueprintPure, Category = "GP|Storage|Threat")
	float GetThreatPerStoredUnit() const;

	/**
	 * Authority-only. Fills first available non-full/non-launching containers in index order.
	 * Returns exact accepted amount details. Overflow stays with caller.
	 */
	UFUNCTION(BlueprintCallable, Category = "GP|Storage")
	FGP_StorageAddResult AddPlanetaryFerronite(float RequestedAmount);

	/**
	 * Authority-only rollback helper. Removes up to RequestedAmount from filled containers
	 * (highest index first). Returns exact removed amount.
	 */
	UFUNCTION(BlueprintCallable, Category = "GP|Storage")
	float RemovePlanetaryFerronite(float RequestedAmount);

	bool ValidateStorageContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if !UE_BUILD_SHIPPING
	/** Contract/diagnostics helper — force a container into Launching scaffold state. */
	void DebugForceContainerLaunching(int32 Index, bool bLaunching);
#endif

	UPROPERTY(BlueprintAssignable, Category = "GP|Storage|Events")
	FGP_OnStorageChanged OnStorageChanged;

protected:
	UFUNCTION()
	void OnRep_Containers();

	void EnsureContainerArray();
	void RefreshContainerState(FGP_StorageContainer& Container) const;
	bool IsFinitePositive(float Value) const;
	bool IsAcceptableFillTarget(const FGP_StorageContainer& Container) const;
	void BroadcastStorageChanged(float PreviousTotal);

	/**
	 * Soft Ferronite identity. Same DA as Cargo/Mining.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Storage|Definition")
	TSoftObjectPtr<UGP_ResourceDefinition> ResourceDefinition;

	/**
	 * Canonical GDD placeholder: 100 Ferronite per container.
	 * Temporary until BuildingDefinition hosts these tunables.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "GP|Storage", meta = (ClampMin = "0.01"))
	float ContainerCapacity = 100.0f;

	/**
	 * Canonical GDD baseline placeholder (e.g. 5 slots). Temporary until BuildingDefinition.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "GP|Storage", meta = (ClampMin = "1"))
	int32 ContainerCount = 5;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_Containers, Category = "GP|Storage")
	TArray<FGP_StorageContainer> Containers;

private:
	mutable TWeakObjectPtr<UGP_ResourceDefinition> CachedResourceDefinition;
};

/** Staged Storage contract test runner (GP-S28 debug console). */
UCLASS()
class GPRUNTIME_API UGP_StorageContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_MainBase> MainBaseWeak;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
