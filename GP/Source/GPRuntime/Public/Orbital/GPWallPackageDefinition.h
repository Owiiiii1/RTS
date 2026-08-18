// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GPWallPackageDefinition.generated.h"

class UTexture2D;

/**
 * Wall Package acquisition (GP-S42A / GP-0305R).
 * Not a READY building drop. Does not own Wall BuildingDefinition / combat identity.
 *
 * Native bootstrap Cost = 150 OrbitalFerronite — catalog placeholder, NOT final balance.
 */
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_WallPackageDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UGP_WallPackageDefinition();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	TSoftObjectPtr<UTexture2D> Icon;

	/** OrbitalFerronite purchase cost. Arrival does not spend again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Acquisition", meta = (ClampMin = "0.0"))
	float Cost = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Acquisition", meta = (ClampMin = "1"))
	int32 SegmentCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Delivery", meta = (ClampMin = "0.0"))
	float DeliveryDescentSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Delivery", meta = (ClampMin = "0.0"))
	float PayloadDeployDelaySeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Identity")
	FGameplayTagContainer DropTags;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	static const TCHAR* PrimaryAssetTypeName();

	/** Native bootstrap Cost. Not final balance. */
	static constexpr float NativeBootstrapCost = 150.0f;

	static constexpr int32 NativeBootstrapSegmentCount = 5;

	bool IsValidForDelivery(int32 InventoryCapacity, TArray<FText>* OutErrors = nullptr) const;
};
