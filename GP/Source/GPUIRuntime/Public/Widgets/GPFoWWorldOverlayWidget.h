// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rendering/RenderingCommon.h"
#include "GPFoWWorldOverlayWidget.generated.h"

class UGP_FoWWorldPresentationSubsystem;
enum class EGP_FoWState : uint8;

struct FGP_FoWOverlayDrawBatch
{
	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;
};

enum class EGP_FoWFeatherEdge : uint8
{
	MinX,
	MaxX,
	MinY,
	MaxY
};

/**
 * Source-only, hit-test-invisible viewport overlay for the local player's world FoW.
 *
 * The widget coalesces horizontal cell runs into projected quads. It caches those quads until either
 * the trusted mirror revision/reset changes or the view projection changes.
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
	bool RebuildProjectedRuns(
		const FGeometry& AllottedGeometry,
		const FMatrix& ViewProjectionMatrix,
		const FMatrix& InverseViewProjectionMatrix,
		const FIntRect& ViewRect,
		float ViewportScale) const;
	void ResetRenderCache() const;
	void AddProjectedRun(
		int32 StartX,
		int32 EndXExclusive,
		int32 CellY,
		const FLinearColor& Color,
		const FGeometry& AllottedGeometry,
		const FMatrix& ViewProjectionMatrix,
		const FIntRect& ViewRect,
		float ViewportScale) const;
	void AddProjectedFeather(
		int32 CellX,
		int32 CellY,
		EGP_FoWState CurrentState,
		EGP_FoWState MoreObscuredNeighbor,
		EGP_FoWFeatherEdge Edge,
		const FGeometry& AllottedGeometry,
		const FMatrix& ViewProjectionMatrix,
		const FIntRect& ViewRect,
		float ViewportScale) const;
	void AddProjectedQuad(
		const FVector (&WorldCorners)[4],
		const FLinearColor (&VertexColors)[4],
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
