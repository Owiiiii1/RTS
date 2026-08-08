// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPHealthBarWidget.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

void UGP_HealthBarWidget::SetHealthRatio(float InRatio)
{
	HealthRatio = FMath::Clamp(InRatio, 0.0f, 1.0f);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UGP_HealthBarWidget::SetColors(
	const FLinearColor& InFill,
	const FLinearColor& InFrame,
	const FLinearColor& InBackground)
{
	FillColor = InFill;
	FrameColor = InFrame;
	BackgroundColor = InBackground;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void UGP_HealthBarWidget::SetLayoutDrawSize(float InWidth, float InHeight)
{
	LayoutWidth = FMath::Max(1.0f, InWidth);
	LayoutHeight = FMath::Max(1.0f, InHeight);
	SetDesiredSizeInViewport(FVector2D(LayoutWidth, LayoutHeight));
	Invalidate(EInvalidateWidgetReason::Layout);
}

TSharedRef<SWidget> UGP_HealthBarWidget::RebuildWidget()
{
	// Native-only UUserWidget needs an explicit Slate root so WidgetComponent
	// receives non-zero layout geometry (NativePaint alone is not enough).
	return SNew(SBox)
		.WidthOverride(LayoutWidth)
		.HeightOverride(LayoutHeight)
		.Visibility(EVisibility::SelfHitTestInvisible)
		[
			SNullWidget::NullWidget
		];
}

void UGP_HealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetDesiredSizeInViewport(FVector2D(LayoutWidth, LayoutHeight));
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

int32 UGP_HealthBarWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	(void)Args;
	(void)MyCullingRect;
	(void)InWidgetStyle;
	(void)bParentEnabled;

	FVector2f Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 1.0f || Size.Y <= 1.0f)
	{
		Size = FVector2f(LayoutWidth, LayoutHeight);
	}
	if (Size.X <= 1.0f || Size.Y <= 1.0f)
	{
		return LayerId;
	}

	const float Border = FMath::Clamp(Size.Y * 0.15f, 1.0f, 3.0f);
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
	if (WhiteBrush == nullptr)
	{
		return LayerId;
	}

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform()),
		WhiteBrush,
		ESlateDrawEffect::None,
		FrameColor);

	const FVector2f InnerPos(Border, Border);
	const FVector2f InnerSize(
		FMath::Max(0.0f, Size.X - Border * 2.0f),
		FMath::Max(0.0f, Size.Y - Border * 2.0f));
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(InnerSize, FSlateLayoutTransform(InnerPos)),
		WhiteBrush,
		ESlateDrawEffect::None,
		BackgroundColor);

	const FVector2f FillSize(InnerSize.X * HealthRatio, InnerSize.Y);
	if (FillSize.X > 0.5f)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(FillSize, FSlateLayoutTransform(InnerPos)),
			WhiteBrush,
			ESlateDrawEffect::None,
			FillColor);
	}

	return LayerId + 3;
}
