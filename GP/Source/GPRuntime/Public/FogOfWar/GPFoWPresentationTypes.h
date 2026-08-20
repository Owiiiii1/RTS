// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPFoWPresentationTypes.generated.h"

/** Compact inclusive run of row-major Fog of War cell indices. */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_FoWCellRange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|FogOfWar")
	int32 StartIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|FogOfWar")
	int32 NumCells = 0;
};

/**
 * Server-originated owner-only presentation payload.
 *
 * Initial snapshots contain all explored cells. Delta updates contain only newly explored cells.
 * VisibleRanges always represents the complete current visible set so removals need no per-cell tombstones.
 */
USTRUCT()
struct GPRUNTIME_API FGP_FoWPresentationUpdate
{
	GENERATED_BODY()

	UPROPERTY()
	bool bInitialSnapshot = false;

	UPROPERTY()
	int32 TeamId = -1;

	UPROPERTY()
	int64 Revision = 0;

	UPROPERTY()
	FVector2D GridOriginWorldXY = FVector2D::ZeroVector;

	UPROPERTY()
	FIntPoint GridDimensions = FIntPoint::ZeroValue;

	UPROPERTY()
	float CellSizeCm = 0.0f;

	UPROPERTY()
	TArray<FGP_FoWCellRange> ExploredRanges;

	UPROPERTY()
	TArray<FGP_FoWCellRange> VisibleRanges;
};
