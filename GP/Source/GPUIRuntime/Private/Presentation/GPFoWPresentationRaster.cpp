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

	FLinearColor OverlayColorForState(EGP_FoWState State)
	{
		return OverlayColorForObscuration(ObscurationForState(State));
	}

	void ResetField(FGP_FoWPresentationRaster& Field)
	{
		Field.Cells.Reset();
		Field.CellMinX = 0;
		Field.CellMinY = 0;
		Field.Width = 0;
		Field.Height = 0;
		Field.CellSizeCm = 0.0f;
		Field.GridOriginWorldXY = FVector2D::ZeroVector;
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

	EGP_FoWState GetCell(const FGP_FoWPresentationRaster& Field, int32 LocalX, int32 LocalY)
	{
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height)
		{
			return EGP_FoWState::Unexplored;
		}
		return Field.Cells[Field.CellIndex(LocalX, LocalY)];
	}

	static bool ShouldFeatherToward(EGP_FoWState Self, EGP_FoWState Neighbor)
	{
		if (Self == EGP_FoWState::Visible || Self == Neighbor)
		{
			return false;
		}
		if (Neighbor == EGP_FoWState::Visible)
		{
			return true;
		}
		return Self == EGP_FoWState::Unexplored && Neighbor == EGP_FoWState::Explored;
	}

	static EGP_FoWState NeighborState(
		const FGP_FoWPresentationRaster& Field,
		int32 LocalX,
		int32 LocalY)
	{
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height)
		{
			return GetCell(Field, FMath::Clamp(LocalX, 0, Field.Width - 1),
				FMath::Clamp(LocalY, 0, Field.Height - 1));
		}
		return GetCell(Field, LocalX, LocalY);
	}

	static bool TryAddQuad(
		FGP_FoWPresentationGeometry& OutGeometry,
		const FVector2D& WorldMin,
		const FVector2D& WorldMax,
		const FLinearColor& C00,
		const FLinearColor& C10,
		const FLinearColor& C11,
		const FLinearColor& C01)
	{
		if (OutGeometry.Quads.Num() >= MaximumOverlayQuads)
		{
			return false;
		}

		FGP_FoWPresentationQuad Quad;
		Quad.WorldMin = WorldMin;
		Quad.WorldMax = WorldMax;
		Quad.CornerColors[0] = C00;
		Quad.CornerColors[1] = C10;
		Quad.CornerColors[2] = C11;
		Quad.CornerColors[3] = C01;
		OutGeometry.Quads.Add(Quad);
		return true;
	}

	bool RebuildPresentation(
		FGP_FoWPresentationRaster& Field,
		FGP_FoWPresentationGeometry& OutGeometry)
	{
		const double StartTime = FPlatformTime::Seconds();
		OutGeometry = FGP_FoWPresentationGeometry();

		if (Field.Width <= 0 || Field.Height <= 0 || Field.CellSizeCm <= KINDA_SMALL_NUMBER
			|| Field.GetCellCount() > MaximumSampledCells
			|| Field.Cells.Num() != Field.GetCellCount())
		{
			return false;
		}

		const float Feather = Field.CellSizeCm * FeatherFraction;
		OutGeometry.FeatherCm = Feather;
		const FLinearColor Transparent(0.0f, 0.0f, 0.0f, 0.0f);

		for (int32 LocalY = 0; LocalY < Field.Height; ++LocalY)
		{
			for (int32 LocalX = 0; LocalX < Field.Width; ++LocalX)
			{
				const EGP_FoWState State = GetCell(Field, LocalX, LocalY);
				if (State == EGP_FoWState::Visible)
				{
					++OutGeometry.VisibleCellsSkipped;
					continue;
				}

				const FLinearColor Solid = OverlayColorForState(State);
				const double MinX =
					Field.GridOriginWorldXY.X
					+ static_cast<double>(Field.CellMinX + LocalX) * Field.CellSizeCm;
				const double MinY =
					Field.GridOriginWorldXY.Y
					+ static_cast<double>(Field.CellMinY + LocalY) * Field.CellSizeCm;
				const double MaxX = MinX + Field.CellSizeCm;
				const double MaxY = MinY + Field.CellSizeCm;

				if (!TryAddQuad(
						OutGeometry,
						FVector2D(MinX, MinY),
						FVector2D(MaxX, MaxY),
						Solid,
						Solid,
						Solid,
						Solid))
				{
					break;
				}
				++OutGeometry.CellTiles;

				const bool bFeatherWest = ShouldFeatherToward(
					State, NeighborState(Field, LocalX - 1, LocalY));
				const bool bFeatherEast = ShouldFeatherToward(
					State, NeighborState(Field, LocalX + 1, LocalY));
				const bool bFeatherSouth = ShouldFeatherToward(
					State, NeighborState(Field, LocalX, LocalY - 1));
				const bool bFeatherNorth = ShouldFeatherToward(
					State, NeighborState(Field, LocalX, LocalY + 1));

				if (bFeatherWest
					&& TryAddQuad(
						OutGeometry,
						FVector2D(MinX - Feather, MinY),
						FVector2D(MinX, MaxY),
						Transparent,
						Solid,
						Solid,
						Transparent))
				{
					++OutGeometry.FeatherQuads;
				}
				if (bFeatherEast
					&& TryAddQuad(
						OutGeometry,
						FVector2D(MaxX, MinY),
						FVector2D(MaxX + Feather, MaxY),
						Solid,
						Transparent,
						Transparent,
						Solid))
				{
					++OutGeometry.FeatherQuads;
				}
				if (bFeatherSouth
					&& TryAddQuad(
						OutGeometry,
						FVector2D(MinX, MinY - Feather),
						FVector2D(MaxX, MinY),
						Transparent,
						Transparent,
						Solid,
						Solid))
				{
					++OutGeometry.FeatherQuads;
				}
				if (bFeatherNorth
					&& TryAddQuad(
						OutGeometry,
						FVector2D(MinX, MaxY),
						FVector2D(MaxX, MaxY + Feather),
						Solid,
						Solid,
						Transparent,
						Transparent))
				{
					++OutGeometry.FeatherQuads;
				}

				if (bFeatherWest && bFeatherSouth
					&& TryAddQuad(
						OutGeometry,
						FVector2D(MinX - Feather, MinY - Feather),
						FVector2D(MinX, MinY),
						Transparent,
						Transparent,
						Solid,
						Transparent))
				{
					++OutGeometry.FeatherQuads;
				}
				if (bFeatherEast && bFeatherSouth
					&& TryAddQuad(
						OutGeometry,
						FVector2D(MaxX, MinY - Feather),
						FVector2D(MaxX + Feather, MinY),
						Transparent,
						Transparent,
						Transparent,
						Solid))
				{
					++OutGeometry.FeatherQuads;
				}
				if (bFeatherEast && bFeatherNorth
					&& TryAddQuad(
						OutGeometry,
						FVector2D(MaxX, MaxY),
						FVector2D(MaxX + Feather, MaxY + Feather),
						Solid,
						Transparent,
						Transparent,
						Transparent))
				{
					++OutGeometry.FeatherQuads;
				}
				if (bFeatherWest && bFeatherNorth
					&& TryAddQuad(
						OutGeometry,
						FVector2D(MinX - Feather, MaxY),
						FVector2D(MinX, MaxY + Feather),
						Transparent,
						Solid,
						Transparent,
						Transparent))
				{
					++OutGeometry.FeatherQuads;
				}
			}
		}

		OutGeometry.RebuildMilliseconds = (FPlatformTime::Seconds() - StartTime) * 1000.0;
		return OutGeometry.CellTiles > 0 || OutGeometry.VisibleCellsSkipped == Field.GetCellCount();
	}

	static float SampleQuadObscuration(const FGP_FoWPresentationQuad& Quad, const FVector2D& WorldXY)
	{
		const double Width = Quad.WorldMax.X - Quad.WorldMin.X;
		const double Height = Quad.WorldMax.Y - Quad.WorldMin.Y;
		if (Width <= KINDA_SMALL_NUMBER || Height <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}
		if (WorldXY.X < Quad.WorldMin.X || WorldXY.X > Quad.WorldMax.X
			|| WorldXY.Y < Quad.WorldMin.Y || WorldXY.Y > Quad.WorldMax.Y)
		{
			return 0.0f;
		}

		const float TX = static_cast<float>((WorldXY.X - Quad.WorldMin.X) / Width);
		const float TY = static_cast<float>((WorldXY.Y - Quad.WorldMin.Y) / Height);
		const FLinearColor Bottom = FMath::Lerp(Quad.CornerColors[0], Quad.CornerColors[1], TX);
		const FLinearColor Top = FMath::Lerp(Quad.CornerColors[3], Quad.CornerColors[2], TX);
		return FMath::Lerp(Bottom, Top, TY).A;
	}

	float SamplePresentationObscuration(
		const FGP_FoWPresentationRaster& Field,
		const FGP_FoWPresentationGeometry& Geometry,
		const FVector2D& WorldXY)
	{
		(void)Field;
		float Best = 0.0f;
		for (const FGP_FoWPresentationQuad& Quad : Geometry.Quads)
		{
			Best = FMath::Max(Best, SampleQuadObscuration(Quad, WorldXY));
		}
		return Best;
	}
}
