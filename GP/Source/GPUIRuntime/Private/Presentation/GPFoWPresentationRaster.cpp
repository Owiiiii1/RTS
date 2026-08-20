// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPFoWPresentationRaster.h"

#include "HAL/PlatformTime.h"

namespace GPFoWPresentationRaster
{
	float ObscurationForState(EGP_FoWState State)
	{
		switch (State)
		{
		case EGP_FoWState::Visible:
			return 0.0f;
		case EGP_FoWState::Explored:
			return ExploredObscuration;
		default:
			return UnexploredObscuration;
		}
	}

	FLinearColor OverlayColorForObscuration(float Obscuration)
	{
		const float Clamped = FMath::Clamp(Obscuration, 0.0f, UnexploredObscuration);
		if (Clamped <= KINDA_SMALL_NUMBER)
		{
			return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
		}

		const FLinearColor ExploredColor(0.025f, 0.025f, 0.035f, ExploredObscuration);
		const FLinearColor UnexploredColor(0.0f, 0.0f, 0.0f, UnexploredObscuration);
		if (Clamped <= ExploredObscuration)
		{
			return FMath::Lerp(
				FLinearColor(0.0f, 0.0f, 0.0f, 0.0f),
				ExploredColor,
				Clamped / ExploredObscuration);
		}

		return FMath::Lerp(
			ExploredColor,
			UnexploredColor,
			(Clamped - ExploredObscuration)
				/ (UnexploredObscuration - ExploredObscuration));
	}

	void ResetField(FGP_FoWPresentationRaster& Field)
	{
		Field.Cells.Reset();
		Field.KnownMask.Reset();
		Field.VisibleMask.Reset();
		Field.Obscuration.Reset();
		Field.CellMinX = 0;
		Field.CellMinY = 0;
		Field.Width = 0;
		Field.Height = 0;
		Field.RasterWidth = 0;
		Field.RasterHeight = 0;
		Field.SuperSample = 0;
		Field.CellSizeCm = 0.0f;
		Field.TexelSizeCm = 0.0f;
		Field.GridOriginWorldXY = FVector2D::ZeroVector;
		Field.RasterOriginWorldXY = FVector2D::ZeroVector;
	}

	void ConfigureField(
		FGP_FoWPresentationRaster& Field,
		int32 CellMinX,
		int32 CellMinY,
		int32 Width,
		int32 Height,
		float CellSizeCm,
		const FVector2D& GridOriginWorldXY)
	{
		ResetField(Field);
		Field.CellMinX = CellMinX;
		Field.CellMinY = CellMinY;
		Field.Width = FMath::Max(Width, 0);
		Field.Height = FMath::Max(Height, 0);
		Field.CellSizeCm = CellSizeCm;
		Field.GridOriginWorldXY = GridOriginWorldXY;
		Field.Cells.Init(EGP_FoWState::Unexplored, Field.GetCellCount());
	}

	void SetCell(FGP_FoWPresentationRaster& Field, int32 LocalX, int32 LocalY, EGP_FoWState State)
	{
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height)
		{
			return;
		}
		Field.Cells[Field.CellIndex(LocalX, LocalY)] = State;
	}

	int32 ChooseSuperSample(int32 Width, int32 Height)
	{
		const int64 CellCount = static_cast<int64>(FMath::Max(Width, 0)) * FMath::Max(Height, 0);
		if (CellCount <= 0)
		{
			return 0;
		}

		int32 SuperSample = TargetSuperSample;
		while (SuperSample >= MinimumSuperSample
			&& CellCount * SuperSample * SuperSample > MaximumPresentationPixels)
		{
			--SuperSample;
		}
		if (CellCount * SuperSample * SuperSample > MaximumPresentationPixels)
		{
			return 0;
		}
		return SuperSample;
	}

	static float SampleCellBilinear(
		const TArray<float>& Values,
		int32 Width,
		int32 Height,
		float Fx,
		float Fy)
	{
		if (Width <= 0 || Height <= 0 || Values.Num() != Width * Height)
		{
			return 0.0f;
		}

		const int32 X0 = FMath::Clamp(FMath::FloorToInt(Fx), 0, Width - 1);
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt(Fy), 0, Height - 1);
		const int32 X1 = FMath::Min(X0 + 1, Width - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);
		const float TX = FMath::Clamp(Fx - static_cast<float>(X0), 0.0f, 1.0f);
		const float TY = FMath::Clamp(Fy - static_cast<float>(Y0), 0.0f, 1.0f);
		const float V00 = Values[Y0 * Width + X0];
		const float V10 = Values[Y0 * Width + X1];
		const float V01 = Values[Y1 * Width + X0];
		const float V11 = Values[Y1 * Width + X1];
		return FMath::Lerp(FMath::Lerp(V00, V10, TX), FMath::Lerp(V01, V11, TX), TY);
	}

	static void BoxBlurSeparable(
		TArray<float>& Values,
		int32 Width,
		int32 Height,
		int32 Radius)
	{
		if (Radius <= 0 || Width <= 0 || Height <= 0 || Values.Num() != Width * Height)
		{
			return;
		}

		TArray<float> Temp;
		Temp.SetNumUninitialized(Values.Num());
		const float InvKernel = 1.0f / static_cast<float>(Radius * 2 + 1);

		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				float Sum = 0.0f;
				for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
				{
					const int32 SampleX = FMath::Clamp(X + Offset, 0, Width - 1);
					Sum += Values[Y * Width + SampleX];
				}
				Temp[Y * Width + X] = Sum * InvKernel;
			}
		}

		for (int32 X = 0; X < Width; ++X)
		{
			for (int32 Y = 0; Y < Height; ++Y)
			{
				float Sum = 0.0f;
				for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
				{
					const int32 SampleY = FMath::Clamp(Y + Offset, 0, Height - 1);
					Sum += Temp[SampleY * Width + X];
				}
				Values[Y * Width + X] = Sum * InvKernel;
			}
		}
	}

	static void CoalesceHorizontalRuns(
		const FGP_FoWPresentationRaster& Field,
		FGP_FoWPresentationGeometry& OutGeometry)
	{
		OutGeometry.Quads.Reset();
		constexpr float MergeEpsilon = 0.05f;
		constexpr float SkipAlpha = 0.02f;

		for (int32 Y = 0; Y < Field.RasterHeight && OutGeometry.Quads.Num() < MaximumOverlayQuads; ++Y)
		{
			int32 RunStart = INDEX_NONE;
			float RunObscuration = 0.0f;
			auto Flush = [&](int32 EndExclusive)
			{
				if (RunStart == INDEX_NONE || EndExclusive <= RunStart)
				{
					RunStart = INDEX_NONE;
					return;
				}
				if (OutGeometry.Quads.Num() >= MaximumOverlayQuads)
				{
					RunStart = INDEX_NONE;
					return;
				}

				FGP_FoWPresentationQuad Quad;
				Quad.WorldMin = FVector2D(
					Field.RasterOriginWorldXY.X + static_cast<double>(RunStart) * Field.TexelSizeCm,
					Field.RasterOriginWorldXY.Y + static_cast<double>(Y) * Field.TexelSizeCm);
				Quad.WorldMax = FVector2D(
					Field.RasterOriginWorldXY.X + static_cast<double>(EndExclusive) * Field.TexelSizeCm,
					Field.RasterOriginWorldXY.Y + static_cast<double>(Y + 1) * Field.TexelSizeCm);
				Quad.Obscuration = RunObscuration;
				OutGeometry.Quads.Add(Quad);
				RunStart = INDEX_NONE;
			};

			for (int32 X = 0; X < Field.RasterWidth; ++X)
			{
				const float Obscuration = Field.Obscuration[Field.RasterIndex(X, Y)];
				if (Obscuration <= SkipAlpha)
				{
					Flush(X);
					continue;
				}
				if (RunStart == INDEX_NONE)
				{
					RunStart = X;
					RunObscuration = Obscuration;
					continue;
				}
				if (FMath::Abs(Obscuration - RunObscuration) > MergeEpsilon)
				{
					Flush(X);
					RunStart = X;
					RunObscuration = Obscuration;
				}
			}
			Flush(Field.RasterWidth);
		}
	}

	bool RebuildPresentation(
		FGP_FoWPresentationRaster& Field,
		FGP_FoWPresentationGeometry& OutGeometry)
	{
		const double StartTime = FPlatformTime::Seconds();
		OutGeometry = FGP_FoWPresentationGeometry();

		if (Field.Width <= 0 || Field.Height <= 0 || Field.CellSizeCm <= KINDA_SMALL_NUMBER
			|| Field.GetCellCount() > MaximumSampledCells)
		{
			return false;
		}

		Field.SuperSample = ChooseSuperSample(Field.Width, Field.Height);
		if (Field.SuperSample < MinimumSuperSample)
		{
			return false;
		}

		Field.RasterWidth = Field.Width * Field.SuperSample;
		Field.RasterHeight = Field.Height * Field.SuperSample;
		Field.TexelSizeCm = Field.CellSizeCm / static_cast<float>(Field.SuperSample);
		Field.RasterOriginWorldXY = FVector2D(
			Field.GridOriginWorldXY.X + static_cast<double>(Field.CellMinX) * Field.CellSizeCm,
			Field.GridOriginWorldXY.Y + static_cast<double>(Field.CellMinY) * Field.CellSizeCm);

		TArray<float> KnownCells;
		TArray<float> VisibleCells;
		KnownCells.SetNum(Field.GetCellCount());
		VisibleCells.SetNum(Field.GetCellCount());
		for (int32 Index = 0; Index < Field.GetCellCount(); ++Index)
		{
			const EGP_FoWState State = Field.Cells[Index];
			VisibleCells[Index] = State == EGP_FoWState::Visible ? 1.0f : 0.0f;
			KnownCells[Index] =
				(State == EGP_FoWState::Visible || State == EGP_FoWState::Explored) ? 1.0f : 0.0f;
		}

		Field.KnownMask.SetNum(Field.GetRasterCount());
		Field.VisibleMask.SetNum(Field.GetRasterCount());
		Field.Obscuration.SetNum(Field.GetRasterCount());

		const float InvSuperSample = 1.0f / static_cast<float>(Field.SuperSample);
		for (int32 RY = 0; RY < Field.RasterHeight; ++RY)
		{
			const float CellY = (static_cast<float>(RY) + 0.5f) * InvSuperSample - 0.5f;
			for (int32 RX = 0; RX < Field.RasterWidth; ++RX)
			{
				const float CellX = (static_cast<float>(RX) + 0.5f) * InvSuperSample - 0.5f;
				const int32 Index = Field.RasterIndex(RX, RY);
				Field.KnownMask[Index] = SampleCellBilinear(
					KnownCells, Field.Width, Field.Height, CellX, CellY);
				Field.VisibleMask[Index] = SampleCellBilinear(
					VisibleCells, Field.Width, Field.Height, CellX, CellY);
			}
		}

		BoxBlurSeparable(Field.KnownMask, Field.RasterWidth, Field.RasterHeight, BlurRadiusSamples);
		BoxBlurSeparable(Field.VisibleMask, Field.RasterWidth, Field.RasterHeight, BlurRadiusSamples);

		for (int32 Index = 0; Index < Field.GetRasterCount(); ++Index)
		{
			const float Known = FMath::Clamp(Field.KnownMask[Index], 0.0f, 1.0f);
			const float Visible = FMath::Clamp(Field.VisibleMask[Index], 0.0f, 1.0f);
			Field.Obscuration[Index] =
				UnexploredObscuration * (1.0f - Known)
				+ ExploredObscuration * Known * (1.0f - Visible);
		}

		CoalesceHorizontalRuns(Field, OutGeometry);

		OutGeometry.SuperSample = Field.SuperSample;
		OutGeometry.RasterDims = FIntPoint(Field.RasterWidth, Field.RasterHeight);
		OutGeometry.RasterPixels = Field.GetRasterCount();
		OutGeometry.RasterBytes = Field.GetRasterCount() * static_cast<int32>(sizeof(float)) * 3;
		OutGeometry.BlurRadiusSamples = BlurRadiusSamples;
		OutGeometry.BlurRadiusCm = BlurRadiusSamples * Field.TexelSizeCm;
		OutGeometry.PresentationTexelWorldSize = Field.TexelSizeCm;
		OutGeometry.RebuildMilliseconds = (FPlatformTime::Seconds() - StartTime) * 1000.0;
		return true;
	}

	float SamplePresentationObscuration(
		const FGP_FoWPresentationRaster& Field,
		const FVector2D& WorldXY)
	{
		if (Field.RasterWidth <= 0 || Field.RasterHeight <= 0
			|| Field.TexelSizeCm <= KINDA_SMALL_NUMBER
			|| Field.Obscuration.Num() != Field.GetRasterCount())
		{
			return UnexploredObscuration;
		}

		const float Fx = static_cast<float>(
			(WorldXY.X - Field.RasterOriginWorldXY.X) / Field.TexelSizeCm - 0.5);
		const float Fy = static_cast<float>(
			(WorldXY.Y - Field.RasterOriginWorldXY.Y) / Field.TexelSizeCm - 0.5);
		return SampleCellBilinear(
			Field.Obscuration,
			Field.RasterWidth,
			Field.RasterHeight,
			Fx,
			Fy);
	}
}
