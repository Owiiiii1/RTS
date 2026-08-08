// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPHealthBarWidget.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UGP_HealthBarWidget::SetHealthRatio(float InRatio)
{
	HealthRatio = FMath::Clamp(InRatio, 0.0f, 1.0f);
}

void UGP_HealthBarWidget::SetColors(
	const FLinearColor& InFill,
	const FLinearColor& InFrame,
	const FLinearColor& InBackground)
{
	FillColor = InFill;
	FrameColor = InFrame;
	BackgroundColor = InBackground;
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

	const FVector2D Size = AllottedGeometry.GetLocalSize();
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
		AllottedGeometry.ToPaintGeometry(FVector2D(0.0f, 0.0f), Size),
		WhiteBrush,
		ESlateDrawEffect::None,
		FrameColor);

	const FVector2D InnerPos(Border, Border);
	const FVector2D InnerSize(FMath::Max(0.0f, Size.X - Border * 2.0f), FMath::Max(0.0f, Size.Y - Border * 2.0f));
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(InnerPos, InnerSize),
		WhiteBrush,
		ESlateDrawEffect::None,
		BackgroundColor);

	const FVector2D FillSize(InnerSize.X * HealthRatio, InnerSize.Y);
	if (FillSize.X > 0.5f)
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 2,
			AllottedGeometry.ToPaintGeometry(InnerPos, FillSize),
			WhiteBrush,
			ESlateDrawEffect::None,
			FillColor);
	}

	return LayerId + 3;
}
