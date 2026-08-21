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
		Field.Obscurations.Reset();
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
		Field.Obscurations.Init(UnexploredObscuration, Field.GetCellCount());
	}

	void SetCell(FGP_FoWPresentationRaster& Field, int32 LocalX, int32 LocalY, EGP_FoWState State)
	{
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height)
		{
			return;
		}
		Field.Cells[Field.CellIndex(LocalX, LocalY)] = State;
		Field.Obscurations[Field.CellIndex(LocalX, LocalY)] = ObscurationForState(State);
	}

	void SetObscuration(FGP_FoWPresentationRaster& Field, int32 LocalX, int32 LocalY, float Obscuration)
	{
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height
			|| Field.Obscurations.Num() != Field.GetCellCount())
		{
			return;
		}
		Field.Obscurations[Field.CellIndex(LocalX, LocalY)] =
			FMath::Clamp(Obscuration, 0.0f, UnexploredObscuration);
	}

	EGP_FoWState GetCell(const FGP_FoWPresentationRaster& Field, int32 LocalX, int32 LocalY)
	{
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height)
		{
			return EGP_FoWState::Unexplored;
		}
		return Field.Cells[Field.CellIndex(LocalX, LocalY)];
	}

	float GetObscuration(const FGP_FoWPresentationRaster& Field, int32 LocalX, int32 LocalY)
	{
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height)
		{
			return UnexploredObscuration;
		}
		if (Field.Obscurations.Num() != Field.GetCellCount())
		{
			return ObscurationForState(GetCell(Field, LocalX, LocalY));
		}
		return Field.Obscurations[Field.CellIndex(LocalX, LocalY)];
	}

	static bool ShouldFeatherToward(float SelfObscuration, float NeighborObscuration)
	{
		return SelfObscuration > NeighborObscuration + FeatherObscurationGap;
	}

	static float NeighborObscuration(
		const FGP_FoWPresentationRaster& Field,
		int32 LocalX,
		int32 LocalY)
	{
		if (Field.Width <= 0 || Field.Height <= 0)
		{
			return UnexploredObscuration;
		}
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height)
		{
			return GetObscuration(
				Field,
				FMath::Clamp(LocalX, 0, Field.Width - 1),
				FMath::Clamp(LocalY, 0, Field.Height - 1));
		}
		return GetObscuration(Field, LocalX, LocalY);
	}

	static bool TryAddQuad(
		FGP_FoWPresentationGeometry& OutGeometry,
		const FVector2D& C00,
		const FVector2D& C10,
		const FVector2D& C11,
		const FVector2D& C01,
		const FLinearColor& Color00,
		const FLinearColor& Color10,
		const FLinearColor& Color11,
		const FLinearColor& Color01)
	{
		if (OutGeometry.Quads.Num() >= MaximumOverlayQuads)
		{
			return false;
		}

		FGP_FoWPresentationQuad Quad;
		Quad.WorldCorners[0] = C00;
		Quad.WorldCorners[1] = C10;
		Quad.WorldCorners[2] = C11;
		Quad.WorldCorners[3] = C01;
		Quad.CornerColors[0] = Color00;
		Quad.CornerColors[1] = Color10;
		Quad.CornerColors[2] = Color11;
		Quad.CornerColors[3] = Color01;
		OutGeometry.Quads.Add(Quad);
		return true;
	}

	static bool TryAddAxisQuad(
		FGP_FoWPresentationGeometry& OutGeometry,
		const FVector2D& WorldMin,
		const FVector2D& WorldMax,
		const FLinearColor& C00,
		const FLinearColor& C10,
		const FLinearColor& C11,
		const FLinearColor& C01)
	{
		return TryAddQuad(
			OutGeometry,
			FVector2D(WorldMin.X, WorldMin.Y),
			FVector2D(WorldMax.X, WorldMin.Y),
			FVector2D(WorldMax.X, WorldMax.Y),
			FVector2D(WorldMin.X, WorldMax.Y),
			C00,
			C10,
			C11,
			C01);
	}

	static FVector2D CornerArcPoint(
		const FVector2D& Center,
		const FVector2D& Axis0,
		const FVector2D& Axis1,
		float Radius,
		float T)
	{
		const float Angle = T * UE_PI * 0.5f;
		const FVector2D Dir = Axis0 * FMath::Cos(Angle) + Axis1 * FMath::Sin(Angle);
		return Center + Dir * Radius;
	}

	static bool TryAddCornerFan(
		FGP_FoWPresentationGeometry& OutGeometry,
		const FVector2D& Center,
		const FVector2D& Axis0,
		const FVector2D& Axis1,
		float Radius,
		const FLinearColor& Solid,
		const FLinearColor& Transparent)
	{
		if (Radius <= KINDA_SMALL_NUMBER)
		{
			return true;
		}

		for (int32 Ring = 0; Ring < CornerRings; ++Ring)
		{
			const float InnerT = static_cast<float>(Ring) / static_cast<float>(CornerRings);
			const float OuterT = static_cast<float>(Ring + 1) / static_cast<float>(CornerRings);
			const float InnerR = Radius * InnerT;
			const float OuterR = Radius * OuterT;
			const FLinearColor InnerColor = FMath::Lerp(Solid, Transparent, InnerT);
			const FLinearColor OuterColor = FMath::Lerp(Solid, Transparent, OuterT);

			for (int32 Segment = 0; Segment < CornerSegments; ++Segment)
			{
				const float T0 = static_cast<float>(Segment) / static_cast<float>(CornerSegments);
				const float T1 = static_cast<float>(Segment + 1) / static_cast<float>(CornerSegments);
				const FVector2D Inner0 = InnerR <= KINDA_SMALL_NUMBER
					? Center
					: CornerArcPoint(Center, Axis0, Axis1, InnerR, T0);
				const FVector2D Inner1 = InnerR <= KINDA_SMALL_NUMBER
					? Center
					: CornerArcPoint(Center, Axis0, Axis1, InnerR, T1);
				const FVector2D Outer0 = CornerArcPoint(Center, Axis0, Axis1, OuterR, T0);
				const FVector2D Outer1 = CornerArcPoint(Center, Axis0, Axis1, OuterR, T1);
				if (!TryAddQuad(
						OutGeometry,
						Inner0,
						Outer0,
						Outer1,
						Inner1,
						InnerColor,
						OuterColor,
						OuterColor,
						InnerColor))
				{
					return false;
				}
				++OutGeometry.FeatherQuads;
			}
		}
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
			|| Field.Cells.Num() != Field.GetCellCount()
			|| Field.Obscurations.Num() != Field.GetCellCount())
		{
			return false;
		}

		const float Outer = Field.CellSizeCm * FeatherFraction;
		const float Inner = Field.CellSizeCm * InnerFeatherFraction;
		OutGeometry.FeatherCm = Outer + Inner;
		const FLinearColor Transparent(0.0f, 0.0f, 0.0f, 0.0f);
		const FVector2D AxisWest(-1.0f, 0.0f);
		const FVector2D AxisEast(1.0f, 0.0f);
		const FVector2D AxisSouth(0.0f, -1.0f);
		const FVector2D AxisNorth(0.0f, 1.0f);

		for (int32 LocalY = 0; LocalY < Field.Height; ++LocalY)
		{
			for (int32 LocalX = 0; LocalX < Field.Width; ++LocalX)
			{
				const float Obscuration = GetObscuration(Field, LocalX, LocalY);
				if (Obscuration <= KINDA_SMALL_NUMBER)
				{
					++OutGeometry.VisibleCellsSkipped;
					continue;
				}

				const FLinearColor Solid = OverlayColorForObscuration(Obscuration);
				const double MinX =
					Field.GridOriginWorldXY.X
					+ static_cast<double>(Field.CellMinX + LocalX) * Field.CellSizeCm;
				const double MinY =
					Field.GridOriginWorldXY.Y
					+ static_cast<double>(Field.CellMinY + LocalY) * Field.CellSizeCm;
				const double MaxX = MinX + Field.CellSizeCm;
				const double MaxY = MinY + Field.CellSizeCm;

				const bool bFeatherWest = ShouldFeatherToward(
					Obscuration, NeighborObscuration(Field, LocalX - 1, LocalY));
				const bool bFeatherEast = ShouldFeatherToward(
					Obscuration, NeighborObscuration(Field, LocalX + 1, LocalY));
				const bool bFeatherSouth = ShouldFeatherToward(
					Obscuration, NeighborObscuration(Field, LocalX, LocalY - 1));
				const bool bFeatherNorth = ShouldFeatherToward(
					Obscuration, NeighborObscuration(Field, LocalX, LocalY + 1));

				const double CoreMinX = MinX + (bFeatherWest ? Inner : 0.0);
				const double CoreMaxX = MaxX - (bFeatherEast ? Inner : 0.0);
				const double CoreMinY = MinY + (bFeatherSouth ? Inner : 0.0);
				const double CoreMaxY = MaxY - (bFeatherNorth ? Inner : 0.0);
				if (CoreMaxX <= CoreMinX || CoreMaxY <= CoreMinY)
				{
					OutGeometry = FGP_FoWPresentationGeometry();
					return false;
				}

				if (!TryAddAxisQuad(
						OutGeometry,
						FVector2D(CoreMinX, CoreMinY),
						FVector2D(CoreMaxX, CoreMaxY),
						Solid,
						Solid,
						Solid,
						Solid))
				{
					OutGeometry = FGP_FoWPresentationGeometry();
					return false;
				}
				++OutGeometry.CellTiles;

				if (bFeatherWest)
				{
					if (!TryAddAxisQuad(
							OutGeometry,
							FVector2D(MinX - Outer, CoreMinY),
							FVector2D(CoreMinX, CoreMaxY),
							Transparent,
							Solid,
							Solid,
							Transparent))
					{
						OutGeometry = FGP_FoWPresentationGeometry();
						return false;
					}
					++OutGeometry.FeatherQuads;
				}
				if (bFeatherEast)
				{
					if (!TryAddAxisQuad(
							OutGeometry,
							FVector2D(CoreMaxX, CoreMinY),
							FVector2D(MaxX + Outer, CoreMaxY),
							Solid,
							Transparent,
							Transparent,
							Solid))
					{
						OutGeometry = FGP_FoWPresentationGeometry();
						return false;
					}
					++OutGeometry.FeatherQuads;
				}
				if (bFeatherSouth)
				{
					if (!TryAddAxisQuad(
							OutGeometry,
							FVector2D(CoreMinX, MinY - Outer),
							FVector2D(CoreMaxX, CoreMinY),
							Transparent,
							Transparent,
							Solid,
							Solid))
					{
						OutGeometry = FGP_FoWPresentationGeometry();
						return false;
					}
					++OutGeometry.FeatherQuads;
				}
				if (bFeatherNorth)
				{
					if (!TryAddAxisQuad(
							OutGeometry,
							FVector2D(CoreMinX, CoreMaxY),
							FVector2D(CoreMaxX, MaxY + Outer),
							Solid,
							Solid,
							Transparent,
							Transparent))
					{
						OutGeometry = FGP_FoWPresentationGeometry();
						return false;
					}
					++OutGeometry.FeatherQuads;
				}

				const float CornerRadius = Inner + Outer;
				if (bFeatherWest && bFeatherSouth)
				{
					if (!TryAddCornerFan(
							OutGeometry,
							FVector2D(CoreMinX, CoreMinY),
							AxisWest,
							AxisSouth,
							CornerRadius,
							Solid,
							Transparent))
					{
						OutGeometry = FGP_FoWPresentationGeometry();
						return false;
					}
				}
				if (bFeatherEast && bFeatherSouth)
				{
					if (!TryAddCornerFan(
							OutGeometry,
							FVector2D(CoreMaxX, CoreMinY),
							AxisSouth,
							AxisEast,
							CornerRadius,
							Solid,
							Transparent))
					{
						OutGeometry = FGP_FoWPresentationGeometry();
						return false;
					}
				}
				if (bFeatherEast && bFeatherNorth)
				{
					if (!TryAddCornerFan(
							OutGeometry,
							FVector2D(CoreMaxX, CoreMaxY),
							AxisEast,
							AxisNorth,
							CornerRadius,
							Solid,
							Transparent))
					{
						OutGeometry = FGP_FoWPresentationGeometry();
						return false;
					}
				}
				if (bFeatherWest && bFeatherNorth)
				{
					if (!TryAddCornerFan(
							OutGeometry,
							FVector2D(CoreMinX, CoreMaxY),
							AxisNorth,
							AxisWest,
							CornerRadius,
							Solid,
							Transparent))
					{
						OutGeometry = FGP_FoWPresentationGeometry();
						return false;
					}
				}
			}
		}

		OutGeometry.RebuildMilliseconds = (FPlatformTime::Seconds() - StartTime) * 1000.0;
		return OutGeometry.CellTiles > 0 || OutGeometry.VisibleCellsSkipped == Field.GetCellCount();
	}

	static bool TryBarycentric(
		const FVector2D& A,
		const FVector2D& B,
		const FVector2D& C,
		const FVector2D& P,
		float& OutU,
		float& OutV,
		float& OutW)
	{
		const FVector2D V0 = B - A;
		const FVector2D V1 = C - A;
		const FVector2D V2 = P - A;
		const double D00 = FVector2D::DotProduct(V0, V0);
		const double D01 = FVector2D::DotProduct(V0, V1);
		const double D11 = FVector2D::DotProduct(V1, V1);
		const double D20 = FVector2D::DotProduct(V2, V0);
		const double D21 = FVector2D::DotProduct(V2, V1);
		const double Denom = D00 * D11 - D01 * D01;
		if (FMath::Abs(Denom) <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		OutV = static_cast<float>((D11 * D20 - D01 * D21) / Denom);
		OutW = static_cast<float>((D00 * D21 - D01 * D20) / Denom);
		OutU = 1.0f - OutV - OutW;
		return OutU >= -KINDA_SMALL_NUMBER && OutV >= -KINDA_SMALL_NUMBER && OutW >= -KINDA_SMALL_NUMBER;
	}

	static float SampleQuadObscuration(const FGP_FoWPresentationQuad& Quad, const FVector2D& WorldXY)
	{
		const FVector2D& C00 = Quad.WorldCorners[0];
		const FVector2D& C10 = Quad.WorldCorners[1];
		const FVector2D& C11 = Quad.WorldCorners[2];
		const FVector2D& C01 = Quad.WorldCorners[3];
		const bool bAxisAligned =
			FMath::IsNearlyEqual(C00.X, C01.X) && FMath::IsNearlyEqual(C10.X, C11.X)
			&& FMath::IsNearlyEqual(C00.Y, C10.Y) && FMath::IsNearlyEqual(C01.Y, C11.Y);
		if (bAxisAligned)
		{
			const double Width = C10.X - C00.X;
			const double Height = C01.Y - C00.Y;
			if (Width <= KINDA_SMALL_NUMBER || Height <= KINDA_SMALL_NUMBER)
			{
				return 0.0f;
			}
			if (WorldXY.X < C00.X || WorldXY.X > C10.X || WorldXY.Y < C00.Y || WorldXY.Y > C01.Y)
			{
				return 0.0f;
			}

			const float TX = static_cast<float>((WorldXY.X - C00.X) / Width);
			const float TY = static_cast<float>((WorldXY.Y - C00.Y) / Height);
			const FLinearColor Bottom = FMath::Lerp(Quad.CornerColors[0], Quad.CornerColors[1], TX);
			const FLinearColor Top = FMath::Lerp(Quad.CornerColors[3], Quad.CornerColors[2], TX);
			return FMath::Lerp(Bottom, Top, TY).A;
		}

		float U = 0.0f;
		float V = 0.0f;
		float W = 0.0f;
		if (TryBarycentric(C00, C10, C11, WorldXY, U, V, W))
		{
			return U * Quad.CornerColors[0].A + V * Quad.CornerColors[1].A + W * Quad.CornerColors[2].A;
		}
		if (!C01.Equals(C11, KINDA_SMALL_NUMBER)
			&& TryBarycentric(C00, C11, C01, WorldXY, U, V, W))
		{
			return U * Quad.CornerColors[0].A + V * Quad.CornerColors[2].A + W * Quad.CornerColors[3].A;
		}
		return 0.0f;
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
