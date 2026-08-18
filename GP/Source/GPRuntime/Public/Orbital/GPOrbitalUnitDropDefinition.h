// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GPOrbitalUnitDropDefinition.generated.h"

class AGP_UnitBase;
class UGP_UnitDefinition;
class UTexture2D;

/**
 * Unit acquisition / delivery catalog entry (GP-S39E).
 * Purchase + per-unit delivery facts. Intrinsic unit gameplay lives on UGP_UnitDefinition.
 * Global pod altitude / spacing / cleanup stay on UGP_OrbitalDeliverySettings.
 */
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_OrbitalUnitDropDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGP_OrbitalUnitDropDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Payload")
	TSoftObjectPtr<UGP_UnitDefinition> UnitDefinition;

	/** Soft payload class. Empty → settings authored class → native fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Payload", meta = (AllowAbstract = "false"))
	TSoftClassPtr<AGP_UnitBase> PayloadClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Acquisition", meta = (ClampMin = "0.0"))
	float Cost = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Acquisition", meta = (ClampMin = "1"))
	int32 TransportSlotCost = 1;

	/** Pod falling / telegraph time. Perceived usable time ≈ Descent + PayloadDeployDelay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Delivery", meta = (ClampMin = "0.05"))
	float DeliveryDescentSeconds = 2.5f;

	/** Delay after impact before payload appears. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Delivery", meta = (ClampMin = "0.0"))
	float PayloadDeployDelaySeconds = 1.25f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	static const TCHAR* PrimaryAssetTypeName();

	const UGP_UnitDefinition* ResolveLoadedUnitDefinition() const;
	TSubclassOf<AGP_UnitBase> ResolveLoadedPayloadClass() const;
};
