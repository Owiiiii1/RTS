// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPUnitDropManifest.generated.h"

/**
 * Client intent for one unit DropPod order (GP-S31R).
 * Server resolves costs/slots/classes from UGP_OrbitalDeliverySettings — client cannot pick payload class.
 */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_UnitDropManifest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GP|Orbital")
	int32 WorkerCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GP|Orbital")
	int32 SalvageWalkerCount = 0;

	int32 GetTotalUnitCount() const
	{
		return FMath::Max(0, WorkerCount) + FMath::Max(0, SalvageWalkerCount);
	}

	bool IsEmpty() const
	{
		return GetTotalUnitCount() <= 0;
	}
};

UENUM(BlueprintType)
enum class EGP_UnitDropRejectReason : uint8
{
	None = 0,
	EmptyManifest,
	InvalidCounts,
	SlotOverflow,
	InsufficientOrbital,
	MissingMainBase,
	MissingDropZone,
	UnitCapReached,
	SpendFailed,
	SpawnFailed,
	MatchFinished
};
