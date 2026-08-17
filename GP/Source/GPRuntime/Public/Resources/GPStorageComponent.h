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
	/** Active launch telegraph (GP-S30). Rejects fill and second launch. */
	Launching UMETA(DisplayName = "Launching")
};

UENUM(BlueprintType)
enum class EGP_ContainerLaunchRejectReason : uint8
{
	None UMETA(DisplayName = "None"),
	NoAuthority UMETA(DisplayName = "No Authority"),
	NoReadyContainer UMETA(DisplayName = "No Ready Container"),
	InvalidAmount UMETA(DisplayName = "Invalid Amount"),
	InvalidOwner UMETA(DisplayName = "Invalid Owner"),
	MissingPlayerState UMETA(DisplayName = "Missing PlayerState"),
	MissingASC UMETA(DisplayName = "Missing ASC"),
	InvalidConfig UMETA(DisplayName = "Invalid Config"),
	MissingGameState UMETA(DisplayName = "Missing GameState"),
	AlreadyLaunching UMETA(DisplayName = "Already Launching"),
	LaunchInFlight UMETA(DisplayName = "Launch In Flight"),
	MatchFinished UMETA(DisplayName = "Match Finished")
};

USTRUCT(BlueprintType)
struct FGP_ContainerLaunchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage|Launch")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage|Launch")
	EGP_ContainerLaunchRejectReason RejectReason = EGP_ContainerLaunchRejectReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage|Launch")
	int32 ContainerIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage|Launch")
	float LaunchedPlanetaryAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Storage|Launch")
	float LaunchDurationSeconds = 0.0f;
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
 * MainBase Planetary Ferronite containers (GP-S28 / GP-S30 launch).
 * Sole writable SoT for raw Planetary Ferronite at base.
 * No permanent Tick. Orbital/Score granted only via Instant GAS GEs on launch completion.
 */
UCLASS(BlueprintType, ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_StorageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_StorageComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	UFUNCTION(BlueprintPure, Category = "GP|Storage|Orbital")
	float GetOrbitalConversionRate() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage|Orbital")
	float GetScoreConversionRate() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage|Launch")
	bool IsLaunchInFlight() const;

	UFUNCTION(BlueprintPure, Category = "GP|Storage|Launch")
	int32 GetActiveLaunchContainerIndex() const { return ActiveLaunchContainerIndex; }

	/**
	 * Authority-only. Selects the first Ready container (lowest index) and starts launch telegraph.
	 * Rewards apply on completion via Instant GAS GEs — not at accept time.
	 */
	UFUNCTION(BlueprintCallable, Category = "GP|Storage|Launch")
	FGP_ContainerLaunchResult TryLaunchReadyContainer();

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

	/** Contract-only: resize Containers without EnsureContainerArray (partial-array validation). */
	void DebugSetContainersNumForValidationTest(int32 NewNum);
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

	int32 FindFirstReadyContainerIndex() const;
	float ResolveLaunchDurationSeconds() const;
	class AGP_PlayerState* ResolveOwningPlayerState() const;
	bool ValidateLaunchPreconditions(
		int32& OutReadyIndex,
		float& OutAmount,
		class AGP_PlayerState*& OutPlayerState,
		class UGP_AbilitySystemComponent*& OutASC,
		class AGP_GameState*& OutGameState,
		EGP_ContainerLaunchRejectReason& OutReason) const;
	void HandleLaunchTelegraphComplete();
	void ClearLaunchRuntimeState();
	bool ApplyLaunchRewards(
		class UGP_AbilitySystemComponent* ASC,
		float PlanetaryAmount,
		float& OutOrbitalGranted,
		float& OutScoreGranted) const;

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

	FTimerHandle LaunchTelegraphTimerHandle;
	int32 ActiveLaunchContainerIndex = INDEX_NONE;
	float ActiveLaunchPlanetaryAmount = 0.0f;
	uint32 ActiveLaunchSerial = 0;
	uint32 NextLaunchSerial = 1;
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

/** GP-S30 container launch / orbital conversion contract. */
UCLASS()
class GPRUNTIME_API UGP_ContainerLaunchContractTestRunner : public UObject
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
	TWeakObjectPtr<class AGP_MainBase> MainBaseWeak;
	TWeakObjectPtr<class AGP_PlayerState> OwnerPSWeak;
	TWeakObjectPtr<class AGP_PlayerState> OtherPSWeak;
	float ThreatBefore = 0.0f;
	float OrbitalBefore = 0.0f;
	float ScoreBefore = 0.0f;
	float StoredBefore = 0.0f;
	float LaunchAmount = 0.0f;
	float ExpectedOrbital = 0.0f;
	float ExpectedScore = 0.0f;
	float ExpectedThreatDelta = 0.0f;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};

/** GP-S30 TEMP HUD Orbital display + Launch button / PC request contract. */
UCLASS()
class GPRUNTIME_API UGP_ContainerLaunchHUDContractTestRunner : public UObject
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
	void HandleOrbitalAttrChanged(const struct FOnAttributeChangeData& Data);

	int32 StageIndex = 0;
	int32 Failures = 0;
	bool bFinished = false;
	FDelegateHandle WorldCleanupHandle;
	FTimerHandle StageTimerHandle;
	FDelegateHandle OrbitalAttrHandle;
	TWeakObjectPtr<UWorld> WorldWeak;
	TWeakObjectPtr<class AGP_MainBase> OwnBaseWeak;
	TWeakObjectPtr<class AGP_MainBase> OtherBaseWeak;
	TWeakObjectPtr<class AGP_PlayerState> OwnPSWeak;
	TWeakObjectPtr<class AGP_PlayerState> OtherPSWeak;
	TWeakObjectPtr<class AGP_PlayerController> OwnPCWeak;
	TWeakObjectPtr<class AGP_PlayerController> OtherPCWeak;
	TWeakObjectPtr<class UGP_TEMP_S28P_PlanetaryFerroniteHUD> HUDWeak;
	TWeakObjectPtr<class UGP_AbilitySystemComponent> BoundASCWeak;
	float LastOrbitalAttrValue = -1.0f;
	int32 OrbitalAttrEventCount = 0;
	uint64 ExecutionId = 0;
	FName OwnerTag;
	bool bCancelled = false;
	FName CancelReason;
};
