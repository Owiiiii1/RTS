// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GPMarqueeSelectionWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UGP_MarqueeSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Full-viewport overlay: no desktop offset; paint local == scaled viewport space.
	SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	SetAlignmentInViewport(FVector2D::ZeroVector);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGP_MarqueeSelectionWidget::SetMarqueeRect(
	const FVector2D& InScreenStart,
	const FVector2D& InScreenCurrent)
{
	bHasActiveRect = true;
	ScreenStart = InScreenStart;
	ScreenCurrent = InScreenCurrent;
	InvalidateLayoutAndVolatility();
}

void UGP_MarqueeSelectionWidget::ClearMarqueeRect()
{
	if (!bHasActiveRect
		&& ScreenStart.IsNearlyZero()
		&& ScreenCurrent.IsNearlyZero())
	{
		return;
	}

	bHasActiveRect = false;
	ScreenStart = FVector2D::ZeroVector;
	ScreenCurrent = FVector2D::ZeroVector;
	InvalidateLayoutAndVolatility();
}

bool UGP_MarqueeSelectionWidget::HasActiveMarqueeRect() const
{
	return bHasActiveRect;
}

int32 UGP_MarqueeSelectionWidget::NativePaint(
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

	if (!bHasActiveRect)
	{
		return MaxLayer;
	}

	// Contract: ScreenStart/Current are viewport-local physical pixels from
	// APlayerController::GetMousePosition (same space as ProjectWorldLocationToScreen).
	// Do NOT pass them through AbsoluteToLocal — they are not desktop-absolute.
	const float SafeViewportScale = FMath::Max(
		UWidgetLayoutLibrary::GetViewportScale(this),
		KINDA_SMALL_NUMBER);
	const FVector2D LocalStart = ScreenStart / SafeViewportScale;
	const FVector2D LocalCurrent = ScreenCurrent / SafeViewportScale;

	float MinX = FMath::Min(LocalStart.X, LocalCurrent.X);
	float MaxX = FMath::Max(LocalStart.X, LocalCurrent.X);
	float MinY = FMath::Min(LocalStart.Y, LocalCurrent.Y);
	float MaxY = FMath::Max(LocalStart.Y, LocalCurrent.Y);

	if ((MaxX - MinX) < MinExtentPixels)
	{
		MaxX = MinX + MinExtentPixels;
	}
	if ((MaxY - MinY) < MinExtentPixels)
	{
		MaxY = MinY + MinExtentPixels;
	}

	const FVector2f RectSize(MaxX - MinX, MaxY - MinY);
	const FPaintGeometry FillGeometry = AllottedGeometry.ToPaintGeometry(
		RectSize,
		FSlateLayoutTransform(FVector2f(MinX, MinY)));

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
	if (WhiteBrush == nullptr)
	{
		return MaxLayer;
	}

	const ESlateDrawEffect DrawEffects =
		bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	const FLinearColor FillColor(0.2f, 0.55f, 1.0f, FillAlpha);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		MaxLayer,
		FillGeometry,
		WhiteBrush,
		DrawEffects,
		FillColor);

	TArray<FVector2f> BorderPoints;
	BorderPoints.Add(FVector2f(MinX, MinY));
	BorderPoints.Add(FVector2f(MaxX, MinY));
	BorderPoints.Add(FVector2f(MaxX, MaxY));
	BorderPoints.Add(FVector2f(MinX, MaxY));
	BorderPoints.Add(FVector2f(MinX, MinY));

	const FLinearColor BorderColor(0.35f, 0.75f, 1.0f, BorderAlpha);
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		MaxLayer + 1,
		AllottedGeometry.ToPaintGeometry(),
		MoveTemp(BorderPoints),
		DrawEffects,
		BorderColor,
		/*bAntialias=*/true,
		BorderThickness);

	return MaxLayer + 1;
}
