// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Resources/GPResourceTypes.h"
#include "GPResourceDefinition.generated.h"

class UTexture2D;

/**
 * Immutable resource type definition (GP-S23R / canonical GP-S23).
 * Tunables for later Mining (S26); no runtime mutable state, no tick, no economy execution.
 *
 * Ore is the current internal EGP_ResourceType name; Ferronite is the canonical gameplay identity.
 */
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_ResourceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGP_ResourceDefinition();

	/** Internal enum identity. Ferronite prototype uses Ore until a dedicated enum rename stage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Identity")
	EGP_ResourceType ResourceType = EGP_ResourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Identity", meta = (MultiLine = "true"))
	FText Description;

	/** Canonical tag, e.g. GP.Resource.Type.Ferronite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Identity")
	FGameplayTag ResourceGameplayTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Identity")
	TSoftObjectPtr<UTexture2D> Icon;

	/**
	 * Amount extracted into cargo per completed mining cycle.
	 * Canonical mining SoT with MiningCycleDurationSeconds (future MiningComponent reads these).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Mining", meta = (ClampMin = "0.01", ForceUnits = "u"))
	float AmountPerMiningCycle = 10.0f;

	/** Seconds per mining cycle. Must be > 0. Canonical mining SoT with AmountPerMiningCycle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Mining", meta = (ClampMin = "0.01", ForceUnits = "s"))
	float MiningCycleDurationSeconds = 1.0f;

	/** Interaction / mining range in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Mining", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float InteractionRangeCm = 200.0f;

	/** 1 shipped unit → FerroniteScore units at orbital launch (metadata only; no execution in S23R). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Orbital", meta = (ClampMin = "0.0"))
	float ScoreConversionRate = 1.0f;

	/**
	 * Multiplier from stored Planetary stock to SWARM FerroniteThreatValue pressure (metadata only).
	 * Prototype placeholder (TDD recommended starting 0.5); not final balance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Orbital", meta = (ClampMin = "0.0"))
	float ThreatPerStoredUnit = 0.5f;

	/** Optional presentation tint metadata (no material creation in S23R). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Resource|Presentation")
	FLinearColor Tint = FLinearColor(0.15f, 0.75f, 0.85f, 1.0f);

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/**
	 * Derived units/sec = AmountPerMiningCycle / MiningCycleDurationSeconds.
	 * UI/diagnostics only — MiningComponent must not treat this as a stored balance field.
	 */
	UFUNCTION(BlueprintPure, Category = "GP|Resource|Mining")
	float GetEffectiveMineRatePerWorker() const;

	bool ValidateDefinition(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	static const TCHAR* PrimaryAssetTypeName();
	static const TCHAR* DefaultFerroniteAssetPath();
};
