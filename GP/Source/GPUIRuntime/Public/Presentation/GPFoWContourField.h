// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"

struct FGP_FoWContourVertex
{
	FVector2D WorldXY = FVector2D::ZeroVector;
	float Obscuration = 0.0f;
};

struct FGP_FoWContourSegment
{
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
};

struct FGP_FoWContourLoop
{
	TArray<FVector2D> Points;
};

struct FGP_FoWContourField
{
	TArray<EGP_FoWState> Cells;
	TArray<float> VisibleSDF;
	TArray<float> KnownSDF;
	int32 CellMinX = 0;
	int32 CellMinY = 0;
	int32 Width = 0;
	int32 Height = 0;
	int32 SdfWidth = 0;
	int32 SdfHeight = 0;
	float CellSizeCm = 0.0f;
	float SdfPixelSizeCm = 0.0f;
	FVector2D GridOriginWorldXY = FVector2D::ZeroVector;
	FVector2D SdfOriginWorldXY = FVector2D::ZeroVector;

	int32 GetCellCount() const { return Width * Height; }
	int32 GetSdfCount() const { return SdfWidth * SdfHeight; }

	int32 CellIndex(int32 LocalX, int32 LocalY) const
	{
		return LocalY * Width + LocalX;
	}

	int32 SdfIndex(int32 SdfX, int32 SdfY) const
	{
		return SdfY * SdfWidth + SdfX;
	}

	FVector2D CellCenterWorldXY(int32 LocalX, int32 LocalY) const
	{
		return FVector2D(
			GridOriginWorldXY.X + (static_cast<double>(CellMinX + LocalX) + 0.5) * CellSizeCm,
			GridOriginWorldXY.Y + (static_cast<double>(CellMinY + LocalY) + 0.5) * CellSizeCm);
	}
};

struct FGP_FoWContourGeometry
{
	TArray<FGP_FoWContourVertex> WorldTriangles;
	TArray<FGP_FoWContourSegment> VisibleIsoSegments;
	TArray<FGP_FoWContourSegment> KnownIsoSegments;
	TArray<FGP_FoWContourLoop> VisibleLoops;
	TArray<FGP_FoWContourLoop> KnownLoops;
	int32 TriangleCount = 0;
	int32 ContourRawVertices = 0;
	int32 ContourSmoothedVertices = 0;
	FIntPoint DistanceFieldDims = FIntPoint::ZeroValue;
	int32 DistanceFieldBytes = 0;
	double MaskRebuildMilliseconds = 0.0;
};

/**
 * Viewport-local continuous FoW presentation.
 *
 * LocalFoW cells remain the only input. KnownMask (Explored|Visible) and VisibleMask each get a
 * Euclidean signed distance field. Iso-contours are eroded inward, Chaikin-smoothed, then filled.
 */
namespace GPFoWContourField
{
	constexpr float ExploredObscuration = 0.68f;
	constexpr float UnexploredObscuration = 1.0f;
	/** Inward Visible iso, in cells. Shrinks visual Visible; must stay below 0.5. */
	constexpr float VisibleInwardBiasCells = 0.40f;
	/** Inward Known iso, in cells. Keeps Unexplored majority black. */
	constexpr float KnownInwardBiasCells = 0.35f;
	constexpr float EdgeFeatherCm = 28.0f;
	constexpr int32 SuperSample = 2;
	constexpr int32 ChaikinIterations = 3;
	constexpr int32 SamplePadCells = 6;
	constexpr int32 MaximumOverlayTriangles = 65536;
	constexpr int32 MaximumIsoSegments = 32768;
	constexpr int32 MaximumSdfPixels = 262144;
	constexpr int32 MaximumContourVertices = 32768;

	inline const TCHAR* GetAlgorithmName()
	{
		return TEXT("ConservativeKnownVisibleSDFChaikin");
	}

	inline const TCHAR* GetMaskModelName()
	{
		return TEXT("KnownMask+VisibleMask");
	}

	inline const TCHAR* GetDistanceTransformName()
	{
		return TEXT("FelzenszwalbParabolicEDT");
	}

	float ObscurationForState(EGP_FoWState State);
	FLinearColor OverlayColorForObscuration(float Obscuration);

	void ResetField(FGP_FoWContourField& Field);
	void ConfigureField(
		FGP_FoWContourField& Field,
		int32 CellMinX,
		int32 CellMinY,
		int32 Width,
		int32 Height,
		float CellSizeCm,
		const FVector2D& GridOriginWorldXY);
	void SetCell(FGP_FoWContourField& Field, int32 LocalX, int32 LocalY, EGP_FoWState State);

	bool RebuildPresentation(
		FGP_FoWContourField& Field,
		FGP_FoWContourGeometry& OutGeometry);

	float SamplePresentationObscuration(
		const FGP_FoWContourField& Field,
		const FVector2D& WorldXY);

	float SampleSignedDistance(
		const TArray<float>& SDF,
		int32 SdfWidth,
		int32 SdfHeight,
		const FVector2D& OriginWorldXY,
		float PixelSizeCm,
		const FVector2D& WorldXY);

	void ResampleClosedLoopByArcLength(
		const FGP_FoWContourLoop& Loop,
		int32 SampleCount,
		TArray<FVector2D>& OutPoints);

	bool LoopHasSelfIntersection(const FGP_FoWContourLoop& Loop);
	int32 CountLongCellStaircaseSteps(const FGP_FoWContourLoop& Loop, float CellSizeCm);
	int32 CountTangentBuckets(const FGP_FoWContourLoop& Loop);
}
