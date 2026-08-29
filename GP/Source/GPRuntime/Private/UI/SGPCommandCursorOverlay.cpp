// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SGPCommandCursorOverlay.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"

namespace GPCommandCursorOverlayPrivate
{
	static void AddSegment(TArray<FVector2D>& Lines, const FVector2D& A, const FVector2D& B)
	{
		Lines.Add(A);
		Lines.Add(B);
	}

	static void DrawLines(
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FGeometry& AllottedGeometry,
		const TArray<FVector2D>& Lines,
		const FLinearColor& Color,
		float Thickness)
	{
		if (Lines.Num() < 2)
		{
			return;
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Lines,
			ESlateDrawEffect::None,
			Color,
			true,
			Thickness);
	}
}

void SGPCommandCursorOverlay::Construct(const FArguments& InArgs)
{
	Mode = InArgs._Mode;
	SetVisibility(EVisibility::HitTestInvisible);
	SetCanTick(false);
}

void SGPCommandCursorOverlay::SetMode(EGP_CommandTargetingMode InMode)
{
	if (Mode != InMode)
	{
		Mode = InMode;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FVector2D SGPCommandCursorOverlay::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	(void)LayoutScaleMultiplier;
	return FVector2D::ZeroVector;
}

int32 SGPCommandCursorOverlay::OnPaint(
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

	if (Mode == EGP_CommandTargetingMode::None || !FSlateApplication::IsInitialized())
	{
		return LayerId;
	}

	const FVector2D LocalCursor = AllottedGeometry.AbsoluteToLocal(FVector2D(FSlateApplication::Get().GetCursorPos()));

	switch (Mode)
	{
	case EGP_CommandTargetingMode::Move:
		DrawMove(OutDrawElements, LayerId, AllottedGeometry, LocalCursor);
		break;
	case EGP_CommandTargetingMode::AttackMove:
		DrawAttackMove(OutDrawElements, LayerId, AllottedGeometry, LocalCursor);
		break;
	case EGP_CommandTargetingMode::Patrol:
		DrawPatrol(OutDrawElements, LayerId, AllottedGeometry, LocalCursor);
		break;
	default:
		break;
	}

	return LayerId + 1;
}

void SGPCommandCursorOverlay::DrawCrosshair(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& AllottedGeometry,
	const FVector2D& Center,
	const FLinearColor& Color) const
{
	const float Arm = 10.0f;
	const float Gap = 3.5f;
	TArray<FVector2D> Lines;
	Lines.Reserve(8);
	GPCommandCursorOverlayPrivate::AddSegment(Lines, Center + FVector2D(0.0f, -Gap - Arm), Center + FVector2D(0.0f, -Gap));
	GPCommandCursorOverlayPrivate::AddSegment(Lines, Center + FVector2D(0.0f, Gap), Center + FVector2D(0.0f, Gap + Arm));
	GPCommandCursorOverlayPrivate::AddSegment(Lines, Center + FVector2D(-Gap - Arm, 0.0f), Center + FVector2D(-Gap, 0.0f));
	GPCommandCursorOverlayPrivate::AddSegment(Lines, Center + FVector2D(Gap, 0.0f), Center + FVector2D(Gap + Arm, 0.0f));
	GPCommandCursorOverlayPrivate::DrawLines(OutDrawElements, LayerId, AllottedGeometry, Lines, Color, 2.0f);
}

void SGPCommandCursorOverlay::DrawCenterDot(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& AllottedGeometry,
	const FVector2D& Center,
	const FLinearColor& Color) const
{
	const float Half = 1.5f;
	TArray<FVector2D> Lines;
	Lines.Reserve(8);
	GPCommandCursorOverlayPrivate::AddSegment(Lines, Center + FVector2D(-Half, -Half), Center + FVector2D(Half, -Half));
	GPCommandCursorOverlayPrivate::AddSegment(Lines, Center + FVector2D(Half, -Half), Center + FVector2D(Half, Half));
	GPCommandCursorOverlayPrivate::AddSegment(Lines, Center + FVector2D(Half, Half), Center + FVector2D(-Half, Half));
	GPCommandCursorOverlayPrivate::AddSegment(Lines, Center + FVector2D(-Half, Half), Center + FVector2D(-Half, -Half));
	GPCommandCursorOverlayPrivate::DrawLines(OutDrawElements, LayerId, AllottedGeometry, Lines, Color, 1.5f);
}

void SGPCommandCursorOverlay::DrawCircle(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& AllottedGeometry,
	const FVector2D& Center,
	float Radius,
	const FLinearColor& Color,
	int32 Segments) const
{
	const int32 SafeSegments = FMath::Max(8, Segments);
	TArray<FVector2D> Lines;
	Lines.Reserve(SafeSegments * 2);
	for (int32 Index = 0; Index < SafeSegments; ++Index)
	{
		const float A0 = (2.0f * PI * static_cast<float>(Index)) / static_cast<float>(SafeSegments);
		const float A1 = (2.0f * PI * static_cast<float>(Index + 1)) / static_cast<float>(SafeSegments);
		GPCommandCursorOverlayPrivate::AddSegment(
			Lines,
			Center + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius,
			Center + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius);
	}
	GPCommandCursorOverlayPrivate::DrawLines(OutDrawElements, LayerId, AllottedGeometry, Lines, Color, 2.0f);
}

void SGPCommandCursorOverlay::DrawMove(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& AllottedGeometry,
	const FVector2D& Center) const
{
	const FLinearColor Color(0.95f, 0.95f, 0.95f, 1.0f);
	DrawCrosshair(OutDrawElements, LayerId, AllottedGeometry, Center, Color);
	DrawCenterDot(OutDrawElements, LayerId, AllottedGeometry, Center, Color);
}

void SGPCommandCursorOverlay::DrawAttackMove(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& AllottedGeometry,
	const FVector2D& Center) const
{
	const FLinearColor Color(0.95f, 0.95f, 0.95f, 1.0f);
	DrawCircle(OutDrawElements, LayerId, AllottedGeometry, Center, 14.0f, Color, 20);
	DrawCrosshair(OutDrawElements, LayerId, AllottedGeometry, Center, Color);
}

void SGPCommandCursorOverlay::DrawPatrol(
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FGeometry& AllottedGeometry,
	const FVector2D& Center) const
{
	const FLinearColor Color(0.95f, 0.95f, 0.95f, 1.0f);
	DrawCrosshair(OutDrawElements, LayerId, AllottedGeometry, Center, Color);

	const float ArrowX = 16.0f;
	const float Head = 4.0f;
	TArray<FVector2D> Arrows;
	Arrows.Reserve(8);
	const FVector2D Right(Center.X + ArrowX, Center.Y);
	GPCommandCursorOverlayPrivate::AddSegment(Arrows, Right + FVector2D(-Head, -Head), Right);
	GPCommandCursorOverlayPrivate::AddSegment(Arrows, Right + FVector2D(-Head, Head), Right);
	const FVector2D Left(Center.X - ArrowX, Center.Y);
	GPCommandCursorOverlayPrivate::AddSegment(Arrows, Left + FVector2D(Head, -Head), Left);
	GPCommandCursorOverlayPrivate::AddSegment(Arrows, Left + FVector2D(Head, Head), Left);
	GPCommandCursorOverlayPrivate::DrawLines(OutDrawElements, LayerId, AllottedGeometry, Arrows, Color, 2.0f);
}
