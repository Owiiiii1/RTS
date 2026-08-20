// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"

struct FGP_FoWPresentationQuad
{
	FVector2D WorldMin = FVector2D::ZeroVector;
	FVector2D WorldMax = FVector2D::ZeroVector;
	FLinearColor CornerColors[4];
};

struct FGP_FoWPresentationRaster
{
	TArray<EGP_FoWState> Cells;
	int32 CellMinX = 0;
	int32 CellMinY = 0;
	int32 Width = 0;
	int32 Height = 0;
	float CellSizeCm = 0.0f;
	FVector2D GridOriginWorldXY = FVector2D::ZeroVector;

	int32 GetCellCount() const { return Width * Height; }

	int32 CellIndex(int32 LocalX, int32 LocalY) const
	{
		return LocalY * Width + LocalX;
	}
};

struct FGP_FoWPresentationGeometry
{
	TArray<FGP_FoWPresentationQuad> Quads;
	int32 CellTiles = 0;
	int32 VisibleCellsSkipped = 0;
	int32 FeatherQuads = 0;
	float FeatherCm = 0.0f;
	double RebuildMilliseconds = 0.0;
};

/**
 * Viewport-local per-cell FoW tiles.
 *
 * Each non-Visible gameplay cell becomes its own world-space quad plus neighbor-aware feather
 * patches. There is no fullscreen mask, raster upsample, or coalesced strip surface.
 */
namespace GPFoWPresentationRaster
{
	constexpr float ExploredObscuration = 0.68f;
	constexpr float UnexploredObscuration = 1.0f;
	constexpr float FeatherFraction = 0.45f;
	constexpr int32 SamplePadCells = 1;
	constexpr int32 MaximumSampledCells = 16384;
	constexpr int32 MaximumOverlayQuads = 98304;
	constexpr int32 MaximumQuadsPerBatch = 8000;

	inline const TCHAR* GetRendererName()
	{
		return TEXT("PerCellBlurredQuadRenderer");
	}

	inline const TCHAR* GetAlgorithmName()
	{
		return TEXT("PerCellFeatheredQuads");
	}

	inline const TCHAR* GetMaskModelName()
	{
		return TEXT("None");
	}

	inline const TCHAR* GetInterpolationName()
	{
		return TEXT("None");
	}

	inline const TCHAR* GetBlurName()
	{
		return TEXT("PerCellEdgeFeather");
	}

	inline bool IsMaskProjectionActive()
	{
		return false;
	}

	float ObscurationForState(EGP_FoWState State);
	FLinearColor OverlayColorForObscuration(float Obscuration);
	FLinearColor OverlayColorForState(EGP_FoWState State);

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

	EGP_FoWState GetCell(const FGP_FoWPresentationRaster& Field, int32 LocalX, int32 LocalY);

	bool RebuildPresentation(
		FGP_FoWPresentationRaster& Field,
		FGP_FoWPresentationGeometry& OutGeometry);

	float SamplePresentationObscuration(
		const FGP_FoWPresentationRaster& Field,
		const FGP_FoWPresentationGeometry& Geometry,
		const FVector2D& WorldXY);
}
