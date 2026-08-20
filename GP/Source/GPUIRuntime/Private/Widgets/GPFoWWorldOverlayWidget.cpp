// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GPFoWWorldOverlayWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "Presentation/GPFoWWorldPresentationSubsystem.h"
#include "Rendering/DrawElements.h"
#include "SceneView.h"
#include "Styling/CoreStyle.h"

void UGP_FoWWorldOverlayWidget::InitializeWithPresentationOwner(
	UGP_FoWWorldPresentationSubsystem* InOwner)
{
	PresentationOwner = InOwner;
	HandlePresentationDataChanged();
}

void UGP_FoWWorldOverlayWidget::HandlePresentationDataChanged()
{
	bHasValidMask = false;
	bHasValidProjection = false;
	InvalidateLayoutAndVolatility();
}

void UGP_FoWWorldOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	SetAlignmentInViewport(FVector2D::ZeroVector);
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Paint every view frame so camera projection changes are observed. FoW data itself remains
	// event/revision-driven and projected contours are cached while both view and revision are unchanged.
	bIsVolatile = true;
}

int32 UGP_FoWWorldOverlayWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);

	UGP_FoWWorldPresentationSubsystem* Owner = PresentationOwner.Get();
	if (Owner == nullptr || !Owner->IsVisualizationEnabled())
	{
		return MaxLayer;
	}

	UGP_LocalFoWComponent* Mirror = Owner->GetBoundMirror();
	if (UGP_FoWWorldPresentationSubsystem::RequiresConservativeFullObscuration(Mirror))
	{
		DrawConservativeFullObscuration(
			AllottedGeometry,
			OutDrawElements,
			MaxLayer + 1,
			bParentEnabled);
		Owner->RecordOverlayStats(FGP_FoWWorldOverlayStats());
		CachedRenderSerial = Owner->GetRenderSerial();
		return MaxLayer + 1;
	}

	ULocalPlayer* LocalPlayer = Owner->GetLocalPlayer();
	UGameViewportClient* ViewportClient =
		LocalPlayer != nullptr ? LocalPlayer->ViewportClient : nullptr;
	FViewport* Viewport = ViewportClient != nullptr ? ViewportClient->Viewport : nullptr;
	FSceneViewProjectionData ProjectionData;
	if (LocalPlayer == nullptr
		|| Viewport == nullptr
		|| !LocalPlayer->GetProjectionData(Viewport, ProjectionData))
	{
		DrawConservativeFullObscuration(
			AllottedGeometry,
			OutDrawElements,
			MaxLayer + 1,
			bParentEnabled);
		return MaxLayer + 1;
	}

	const FMatrix ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
	const FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	const float ViewportScale = FMath::Max(
		UWidgetLayoutLibrary::GetViewportScale(this),
		KINDA_SMALL_NUMBER);

	const bool bViewChanged = !bHasValidProjection
		|| CachedViewRect != ViewRect
		|| !CachedViewProjection.Equals(ViewProjectionMatrix, 1.e-4)
		|| !CachedLocalSize.Equals(LocalSize, 0.5f);
	const bool bDataChanged = CachedRenderSerial != Owner->GetRenderSerial()
		|| !bHasValidMask;

	FIntPoint ViewMinCell = FIntPoint::ZeroValue;
	FIntPoint ViewMaxCell = FIntPoint::ZeroValue;
	const bool bHaveViewCells = ComputeViewCellRange(
		ViewProjectionMatrix.InverseFast(),
		ViewRect,
		Mirror,
		ViewMinCell,
		ViewMaxCell);

	const bool bViewOutsideMask = bHaveViewCells && bHasValidMask
		&& (ViewMinCell.X < CachedMaskMin.X
			|| ViewMinCell.Y < CachedMaskMin.Y
			|| ViewMaxCell.X > CachedMaskMax.X
			|| ViewMaxCell.Y > CachedMaskMax.Y);
	const bool bNeedMask = !bHasValidMask
		|| bDataChanged
		|| bViewOutsideMask
		|| (Mirror != nullptr && Mirror->GetRevision() != CachedMaskRevision);

	bLastMaskRebuilt = false;
	bLastProjectionRebuilt = false;

	if (!bHaveViewCells)
	{
		bConservativeFallback = true;
	}
	else if (bNeedMask)
	{
		bConservativeFallback = !RebuildWorldMask(Mirror, ViewMinCell, ViewMaxCell);
		bLastMaskRebuilt = !bConservativeFallback;
		bHasValidMask = !bConservativeFallback;
	}

	if (!bConservativeFallback && (bNeedMask || bViewChanged || !bHasValidProjection))
	{
		ProjectCachedWorldTriangles(
			AllottedGeometry,
			ViewProjectionMatrix,
			ViewRect,
			ViewportScale);
		bLastProjectionRebuilt = true;
		bHasValidProjection = true;
		CachedViewProjection = ViewProjectionMatrix;
		CachedViewRect = ViewRect;
		CachedLocalSize = LocalSize;
		CachedRenderSerial = Owner->GetRenderSerial();
	}

	FGP_FoWWorldOverlayStats Stats;
	const int32 ViewW = FMath::Max(0, CachedViewMax.X - CachedViewMin.X + 1);
	const int32 ViewH = FMath::Max(0, CachedViewMax.Y - CachedViewMin.Y + 1);
	const int32 PadW = FMath::Max(0, CachedMaskMax.X - CachedMaskMin.X + 1);
	const int32 PadH = FMath::Max(0, CachedMaskMax.Y - CachedMaskMin.Y + 1);
	Stats.SampledCells = ViewW * ViewH;
	Stats.PaddedCells = PadW * PadH;
	Stats.DistanceFieldDims = CachedGeometry.DistanceFieldDims;
	Stats.ContourRawVertices = CachedGeometry.ContourRawVertices;
	Stats.ContourSmoothedVertices = CachedGeometry.ContourSmoothedVertices;
	Stats.OverlayVertices = CachedGeometry.WorldTriangles.Num();
	Stats.OverlayTriangles = CachedGeometry.TriangleCount;
	Stats.DrawBatches = CachedBatches.Num();
	Stats.MinCell = CachedMaskMin;
	Stats.MaxCell = CachedMaskMax;
	Stats.ConsumedSerial = Owner->GetRenderSerial();
	Stats.MaskRevision = CachedMaskRevision;
	Stats.bMaskRebuilt = bLastMaskRebuilt;
	Stats.bProjectionRebuilt = bLastProjectionRebuilt;
	Stats.MaskRebuildMilliseconds = CachedGeometry.MaskRebuildMilliseconds;
	Stats.DistanceFieldBytes = CachedGeometry.DistanceFieldBytes;
	Owner->RecordOverlayStats(Stats);

	if (bConservativeFallback)
	{
		DrawConservativeFullObscuration(
			AllottedGeometry,
			OutDrawElements,
			MaxLayer + 1,
			bParentEnabled);
		return MaxLayer + 1;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	if (WhiteBrush == nullptr || !WhiteBrush->GetRenderingResource().IsValid())
	{
		DrawConservativeFullObscuration(
			AllottedGeometry,
			OutDrawElements,
			MaxLayer + 1,
			bParentEnabled);
		return MaxLayer + 1;
	}

	int32 DrawLayer = MaxLayer + 1;
	for (const FGP_FoWOverlayDrawBatch& Batch : CachedBatches)
	{
		if (Batch.Vertices.IsEmpty() || Batch.Indices.IsEmpty())
		{
			continue;
		}

		FSlateDrawElement::MakeCustomVerts(
			OutDrawElements,
			DrawLayer,
			WhiteBrush->GetRenderingResource(),
			Batch.Vertices,
			Batch.Indices,
			nullptr,
			0,
			0);
		++DrawLayer;
	}

	return FMath::Max(MaxLayer, DrawLayer - 1);
}

bool UGP_FoWWorldOverlayWidget::ComputeViewCellRange(
	const FMatrix& InverseViewProjectionMatrix,
	const FIntRect& ViewRect,
	const UGP_LocalFoWComponent* Mirror,
	FIntPoint& OutMinCell,
	FIntPoint& OutMaxCell) const
{
	if (Mirror == nullptr || !Mirror->IsReady())
	{
		return false;
	}

	const FVector2D ScreenCorners[] =
	{
		FVector2D(ViewRect.Min.X, ViewRect.Min.Y),
		FVector2D(ViewRect.Max.X - 1, ViewRect.Min.Y),
		FVector2D(ViewRect.Max.X - 1, ViewRect.Max.Y - 1),
		FVector2D(ViewRect.Min.X, ViewRect.Max.Y - 1)
	};

	FBox2D GroundBounds(ForceInit);
	for (const FVector2D& ScreenCorner : ScreenCorners)
	{
		FVector RayOrigin;
		FVector RayDirection;
		FSceneView::DeprojectScreenToWorld(
			ScreenCorner,
			ViewRect,
			InverseViewProjectionMatrix,
			RayOrigin,
			RayDirection);

		if (RayDirection.Z >= -KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const double Distance =
			(UGP_FoWWorldPresentationSubsystem::GetProjectionGroundZ() - RayOrigin.Z)
			/ RayDirection.Z;
		if (!FMath::IsFinite(Distance) || Distance <= 0.0)
		{
			return false;
		}

		const FVector GroundPoint = RayOrigin + RayDirection * Distance;
		GroundBounds += FVector2D(GroundPoint.X, GroundPoint.Y);
	}

	if (!GroundBounds.bIsValid)
	{
		return false;
	}

	const FVector2D GridOrigin = Mirror->GetGridOriginWorldXY();
	const FIntPoint GridDimensions = Mirror->GetGridDimensions();
	const float CellSize = Mirror->GetCellSizeCm();
	if (GridDimensions.X <= 0 || GridDimensions.Y <= 0 || CellSize <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutMinCell = FIntPoint(
		FMath::Clamp(FMath::FloorToInt((GroundBounds.Min.X - GridOrigin.X) / CellSize), 0, GridDimensions.X - 1),
		FMath::Clamp(FMath::FloorToInt((GroundBounds.Min.Y - GridOrigin.Y) / CellSize), 0, GridDimensions.Y - 1));
	OutMaxCell = FIntPoint(
		FMath::Clamp(FMath::FloorToInt((GroundBounds.Max.X - GridOrigin.X) / CellSize), 0, GridDimensions.X - 1),
		FMath::Clamp(FMath::FloorToInt((GroundBounds.Max.Y - GridOrigin.Y) / CellSize), 0, GridDimensions.Y - 1));
	return OutMaxCell.X >= OutMinCell.X && OutMaxCell.Y >= OutMinCell.Y;
}

bool UGP_FoWWorldOverlayWidget::RebuildWorldMask(
	const UGP_LocalFoWComponent* Mirror,
	const FIntPoint& ViewMinCell,
	const FIntPoint& ViewMaxCell) const
{
	if (Mirror == nullptr || !Mirror->IsReady())
	{
		return false;
	}

	const FVector2D GridOrigin = Mirror->GetGridOriginWorldXY();
	const FIntPoint GridDimensions = Mirror->GetGridDimensions();
	const float CellSize = Mirror->GetCellSizeCm();
	const int32 Pad = UGP_FoWWorldPresentationSubsystem::GetSamplePadCells();
	const int32 MinX = FMath::Clamp(ViewMinCell.X - Pad, 0, GridDimensions.X - 1);
	const int32 MinY = FMath::Clamp(ViewMinCell.Y - Pad, 0, GridDimensions.Y - 1);
	const int32 MaxX = FMath::Clamp(ViewMaxCell.X + Pad, 0, GridDimensions.X - 1);
	const int32 MaxY = FMath::Clamp(ViewMaxCell.Y + Pad, 0, GridDimensions.Y - 1);
	const int32 Width = MaxX - MinX + 1;
	const int32 Height = MaxY - MinY + 1;
	const int64 CellCount = static_cast<int64>(Width) * static_cast<int64>(Height);
	if (CellCount <= 0
		|| CellCount > UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells())
	{
		return false;
	}

	FGP_FoWContourField Field;
	GPFoWContourField::ConfigureField(
		Field,
		MinX,
		MinY,
		Width,
		Height,
		CellSize,
		GridOrigin);

	for (int32 LocalY = 0; LocalY < Height; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < Width; ++LocalX)
		{
			const FVector CellCenter(
				GridOrigin.X + (static_cast<double>(MinX + LocalX) + 0.5) * CellSize,
				GridOrigin.Y + (static_cast<double>(MinY + LocalY) + 0.5) * CellSize,
				UGP_FoWWorldPresentationSubsystem::GetProjectionGroundZ());
			GPFoWContourField::SetCell(
				Field,
				LocalX,
				LocalY,
				Mirror->GetStateAtWorldLocation(CellCenter));
		}
	}

	if (!GPFoWContourField::RebuildPresentation(Field, CachedGeometry)
		|| CachedGeometry.TriangleCount
			> UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayTriangles())
	{
		CachedWorldTriangles.Reset();
		return false;
	}

	CachedWorldTriangles = CachedGeometry.WorldTriangles;
	CachedMaskMin = FIntPoint(MinX, MinY);
	CachedMaskMax = FIntPoint(MaxX, MaxY);
	CachedViewMin = ViewMinCell;
	CachedViewMax = ViewMaxCell;
	CachedMaskRevision = Mirror->GetRevision();
	return true;
}

void UGP_FoWWorldOverlayWidget::ProjectCachedWorldTriangles(
	const FGeometry& AllottedGeometry,
	const FMatrix& ViewProjectionMatrix,
	const FIntRect& ViewRect,
	float ViewportScale) const
{
	ResetProjectionCache();
	for (int32 VertexIndex = 0; VertexIndex + 2 < CachedWorldTriangles.Num(); VertexIndex += 3)
	{
		AddProjectedTriangle(
			CachedWorldTriangles[VertexIndex],
			CachedWorldTriangles[VertexIndex + 1],
			CachedWorldTriangles[VertexIndex + 2],
			AllottedGeometry,
			ViewProjectionMatrix,
			ViewRect,
			ViewportScale);
	}
}

void UGP_FoWWorldOverlayWidget::ResetProjectionCache() const
{
	CachedBatches.Reset();
}

void UGP_FoWWorldOverlayWidget::AddProjectedTriangle(
	const FGP_FoWContourVertex& A,
	const FGP_FoWContourVertex& B,
	const FGP_FoWContourVertex& C,
	const FGeometry& AllottedGeometry,
	const FMatrix& ViewProjectionMatrix,
	const FIntRect& ViewRect,
	float ViewportScale) const
{
	const FGP_FoWContourVertex Source[3] = {A, B, C};
	FVector2f LocalCorners[3];
	FLinearColor Colors[3];
	const double GroundZ = UGP_FoWWorldPresentationSubsystem::GetProjectionGroundZ();

	for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
	{
		FVector2D ScreenPosition;
		if (!FSceneView::ProjectWorldToScreen(
				FVector(Source[CornerIndex].WorldXY.X, Source[CornerIndex].WorldXY.Y, GroundZ),
				ViewRect,
				ViewProjectionMatrix,
				ScreenPosition,
				true))
		{
			return;
		}

		ScreenPosition -= FVector2D(ViewRect.Min);
		ScreenPosition /= ViewportScale;
		LocalCorners[CornerIndex] = FVector2f(ScreenPosition);
		Colors[CornerIndex] = UGP_FoWWorldPresentationSubsystem::GetOverlayColorForObscuration(
			Source[CornerIndex].Obscuration);
	}

	if (Colors[0].A <= 0.0f && Colors[1].A <= 0.0f && Colors[2].A <= 0.0f)
	{
		return;
	}

	if (CachedBatches.IsEmpty()
		|| CachedBatches.Last().Vertices.Num() + 3
			> UGP_FoWWorldPresentationSubsystem::GetMaximumQuadsPerBatch() * 4)
	{
		CachedBatches.AddDefaulted();
	}

	FGP_FoWOverlayDrawBatch& Batch = CachedBatches.Last();
	const SlateIndex BaseIndex = static_cast<SlateIndex>(Batch.Vertices.Num());
	const FSlateRenderTransform& SlateRenderTransform =
		AllottedGeometry.GetAccumulatedRenderTransform();

	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[0], FVector2f(0.0f, 0.0f), Colors[0].ToFColor(true)));
	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[1], FVector2f(1.0f, 0.0f), Colors[1].ToFColor(true)));
	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[2], FVector2f(1.0f, 1.0f), Colors[2].ToFColor(true)));
	Batch.Indices.Add(BaseIndex + 0);
	Batch.Indices.Add(BaseIndex + 1);
	Batch.Indices.Add(BaseIndex + 2);
}

void UGP_FoWWorldOverlayWidget::DrawConservativeFullObscuration(
	const FGeometry& AllottedGeometry,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	bool bParentEnabled) const
{
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	if (WhiteBrush == nullptr)
	{
		return;
	}

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		WhiteBrush,
		bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
		UGP_FoWWorldPresentationSubsystem::GetOverlayColorForState(
			EGP_FoWState::Unexplored));
}
