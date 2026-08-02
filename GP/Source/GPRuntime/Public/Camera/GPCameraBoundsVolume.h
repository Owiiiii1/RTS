// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GPCameraBoundsVolume.generated.h"

class UBoxComponent;
class FDataValidationContext;

/**
 * Optional level-placed axis-aligned camera bounds provider.
 * Does not move the camera; CameraPawn discovers and clamps against it.
 */
UCLASS()
class GPRUNTIME_API AGP_CameraBoundsVolume : public AActor
{
	GENERATED_BODY()

public:
	AGP_CameraBoundsVolume();

	FBox GetCameraBounds() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> BoundsBox;
};
