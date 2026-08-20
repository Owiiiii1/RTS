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
	bHasValidCache = false;
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
		Owner->RecordOverlayStats(
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			1,
			FIntPoint::ZeroValue,
			FIntPoint::ZeroValue,
			Owner->GetRenderSerial());
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

	const bool bViewChanged = !bHasValidCache
		|| CachedViewRect != ViewRect
		|| !CachedViewProjection.Equals(ViewProjectionMatrix, 1.e-4)
		|| !CachedLocalSize.Equals(LocalSize, 0.5f);
	const bool bDataChanged = CachedRenderSerial != Owner->GetRenderSerial();

	if (bViewChanged || bDataChanged)
	{
		const bool bRebuilt = RebuildProjectedContours(
			AllottedGeometry,
			ViewProjectionMatrix,
			ViewProjectionMatrix.InverseFast(),
			ViewRect,
			ViewportScale);

		CachedViewProjection = ViewProjectionMatrix;
		CachedViewRect = ViewRect;
		CachedLocalSize = LocalSize;
		CachedRenderSerial = Owner->GetRenderSerial();
		bHasValidCache = true;
		bConservativeFallback = !bRebuilt;
		if (!bRebuilt)
		{
			Owner->RecordOverlayStats(
				0,
				0,
				0,
				0,
				0,
				0,
				0,
				1,
				FIntPoint::ZeroValue,
				FIntPoint::ZeroValue,
				Owner->GetRenderSerial());
		}
	}

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

bool UGP_FoWWorldOverlayWidget::RebuildProjectedContours(
	const FGeometry& AllottedGeometry,
	const FMatrix& ViewProjectionMatrix,
	const FMatrix& InverseViewProjectionMatrix,
	const FIntRect& ViewRect,
	float ViewportScale) const
{
	ResetRenderCache();

	UGP_FoWWorldPresentationSubsystem* Owner = PresentationOwner.Get();
	UGP_LocalFoWComponent* Mirror = Owner != nullptr ? Owner->GetBoundMirror() : nullptr;
	if (Owner == nullptr || Mirror == nullptr || !Mirror->IsReady())
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

	const int32 Pad = UGP_FoWWorldPresentationSubsystem::GetSamplePadCells();
	const int32 MinX = FMath::Clamp(
		FMath::FloorToInt((GroundBounds.Min.X - GridOrigin.X) / CellSize) - Pad,
		0,
		GridDimensions.X - 1);
	const int32 MinY = FMath::Clamp(
		FMath::FloorToInt((GroundBounds.Min.Y - GridOrigin.Y) / CellSize) - Pad,
		0,
		GridDimensions.Y - 1);
	const int32 MaxX = FMath::Clamp(
		FMath::FloorToInt((GroundBounds.Max.X - GridOrigin.X) / CellSize) + Pad,
		0,
		GridDimensions.X - 1);
	const int32 MaxY = FMath::Clamp(
		FMath::FloorToInt((GroundBounds.Max.Y - GridOrigin.Y) / CellSize) + Pad,
		0,
		GridDimensions.Y - 1);

	if (MaxX < MinX || MaxY < MinY)
	{
		return false;
	}

	const int32 Width = MaxX - MinX + 1;
	const int32 Height = MaxY - MinY + 1;
	const int64 PaddedCellCount64 = static_cast<int64>(Width) * static_cast<int64>(Height);
	if (PaddedCellCount64 <= 0
		|| PaddedCellCount64 > UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells())
	{
		return false;
	}

	FGP_FoWContourField Field;
	GPFoWContourField::ConfigureField(
		Field,
		MinX - 1,
		MinY - 1,
		Width + 2,
		Height + 2,
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
				LocalX + 1,
				LocalY + 1,
				Mirror->GetStateAtWorldLocation(CellCenter));
		}
	}

	GPFoWContourField::BuildCenterSamples(Field);

	FGP_FoWContourGeometry Geometry;
	GPFoWContourField::GenerateOverlayGeometry(Field, Geometry);
	if (Geometry.TriangleCount > UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayTriangles()
		|| Geometry.VisibleIsoSegments.Num() + Geometry.UnexploredIsoSegments.Num()
			> UGP_FoWWorldPresentationSubsystem::GetMaximumIsoSegments())
	{
		return false;
	}

	for (int32 VertexIndex = 0; VertexIndex + 2 < Geometry.TriangleVertices.Num(); VertexIndex += 3)
	{
		AddProjectedTriangle(
			Geometry.TriangleVertices[VertexIndex],
			Geometry.TriangleVertices[VertexIndex + 1],
			Geometry.TriangleVertices[VertexIndex + 2],
			AllottedGeometry,
			ViewProjectionMatrix,
			ViewRect,
			ViewportScale);
	}

	const int32 ViewportCellCount = FMath::Max(0, (Width - 2 * Pad) * (Height - 2 * Pad));
	Owner->RecordOverlayStats(
		ViewportCellCount > 0 ? ViewportCellCount : static_cast<int32>(PaddedCellCount64),
		static_cast<int32>(PaddedCellCount64),
		Geometry.VisibleIsoSegments.Num() + Geometry.UnexploredIsoSegments.Num(),
		Geometry.TriangleVertices.Num(),
		Geometry.TriangleCount,
		Geometry.MixedCellCount,
		Geometry.CoalescedQuadCount,
		CachedBatches.Num(),
		FIntPoint(MinX, MinY),
		FIntPoint(MaxX, MaxY),
		Owner->GetRenderSerial());
	return true;
}

void UGP_FoWWorldOverlayWidget::ResetRenderCache() const
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
