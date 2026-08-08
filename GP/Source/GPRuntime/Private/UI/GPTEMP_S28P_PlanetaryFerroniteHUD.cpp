// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::NativeConstruct()
{
	Super::NativeConstruct();
	SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	SetAlignmentInViewport(FVector2D::ZeroVector);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetPlanetaryFerroniteDisplay(float StoredAmount, bool bHasBase)
{
	bHasResolvedBase = bHasBase;
	DisplayStored = StoredAmount;
	if (!bHasBase)
	{
		DisplayText = TEXT("Ferronite: --");
	}
	else
	{
		const int32 Rounded = FMath::RoundToInt(StoredAmount);
		DisplayText = FString::Printf(TEXT("Ferronite: %d"), Rounded);
	}
	InvalidateLayoutAndVolatility();
}

int32 UGP_TEMP_S28P_PlanetaryFerroniteHUD::NativePaint(
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

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18);
	const FVector2D Position(PadX, PadY);
	const FVector2D Size(420.0f, 36.0f);
	const FSlateLayoutTransform LayoutTransform(Position);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		MaxLayer + 1,
		AllottedGeometry.ToPaintGeometry(Size, LayoutTransform),
		DisplayText,
		Font,
		ESlateDrawEffect::None,
		FLinearColor(0.92f, 0.95f, 0.85f, 1.0f));

	(void)bHasResolvedBase;
	(void)DisplayStored;
	return MaxLayer + 1;
}
