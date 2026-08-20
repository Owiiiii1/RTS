// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Templates/Function.h"

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

struct FGP_FoWContourField
{
	TArray<EGP_FoWState> Cells;
	TArray<float> CenterObscuration;
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

	FVector2D CenterWorldXY(int32 LocalX, int32 LocalY) const
	{
		return FVector2D(
			GridOriginWorldXY.X + (static_cast<double>(CellMinX + LocalX) + 0.5) * CellSizeCm,
			GridOriginWorldXY.Y + (static_cast<double>(CellMinY + LocalY) + 0.5) * CellSizeCm);
	}
};

struct FGP_FoWContourGeometry
{
	TArray<FGP_FoWContourVertex> TriangleVertices;
	TArray<FGP_FoWContourSegment> VisibleIsoSegments;
	TArray<FGP_FoWContourSegment> UnexploredIsoSegments;
	int32 TriangleCount = 0;
	int32 CoalescedQuadCount = 0;
	int32 MixedCellCount = 0;
};

/**
 * Viewport-local continuous FoW presentation field.
 *
 * Discrete LocalFoW cells remain the only input. Scalar samples live at cell centers. Dual marching
 * squares interpolates between neighboring centers so diagonal/circular masks form smooth contours.
 * ConservativeBoundaryT < 0.5 keeps every iso on the clearer side of a dual edge.
 */
namespace GPFoWContourField
{
	constexpr float ExploredObscuration = 0.68f;
	constexpr float UnexploredObscuration = 1.0f;
	/** Fraction from the clearer cell center toward the darker neighbor. Must stay below 0.5. */
	constexpr float ConservativeBoundaryT = 0.42f;
	/** Mixed dual quads are marched at this density so bilinear isos become curved. */
	constexpr int32 SubcellsPerCell = 4;
	constexpr int32 SamplePadCells = 1;
	constexpr int32 MaximumOverlayTriangles = 65536;
	constexpr int32 MaximumIsoSegments = 32768;

	inline const TCHAR* GetAlgorithmName()
	{
		return TEXT("ConservativeDualMarchingSquares");
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
	void BuildCenterSamples(FGP_FoWContourField& Field);

	float SampleBilinearObscuration(const FGP_FoWContourField& Field, const FVector2D& WorldXY);

	void GenerateOverlayGeometry(
		const FGP_FoWContourField& Field,
		FGP_FoWContourGeometry& OutGeometry);

	void ExtractIsoSegments(
		const FGP_FoWContourField& Field,
		TFunctionRef<bool(float Obscuration)> IsInside,
		TArray<FGP_FoWContourSegment>& OutSegments);
}
