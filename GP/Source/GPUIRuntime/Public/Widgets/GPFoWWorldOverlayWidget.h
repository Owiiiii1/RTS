// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Presentation/GPFoWContourField.h"
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
 * Known/Visible signed-distance contours are cached in world space on LocalFoW revision (or when the
 * view leaves the padded sample). Camera motion only reprojects those cached world triangles.
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
	bool ComputeViewCellRange(
		const FMatrix& InverseViewProjectionMatrix,
		const FIntRect& ViewRect,
		const UGP_LocalFoWComponent* Mirror,
		FIntPoint& OutMinCell,
		FIntPoint& OutMaxCell) const;
	bool RebuildWorldMask(
		const UGP_LocalFoWComponent* Mirror,
		const FIntPoint& ViewMinCell,
		const FIntPoint& ViewMaxCell) const;
	void ProjectCachedWorldTriangles(
		const FGeometry& AllottedGeometry,
		const FMatrix& ViewProjectionMatrix,
		const FIntRect& ViewRect,
		float ViewportScale) const;
	void ResetProjectionCache() const;
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
	mutable TArray<FGP_FoWContourVertex> CachedWorldTriangles;
	mutable FGP_FoWContourGeometry CachedGeometry;
	mutable FMatrix CachedViewProjection = FMatrix::Identity;
	mutable FIntRect CachedViewRect;
	mutable FVector2D CachedLocalSize = FVector2D::ZeroVector;
	mutable uint64 CachedRenderSerial = 0;
	mutable int64 CachedMaskRevision = -1;
	mutable FIntPoint CachedMaskMin = FIntPoint::ZeroValue;
	mutable FIntPoint CachedMaskMax = FIntPoint::ZeroValue;
	mutable FIntPoint CachedViewMin = FIntPoint::ZeroValue;
	mutable FIntPoint CachedViewMax = FIntPoint::ZeroValue;
	mutable bool bHasValidProjection = false;
	mutable bool bHasValidMask = false;
	mutable bool bConservativeFallback = true;
	mutable bool bLastMaskRebuilt = false;
	mutable bool bLastProjectionRebuilt = false;
};
