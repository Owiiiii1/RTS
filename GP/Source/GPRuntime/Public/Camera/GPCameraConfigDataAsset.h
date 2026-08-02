// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GPCameraConfigDataAsset.generated.h"

class FDataValidationContext;

/**
 * Immutable designer-tunable RTS camera profile.
 * Consumed later by AGP_CameraPawn via soft reference; no runtime camera logic here.
 */
UCLASS(BlueprintType)
class GPRUNTIME_API UGP_CameraConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	// --- Pan ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Pan",
		meta = (ClampMin = "0.001", UIMin = "0.001", Units = "cm/s"))
	float PanSpeed = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Pan",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ZoomPanScale = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Pan",
		meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MoveAccelTime = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Pan",
		meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float MoveDecelTime = 0.10f;

	// --- Edge Scroll ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Edge Scroll")
	bool bEdgeScrollEnabled = true;

	/** Multiplier on PanSpeed while edge-scrolling (1.0 = parity with WASD). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Edge Scroll",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EdgeScrollSpeed = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Edge Scroll",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 EdgeThresholdPx = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Edge Scroll",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 EdgeFalloffPx = 24;

	// --- Zoom ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Zoom",
		meta = (ClampMin = "0.001", UIMin = "0.001", Units = "cm"))
	float MinArmLength = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Zoom",
		meta = (ClampMin = "0.001", UIMin = "0.001", Units = "cm"))
	float MaxArmLength = 4500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Zoom",
		meta = (ClampMin = "0.001", UIMin = "0.001", Units = "cm"))
	float DefaultArmLength = 2500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Zoom",
		meta = (ClampMin = "0.001", UIMin = "0.001", Units = "cm"))
	float ZoomStep = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Zoom",
		meta = (ClampMin = "0.001", UIMin = "0.001"))
	float ZoomInterpSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Zoom")
	bool bPitchInterpEnabled = true;

	/** Pitch (degrees) at farthest zoom. More negative = steeper look-down. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Zoom",
		meta = (ClampMin = "-89.0", ClampMax = "0.0", UIMin = "-89.0", UIMax = "0.0", Units = "deg"))
	float PitchAtMaxZoom = -65.0f;

	/** Pitch (degrees) at closest zoom. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Zoom",
		meta = (ClampMin = "-89.0", ClampMax = "0.0", UIMin = "-89.0", UIMax = "0.0", Units = "deg"))
	float PitchAtMinZoom = -45.0f;

	// --- Rotation ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Rotation",
		meta = (ClampMin = "0.001", UIMin = "0.001", Units = "deg"))
	float RotateSpeed = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Rotation")
	bool bInvertRotate = false;

	// --- Bounds ---

	/** Fallback world-space XYZ clamp when no AGP_CameraBoundsVolume is present. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera|Bounds")
	FBox FallbackBounds = FBox(
		FVector(-50000.0, -50000.0, -1000.0),
		FVector(50000.0, 50000.0, 5000.0));
};
