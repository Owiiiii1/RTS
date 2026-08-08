// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GPHealthBarWidget.generated.h"

/** TEMP playable-pass health bar paint widget (GP-S29R). Green fill; neutral frame. */
UCLASS()
class GPRUNTIME_API UGP_HealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHealthRatio(float InRatio);
	float GetHealthRatio() const { return HealthRatio; }

	void SetColors(const FLinearColor& InFill, const FLinearColor& InFrame, const FLinearColor& InBackground);

protected:
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
	FLinearColor FillColor = FLinearColor(0.15f, 0.85f, 0.25f, 1.0f);
	FLinearColor FrameColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.85f);
	FLinearColor BackgroundColor = FLinearColor(0.12f, 0.12f, 0.12f, 0.75f);
};
