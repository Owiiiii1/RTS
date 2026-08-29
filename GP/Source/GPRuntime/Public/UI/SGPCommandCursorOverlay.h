// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Player/GPPlayerController.h"
#include "Widgets/SLeafWidget.h"

/**
 * Full-viewport HitTestInvisible software cursor drawn with Slate primitives.
 * No assets. Paint follows the platform mouse position.
 */
class SGPCommandCursorOverlay : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGPCommandCursorOverlay)
		: _Mode(EGP_CommandTargetingMode::Move)
	{}
		SLATE_ARGUMENT(EGP_CommandTargetingMode, Mode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetMode(EGP_CommandTargetingMode InMode);
	EGP_CommandTargetingMode GetMode() const { return Mode; }

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual bool ComputeVolatility() const override { return true; }
	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	void DrawMove(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& AllottedGeometry,
		const FVector2D& Center) const;
	void DrawAttackMove(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& AllottedGeometry,
		const FVector2D& Center) const;
	void DrawPatrol(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& AllottedGeometry,
		const FVector2D& Center) const;
	void DrawCrosshair(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& AllottedGeometry,
		const FVector2D& Center,
		const FLinearColor& Color) const;
	void DrawCenterDot(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& AllottedGeometry,
		const FVector2D& Center,
		const FLinearColor& Color) const;
	void DrawCircle(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& AllottedGeometry,
		const FVector2D& Center,
		float Radius,
		const FLinearColor& Color,
		int32 Segments) const;

	EGP_CommandTargetingMode Mode = EGP_CommandTargetingMode::Move;
};
