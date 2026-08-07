// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Resources/GPResourceTypes.h"
#include "GPResourceNode.generated.h"

class UBoxComponent;
class USceneComponent;
class UGP_ResourceDefinition;
class UGP_ResourceNodeVisualComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogGPResourceNode, Log, All);

/**
 * Result of deposit-side miner slot request (GP-S24R).
 * Authority-only mutation; no mining execution in this stage.
 */
UENUM(BlueprintType)
enum class EGP_MiningSlotRequestResult : uint8
{
	Granted UMETA(DisplayName = "Granted"),
	Waiting UMETA(DisplayName = "Waiting"),
	AlreadyActive UMETA(DisplayName = "Already Active"),
	AlreadyWaiting UMETA(DisplayName = "Already Waiting"),
	RejectedInvalidMiner UMETA(DisplayName = "Rejected Invalid Miner"),
	RejectedNoAuthority UMETA(DisplayName = "Rejected No Authority"),
	RejectedDepositInvalid UMETA(DisplayName = "Rejected Deposit Invalid")
};

/** Server-local miner occupancy state for deposit slot events (GP-S26). */
UENUM(BlueprintType)
enum class EGP_MinerOccupancyState : uint8
{
	None UMETA(DisplayName = "None"),
	Waiting UMETA(DisplayName = "Waiting"),
	Active UMETA(DisplayName = "Active")
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FGP_OnMinerSlotStateChanged,
	AActor* /* Miner */,
	EGP_MinerOccupancyState /* OldState */,
	EGP_MinerOccupancyState /* NewState */);

/** Blueprint presentation signal — fires once when deposit crosses into depleted (GP-S28P2). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FGP_OnResourceDepleted,
	AGP_ResourceNode*, ResourceNode,
	int32, PreviousAmount);

/**
 * Canonical Ferronite Deposit runtime actor (GP-S24R / GP-S27A1 / GP-S28P2).
 *
 * Gameplay identity: Ferronite Deposit.
 * Implementation class: AGP_ResourceNode (AActor; not BuildingBase).
 * Internal resource enum: EGP_ResourceType::Ore.
 *
 * ResourceDefinition owns identity + mining metadata.
 * ResourceNode owns deposit runtime state (amounts, occupancy/queue, depletion).
 * No Cargo/MiningComponent/Worker/ASC/Team ownership/permanent tick.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_ResourceNode : public AActor
{
	GENERATED_BODY()

public:
	AGP_ResourceNode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	EGP_ResourceType GetResourceType() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	int32 GetMaxAmount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	int32 GetCurrentAmount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	bool IsDepleted() const;

	/** True after one-shot authority depletion transition completed (replicated). */
	UFUNCTION(BlueprintPure, Category = "GP|Resource|Depletion")
	bool HasCompletedDepletionTransition() const { return bHasDepleted; }

	/** True while deferred destroy timer is armed (authority). */
	UFUNCTION(BlueprintPure, Category = "GP|Resource|Depletion")
	bool IsDestroyPending() const { return bDestroyPending; }

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Depletion")
	float GetDepletionDestroyDelaySeconds() const;

	/**
	 * Attach parent for Blueprint/SCS meshes (CollisionBox root).
	 * Does not replace UGP_ResourceNodeVisualComponent AuthoredComponents path.
	 */
	UFUNCTION(BlueprintPure, Category = "GP|Resource|Presentation")
	USceneComponent* GetPresentationRoot() const;

	/** Remaining deposit fraction in [0,1]. Depleted → 0. */
	UFUNCTION(BlueprintPure, Category = "GP|Resource|Presentation")
	float GetRemainingNormalized() const;

	/**
	 * When true (default), UGP_ResourceNodeVisualComponent builds Engine prototype primitives.
	 * When false, generated shapes are cleared; authored Blueprint/SCS meshes remain.
	 * CollisionBox / Mine hit stay authoritative either way.
	 */
	UFUNCTION(BlueprintPure, Category = "GP|Resource|Presentation")
	bool GetUseGeneratedPrototypeVisual() const { return bUseGeneratedPrototypeVisual; }

	UFUNCTION(BlueprintCallable, Category = "GP|Resource|Presentation")
	void SetUseGeneratedPrototypeVisual(bool bUse);

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Definition")
	TSoftObjectPtr<UGP_ResourceDefinition> GetResourceDefinitionSoft() const;

	/** Editor/seed assignment helper. Not a client gameplay mutation path. */
	void SetResourceDefinitionSoft(TSoftObjectPtr<UGP_ResourceDefinition> InDefinition);

	/** Resolved definition if already loaded / cached. Does not sync-load. */
	UFUNCTION(BlueprintPure, Category = "GP|Resource|Definition")
	UGP_ResourceDefinition* GetResolvedResourceDefinition() const;

	/**
	 * Resolve ResourceDefinition.
	 * Prefers already-loaded soft ptr / Asset Manager primary object.
	 * Synchronous LoadSynchronous only when bAllowSynchronousLoad=true
	 * (explicit AlwaysCook primary-asset resolve for validate/Mine paths).
	 */
	UGP_ResourceDefinition* ResolveResourceDefinition(bool bAllowSynchronousLoad) const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Tags")
	void GetResourceCapabilityTags(FGameplayTagContainer& OutTags) const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Tags")
	bool HasResourceCapabilityTag(FGameplayTag CapabilityTag) const;

	/**
	 * True when this deposit is a valid Mine command target.
	 * Does not execute mining. May sync-resolve AlwaysCook definition when allowed.
	 */
	bool CanAcceptMineCommand(bool bAllowSynchronousDefinitionLoad = true, FString* OutFailReason = nullptr) const;

	/**
	 * Authority-only. Consumes up to RequestedAmount; returns actual consumed.
	 * Crossing PreviousAmount>0 → NewAmount<=0 triggers one-shot depletion transition
	 * (occupancy clear without promote, collision off, OnResourceDepleted, deferred Destroy).
	 */
	int32 ConsumeResource(int32 RequestedAmount);

	/** Blueprint presentation: one-shot depleted signal (authority + client OnRep). */
	UPROPERTY(BlueprintAssignable, Category = "GP|Resource|Presentation")
	FGP_OnResourceDepleted OnResourceDepleted;

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	int32 GetMaxConcurrentMiners() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	int32 GetActiveMinerCount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	int32 GetWaitingMinerCount() const;

	/** 0-based waiting FIFO index, or INDEX_NONE. Authority occupancy arrays. */
	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	int32 FindWaitingMinerIndex(const AActor* Miner) const;

	/** Authority-only register/queue. Generic miner actor (Worker arrives in GP-S27). */
	UFUNCTION(BlueprintCallable, Category = "GP|Resource|Occupancy")
	EGP_MiningSlotRequestResult RequestMiningSlot(AActor* Miner);

	/** Authority-only release; promotes waiting FIFO head when an active slot frees. */
	UFUNCTION(BlueprintCallable, Category = "GP|Resource|Occupancy")
	void ReleaseMiningSlot(AActor* Miner);

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	bool HasActiveMiningSlot(AActor* Miner) const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	bool IsWaitingForMiningSlot(AActor* Miner) const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	EGP_MinerOccupancyState GetMinerOccupancyState(AActor* Miner) const;

	/** True while EndPlay occupancy teardown is in progress (reject new slots / no promote). */
	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	bool IsClearingOccupancy() const { return bIsClearingOccupancy; }

	/**
	 * Server-local occupancy change notifications (Granted/Waiting/release/FIFO promotion/cleanup).
	 * Not replicated. MiningComponent binds while targeting this node.
	 */
	FGP_OnMinerSlotStateChanged& GetOnMinerSlotStateChanged() { return OnMinerSlotStateChanged; }

	bool ValidateDepositContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UGP_ResourceNodeVisualComponent* GetResourceNodeVisualComponent() const;
	UBoxComponent* GetCollisionBox() const;

#if !UE_BUILD_SHIPPING
	/** Contract helper: force CurrentAmount and optionally trip depletion transition. */
	void DebugSetCurrentAmountForTest(int32 NewAmount, bool bAllowDepletionTransition);
#endif

protected:
	UFUNCTION()
	void OnRep_CurrentAmount();

	UFUNCTION()
	void OnRep_bHasDepleted();

	void ClampCurrentAmountToMax();
	void NormalizeAmountsOnConstruction();
	void ApplyIdentityFromDefinition(const UGP_ResourceDefinition* Definition);
	void CleanupInvalidMiners();
	void PromoteWaitingMiners(TArray<AActor*>& OutPromotedMiners);
	void RefreshOccupancyCounts();
	void BroadcastMinerSlotStateChanged(AActor* Miner, EGP_MinerOccupancyState OldState, EGP_MinerOccupancyState NewState);
	bool IsValidMinerActor(const AActor* Miner) const;
	bool IsDepositStateValidForMining() const;

	void HandleDepletionTransition(int32 PreviousAmount);
	void ClearOccupancyWithoutPromotion();
	void DisableGameplayInteraction();
	void ScheduleDeferredDestroy();
	void ExecuteDeferredDestroy();
	void BroadcastDepletionPresentation(int32 PreviousAmount);
	void RegisterWithGameState();
	void UnregisterFromGameState();

	/** Gameplay collision root — blocks movement/nav. Visual parts are separate NoCollision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Resource", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Resource|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_ResourceNodeVisualComponent> ResourceNodeVisualComponent;

	/**
	 * GP-S28P1: toggle C++ prototype ore shapes vs authored BP meshes only.
	 * Default true preserves diagnostic/plain C++ nodes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Presentation",
		meta = (AllowPrivateAccess = "true", DisplayName = "Use Generated Prototype Visual"))
	bool bUseGeneratedPrototypeVisual = true;

	/**
	 * Soft reference to immutable resource identity / mining metadata.
	 * Default: DA_GP_Resource_Ferronite. No silent gameplay TryLoad in tick.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Resource|Definition")
	TSoftObjectPtr<UGP_ResourceDefinition> ResourceDefinition;

	/** Runtime / replicated enum mirror. Source of truth for type identity is ResourceDefinition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "GP|Resource")
	EGP_ResourceType ResourceType = EGP_ResourceType::Ore;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "GP|Resource", meta = (ClampMin = "1"))
	int32 MaxAmount = 5000;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentAmount, Category = "GP|Resource", meta = (ClampMin = "0"))
	int32 CurrentAmount = 5000;

	/**
	 * One-shot depleted transition flag (GP-S28P2).
	 * Not derived from CurrentAmount alone — prevents OnRep double-fire.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_bHasDepleted, Category = "GP|Resource|Depletion")
	bool bHasDepleted = false;

	/**
	 * Soft-cap for concurrent active miners (TDD MaxConcurrentWorkers = 4).
	 * Excess miners enter waiting FIFO queue. Not Worker-specific.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Occupancy", meta = (ClampMin = "1"))
	int32 MaxConcurrentMiners = 4;

	/** Replicated counts only — actor queues stay server-local. */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "GP|Resource|Occupancy")
	int32 ActiveMinerCount = 0;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "GP|Resource|Occupancy")
	int32 WaitingMinerCount = 0;

private:
	/** Transient resolved cache; never authoritative storage. */
	mutable TWeakObjectPtr<UGP_ResourceDefinition> CachedResourceDefinition;

	/** Authority-only active miners. Not replicated. */
	TArray<TWeakObjectPtr<AActor>> ActiveMiners;

	/** Authority-only waiting FIFO. Not replicated. */
	TArray<TWeakObjectPtr<AActor>> WaitingMiners;

	/** Server-local only. */
	FGP_OnMinerSlotStateChanged OnMinerSlotStateChanged;

	/**
	 * Set during EndPlay occupancy teardown.
	 * Production APIs become no-op / reject; snapshot broadcasts are the only notifications.
	 */
	bool bIsClearingOccupancy = false;

	bool bDestroyPending = false;
	bool bDepletionPresentationBroadcast = false;
	int32 DepletionPreviousAmountCached = 0;
	FTimerHandle DepletionDestroyTimerHandle;
	bool bRegisteredWithGameState = false;
};
