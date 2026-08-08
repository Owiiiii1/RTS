// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GPHealthBarWidget.generated.h"

/** C++-only health bar (GP-S29R). Fixed Slate root + NativePaint fill; no BP asset. */
UCLASS()
class GPRUNTIME_API UGP_HealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHealthRatio(float InRatio);
	float GetHealthRatio() const { return HealthRatio; }

	void SetColors(const FLinearColor& InFill, const FLinearColor& InFrame, const FLinearColor& InBackground);

	/** Sync layout box with WidgetComponent DrawSize. */
	void SetLayoutDrawSize(float InWidth, float InHeight);

	FVector2D GetLayoutDrawSize() const { return FVector2D(LayoutWidth, LayoutHeight); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
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
	float HealthRatio = 1.0f;
	float LayoutWidth = 120.0f;
	float LayoutHeight = 14.0f;
	FLinearColor FillColor = FLinearColor(0.15f, 0.85f, 0.25f, 1.0f);
	FLinearColor FrameColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.85f);
	FLinearColor BackgroundColor = FLinearColor(0.12f, 0.12f, 0.12f, 0.75f);
};
