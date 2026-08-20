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
	// event/revision-driven and projected runs are cached while both view and revision are unchanged.
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
			1,
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
		const bool bRebuilt = RebuildProjectedRuns(
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
				1,
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

bool UGP_FoWWorldOverlayWidget::RebuildProjectedRuns(
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

	const int32 MinX = FMath::Clamp(
		FMath::FloorToInt((GroundBounds.Min.X - GridOrigin.X) / CellSize) - 1,
		0,
		GridDimensions.X - 1);
	const int32 MinY = FMath::Clamp(
		FMath::FloorToInt((GroundBounds.Min.Y - GridOrigin.Y) / CellSize) - 1,
		0,
		GridDimensions.Y - 1);
	const int32 MaxX = FMath::Clamp(
		FMath::FloorToInt((GroundBounds.Max.X - GridOrigin.X) / CellSize) + 1,
		0,
		GridDimensions.X - 1);
	const int32 MaxY = FMath::Clamp(
		FMath::FloorToInt((GroundBounds.Max.Y - GridOrigin.Y) / CellSize) + 1,
		0,
		GridDimensions.Y - 1);

	if (MaxX < MinX || MaxY < MinY)
	{
		return false;
	}

	const int64 SampledCellCount64 =
		static_cast<int64>(MaxX - MinX + 1) * static_cast<int64>(MaxY - MinY + 1);
	if (SampledCellCount64 <= 0
		|| SampledCellCount64 > UGP_FoWWorldPresentationSubsystem::GetMaximumSampledCells())
	{
		return false;
	}

	int32 OverlayRuns = 0;
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		int32 RunStartX = INDEX_NONE;
		int32 CurrentRunEndX = MinX;
		EGP_FoWState RunState = EGP_FoWState::Visible;

		auto FlushRun = [&]()
		{
			if (RunStartX == INDEX_NONE)
			{
				return;
			}

			AddProjectedRun(
				RunStartX,
				CurrentRunEndX,
				Y,
				UGP_FoWWorldPresentationSubsystem::GetOverlayColorForState(RunState),
				AllottedGeometry,
				ViewProjectionMatrix,
				ViewRect,
				ViewportScale);
			++OverlayRuns;
			RunStartX = INDEX_NONE;
		};

		for (int32 X = MinX; X <= MaxX; ++X)
		{
			CurrentRunEndX = X;
			const FVector CellCenter(
				GridOrigin.X + (static_cast<double>(X) + 0.5) * CellSize,
				GridOrigin.Y + (static_cast<double>(Y) + 0.5) * CellSize,
				UGP_FoWWorldPresentationSubsystem::GetProjectionGroundZ());
			const EGP_FoWState State = Mirror->GetStateAtWorldLocation(CellCenter);

			if (State == EGP_FoWState::Visible)
			{
				FlushRun();
				continue;
			}

			if (RunStartX == INDEX_NONE)
			{
				RunStartX = X;
				RunState = State;
				continue;
			}

			if (State != RunState)
			{
				FlushRun();
				RunStartX = X;
				RunState = State;
			}
		}

		CurrentRunEndX = MaxX + 1;
		FlushRun();
	}

	Owner->RecordOverlayStats(
		static_cast<int32>(SampledCellCount64),
		OverlayRuns,
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

void UGP_FoWWorldOverlayWidget::AddProjectedRun(
	int32 StartX,
	int32 EndXExclusive,
	int32 CellY,
	const FLinearColor& Color,
	const FGeometry& AllottedGeometry,
	const FMatrix& ViewProjectionMatrix,
	const FIntRect& ViewRect,
	float ViewportScale) const
{
	if (StartX >= EndXExclusive || Color.A <= 0.0f)
	{
		return;
	}

	UGP_FoWWorldPresentationSubsystem* Owner = PresentationOwner.Get();
	UGP_LocalFoWComponent* Mirror = Owner != nullptr ? Owner->GetBoundMirror() : nullptr;
	if (Mirror == nullptr)
	{
		return;
	}

	const FVector2D GridOrigin = Mirror->GetGridOriginWorldXY();
	const float CellSize = Mirror->GetCellSizeCm();
	const double WorldMinX = GridOrigin.X + static_cast<double>(StartX) * CellSize;
	const double WorldMaxX = GridOrigin.X + static_cast<double>(EndXExclusive) * CellSize;
	const double WorldMinY = GridOrigin.Y + static_cast<double>(CellY) * CellSize;
	const double WorldMaxY = GridOrigin.Y + static_cast<double>(CellY + 1) * CellSize;
	const double GroundZ = UGP_FoWWorldPresentationSubsystem::GetProjectionGroundZ();

	const FVector WorldCorners[] =
	{
		FVector(WorldMinX, WorldMinY, GroundZ),
		FVector(WorldMaxX, WorldMinY, GroundZ),
		FVector(WorldMaxX, WorldMaxY, GroundZ),
		FVector(WorldMinX, WorldMaxY, GroundZ)
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
	const FColor VertexColor = Color.ToFColor(true);
	const FSlateRenderTransform& SlateRenderTransform =
		AllottedGeometry.GetAccumulatedRenderTransform();

	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[0], FVector2f(0.0f, 0.0f), VertexColor));
	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[1], FVector2f(1.0f, 0.0f), VertexColor));
	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[2], FVector2f(1.0f, 1.0f), VertexColor));
	Batch.Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
		SlateRenderTransform, LocalCorners[3], FVector2f(0.0f, 1.0f), VertexColor));

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
