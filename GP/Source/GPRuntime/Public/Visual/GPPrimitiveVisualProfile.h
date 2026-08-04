// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "GPPrimitiveVisualProfile.generated.h"

/**
 * Editable cosmetic primitive composition (GP-S26B2A).
 * No gameplay stats. Invalid profiles must never crash actors — callers use native fallback.
 */
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_PrimitiveVisualProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	FName ProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	EGP_PrimitiveVisualProfileCategory Category = EGP_PrimitiveVisualProfileCategory::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual", meta = (ClampMin = "1"))
	int32 ProfileVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	TArray<FGP_PrimitiveVisualPart> Parts;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintCallable, Category = "GP|Visual")
	bool ValidateProfile(TArray<FString>& OutErrors) const;

	/** True when sanitized definition is usable. Warnings may still exist. */
	bool GetValidatedDefinition(FGP_PrimitiveVisualDefinition& OutDefinition, TArray<FString>& OutErrors) const;

	FGP_PrimitiveVisualDefinition SanitizeDefinition() const;
};
