// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GPFoWWorldOverlayWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/PlatformTime.h"
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

	// Paint every view frame so camera projection changes rebuild per-cell tiles.
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
		FGP_FoWWorldOverlayStats NotReadyStats;
		NotReadyStats.ConsumedSerial = Owner->GetRenderSerial();
		NotReadyStats.bFallbackActive = true;
		Owner->RecordOverlayStats(NotReadyStats);
		CachedRenderSerial = Owner->GetRenderSerial();
		ActiveFades.Reset();
		CachedVisualObscuration.Reset();
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
	const bool bFadeTick = ActiveFades.Num() > 0;

	bLastCameraResample = false;
	if (bViewChanged || bDataChanged || bFadeTick)
	{
		const bool bRebuilt = RebuildProjectedOverlay(
			AllottedGeometry,
			ViewProjectionMatrix,
			ViewProjectionMatrix.InverseFast(),
			ViewRect,
			ViewportScale);

		CachedViewProjection = ViewProjectionMatrix;
		CachedViewRect = ViewRect;
		CachedLocalSize = LocalSize;
		CachedRenderSerial = Owner->GetRenderSerial();
		bLastCameraResample = bViewChanged;
		if (bRebuilt)
		{
			bHasValidCache = true;
			bConservativeFallback = false;
		}
		else if (!bHasValidCache)
		{
			bConservativeFallback = true;
		}
	}

	FGP_FoWWorldOverlayStats Stats;
	Stats.SampledGameplayCells = bHasValidCache
		? (CachedMaxCell.X - CachedMinCell.X + 1) * (CachedMaxCell.Y - CachedMinCell.Y + 1)
		: 0;
	Stats.CellTiles = CachedGeometry.CellTiles;
	Stats.FeatherQuads = CachedGeometry.FeatherQuads;
	Stats.VisibleCellsSkipped = CachedGeometry.VisibleCellsSkipped;
	Stats.FeatherCm = CachedGeometry.FeatherCm;
	Stats.OverlayVertices = 0;
	for (const FGP_FoWOverlayDrawBatch& Batch : CachedBatches)
	{
		Stats.OverlayVertices += Batch.Vertices.Num();
	}
	Stats.OverlayQuads = CachedGeometry.Quads.Num();
	Stats.DrawBatches = CachedBatches.Num();
	Stats.MinCell = CachedMinCell;
	Stats.MaxCell = CachedMaxCell;
	Stats.ConsumedSerial = Owner->GetRenderSerial();
	Stats.MaskRevision = CachedMaskRevision;
	Stats.bCameraResample = bLastCameraResample;
	Stats.bFallbackActive = bConservativeFallback;
	Stats.RebuildMilliseconds = CachedGeometry.RebuildMilliseconds;
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

bool UGP_FoWWorldOverlayWidget::ComputePaddedViewCells(
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

	const FVector2D ScreenSamples[] =
	{
		FVector2D(ViewRect.Min.X, ViewRect.Min.Y),
		FVector2D(ViewRect.Max.X - 1, ViewRect.Min.Y),
		FVector2D(ViewRect.Max.X - 1, ViewRect.Max.Y - 1),
		FVector2D(ViewRect.Min.X, ViewRect.Max.Y - 1),
		FVector2D(
			(ViewRect.Min.X + ViewRect.Max.X) * 0.5,
			(ViewRect.Min.Y + ViewRect.Max.Y) * 0.5)
	};

	FBox2D GroundBounds(ForceInit);
	int32 GroundHits = 0;
	FVector LastRayOrigin = FVector::ZeroVector;
	for (const FVector2D& ScreenSample : ScreenSamples)
	{
		FVector RayOrigin;
		FVector RayDirection;
		FSceneView::DeprojectScreenToWorld(
			ScreenSample,
			ViewRect,
			InverseViewProjectionMatrix,
			RayOrigin,
			RayDirection);
		LastRayOrigin = RayOrigin;

		if (RayDirection.Z < -KINDA_SMALL_NUMBER)
		{
			const double Distance =
				(UGP_FoWWorldPresentationSubsystem::GetProjectionGroundZ() - RayOrigin.Z)
				/ RayDirection.Z;
			if (FMath::IsFinite(Distance) && Distance > 0.0)
			{
				const FVector GroundPoint = RayOrigin + RayDirection * Distance;
				GroundBounds += FVector2D(GroundPoint.X, GroundPoint.Y);
				++GroundHits;
				continue;
			}
		}

		FVector2D DirXY(RayDirection.X, RayDirection.Y);
		if (DirXY.Normalize())
		{
			GroundBounds += FVector2D(RayOrigin.X, RayOrigin.Y) + DirXY * 6000.0;
			++GroundHits;
		}
	}

	if (GroundHits <= 0)
	{
		GroundBounds += FVector2D(LastRayOrigin.X, LastRayOrigin.Y);
		GroundBounds += FVector2D(LastRayOrigin.X + 4000.0, LastRayOrigin.Y + 4000.0);
		GroundBounds += FVector2D(LastRayOrigin.X - 4000.0, LastRayOrigin.Y - 4000.0);
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
	const int32 ExtraPad = GroundHits < 4 ? 4 : 0;
	OutMinCell = FIntPoint(
		FMath::Clamp(
			FMath::FloorToInt((GroundBounds.Min.X - GridOrigin.X) / CellSize) - Pad - ExtraPad,
			0,
			GridDimensions.X - 1),
		FMath::Clamp(
			FMath::FloorToInt((GroundBounds.Min.Y - GridOrigin.Y) / CellSize) - Pad - ExtraPad,
			0,
			GridDimensions.Y - 1));
	OutMaxCell = FIntPoint(
		FMath::Clamp(
			FMath::FloorToInt((GroundBounds.Max.X - GridOrigin.X) / CellSize) + Pad + ExtraPad,
			0,
			GridDimensions.X - 1),
		FMath::Clamp(
			FMath::FloorToInt((GroundBounds.Max.Y - GridOrigin.Y) / CellSize) + Pad + ExtraPad,
			0,
			GridDimensions.Y - 1));
	return OutMaxCell.X >= OutMinCell.X && OutMaxCell.Y >= OutMinCell.Y;
}

bool UGP_FoWWorldOverlayWidget::RebuildProjectedOverlay(
	const FGeometry& AllottedGeometry,
	const FMatrix& ViewProjectionMatrix,
	const FMatrix& InverseViewProjectionMatrix,
	const FIntRect& ViewRect,
	float ViewportScale) const
{
	UGP_FoWWorldPresentationSubsystem* Owner = PresentationOwner.Get();
	UGP_LocalFoWComponent* Mirror = Owner != nullptr ? Owner->GetBoundMirror() : nullptr;
	if (Owner == nullptr || Mirror == nullptr || !Mirror->IsReady())
	{
		return false;
	}

	FIntPoint MinCell = FIntPoint::ZeroValue;
	FIntPoint MaxCell = FIntPoint::ZeroValue;
	if (!ComputePaddedViewCells(
			InverseViewProjectionMatrix,
			ViewRect,
			Mirror,
			MinCell,
			MaxCell))
	{
		return false;
	}

	int32 Width = MaxCell.X - MinCell.X + 1;
	int32 Height = MaxCell.Y - MinCell.Y + 1;
	const int64 SampledCellCount64 = static_cast<int64>(Width) * static_cast<int64>(Height);
	if (SampledCellCount64 <= 0
		|| SampledCellCount64 > UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells())
	{
		ResetRenderCache();
		CachedGeometry = FGP_FoWPresentationGeometry();
		CachedMinCell = MinCell;
		CachedMaxCell = MaxCell;
		bHasValidCache = false;
		bConservativeFallback = true;
		CachedMaskRevision = Mirror->GetRevision();
		ActiveFades.Reset();
		CachedVisualObscuration.Reset();
		return false;
	}

	FGP_FoWPresentationRaster Field;
	GPFoWPresentationRaster::ConfigureField(
		Field,
		MinCell.X,
		MinCell.Y,
		Width,
		Height,
		Mirror->GetCellSizeCm(),
		Mirror->GetGridOriginWorldXY());

	const FVector2D GridOrigin = Mirror->GetGridOriginWorldXY();
	const float CellSize = Mirror->GetCellSizeCm();
	const FIntPoint GridDimensions = Mirror->GetGridDimensions();
	const UWorld* World = GetWorld();
	const double NowSeconds =
		World != nullptr ? World->GetTimeSeconds() : FPlatformTime::Seconds();
	const int32 PreviousWidth = CachedMaxCell.X - CachedMinCell.X + 1;
	const int32 PreviousHeight = CachedMaxCell.Y - CachedMinCell.Y + 1;
	const bool bPreviousValid =
		CachedVisualObscuration.Num() == PreviousWidth * PreviousHeight
		&& PreviousWidth > 0
		&& PreviousHeight > 0;
	TArray<float> NewVisualObscuration;
	NewVisualObscuration.SetNumUninitialized(Width * Height);

	for (int32 LocalY = 0; LocalY < Height; ++LocalY)
	{
		for (int32 LocalX = 0; LocalX < Width; ++LocalX)
		{
			const int32 CellX = MinCell.X + LocalX;
			const int32 CellY = MinCell.Y + LocalY;
			const FVector CellCenter(
				GridOrigin.X + (static_cast<double>(CellX) + 0.5) * CellSize,
				GridOrigin.Y + (static_cast<double>(CellY) + 0.5) * CellSize,
				UGP_FoWWorldPresentationSubsystem::GetProjectionGroundZ());
			const EGP_FoWState GameplayState = Mirror->GetStateAtWorldLocation(CellCenter);
			GPFoWPresentationRaster::SetCell(Field, LocalX, LocalY, GameplayState);

			const float TargetObscuration =
				GPFoWPresentationRaster::ObscurationForState(GameplayState);
			if (GridDimensions.X <= 0)
			{
				NewVisualObscuration[LocalY * Width + LocalX] = TargetObscuration;
				GPFoWPresentationRaster::SetObscuration(Field, LocalX, LocalY, TargetObscuration);
				continue;
			}

			bool bHasPrevious = false;
			float PreviousObscuration = TargetObscuration;
			if (bPreviousValid
				&& CellX >= CachedMinCell.X && CellX <= CachedMaxCell.X
				&& CellY >= CachedMinCell.Y && CellY <= CachedMaxCell.Y)
			{
				PreviousObscuration = CachedVisualObscuration[
					(CellY - CachedMinCell.Y) * PreviousWidth + (CellX - CachedMinCell.X)];
				bHasPrevious = true;
			}

			const int32 GlobalIndex = CellY * GridDimensions.X + CellX;
			const float VisualObscuration = ResolveVisualObscuration(
				GlobalIndex,
				TargetObscuration,
				bHasPrevious,
				PreviousObscuration,
				NowSeconds);
			NewVisualObscuration[LocalY * Width + LocalX] = VisualObscuration;
			GPFoWPresentationRaster::SetObscuration(Field, LocalX, LocalY, VisualObscuration);
		}
	}

	FGP_FoWPresentationGeometry NewGeometry;
	if (!GPFoWPresentationRaster::RebuildPresentation(Field, NewGeometry))
	{
		ResetRenderCache();
		CachedGeometry = FGP_FoWPresentationGeometry();
		bHasValidCache = false;
		bConservativeFallback = true;
		CachedMaskRevision = Mirror->GetRevision();
		return false;
	}

	ResetRenderCache();
	CachedGeometry = NewGeometry;
	for (const FGP_FoWPresentationQuad& Quad : CachedGeometry.Quads)
	{
		AddProjectedQuad(
			Quad,
			AllottedGeometry,
			ViewProjectionMatrix,
			ViewRect,
			ViewportScale);
	}

	CachedMinCell = MinCell;
	CachedMaxCell = MaxCell;
	CachedMaskRevision = Mirror->GetRevision();
	CachedVisualObscuration = MoveTemp(NewVisualObscuration);
	PruneFadesOutsideRegion(MinCell, MaxCell, GridDimensions.X);
	return true;
}

void UGP_FoWWorldOverlayWidget::ResetRenderCache() const
{
	CachedBatches.Reset();
}

void UGP_FoWWorldOverlayWidget::AddProjectedQuad(
	const FGP_FoWPresentationQuad& Quad,
	const FGeometry& AllottedGeometry,
	const FMatrix& ViewProjectionMatrix,
	const FIntRect& ViewRect,
	float ViewportScale) const
{
	if (Quad.CornerColors[0].A <= 0.0f
		&& Quad.CornerColors[1].A <= 0.0f
		&& Quad.CornerColors[2].A <= 0.0f
		&& Quad.CornerColors[3].A <= 0.0f)
	{
		return;
	}

	const double GroundZ = UGP_FoWWorldPresentationSubsystem::GetProjectionGroundZ();
	const FVector WorldCorners[] =
	{
		FVector(Quad.WorldCorners[0].X, Quad.WorldCorners[0].Y, GroundZ),
		FVector(Quad.WorldCorners[1].X, Quad.WorldCorners[1].Y, GroundZ),
		FVector(Quad.WorldCorners[2].X, Quad.WorldCorners[2].Y, GroundZ),
		FVector(Quad.WorldCorners[3].X, Quad.WorldCorners[3].Y, GroundZ)
	};

	FVector2f LocalCorners[4];
	for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
	{
		FVector2D ScreenPosition;
		if (!FSceneView::ProjectWorldToScreen(
				WorldCorners[CornerIndex],
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
	}

	if (CachedBatches.IsEmpty()
		|| CachedBatches.Last().Vertices.Num() + 4
			> UGP_FoWWorldPresentationSubsystem::GetMaximumQuadsPerBatch() * 4)
	{
		CachedBatches.AddDefaulted();
	}

	FGP_FoWOverlayDrawBatch& Batch = CachedBatches.Last();
	const SlateIndex BaseIndex = static_cast<SlateIndex>(Batch.Vertices.Num());
	const FSlateRenderTransform& SlateRenderTransform =
		AllottedGeometry.GetAccumulatedRenderTransform();

	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[0], FVector2f(0.0f, 0.0f), Quad.CornerColors[0].ToFColor(true)));
	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[1], FVector2f(1.0f, 0.0f), Quad.CornerColors[1].ToFColor(true)));
	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[2], FVector2f(1.0f, 1.0f), Quad.CornerColors[2].ToFColor(true)));
	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[3], FVector2f(0.0f, 1.0f), Quad.CornerColors[3].ToFColor(true)));
	Batch.Indices.Add(BaseIndex + 0);
	Batch.Indices.Add(BaseIndex + 1);
	Batch.Indices.Add(BaseIndex + 2);
	Batch.Indices.Add(BaseIndex + 0);
	Batch.Indices.Add(BaseIndex + 2);
	Batch.Indices.Add(BaseIndex + 3);
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

float UGP_FoWWorldOverlayWidget::ResolveVisualObscuration(
	int32 GlobalCellIndex,
	float TargetObscuration,
	bool bHasPrevious,
	float PreviousObscuration,
	double NowSeconds) const
{
	constexpr float EqualTolerance = 0.01f;
	if (FGP_FoWCellFade* Fade = ActiveFades.Find(GlobalCellIndex))
	{
		const float Elapsed = static_cast<float>(NowSeconds - Fade->StartSeconds);
		Fade->CurrentObscuration = GPFoWPresentationRaster::EvaluateFade(
			Fade->StartObscuration,
			Fade->TargetObscuration,
			Elapsed,
			Fade->DurationSeconds);
		if (!FMath::IsNearlyEqual(Fade->TargetObscuration, TargetObscuration, EqualTolerance))
		{
			Fade->StartObscuration = Fade->CurrentObscuration;
			Fade->TargetObscuration = TargetObscuration;
			Fade->StartSeconds = NowSeconds;
			Fade->DurationSeconds = GPFoWPresentationRaster::DurationForObscurationChange(
				Fade->StartObscuration,
				TargetObscuration);
			Fade->CurrentObscuration = Fade->StartObscuration;
		}
		else if (Elapsed >= Fade->DurationSeconds)
		{
			ActiveFades.Remove(GlobalCellIndex);
			return TargetObscuration;
		}
		return Fade->CurrentObscuration;
	}

	const float FromObscuration = bHasPrevious ? PreviousObscuration : TargetObscuration;
	if (!bHasPrevious || FMath::IsNearlyEqual(FromObscuration, TargetObscuration, EqualTolerance))
	{
		return TargetObscuration;
	}

	FGP_FoWCellFade Fade;
	Fade.StartObscuration = FromObscuration;
	Fade.TargetObscuration = TargetObscuration;
	Fade.CurrentObscuration = FromObscuration;
	Fade.StartSeconds = NowSeconds;
	Fade.DurationSeconds = GPFoWPresentationRaster::DurationForObscurationChange(
		FromObscuration,
		TargetObscuration);
	ActiveFades.Add(GlobalCellIndex, Fade);
	return FromObscuration;
}

void UGP_FoWWorldOverlayWidget::PruneFadesOutsideRegion(
	const FIntPoint& MinCell,
	const FIntPoint& MaxCell,
	int32 GridWidth) const
{
	if (GridWidth <= 0)
	{
		ActiveFades.Reset();
		return;
	}

	for (auto It = ActiveFades.CreateIterator(); It; ++It)
	{
		const int32 CellX = It.Key() % GridWidth;
		const int32 CellY = It.Key() / GridWidth;
		if (CellX < MinCell.X || CellX > MaxCell.X || CellY < MinCell.Y || CellY > MaxCell.Y)
		{
			It.RemoveCurrent();
		}
	}
}
