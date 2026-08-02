// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GPCommandRequest.generated.h"

class AGP_UnitBase;
class AActor;

/**
 * Canonical command intent payload (GP-S17 request prerequisite).
 * Pass-by-value RPC candidate. Not authoritative state.
 * Server must normalize/validate a copy; no client TeamId/owner fields.
 */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_CommandRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GP|Command")
	FGameplayTag CommandTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GP|Command")
	TArray<TObjectPtr<AGP_UnitBase>> IssuingUnits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GP|Command")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GP|Command")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GP|Command")
	bool bQueue = false;
};
