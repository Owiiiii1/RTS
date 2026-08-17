// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GPOrbitalDropDefinition.generated.h"

class UGP_BuildingDefinition;

/**
 * Acquisition / delivery catalog entry (GP-S35B).
 * Cost SoT for orbital Purchase. Intrinsic building facts live on UGP_BuildingDefinition.
 */
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_OrbitalDropDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGP_OrbitalDropDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	FGameplayTagContainer DropTags;

	/** OrbitalFerronite purchase cost. Deploy does not spend again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Acquisition", meta = (ClampMin = "0.0"))
	float Cost = 0.0f;

	/** Soft associated building identity. Class / footprint come from this asset, not from Cost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Payload")
	TSoftObjectPtr<UGP_BuildingDefinition> BuildingDefinition;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	static const TCHAR* PrimaryAssetTypeName();

	/** Already-loaded BuildingDefinition only. Does not LoadObject / LoadSynchronous. */
	UGP_BuildingDefinition* ResolveLoadedBuildingDefinition() const;

	/** Display SoT is BuildingDefinition.DisplayName when resolvable. */
	FText GetAcquisitionDisplayName() const;
};
