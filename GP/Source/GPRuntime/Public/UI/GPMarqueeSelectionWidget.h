// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GPMarqueeSelectionWidget.generated.h"

/**
 * Local-only fullscreen overlay that paints the active marquee rectangle.
 * No selection logic, world scans, or input capture.
 */
UCLASS()
class GPRUNTIME_API UGP_MarqueeSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMarqueeRect(const FVector2D& InScreenStart, const FVector2D& InScreenCurrent);
	void ClearMarqueeRect();
	bool HasActiveMarqueeRect() const;

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
	bool bHasActiveRect = false;
	FVector2D ScreenStart = FVector2D::ZeroVector;
	FVector2D ScreenCurrent = FVector2D::ZeroVector;

	static constexpr float FillAlpha = 0.15f;
	static constexpr float BorderAlpha = 0.9f;
	static constexpr float BorderThickness = 1.5f;
	static constexpr float MinExtentPixels = 1.0f;
};
