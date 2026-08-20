// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Presentation/GPFoWContourField.h"
#include "Rendering/RenderingCommon.h"
#include "GPFoWWorldOverlayWidget.generated.h"

class UGP_FoWWorldPresentationSubsystem;

struct FGP_FoWOverlayDrawBatch
{
	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;
};

/**
 * Source-only, hit-test-invisible viewport overlay for the local player's world FoW.
 *
 * Discrete LocalFoW cells are converted into a conservative cell-center scalar field. Dual marching
 * squares then emit interpolated contour triangles, cached until the mirror serial or view projection changes.
 */
UCLASS(NotBlueprintable)
class GPUIRUNTIME_API UGP_FoWWorldOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeWithPresentationOwner(UGP_FoWWorldPresentationSubsystem* InOwner);
	void HandlePresentationDataChanged();
	uint64 GetConsumedRenderSerial() const { return CachedRenderSerial; }

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	bool RebuildProjectedContours(
		const FGeometry& AllottedGeometry,
		const FMatrix& ViewProjectionMatrix,
		const FMatrix& InverseViewProjectionMatrix,
		const FIntRect& ViewRect,
		float ViewportScale) const;
	void ResetRenderCache() const;
	void AddProjectedTriangle(
		const FGP_FoWContourVertex& A,
		const FGP_FoWContourVertex& B,
		const FGP_FoWContourVertex& C,
		const FGeometry& AllottedGeometry,
		const FMatrix& ViewProjectionMatrix,
		const FIntRect& ViewRect,
		float ViewportScale) const;
	void DrawConservativeFullObscuration(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		bool bParentEnabled) const;

	TWeakObjectPtr<UGP_FoWWorldPresentationSubsystem> PresentationOwner;

	mutable TArray<FGP_FoWOverlayDrawBatch> CachedBatches;
	mutable FMatrix CachedViewProjection = FMatrix::Identity;
	mutable FIntRect CachedViewRect;
	mutable FVector2D CachedLocalSize = FVector2D::ZeroVector;
	mutable uint64 CachedRenderSerial = 0;
	mutable bool bHasValidCache = false;
	mutable bool bConservativeFallback = true;
};
