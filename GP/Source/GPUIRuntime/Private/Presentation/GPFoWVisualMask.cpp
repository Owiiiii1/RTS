// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPFoWVisualMask.h"

#include "FogOfWar/GPLocalFoWComponent.h"

namespace GPFoWVisualMask
{
	static void SamplePackedChannelBilinear(
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height,
		float Fx,
		float Fy,
		float& OutKnown,
		float& OutVisible)
	{
		OutKnown = 0.0f;
		OutVisible = 0.0f;
		if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
		{
			return;
		}

		const int32 X0 = FMath::Clamp(FMath::FloorToInt(Fx), 0, Width - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(Fy), 0, Height - 1);
		const int32 X1 = FMath::Min(X0 + 1, Width - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
		const float TX = FMath::Clamp(Fx - static_cast<float>(X0), 0.0f, 1.0f);
		const float TY = FMath::Clamp(Fy - static_cast<float>(Y0), 0.0f, 1.0f);

		auto KnownAt = [&Pixels, Width](int32 X, int32 Y)
		{
			return Pixels[Y * Width + X].R / 255.0f;
		};
		auto VisibleAt = [&Pixels, Width](int32 X, int32 Y)
		{
			return Pixels[Y * Width + X].G / 255.0f;
		};

		const float K00 = KnownAt(X0, Y0);
		const float K10 = KnownAt(X1, Y0);
		const float K01 = KnownAt(X0, Y1);
		const float K11 = KnownAt(X1, Y1);
		const float V00 = VisibleAt(X0, Y0);
		const float V10 = VisibleAt(X1, Y0);
		const float V01 = VisibleAt(X0, Y1);
		const float V11 = VisibleAt(X1, Y1);
		OutKnown = FMath::Lerp(FMath::Lerp(K00, K10, TX), FMath::Lerp(K01, K11, TX), TY);
		OutVisible = FMath::Lerp(FMath::Lerp(V00, V10, TX), FMath::Lerp(V01, V11, TX), TY);
	}

	float ObscurationForState(EGP_FoWState State)
	{
		switch (State)
		{
		case EGP_FoWState::Visible:
			return VisibleObscuration;
		case EGP_FoWState::Explored:
			return ExploredObscuration;
		default:
			return UnexploredObscuration;
		}
	}

	FLinearColor ComposeSceneColor(
		const FLinearColor& SceneColor,
		float Known,
		float Visible,
		bool bReady)
	{
		if (!bReady)
		{
			return FLinearColor(0.0f, 0.0f, 0.0f, SceneColor.A);
		}

		const float ClampedKnown = FMath::Clamp(Known, 0.0f, 1.0f);
		const float ClampedVisible = FMath::Clamp(Visible, 0.0f, 1.0f);
		const FLinearColor Explored = SceneColor * ExploredDimFactor;
		const FLinearColor Lit = FMath::Lerp(Explored, SceneColor, ClampedVisible);
		return FMath::Lerp(FLinearColor(0.0f, 0.0f, 0.0f, SceneColor.A), Lit, ClampedKnown);
	}

	FLinearColor ComposeSceneColorFromPacked(
		const FLinearColor& SceneColor,
		const FColor& Packed,
		bool bReady)
	{
		return ComposeSceneColor(SceneColor, Packed.R / 255.0f, Packed.G / 255.0f, bReady);
	}

	FVector2D WorldXYToUV(
		const FVector2D& WorldXY,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY)
	{
		if (ExtentWorldXY.X <= KINDA_SMALL_NUMBER || ExtentWorldXY.Y <= KINDA_SMALL_NUMBER)
		{
			return FVector2D(-1.0, -1.0);
		}

		return FVector2D(
			(WorldXY.X - OriginWorldXY.X) / ExtentWorldXY.X,
			(WorldXY.Y - OriginWorldXY.Y) / ExtentWorldXY.Y);
	}

	FVector2D UVToWorldXY(
		const FVector2D& UV,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY)
	{
		return FVector2D(
			OriginWorldXY.X + UV.X * ExtentWorldXY.X,
			OriginWorldXY.Y + UV.Y * ExtentWorldXY.Y);
	}

	bool IsUVInBounds(const FVector2D& UV)
	{
		return UV.X >= 0.0 && UV.X <= 1.0 && UV.Y >= 0.0 && UV.Y <= 1.0;
	}

	FColor PackCell(bool bKnown, bool bVisible)
	{
		return FColor(bKnown ? 255 : 0, bVisible ? 255 : 0, 0, 255);
	}

	void EncodePackedFromStates(
		TArray<FColor>& OutPixels,
		int32& OutWidth,
		int32& OutHeight,
		const TArray<EGP_FoWState>& Cells,
		int32 GridWidth,
		int32 GridHeight,
		float CellSizeCm,
		const FVector2D& OriginWorldXY)
	{
		(void)CellSizeCm;
		(void)OriginWorldXY;
		OutWidth = FMath::Max(GridWidth, 0);
		OutHeight = FMath::Max(GridHeight, 0);
		const int32 Count = OutWidth * OutHeight;
		OutPixels.SetNum(Count);
		if (Count <= 0 || Cells.Num() != Count)
		{
			OutPixels.Reset();
			OutWidth = 0;
			OutHeight = 0;
			return;
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const EGP_FoWState State = Cells[Index];
			OutPixels[Index] = PackCell(
				State == EGP_FoWState::Explored || State == EGP_FoWState::Visible,
				State == EGP_FoWState::Visible);
		}
	}

	bool EncodePackedFromLocalFoW(
		TArray<FColor>& OutPixels,
		int32& OutWidth,
		int32& OutHeight,
		FVector2D& OutOriginWorldXY,
		FVector2D& OutExtentWorldXY,
		const UGP_LocalFoWComponent* Mirror)
	{
		OutPixels.Reset();
		OutWidth = 0;
		OutHeight = 0;
		OutOriginWorldXY = FVector2D::ZeroVector;
		OutExtentWorldXY = FVector2D::ZeroVector;
		if (Mirror == nullptr || !Mirror->IsReady() || !Mirror->BuildPresentationMaskRGBA(OutPixels))
		{
			return false;
		}

		const FIntPoint Dimensions = Mirror->GetGridDimensions();
		OutWidth = Dimensions.X;
		OutHeight = Dimensions.Y;
		OutOriginWorldXY = Mirror->GetGridOriginWorldXY();
		OutExtentWorldXY = FVector2D(
			static_cast<double>(Dimensions.X) * Mirror->GetCellSizeCm(),
			static_cast<double>(Dimensions.Y) * Mirror->GetCellSizeCm());
		return OutPixels.Num() == OutWidth * OutHeight && OutWidth > 0 && OutHeight > 0;
	}

	void SamplePackedBilinear(
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height,
		const FVector2D& WorldXY,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY,
		float& OutKnown,
		float& OutVisible)
	{
		OutKnown = 0.0f;
		OutVisible = 0.0f;
		const FVector2D UV = WorldXYToUV(WorldXY, OriginWorldXY, ExtentWorldXY);
		if (!IsUVInBounds(UV) || Width <= 0 || Height <= 0)
		{
			return;
		}

		const float Fx = static_cast<float>(UV.X * Width - 0.5);
		const float Fy = static_cast<float>(UV.Y * Height - 0.5);
		SamplePackedChannelBilinear(Pixels, Width, Height, Fx, Fy, OutKnown, OutVisible);
	}

	void SamplePacked9Tap(
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height,
		const FVector2D& WorldXY,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY,
		float BlurRadiusTexelsIn,
		float& OutKnown,
		float& OutVisible)
	{
		OutKnown = 0.0f;
		OutVisible = 0.0f;
		const FVector2D UV = WorldXYToUV(WorldXY, OriginWorldXY, ExtentWorldXY);
		if (!IsUVInBounds(UV) || Width <= 0 || Height <= 0)
		{
			return;
		}

		const float Radius = FMath::Max(BlurRadiusTexelsIn, 0.0f);
		const float TexelX = 1.0f / static_cast<float>(Width);
		const float TexelY = 1.0f / static_cast<float>(Height);
		float KnownSum = 0.0f;
		float VisibleSum = 0.0f;
		int32 TapCount = 0;
		for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
		{
			for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
			{
				const FVector2D TapUV(
					FMath::Clamp(UV.X + OffsetX * TexelX * Radius, 0.0, 1.0),
					FMath::Clamp(UV.Y + OffsetY * TexelY * Radius, 0.0, 1.0));
				const float Fx = static_cast<float>(TapUV.X * Width - 0.5);
				const float Fy = static_cast<float>(TapUV.Y * Height - 0.5);
				float Known = 0.0f;
				float Visible = 0.0f;
				SamplePackedChannelBilinear(Pixels, Width, Height, Fx, Fy, Known, Visible);
				KnownSum += Known;
				VisibleSum += Visible;
				++TapCount;
			}
		}

		if (TapCount > 0)
		{
			OutKnown = KnownSum / static_cast<float>(TapCount);
			OutVisible = VisibleSum / static_cast<float>(TapCount);
		}
	}

	void ResetRuntime(FGP_FoWVisualMaskRuntime& Runtime)
	{
		Runtime = FGP_FoWVisualMaskRuntime();
	}

	void BeginNewTarget(
		FGP_FoWVisualMaskRuntime& Runtime,
		int32 Width,
		int32 Height,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY,
		int64 Revision)
	{
		Runtime.PreviousRevision = Runtime.MaskRevision;
		Runtime.Width = Width;
		Runtime.Height = Height;
		Runtime.OriginWorldXY = OriginWorldXY;
		Runtime.ExtentWorldXY = ExtentWorldXY;
		Runtime.BlendAlpha = 0.0f;
		Runtime.MaskRevision = Revision;
		Runtime.bReady = Width > 0 && Height > 0;
		++Runtime.BuildCount;
	}

	void AdvanceBlend(FGP_FoWVisualMaskRuntime& Runtime, float DeltaSeconds)
	{
		if (!Runtime.bReady || BlendDurationSeconds <= KINDA_SMALL_NUMBER)
		{
			Runtime.BlendAlpha = Runtime.bReady ? 1.0f : 0.0f;
			return;
		}

		Runtime.BlendAlpha = FMath::Clamp(
			Runtime.BlendAlpha + FMath::Max(DeltaSeconds, 0.0f) / BlendDurationSeconds,
			0.0f,
			1.0f);
	}
}
