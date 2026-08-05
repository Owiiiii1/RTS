// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Resources/GPResourceTypes.h"
#include "GPResourceNode.generated.h"

class UBoxComponent;
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

/**
 * Canonical Ferronite Deposit runtime actor (GP-S24R / GP-S27A1).
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

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	EGP_ResourceType GetResourceType() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	int32 GetMaxAmount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	int32 GetCurrentAmount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	bool IsDepleted() const;

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
	 * Does not destroy the actor or change visuals on depletion.
	 */
	int32 ConsumeResource(int32 RequestedAmount);

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	int32 GetMaxConcurrentMiners() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	int32 GetActiveMinerCount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource|Occupancy")
	int32 GetWaitingMinerCount() const;

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

	bool ValidateDepositContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UGP_ResourceNodeVisualComponent* GetResourceNodeVisualComponent() const;
	UBoxComponent* GetCollisionBox() const;

protected:
	UFUNCTION()
	void OnRep_CurrentAmount();

	void ClampCurrentAmountToMax();
	void NormalizeAmountsOnConstruction();
	void ApplyIdentityFromDefinition(const UGP_ResourceDefinition* Definition);
	void CleanupInvalidMiners();
	void PromoteWaitingMiners();
	void RefreshOccupancyCounts();
	bool IsValidMinerActor(const AActor* Miner) const;
	bool IsDepositStateValidForMining() const;

	/** Gameplay collision root — blocks movement/nav. Visual parts are separate NoCollision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Resource", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Resource|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_ResourceNodeVisualComponent> ResourceNodeVisualComponent;

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
};
