// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Presentation/GPFoWPresentationRaster.h"
#include "Rendering/RenderingCommon.h"
#include "GPFoWWorldOverlayWidget.generated.h"

class UGP_FoWWorldPresentationSubsystem;
class UGP_LocalFoWComponent;

struct FGP_FoWOverlayDrawBatch
{
	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;
};

/**
 * Source-only, hit-test-invisible viewport overlay for the local player's world FoW.
 *
 * Rebuilds viewport-local per-cell feathered quads whenever the LocalFoW revision or camera/view
 * projection changes. Visible cells are skipped. There is no sampled fullscreen mask.
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
	bool RebuildProjectedOverlay(
		const FGeometry& AllottedGeometry,
		const FMatrix& ViewProjectionMatrix,
		const FMatrix& InverseViewProjectionMatrix,
		const FIntRect& ViewRect,
		float ViewportScale) const;
	bool ComputePaddedViewCells(
		const FMatrix& InverseViewProjectionMatrix,
		const FIntRect& ViewRect,
		const UGP_LocalFoWComponent* Mirror,
		FIntPoint& OutMinCell,
		FIntPoint& OutMaxCell) const;
	void ResetRenderCache() const;
	void AddProjectedQuad(
		const FGP_FoWPresentationQuad& Quad,
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
	mutable FGP_FoWPresentationGeometry CachedGeometry;
	mutable FMatrix CachedViewProjection = FMatrix::Identity;
	mutable FIntRect CachedViewRect;
	mutable FVector2D CachedLocalSize = FVector2D::ZeroVector;
	mutable uint64 CachedRenderSerial = 0;
	mutable int64 CachedMaskRevision = -1;
	mutable FIntPoint CachedMinCell = FIntPoint::ZeroValue;
	mutable FIntPoint CachedMaxCell = FIntPoint::ZeroValue;
	mutable bool bHasValidCache = false;
	mutable bool bConservativeFallback = true;
	mutable bool bLastCameraResample = false;
};
