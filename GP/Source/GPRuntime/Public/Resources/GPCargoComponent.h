// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Resources/GPResourceTypes.h"
#include "GPCargoComponent.generated.h"

class UGP_ResourceDefinition;

DECLARE_LOG_CATEGORY_EXTERN(LogGPCargo, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FGP_OnCargoAmountChanged,
	float, PreviousAmount,
	float, NewAmount,
	float, Capacity,
	float, Delta);

/**
 * Replicated temporary Planetary Ferronite cargo (GP-S25).
 * Sole writable runtime source of truth for carried raw Ferronite.
 * Owner-agnostic; Worker (GP-S27) will own this component. No tick/timers/mining.
 */
UCLASS(BlueprintType, ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_CargoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_CargoComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "GP|Cargo")
	float GetCargoCapacity() const;

	/** Authority-safe capacity apply from UnitDefinition. 0 is valid (no cargo). */
	void ApplyCapacityFromDefinition(float NewCapacity);

	UFUNCTION(BlueprintPure, Category = "GP|Cargo")
	float GetCurrentCargoAmount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Cargo")
	float GetRemainingCapacity() const;

	UFUNCTION(BlueprintPure, Category = "GP|Cargo")
	float GetFillRatio() const;

	UFUNCTION(BlueprintPure, Category = "GP|Cargo")
	bool IsEmpty() const;

	UFUNCTION(BlueprintPure, Category = "GP|Cargo")
	bool IsFull() const;

	UFUNCTION(BlueprintPure, Category = "GP|Cargo|Definition")
	TSoftObjectPtr<UGP_ResourceDefinition> GetResourceDefinitionSoft() const;

	/** Resolved if already loaded / cached. Does not sync-load. */
	UFUNCTION(BlueprintPure, Category = "GP|Cargo|Definition")
	UGP_ResourceDefinition* GetResolvedResourceDefinition() const;

	UGP_ResourceDefinition* ResolveResourceDefinition(bool bAllowSynchronousLoad) const;

	UFUNCTION(BlueprintPure, Category = "GP|Cargo|Definition")
	EGP_ResourceType GetCarriedResourceType() const;

	/**
	 * True if Amount is finite > 0 and remaining capacity > 0
	 * (partial accept may still occur via AddCargo).
	 */
	UFUNCTION(BlueprintPure, Category = "GP|Cargo")
	bool CanAcceptCargo(float Amount) const;

	/** Authority-only. Returns exact accepted amount (overflow clamped). */
	UFUNCTION(BlueprintCallable, Category = "GP|Cargo")
	float AddCargo(float RequestedAmount);

	/** Authority-only. Returns exact removed amount (clamped to current). */
	UFUNCTION(BlueprintCallable, Category = "GP|Cargo")
	float RemoveCargo(float RequestedAmount);

	/** Authority-only. Clears cargo; returns amount removed. */
	UFUNCTION(BlueprintCallable, Category = "GP|Cargo")
	float ClearCargo();

	bool ValidateCargoContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(BlueprintAssignable, Category = "GP|Cargo|Events")
	FGP_OnCargoAmountChanged OnCargoAmountChanged;

protected:
	UFUNCTION()
	void OnRep_CurrentCargoAmount(float PreviousAmount);

	void ClampCargoState();
	void ApplyCargoAmount(float NewAmount);
	void BroadcastCargoChanged(float PreviousAmount, float NewAmount);
	bool IsFinitePositive(float Value) const;
	bool IsFiniteNonNegative(float Value) const;

	/**
	 * Soft identity for carried resource. Default: Ferronite DA.
	 * Capacity is cargo/unit tuning — not taken from ResourceDefinition.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Cargo|Definition")
	TSoftObjectPtr<UGP_ResourceDefinition> ResourceDefinition;

	/**
	 * Compatibility fallback when UnitDefinition is empty. Canonical: UGP_UnitDefinition.CargoCapacity.
	 * 0 = no cargo capacity.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "GP|Cargo", meta = (ClampMin = "0.0"))
	float CargoCapacity = 50.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentCargoAmount, Category = "GP|Cargo")
	float CurrentCargoAmount = 0.0f;

private:
	mutable TWeakObjectPtr<UGP_ResourceDefinition> CachedResourceDefinition;
};

/**
 * Transient replicated host for cargo diagnostics / PIE listen-server checks.
 * Not attached to combat units. Spawn via gp.Cargo.SpawnDiagnosticHost — do not save to maps.
 */
UCLASS(NotPlaceable, Transient)
class GPRUNTIME_API AGP_CargoDiagnosticHost : public AActor
{
	GENERATED_BODY()

public:
	AGP_CargoDiagnosticHost();

	UFUNCTION(BlueprintPure, Category = "GP|Cargo")
	UGP_CargoComponent* GetCargoComponent() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Cargo")
	TObjectPtr<UGP_CargoComponent> CargoComponent;
};
