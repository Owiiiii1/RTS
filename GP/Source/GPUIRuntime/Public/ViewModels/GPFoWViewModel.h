// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "MVVMViewModelBase.h"
#include "GPFoWViewModel.generated.h"

class UGP_LocalFoWComponent;
class UGP_FoWViewModelAdapter;

/** Read-only MVVM projection of the owning player's trusted local FoW mirror. */
UCLASS(BlueprintType)
class GPUIRUNTIME_API UGP_FoWViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|FogOfWar")
	int32 LocalTeamId = -1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|FogOfWar")
	FVector2D GridOrigin = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|FogOfWar")
	FIntPoint GridDimensions = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|FogOfWar")
	float CellSizeCm = 0.0f;

	/** Coarse FieldNotify invalidation token for per-cell changes. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|FogOfWar")
	int64 Revision = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|FogOfWar")
	bool bIsReady = false;

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar")
	EGP_FoWState GetStateAtWorldLocation(const FVector& WorldLocation) const;

private:
	friend class UGP_FoWViewModelAdapter;

	void RefreshFromMirror(UGP_LocalFoWComponent* Mirror);

	TWeakObjectPtr<UGP_LocalFoWComponent> BoundMirror;
};
