// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/GPDeathSink.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "GPUnitBase.generated.h"

class UGP_AbilitySystemComponent;
class UGP_CombatPresentationComponent;
class UGP_HealthBarComponent;
class UGP_TeamPresentationComponent;
class UGP_UnitAttributeSet;
class UGP_UnitCommandComponent;
class UGP_UnitDefinition;
struct FGP_DamageApplicationResult;
struct FGP_UnitCommand;
struct FGameplayEffectModCallbackData;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE_OneParam(FGP_OnUnitDied, AGP_UnitBase*);

/**
 * Abstract replicated unit ancestor.
 * Provides TeamId + interim CapabilityTags for selection eligibility facts.
 * Owns UGP_UnitCommandComponent for server-authoritative Held Command state (GP-S18).
 * Owns unit ASC + UnitAttributeSet + death contract (GP-S25A).
 * Owns UGP_CombatPresentationComponent for cosmetic combat events (GP-S26A).
 */
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_UnitBase : public APawn, public IAbilitySystemInterface, public IGP_DeathSink
{
	GENERATED_BODY()

public:
	AGP_UnitBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;

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

	UGP_CombatPresentationComponent* GetCombatPresentationComponent() const;

	UGP_TeamPresentationComponent* GetTeamPresentationComponent() const;

	UGP_HealthBarComponent* GetHealthBarComponent() const;

	/** Local presentation-only FoW gate. Does not change replication, collision, or gameplay state. */
	void SetLocalFoWPresentationVisible(bool bVisible);
	bool IsLocalFoWPresentationVisible() const { return bLocalFoWPresentationVisible; }

	UFUNCTION(BlueprintPure, Category = "GP|Team")
	int32 GetTeamId() const;

	UFUNCTION(BlueprintPure, Category = "GP|Presentation|Team")
	FLinearColor GetTeamPresentationColor() const;

	/** Authority-only. Silent no-op without authority. */
	void SetTeamId(int32 NewTeamId);

	UFUNCTION(BlueprintPure, Category = "GP|Team")
	bool IsNeutral() const;

	/**
	 * Authority hook after TeamId mutates (GP-S28).
	 * Used by MainBase registry refresh; default no-op.
	 */
	virtual void NotifyTeamIdChanged(int32 OldTeamId, int32 NewTeamId);

	/** Worker / Salvage Walker only. Buildings and other UnitBase children do not count. */
	virtual bool CountsTowardPlayerUnitCap() const { return false; }

	bool HasBeenCountedTowardPlayerUnitCap() const { return bCountedTowardPlayerUnitCap; }

	/** Authority one-shot register against owning PlayerState. */
	void TryRegisterPlayerUnitCap();

	void UnregisterPlayerUnitCap();

	void MarkCountedTowardPlayerUnitCap(class AGP_PlayerState* OwnerPlayerState);
	void ClearCountedTowardPlayerUnitCap();

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

	/**
	 * Designer-facing intrinsic stats. Soft only — no LoadSynchronous.
	 * Valid non-empty ref async-loads then initializes. Empty = immediate Default* fallback.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Definition")
	TSoftObjectPtr<UGP_UnitDefinition> UnitDefinitionAsset;

	/** Already-resident definition only. Does not LoadSynchronous or start a load. */
	UFUNCTION(BlueprintPure, Category = "GP|Definition")
	const UGP_UnitDefinition* ResolveLoadedUnitDefinition() const;

	/** True after empty-ref fallback, load failure fallback, or successful definition apply. */
	UFUNCTION(BlueprintPure, Category = "GP|Definition")
	bool IsUnitDefinitionReady() const { return bUnitDefinitionReady; }

	UFUNCTION(BlueprintPure, Category = "GP|Definition")
	bool IsUnitDefinitionLoadPending() const { return bUnitDefinitionLoadPending; }

	/** Canonical resolved definition value, or actor compatibility fallback after empty/load-failed definition. */
	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar")
	float GetFogOfWarSightRadiusCm() const;

	/** False while definition ownership is unresolved, after death, or when the canonical definition opts out. */
	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar")
	bool GrantsFogOfWarVision() const;

	/**
	 * Canonical duration for GP-S40R retaliation pursuit. 0 disables.
	 * After ready: definition value, or 5.0 fallback when empty-ref / load failure.
	 * Pending/unready: documented baseline 5.0 (not 0).
	 */
	UFUNCTION(BlueprintPure, Category = "GP|Behavior|Retaliation")
	float GetRetaliationPursuitSeconds() const;

	static constexpr float FallbackRetaliationPursuitSeconds = 5.0f;

#if !UE_BUILD_SHIPPING
	/** Apply the same ResolvedRetaliationPursuitSeconds formula as definition completion. */
	void DebugApplyRetaliationPursuitSecondsFromDefinition(const UGP_UnitDefinition* Definition);
	/** Force the unresolved-soft RequestAsyncLoad path. Injected def is applied on completion. */
	void DebugForceUnresolvedSoftDefinitionLoad(UGP_UnitDefinition* InjectedDefinition, bool bHoldCompletion);
	bool DebugDidRequestAsyncUnitDefinitionLoad() const { return bDebugDidRequestAsyncUnitDefinitionLoad; }
	void DebugCompletePendingUnitDefinitionLoad();
	int32 DebugGetCombatAttributesInitializationCount() const { return DebugCombatAttributesInitializationCount; }
#endif

protected:
	UFUNCTION()
	void OnRep_TeamId();

	UFUNCTION()
	void OnRep_IsDead();

	void InitializeAbilitySystemActorInfo();
	/** Derived actors may wait for an upstream definition owner before selecting UnitDefinitionAsset. */
	virtual bool ShouldDeferUnitDefinitionInitialization() const;
	void BeginUnitDefinitionInitialization();
	void RequestAsyncUnitDefinitionLoad();
	void HandleUnitDefinitionLoaded();
	void FinishUnitDefinitionLoadResolve();
	void CompleteUnitDefinitionInitialization(const UGP_UnitDefinition* DefinitionOrNull);
	void CancelPendingUnitDefinitionLoad();
	void InitializeCombatAttributesIfNeeded();
	void ApplyUnitDefinitionComponentTuningIfNeeded();
	void RefreshFogOfWarSightSourceRegistration();
	void HandleDeathInternal();
	virtual void NotifyAuthorityDeath();
	void ApplyClientDeadPresentation();
	void AttachHealthBarToOwnerRoot();
	class AGP_PlayerState* ResolveOwningPlayerStateForUnitCap() const;

	/** -1 unassigned, 0 neutral, 1+ playable teams. */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_TeamId, Category = "GP|Team")
	int32 TeamId = -1;

	/**
	 * Interim selection capability tags (CDO / class defaults only).
	 * Future UnitDefinition becomes the canonical source; query API stays stable.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Selection")
	FGameplayTagContainer CapabilityTags;

	/** Compatibility fallback when UnitDefinitionAsset is empty. Prefer UGP_UnitDefinition. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults|Fallback")
	float DefaultMaxHealth = 100.0f;

	/** Compatibility fallback when UnitDefinitionAsset is empty. Prefer UGP_UnitDefinition. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults|Fallback")
	float DefaultHealth = 100.0f;

	/** Compatibility fallback when UnitDefinitionAsset is empty. Prefer UGP_UnitDefinition. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults|Fallback")
	float DefaultDamage = 25.0f;

	/** Compatibility fallback when UnitDefinitionAsset is empty. Prefer UGP_UnitDefinition. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults|Fallback")
	float DefaultArmor = 0.0f;

	/** Compatibility fallback when UnitDefinitionAsset is empty. Prefer UGP_UnitDefinition. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults|Fallback")
	float DefaultDamageResistance = 0.0f;

	/** Compatibility fallback when UnitDefinitionAsset is empty. Prefer UGP_UnitDefinition. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults|Fallback")
	float DefaultAttackCooldown = 1.0f;

	/** Compatibility fallback when UnitDefinitionAsset is empty. Prefer UGP_UnitDefinition. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Combat|Defaults|Fallback")
	float DefaultAttackRange = 250.0f;

	/** Compatibility fallback used only when no UnitDefinition can be resolved. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|FogOfWar|Defaults|Fallback", meta = (ClampMin = "0.0"))
	float FallbackFogOfWarSightRadiusCm = 900.0f;

	/** Compatibility fallback used only when no UnitDefinition can be resolved. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|FogOfWar|Defaults|Fallback")
	bool bFallbackGrantsFogOfWarVision = true;

	/** Seconds until Destroy after death. 0 = remain indefinitely. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Death")
	float DeadActorLifeSpan = 2.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead, VisibleInstanceOnly, Category = "GP|Death")
	bool bIsDead = false;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Command", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_UnitCommandComponent> UnitCommandComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Combat|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_CombatPresentationComponent> CombatPresentationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation|Team", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_TeamPresentationComponent> TeamPresentationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation|Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_HealthBarComponent> HealthBarComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UGP_UnitAttributeSet> UnitAttributeSet;

	FGP_OnUnitDied UnitDiedDelegate;

	TSharedPtr<FStreamableHandle> UnitDefinitionLoadHandle;

	bool bCombatAttributesInitialized = false;
	bool bDefinitionTuningApplied = false;
	bool bUnitDefinitionReady = false;
	bool bUnitDefinitionLoadPending = false;
	bool bUnitDefinitionLoadAbandoned = false;
	float ResolvedRetaliationPursuitSeconds = FallbackRetaliationPursuitSeconds;
	bool bDeathHandled = false;
	bool bCountedTowardPlayerUnitCap = false;
	bool bLocalFoWPresentationVisible = true;
	TWeakObjectPtr<class AGP_PlayerState> UnitCapOwnerWeak;

	UPROPERTY(Transient)
	TObjectPtr<UGP_UnitDefinition> DebugInjectedUnitDefinition;

#if !UE_BUILD_SHIPPING
	bool bDebugForceUnresolvedSoftPath = false;
	bool bDebugHoldAsyncCompletion = false;
	bool bDebugDidRequestAsyncUnitDefinitionLoad = false;
	int32 DebugCombatAttributesInitializationCount = 0;
#endif
};
