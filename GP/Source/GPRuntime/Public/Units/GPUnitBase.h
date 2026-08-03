// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/GPDeathSink.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "GPUnitBase.generated.h"

class UGP_AbilitySystemComponent;
class UGP_UnitAttributeSet;
class UGP_UnitCommandComponent;
struct FGP_DamageApplicationResult;
struct FGP_UnitCommand;
struct FGameplayEffectModCallbackData;

DECLARE_MULTICAST_DELEGATE_OneParam(FGP_OnUnitDied, AGP_UnitBase*);

/**
 * Abstract replicated unit ancestor.
 * Provides TeamId + interim CapabilityTags for selection eligibility facts.
 * Owns UGP_UnitCommandComponent for server-authoritative Held Command state (GP-S18).
 * Owns unit ASC + UnitAttributeSet + death contract (GP-S25A).
 */
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_UnitBase : public APawn, public IAbilitySystemInterface, public IGP_DeathSink
{
	GENERATED_BODY()

public:
	AGP_UnitBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "GP|AbilitySystem")
	UGP_AbilitySystemComponent* GetGPAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Attributes")
	const UGP_UnitAttributeSet* GetUnitAttributeSet() const;

	/**
	 * Server delivery boundary for a validated unit command (GP-S17 Phase E).
	 * Authority guard + diagnostic Received log, then forward to UnitCommandComponent.
	 * Dead units reject before command-component dispatch (GP-S25A).
	 */
	virtual void ReceiveCommand(const FGP_UnitCommand& Command);

	/** Authority-only. Applies Instant UGP_GE_Damage_Basic via source→target ASC. */
	bool ApplyDamageFromUnit(AGP_UnitBase* SourceUnit, FGP_DamageApplicationResult& OutResult);

	virtual void HandleGASDeath(const FGameplayEffectModCallbackData& Data) override;

	UFUNCTION(BlueprintPure, Category = "GP|Combat")
	bool IsDead() const;

	FGP_OnUnitDied& OnUnitDied();

	UGP_UnitCommandComponent* GetUnitCommandComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Team")
	int32 GetTeamId() const;

	/** Authority-only. Silent no-op without authority. */
	void SetTeamId(int32 NewTeamId);

	UFUNCTION(BlueprintPure, Category = "GP|Team")
	bool IsNeutral() const;

	UFUNCTION(BlueprintPure, Category = "GP|Team")
	bool HasAssignedTeam() const;

	UFUNCTION(BlueprintPure, Category = "GP|Selection")
	const FGameplayTagContainer& GetCapabilityTags() const;

	UFUNCTION(BlueprintPure, Category = "GP|Selection")
	bool HasCapabilityTag(FGameplayTag CapabilityTag) const;

	/**
	 * True if CapabilityTags contains GP.Capability.Selectable.
	 * Named to avoid overriding AActor::IsSelectable (editor selectability).
	 * Dead units are not gameplay-selectable (GP-S25A).
	 */
	UFUNCTION(BlueprintPure, Category = "GP|Selection")
	bool IsGameplaySelectable() const;

	UFUNCTION(BlueprintPure, Category = "GP|Selection")
	bool IsGameplayInspectable() const;

	UFUNCTION(BlueprintPure, Category = "GP|Selection")
	bool IsSelectionTypeUnit() const;

	UFUNCTION(BlueprintPure, Category = "GP|Selection")
	bool IsSelectionTypeBuilding() const;

protected:
	UFUNCTION()
	void OnRep_TeamId();

	UFUNCTION()
	void OnRep_IsDead();

	void InitializeAbilitySystemActorInfo();
	void InitializeCombatAttributesIfNeeded();
	void HandleDeathInternal();
	void ApplyClientDeadPresentation();

	/** -1 unassigned, 0 neutral, 1+ playable teams. */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_TeamId, Category = "GP|Team")
	int32 TeamId = -1;

	/**
	 * Interim selection capability tags (CDO / class defaults only).
	 * Future UnitDefinition becomes the canonical source; query API stays stable.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Selection")
	FGameplayTagContainer CapabilityTags;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults")
	float DefaultMaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults")
	float DefaultHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults")
	float DefaultDamage = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults")
	float DefaultArmor = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults")
	float DefaultDamageResistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults")
	float DefaultAttackCooldown = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults")
	float DefaultAttackRange = 250.0f;

	/** Seconds until Destroy after death. 0 = remain indefinitely. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Death")
	float DeadActorLifeSpan = 2.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead, VisibleInstanceOnly, Category = "GP|Death")
	bool bIsDead = false;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Command", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_UnitCommandComponent> UnitCommandComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UGP_UnitAttributeSet> UnitAttributeSet;

	FGP_OnUnitDied UnitDiedDelegate;

	bool bCombatAttributesInitialized = false;
	bool bDeathHandled = false;
};
