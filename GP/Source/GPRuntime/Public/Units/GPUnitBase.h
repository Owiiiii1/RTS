// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameplayTagContainer.h"
#include "GPUnitBase.generated.h"

/**
 * Abstract replicated unit ancestor.
 * Provides TeamId + interim CapabilityTags for selection eligibility facts.
 * Full gameplay UnitBase (ASC, Definition, highlight, death) remains GP-S18.
 */
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_UnitBase : public APawn
{
	GENERATED_BODY()

public:
	AGP_UnitBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	/** -1 unassigned, 0 neutral, 1+ playable teams. */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_TeamId, Category = "GP|Team")
	int32 TeamId = -1;

	/**
	 * Interim selection capability tags (CDO / class defaults only).
	 * Future UnitDefinition becomes the canonical source; query API stays stable.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "GP|Selection")
	FGameplayTagContainer CapabilityTags;
};
