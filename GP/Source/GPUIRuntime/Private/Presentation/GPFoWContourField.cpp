// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPFoWContourField.h"

namespace GPFoWContourField
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

	void ResetField(FGP_FoWContourField& Field)
	{
		Field.Cells.Reset();
		Field.CenterObscuration.Reset();
		Field.CellMinX = 0;
		Field.CellMinY = 0;
		Field.Width = 0;
		Field.Height = 0;
		Field.CellSizeCm = 0.0f;
		Field.GridOriginWorldXY = FVector2D::ZeroVector;
	}

	void ConfigureField(
		FGP_FoWContourField& Field,
		int32 CellMinX,
		int32 CellMinY,
		int32 Width,
		int32 Height,
		float CellSizeCm,
		const FVector2D& GridOriginWorldXY)
	{
		Field.CellMinX = CellMinX;
		Field.CellMinY = CellMinY;
		Field.Width = FMath::Max(Width, 0);
		Field.Height = FMath::Max(Height, 0);
		Field.CellSizeCm = CellSizeCm;
		Field.GridOriginWorldXY = GridOriginWorldXY;
		Field.Cells.Init(EGP_FoWState::Unexplored, Field.GetCellCount());
		Field.CenterObscuration.Init(UnexploredObscuration, Field.GetCellCount());
	}

	void SetCell(FGP_FoWContourField& Field, int32 LocalX, int32 LocalY, EGP_FoWState State)
	{
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height)
		{
			return;
		}
		Field.Cells[Field.CellIndex(LocalX, LocalY)] = State;
	}

	void BuildCenterSamples(FGP_FoWContourField& Field)
	{
		Field.CenterObscuration.SetNum(Field.GetCellCount());
		for (int32 Index = 0; Index < Field.Cells.Num(); ++Index)
		{
			Field.CenterObscuration[Index] = ObscurationForState(Field.Cells[Index]);
		}
	}

	float SampleBilinearObscuration(const FGP_FoWContourField& Field, const FVector2D& WorldXY)
	{
		if (Field.Width <= 0 || Field.Height <= 0 || Field.CellSizeCm <= KINDA_SMALL_NUMBER
			|| Field.CenterObscuration.Num() != Field.GetCellCount())
		{
			return UnexploredObscuration;
		}

		if (Field.Width == 1 && Field.Height == 1)
		{
			return Field.CenterObscuration[0];
		}

		const double LocalX =
			(WorldXY.X - Field.GridOriginWorldXY.X) / Field.CellSizeCm - Field.CellMinX;
		const double LocalY =
			(WorldXY.Y - Field.GridOriginWorldXY.Y) / Field.CellSizeCm - Field.CellMinY;
		double SampleX = LocalX - 0.5;
		double SampleY = LocalY - 0.5;
		int32 CX = FMath::FloorToInt(static_cast<float>(SampleX));
		int32 CY = FMath::FloorToInt(static_cast<float>(SampleY));
		float TX = static_cast<float>(SampleX - CX);
		float TY = static_cast<float>(SampleY - CY);

		if (Field.Width < 2)
		{
			CX = 0;
			TX = 0.0f;
		}
		else if (CX < 0)
		{
			CX = 0;
			TX = 0.0f;
		}
		else if (CX > Field.Width - 2)
		{
			CX = Field.Width - 2;
			TX = 1.0f;
		}

		if (Field.Height < 2)
		{
			CY = 0;
			TY = 0.0f;
		}
		else if (CY < 0)
		{
			CY = 0;
			TY = 0.0f;
		}
		else if (CY > Field.Height - 2)
		{
			CY = Field.Height - 2;
			TY = 1.0f;
		}

		const float V00 = Field.CenterObscuration[Field.CellIndex(CX, CY)];
		const float V10 = Field.CenterObscuration[Field.CellIndex(
			FMath::Min(CX + 1, Field.Width - 1), CY)];
		const float V01 = Field.CenterObscuration[Field.CellIndex(
			CX, FMath::Min(CY + 1, Field.Height - 1))];
		const float V11 = Field.CenterObscuration[Field.CellIndex(
			FMath::Min(CX + 1, Field.Width - 1),
			FMath::Min(CY + 1, Field.Height - 1))];
		const float V0 = FMath::Lerp(V00, V10, TX);
		const float V1 = FMath::Lerp(V01, V11, TX);
		return FMath::Lerp(V0, V1, TY);
	}

	static FGP_FoWContourVertex MakeVertex(const FVector2D& WorldXY, float Obscuration)
	{
		FGP_FoWContourVertex Vertex;
		Vertex.WorldXY = WorldXY;
		Vertex.Obscuration = Obscuration;
		return Vertex;
	}

	static FGP_FoWContourVertex InterpolateBoundary(
		const FVector2D& ClearXY,
		float ClearObscuration,
		const FVector2D& DarkXY,
		float DarkObscuration)
	{
		return MakeVertex(
			FMath::Lerp(ClearXY, DarkXY, ConservativeBoundaryT),
			FMath::Lerp(ClearObscuration, DarkObscuration, ConservativeBoundaryT));
	}

	static bool CanAddTriangles(const FGP_FoWContourGeometry& Geometry, int32 AdditionalTriangles)
	{
		return Geometry.TriangleCount + AdditionalTriangles <= MaximumOverlayTriangles;
	}

	static void EmitFan(
		const TArray<FGP_FoWContourVertex, TInlineAllocator<6>>& Polygon,
		FGP_FoWContourGeometry& OutGeometry)
	{
		if (Polygon.Num() < 3)
		{
			return;
		}

		const int32 Additional = Polygon.Num() - 2;
		if (!CanAddTriangles(OutGeometry, Additional))
		{
			return;
		}

		for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
		{
			OutGeometry.TriangleVertices.Add(Polygon[0]);
			OutGeometry.TriangleVertices.Add(Polygon[Index]);
			OutGeometry.TriangleVertices.Add(Polygon[Index + 1]);
			++OutGeometry.TriangleCount;
		}
	}

	static void EmitQuad(
		const FGP_FoWContourVertex& SW,
		const FGP_FoWContourVertex& SE,
		const FGP_FoWContourVertex& NE,
		const FGP_FoWContourVertex& NW,
		FGP_FoWContourGeometry& OutGeometry)
	{
		if (!CanAddTriangles(OutGeometry, 2))
		{
			return;
		}

		OutGeometry.TriangleVertices.Add(SW);
		OutGeometry.TriangleVertices.Add(SE);
		OutGeometry.TriangleVertices.Add(NE);
		OutGeometry.TriangleVertices.Add(SW);
		OutGeometry.TriangleVertices.Add(NE);
		OutGeometry.TriangleVertices.Add(NW);
		OutGeometry.TriangleCount += 2;
	}

	static void AddIsoSegment(
		TArray<FGP_FoWContourSegment>& OutSegments,
		const FGP_FoWContourVertex& A,
		const FGP_FoWContourVertex& B)
	{
		if (OutSegments.Num() >= MaximumIsoSegments)
		{
			return;
		}

		FGP_FoWContourSegment Segment;
		Segment.Start = A.WorldXY;
		Segment.End = B.WorldXY;
		OutSegments.Add(Segment);
	}

	static int32 ClassifyMask(
		float SW,
		float SE,
		float NE,
		float NW,
		TFunctionRef<bool(float Obscuration)> IsInside)
	{
		int32 Mask = 0;
		if (IsInside(SW))
		{
			Mask |= 1;
		}
		if (IsInside(SE))
		{
			Mask |= 2;
		}
		if (IsInside(NE))
		{
			Mask |= 4;
		}
		if (IsInside(NW))
		{
			Mask |= 8;
		}
		return Mask;
	}

	static FGP_FoWContourVertex PickBoundary(
		const FGP_FoWContourVertex& A,
		bool bAInside,
		const FGP_FoWContourVertex& B,
		bool bBInside)
	{
		if (bAInside == bBInside)
		{
			return A;
		}
		return bAInside
			? InterpolateBoundary(B.WorldXY, B.Obscuration, A.WorldXY, A.Obscuration)
			: InterpolateBoundary(A.WorldXY, A.Obscuration, B.WorldXY, B.Obscuration);
	}

	static FGP_FoWContourVertex BilerpCorners(
		const FGP_FoWContourVertex& SW,
		const FGP_FoWContourVertex& SE,
		const FGP_FoWContourVertex& NE,
		const FGP_FoWContourVertex& NW,
		float U,
		float V)
	{
		const FVector2D Bottom = FMath::Lerp(SW.WorldXY, SE.WorldXY, U);
		const FVector2D Top = FMath::Lerp(NW.WorldXY, NE.WorldXY, U);
		const float ObBottom = FMath::Lerp(SW.Obscuration, SE.Obscuration, U);
		const float ObTop = FMath::Lerp(NW.Obscuration, NE.Obscuration, U);
		return MakeVertex(FMath::Lerp(Bottom, Top, V), FMath::Lerp(ObBottom, ObTop, V));
	}

	static void MarchCell(
		const FGP_FoWContourVertex& SW,
		const FGP_FoWContourVertex& SE,
		const FGP_FoWContourVertex& NE,
		const FGP_FoWContourVertex& NW,
		TFunctionRef<bool(float Obscuration)> IsInside,
		TArray<FGP_FoWContourSegment>* IsoSegments,
		FGP_FoWContourGeometry* Geometry)
	{
		const bool bSW = IsInside(SW.Obscuration);
		const bool bSE = IsInside(SE.Obscuration);
		const bool bNE = IsInside(NE.Obscuration);
		const bool bNW = IsInside(NW.Obscuration);
		const int32 Mask = (bSW ? 1 : 0) | (bSE ? 2 : 0) | (bNE ? 4 : 0) | (bNW ? 8 : 0);
		if (Mask == 0)
		{
			return;
		}

		const FGP_FoWContourVertex Bottom = PickBoundary(SW, bSW, SE, bSE);
		const FGP_FoWContourVertex Right = PickBoundary(SE, bSE, NE, bNE);
		const FGP_FoWContourVertex Top = PickBoundary(NE, bNE, NW, bNW);
		const FGP_FoWContourVertex Left = PickBoundary(NW, bNW, SW, bSW);

		TArray<FGP_FoWContourVertex, TInlineAllocator<6>> Polygon;
		auto AddIso = [&](const FGP_FoWContourVertex& A, const FGP_FoWContourVertex& B)
		{
			if (IsoSegments != nullptr)
			{
				AddIsoSegment(*IsoSegments, A, B);
			}
		};

		switch (Mask)
		{
		case 1:
			Polygon.Add(SW);
			Polygon.Add(Bottom);
			Polygon.Add(Left);
			AddIso(Bottom, Left);
			break;
		case 2:
			Polygon.Add(SE);
			Polygon.Add(Right);
			Polygon.Add(Bottom);
			AddIso(Right, Bottom);
			break;
		case 3:
			Polygon.Add(SW);
			Polygon.Add(SE);
			Polygon.Add(Right);
			Polygon.Add(Left);
			AddIso(Right, Left);
			break;
		case 4:
			Polygon.Add(NE);
			Polygon.Add(Top);
			Polygon.Add(Right);
			AddIso(Top, Right);
			break;
		case 6:
			Polygon.Add(SE);
			Polygon.Add(NE);
			Polygon.Add(Top);
			Polygon.Add(Bottom);
			AddIso(Top, Bottom);
			break;
		case 7:
			Polygon.Add(SW);
			Polygon.Add(SE);
			Polygon.Add(NE);
			Polygon.Add(Top);
			Polygon.Add(Left);
			AddIso(Top, Left);
			break;
		case 8:
			Polygon.Add(NW);
			Polygon.Add(Left);
			Polygon.Add(Top);
			AddIso(Left, Top);
			break;
		case 9:
			Polygon.Add(SW);
			Polygon.Add(Bottom);
			Polygon.Add(Top);
			Polygon.Add(NW);
			AddIso(Bottom, Top);
			break;
		case 11:
			Polygon.Add(SW);
			Polygon.Add(SE);
			Polygon.Add(Right);
			Polygon.Add(Top);
			Polygon.Add(NW);
			AddIso(Right, Top);
			break;
		case 12:
			Polygon.Add(NE);
			Polygon.Add(NW);
			Polygon.Add(Left);
			Polygon.Add(Right);
			AddIso(Left, Right);
			break;
		case 13:
			Polygon.Add(SW);
			Polygon.Add(Bottom);
			Polygon.Add(Right);
			Polygon.Add(NE);
			Polygon.Add(NW);
			AddIso(Bottom, Right);
			break;
		case 14:
			Polygon.Add(SE);
			Polygon.Add(NE);
			Polygon.Add(NW);
			Polygon.Add(Left);
			Polygon.Add(Bottom);
			AddIso(Left, Bottom);
			break;
		case 15:
			Polygon.Add(SW);
			Polygon.Add(SE);
			Polygon.Add(NE);
			Polygon.Add(NW);
			break;
		case 5:
			Polygon.Add(SW);
			Polygon.Add(Bottom);
			Polygon.Add(Left);
			AddIso(Bottom, Left);
			if (Geometry != nullptr)
			{
				EmitFan(Polygon, *Geometry);
			}
			Polygon.Reset();
			Polygon.Add(NE);
			Polygon.Add(Top);
			Polygon.Add(Right);
			AddIso(Top, Right);
			break;
		case 10:
			Polygon.Add(SE);
			Polygon.Add(Right);
			Polygon.Add(Bottom);
			AddIso(Right, Bottom);
			if (Geometry != nullptr)
			{
				EmitFan(Polygon, *Geometry);
			}
			Polygon.Reset();
			Polygon.Add(NW);
			Polygon.Add(Left);
			Polygon.Add(Top);
			AddIso(Left, Top);
			break;
		default:
			break;
		}

		if (Geometry != nullptr)
		{
			EmitFan(Polygon, *Geometry);
		}
	}

	static void MarchSubdividedCell(
		const FGP_FoWContourVertex& SW,
		const FGP_FoWContourVertex& SE,
		const FGP_FoWContourVertex& NE,
		const FGP_FoWContourVertex& NW,
		TFunctionRef<bool(float Obscuration)> IsInside,
		TArray<FGP_FoWContourSegment>* IsoSegments,
		FGP_FoWContourGeometry* Geometry)
	{
		const float Step = 1.0f / static_cast<float>(SubcellsPerCell);
		for (int32 SubY = 0; SubY < SubcellsPerCell; ++SubY)
		{
			const float V0 = static_cast<float>(SubY) * Step;
			const float V1 = static_cast<float>(SubY + 1) * Step;
			for (int32 SubX = 0; SubX < SubcellsPerCell; ++SubX)
			{
				const float U0 = static_cast<float>(SubX) * Step;
				const float U1 = static_cast<float>(SubX + 1) * Step;
				MarchCell(
					BilerpCorners(SW, SE, NE, NW, U0, V0),
					BilerpCorners(SW, SE, NE, NW, U1, V0),
					BilerpCorners(SW, SE, NE, NW, U1, V1),
					BilerpCorners(SW, SE, NE, NW, U0, V1),
					IsInside,
					IsoSegments,
					Geometry);
			}
		}
	}

	static bool CornersUniform(
		float SW,
		float SE,
		float NE,
		float NW,
		float& OutValue)
	{
		OutValue = SW;
		return FMath::IsNearlyEqual(SW, SE, 0.001f)
			&& FMath::IsNearlyEqual(SE, NE, 0.001f)
			&& FMath::IsNearlyEqual(NE, NW, 0.001f);
	}

	static void GenerateBand(
		const FGP_FoWContourField& Field,
		TFunctionRef<bool(float Obscuration)> IsInside,
		TArray<FGP_FoWContourSegment>* IsoSegments,
		bool bSkipUniformUnexplored,
		FGP_FoWContourGeometry& OutGeometry)
	{
		for (int32 LocalY = 0; LocalY + 1 < Field.Height; ++LocalY)
		{
			int32 RunStart = INDEX_NONE;
			float RunObscuration = 0.0f;

			auto FlushRun = [&](int32 EndExclusive)
			{
				if (RunStart == INDEX_NONE || EndExclusive <= RunStart)
				{
					RunStart = INDEX_NONE;
					return;
				}

				const FGP_FoWContourVertex SW = MakeVertex(
					Field.CenterWorldXY(RunStart, LocalY),
					RunObscuration);
				const FGP_FoWContourVertex SE = MakeVertex(
					Field.CenterWorldXY(EndExclusive, LocalY),
					RunObscuration);
				const FGP_FoWContourVertex NE = MakeVertex(
					Field.CenterWorldXY(EndExclusive, LocalY + 1),
					RunObscuration);
				const FGP_FoWContourVertex NW = MakeVertex(
					Field.CenterWorldXY(RunStart, LocalY + 1),
					RunObscuration);
				EmitQuad(SW, SE, NE, NW, OutGeometry);
				++OutGeometry.CoalescedQuadCount;
				RunStart = INDEX_NONE;
			};

			for (int32 LocalX = 0; LocalX + 1 < Field.Width; ++LocalX)
			{
				const float SW = Field.CenterObscuration[Field.CellIndex(LocalX, LocalY)];
				const float SE = Field.CenterObscuration[Field.CellIndex(LocalX + 1, LocalY)];
				const float NE = Field.CenterObscuration[Field.CellIndex(LocalX + 1, LocalY + 1)];
				const float NW = Field.CenterObscuration[Field.CellIndex(LocalX, LocalY + 1)];
				const int32 Mask = ClassifyMask(SW, SE, NE, NW, IsInside);
				float UniformValue = 0.0f;
				const bool bUniformInside = Mask == 15 && CornersUniform(SW, SE, NE, NW, UniformValue);

				if (bUniformInside)
				{
					if (bSkipUniformUnexplored
						&& UniformValue >= UnexploredObscuration - 0.001f)
					{
						FlushRun(LocalX);
						continue;
					}

					if (RunStart == INDEX_NONE)
					{
						RunStart = LocalX;
						RunObscuration = UniformValue;
					}
					else if (!FMath::IsNearlyEqual(RunObscuration, UniformValue, 0.001f))
					{
						FlushRun(LocalX);
						RunStart = LocalX;
						RunObscuration = UniformValue;
					}
					continue;
				}

				FlushRun(LocalX);
				if (Mask == 0)
				{
					continue;
				}

				++OutGeometry.MixedCellCount;
				MarchSubdividedCell(
					MakeVertex(Field.CenterWorldXY(LocalX, LocalY), SW),
					MakeVertex(Field.CenterWorldXY(LocalX + 1, LocalY), SE),
					MakeVertex(Field.CenterWorldXY(LocalX + 1, LocalY + 1), NE),
					MakeVertex(Field.CenterWorldXY(LocalX, LocalY + 1), NW),
					IsInside,
					IsoSegments,
					&OutGeometry);
			}

			FlushRun(Field.Width - 1);
		}
	}

	void GenerateOverlayGeometry(
		const FGP_FoWContourField& Field,
		FGP_FoWContourGeometry& OutGeometry)
	{
		OutGeometry.TriangleVertices.Reset();
		OutGeometry.VisibleIsoSegments.Reset();
		OutGeometry.UnexploredIsoSegments.Reset();
		OutGeometry.TriangleCount = 0;
		OutGeometry.CoalescedQuadCount = 0;
		OutGeometry.MixedCellCount = 0;

		if (Field.Width < 2 || Field.Height < 2
			|| Field.CenterObscuration.Num() != Field.GetCellCount())
		{
			return;
		}

		auto IsExploredOrDarker = [](float Obscuration)
		{
			return Obscuration > KINDA_SMALL_NUMBER;
		};
		auto IsUnexploredCorner = [](float Obscuration)
		{
			return Obscuration >= UnexploredObscuration - 0.001f;
		};

		GenerateBand(
			Field,
			IsExploredOrDarker,
			&OutGeometry.VisibleIsoSegments,
			true,
			OutGeometry);
		GenerateBand(
			Field,
			IsUnexploredCorner,
			&OutGeometry.UnexploredIsoSegments,
			false,
			OutGeometry);
	}

	void ExtractIsoSegments(
		const FGP_FoWContourField& Field,
		TFunctionRef<bool(float Obscuration)> IsInside,
		TArray<FGP_FoWContourSegment>& OutSegments)
	{
		OutSegments.Reset();
		if (Field.Width < 2 || Field.Height < 2
			|| Field.CenterObscuration.Num() != Field.GetCellCount())
		{
			return;
		}

		for (int32 LocalY = 0; LocalY + 1 < Field.Height; ++LocalY)
		{
			for (int32 LocalX = 0; LocalX + 1 < Field.Width; ++LocalX)
			{
				const float SW = Field.CenterObscuration[Field.CellIndex(LocalX, LocalY)];
				const float SE = Field.CenterObscuration[Field.CellIndex(LocalX + 1, LocalY)];
				const float NE = Field.CenterObscuration[Field.CellIndex(LocalX + 1, LocalY + 1)];
				const float NW = Field.CenterObscuration[Field.CellIndex(LocalX, LocalY + 1)];
				MarchSubdividedCell(
					MakeVertex(Field.CenterWorldXY(LocalX, LocalY), SW),
					MakeVertex(Field.CenterWorldXY(LocalX + 1, LocalY), SE),
					MakeVertex(Field.CenterWorldXY(LocalX + 1, LocalY + 1), NE),
					MakeVertex(Field.CenterWorldXY(LocalX, LocalY + 1), NW),
					IsInside,
					&OutSegments,
					nullptr);
			}
		}
	}
}
