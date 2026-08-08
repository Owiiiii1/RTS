// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/GPBuildingBase.h"
#include "GPLogisticsHub.generated.h"

class UCapsuleComponent;
class USceneComponent;

/**
 * Native Logistics Hub building identity (GP-S32R).
 * Minimal orbital-deploy payload — no MaxUnits bonus, no storage/GAS extras in this slice.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_LogisticsHub : public AGP_BuildingBase
{
	GENERATED_BODY()

public:
	AGP_LogisticsHub();

	UFUNCTION(BlueprintPure, Category = "GP|Building")
	UCapsuleComponent* GetCapsuleComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Building|Presentation")
	USceneComponent* GetPresentationRoot() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PresentationRoot;
};
