// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GPBuildingDefinition.generated.h"

class AGP_BuildingBase;
class UTexture2D;

/**
 * Intrinsic building identity / gameplay facts (GP-S35B).
 * Not an acquisition catalog. Orbital purchase cost lives on UGP_OrbitalDropDefinition.
 */
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_BuildingDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGP_BuildingDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	FGameplayTagContainer BuildingTags;

	/** Soft spawned actor class. Resolved only when already loaded / AssetManager-ready. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Payload", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AGP_BuildingBase> SpawnedClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Vitals", meta = (ClampMin = "1.0"))
	float MaxHealth = 500.0f;

	/**
	 * Compatibility/default BuildGrid footprint (cells).
	 * Runtime precedence:
	 * 1) payload CDO PlacementFootprintBounds when effective authored XY half-extent >= 1 cm
	 *    (UnscaledBoxExtent * RelativeScale3D; actor/world scale ignored)
	 * 2) this field, when both axes > 0
	 * Do not delete. Tests and classes without usable bounds still use this fallback.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|BuildGrid")
	FIntPoint FootprintCells = FIntPoint(1, 1);

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	static const TCHAR* PrimaryAssetTypeName();

	/** Already-loaded spawned class only. Does not LoadObject / LoadSynchronous. */
	TSubclassOf<AGP_BuildingBase> ResolveLoadedSpawnedClass() const;
};
