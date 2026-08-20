// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"

struct FGP_FoWPresentationQuad
{
	FVector2D WorldMin = FVector2D::ZeroVector;
	FVector2D WorldMax = FVector2D::ZeroVector;
	float Obscuration = 0.0f;
};

struct FGP_FoWPresentationRaster
{
	TArray<EGP_FoWState> Cells;
	TArray<float> KnownMask;
	TArray<float> VisibleMask;
	TArray<float> Obscuration;
	int32 CellMinX = 0;
	int32 CellMinY = 0;
	int32 Width = 0;
	int32 Height = 0;
	int32 RasterWidth = 0;
	int32 RasterHeight = 0;
	int32 SuperSample = 0;
	float CellSizeCm = 0.0f;
	float TexelSizeCm = 0.0f;
	FVector2D GridOriginWorldXY = FVector2D::ZeroVector;
	FVector2D RasterOriginWorldXY = FVector2D::ZeroVector;

	int32 GetCellCount() const { return Width * Height; }
	int32 GetRasterCount() const { return RasterWidth * RasterHeight; }

	int32 CellIndex(int32 LocalX, int32 LocalY) const
	{
		return LocalY * Width + LocalX;
	}

	int32 RasterIndex(int32 RasterX, int32 RasterY) const
	{
		return RasterY * RasterWidth + RasterX;
	}
};

struct FGP_FoWPresentationGeometry
{
	TArray<FGP_FoWPresentationQuad> Quads;
	int32 SuperSample = 0;
	FIntPoint RasterDims = FIntPoint::ZeroValue;
	int32 RasterPixels = 0;
	int32 RasterBytes = 0;
	int32 BlurRadiusSamples = 0;
	float BlurRadiusCm = 0.0f;
	float PresentationTexelWorldSize = 0.0f;
	double RebuildMilliseconds = 0.0;
};

/**
 * Viewport-local high-resolution FoW presentation raster.
 *
 * LocalFoW cells remain the only input. Known (Explored|Visible) and Visible masks are bilinearly
 * upsampled, box-blurred, then composed into obscuration. Gameplay CellSize is never changed.
 */
namespace GPFoWPresentationRaster
{
	constexpr float ExploredObscuration = 0.68f;
	constexpr float UnexploredObscuration = 1.0f;
	constexpr int32 TargetSuperSample = 10;
	constexpr int32 MinimumSuperSample = 4;
	constexpr int32 BlurRadiusSamples = 8;
	constexpr int32 SamplePadCells = 2;
	constexpr int32 MaximumSampledCells = 65536;
	constexpr int32 MaximumPresentationPixels = 262144;
	constexpr int32 MaximumOverlayQuads = 16384;
	constexpr int32 MaximumQuadsPerBatch = 8000;

	inline const TCHAR* GetAlgorithmName()
	{
		return TEXT("BilinearUpsampleSeparableBoxBlur");
	}

	inline const TCHAR* GetMaskModelName()
	{
		return TEXT("KnownMask+VisibleMask");
	}

	inline const TCHAR* GetInterpolationName()
	{
		return TEXT("BilinearCellCenter");
	}

	inline const TCHAR* GetBlurName()
	{
		return TEXT("SeparableBox");
	}

	float ObscurationForState(EGP_FoWState State);
	FLinearColor OverlayColorForObscuration(float Obscuration);

	void ResetField(FGP_FoWPresentationRaster& Field);
	void ConfigureField(
		FGP_FoWPresentationRaster& Field,
		int32 CellMinX,
		int32 CellMinY,
		int32 Width,
		int32 Height,
		float CellSizeCm,
		const FVector2D& GridOriginWorldXY);
	void SetCell(FGP_FoWPresentationRaster& Field, int32 LocalX, int32 LocalY, EGP_FoWState State);

	int32 ChooseSuperSample(int32 Width, int32 Height);

	bool RebuildPresentation(
		FGP_FoWPresentationRaster& Field,
		FGP_FoWPresentationGeometry& OutGeometry);

	float SamplePresentationObscuration(
		const FGP_FoWPresentationRaster& Field,
		const FVector2D& WorldXY);
}
