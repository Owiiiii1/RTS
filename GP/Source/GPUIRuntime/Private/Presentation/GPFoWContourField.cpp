// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPFoWContourField.h"

#include "Algo/Reverse.h"
#include "HAL/PlatformTime.h"
#include "Templates/Function.h"

namespace GPFoWContourField
{
	static constexpr float EdtInfinity = 1.0e12f;

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
		Field.VisibleSDF.Reset();
		Field.KnownSDF.Reset();
		Field.CellMinX = 0;
		Field.CellMinY = 0;
		Field.Width = 0;
		Field.Height = 0;
		Field.SdfWidth = 0;
		Field.SdfHeight = 0;
		Field.CellSizeCm = 0.0f;
		Field.SdfPixelSizeCm = 0.0f;
		Field.GridOriginWorldXY = FVector2D::ZeroVector;
		Field.SdfOriginWorldXY = FVector2D::ZeroVector;
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
		ResetField(Field);
		Field.CellMinX = CellMinX;
		Field.CellMinY = CellMinY;
		Field.Width = FMath::Max(Width, 0);
		Field.Height = FMath::Max(Height, 0);
		Field.CellSizeCm = CellSizeCm;
		Field.GridOriginWorldXY = GridOriginWorldXY;
		Field.Cells.Init(EGP_FoWState::Unexplored, Field.GetCellCount());
	}

	void SetCell(FGP_FoWContourField& Field, int32 LocalX, int32 LocalY, EGP_FoWState State)
	{
		if (LocalX < 0 || LocalY < 0 || LocalX >= Field.Width || LocalY >= Field.Height)
		{
			return;
		}
		Field.Cells[Field.CellIndex(LocalX, LocalY)] = State;
	}

	static void DistanceTransform1D(const TArray<float>& FunctionValues, TArray<float>& Out, int32 Count)
	{
		Out.SetNumUninitialized(Count);
		if (Count <= 0)
		{
			return;
		}
		if (Count == 1)
		{
			Out[0] = FunctionValues[0];
			return;
		}

		TArray<int32> Vertex;
		TArray<float> Intersection;
		Vertex.SetNumUninitialized(Count);
		Intersection.SetNumUninitialized(Count + 1);
		int32 K = 0;
		Vertex[0] = 0;
		Intersection[0] = -EdtInfinity;
		Intersection[1] = EdtInfinity;

		for (int32 Q = 1; Q < Count; ++Q)
		{
			float S = 0.0f;
			while (K >= 0)
			{
				const int32 P = Vertex[K];
				const float Denom = 2.0f * static_cast<float>(Q - P);
				if (FMath::Abs(Denom) <= KINDA_SMALL_NUMBER)
				{
					S = EdtInfinity;
					break;
				}
				S = ((FunctionValues[Q] + static_cast<float>(Q) * static_cast<float>(Q))
					- (FunctionValues[P] + static_cast<float>(P) * static_cast<float>(P)))
					/ Denom;
				if (S > Intersection[K])
				{
					break;
				}
				--K;
			}

			++K;
			Vertex[K] = Q;
			Intersection[K] = S;
			Intersection[K + 1] = EdtInfinity;
		}

		K = 0;
		for (int32 Q = 0; Q < Count; ++Q)
		{
			while (Intersection[K + 1] < static_cast<float>(Q))
			{
				++K;
			}
			const int32 P = Vertex[K];
			const float Delta = static_cast<float>(Q - P);
			Out[Q] = Delta * Delta + FunctionValues[P];
		}
	}

	static void ComputeUnsignedDistance(
		const TArray<uint8>& Inside,
		int32 Width,
		int32 Height,
		TArray<float>& OutPixels)
	{
		TArray<float> Row;
		TArray<float> RowOut;
		TArray<float> Column;
		TArray<float> Transformed;
		Row.SetNum(Width);
		Column.SetNum(Height);
		OutPixels.SetNum(Width * Height);

		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				Row[X] = Inside[Y * Width + X] ? 0.0f : EdtInfinity;
			}
			DistanceTransform1D(Row, RowOut, Width);
			for (int32 X = 0; X < Width; ++X)
			{
				OutPixels[Y * Width + X] = RowOut[X];
			}
		}

		for (int32 X = 0; X < Width; ++X)
		{
			for (int32 Y = 0; Y < Height; ++Y)
			{
				Column[Y] = OutPixels[Y * Width + X];
			}
			DistanceTransform1D(Column, Transformed, Height);
			for (int32 Y = 0; Y < Height; ++Y)
			{
				OutPixels[Y * Width + X] = Transformed[Y];
			}
		}

		for (float& Value : OutPixels)
		{
			Value = FMath::Sqrt(FMath::Min(Value, EdtInfinity));
		}
	}

	static void ComputeSignedDistance(
		const TArray<uint8>& Inside,
		int32 Width,
		int32 Height,
		float PixelSizeCm,
		TArray<float>& OutSdf)
	{
		TArray<uint8> Outside;
		Outside.SetNum(Inside.Num());
		for (int32 Index = 0; Index < Inside.Num(); ++Index)
		{
			Outside[Index] = Inside[Index] ? 0 : 1;
		}

		TArray<float> DistInside;
		TArray<float> DistOutside;
		ComputeUnsignedDistance(Inside, Width, Height, DistInside);
		ComputeUnsignedDistance(Outside, Width, Height, DistOutside);

		OutSdf.SetNum(Inside.Num());
		for (int32 Index = 0; Index < Inside.Num(); ++Index)
		{
			OutSdf[Index] = (DistInside[Index] - DistOutside[Index]) * PixelSizeCm;
		}
	}

	float SampleSignedDistance(
		const TArray<float>& SDF,
		int32 SdfWidth,
		int32 SdfHeight,
		const FVector2D& OriginWorldXY,
		float PixelSizeCm,
		const FVector2D& WorldXY)
	{
		if (SdfWidth <= 0 || SdfHeight <= 0 || PixelSizeCm <= KINDA_SMALL_NUMBER
			|| SDF.Num() != SdfWidth * SdfHeight)
		{
			return 1.0e6f;
		}

		const double Fx = (WorldXY.X - OriginWorldXY.X) / PixelSizeCm - 0.5;
		const double Fy = (WorldXY.Y - OriginWorldXY.Y) / PixelSizeCm - 0.5;
		int32 X0 = FMath::FloorToInt(static_cast<float>(Fx));
		int32 Y0 = FMath::FloorToInt(static_cast<float>(Fy));
		const float TX = static_cast<float>(Fx - X0);
		const float TY = static_cast<float>(Fy - Y0);
		X0 = FMath::Clamp(X0, 0, SdfWidth - 1);
		Y0 = FMath::Clamp(Y0, 0, SdfHeight - 1);
		const int32 X1 = FMath::Min(X0 + 1, SdfWidth - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, SdfHeight - 1);
		const float V00 = SDF[Y0 * SdfWidth + X0];
		const float V10 = SDF[Y0 * SdfWidth + X1];
		const float V01 = SDF[Y1 * SdfWidth + X0];
		const float V11 = SDF[Y1 * SdfWidth + X1];
		return FMath::Lerp(FMath::Lerp(V00, V10, TX), FMath::Lerp(V01, V11, TX), TY);
	}

	float SamplePresentationObscuration(
		const FGP_FoWContourField& Field,
		const FVector2D& WorldXY)
	{
		if (Field.SdfWidth <= 0 || Field.KnownSDF.Num() != Field.GetSdfCount())
		{
			return UnexploredObscuration;
		}

		const float VisibleSdf = SampleSignedDistance(
			Field.VisibleSDF,
			Field.SdfWidth,
			Field.SdfHeight,
			Field.SdfOriginWorldXY,
			Field.SdfPixelSizeCm,
			WorldXY);
		const float KnownSdf = SampleSignedDistance(
			Field.KnownSDF,
			Field.SdfWidth,
			Field.SdfHeight,
			Field.SdfOriginWorldXY,
			Field.SdfPixelSizeCm,
			WorldXY);
		const float VisibleIso = -VisibleInwardBiasCells * Field.CellSizeCm;
		const float KnownIso = -KnownInwardBiasCells * Field.CellSizeCm;
		if (VisibleSdf <= VisibleIso)
		{
			return 0.0f;
		}
		if (KnownSdf <= KnownIso)
		{
			return ExploredObscuration;
		}
		return UnexploredObscuration;
	}

	static FVector2D InterpolateIso(
		const FVector2D& A,
		float SdfA,
		const FVector2D& B,
		float SdfB,
		float Iso)
	{
		const float Denom = SdfB - SdfA;
		const float T = FMath::Abs(Denom) <= KINDA_SMALL_NUMBER
			? 0.5f
			: FMath::Clamp((Iso - SdfA) / Denom, 0.0f, 1.0f);
		return FMath::Lerp(A, B, T);
	}

	static FVector2D SdfPixelWorld(
		const FGP_FoWContourField& Field,
		int32 SdfX,
		int32 SdfY)
	{
		return FVector2D(
			Field.SdfOriginWorldXY.X + (static_cast<double>(SdfX) + 0.5) * Field.SdfPixelSizeCm,
			Field.SdfOriginWorldXY.Y + (static_cast<double>(SdfY) + 0.5) * Field.SdfPixelSizeCm);
	}

	static void CollectIsoSegments(
		const FGP_FoWContourField& Field,
		const TArray<float>& SDF,
		float Iso,
		TArray<FGP_FoWContourSegment>& OutSegments)
	{
		OutSegments.Reset();
		if (Field.SdfWidth < 2 || Field.SdfHeight < 2)
		{
			return;
		}

		auto AddSeg = [&](const FVector2D& A, const FVector2D& B)
		{
			if (OutSegments.Num() >= MaximumIsoSegments
				|| FVector2D::DistSquared(A, B) <= KINDA_SMALL_NUMBER)
			{
				return;
			}
			FGP_FoWContourSegment Segment;
			Segment.Start = A;
			Segment.End = B;
			OutSegments.Add(Segment);
		};

		for (int32 Y = 0; Y + 1 < Field.SdfHeight && OutSegments.Num() < MaximumIsoSegments; ++Y)
		{
			for (int32 X = 0; X + 1 < Field.SdfWidth && OutSegments.Num() < MaximumIsoSegments; ++X)
			{
				const float SW = SDF[Field.SdfIndex(X, Y)];
				const float SE = SDF[Field.SdfIndex(X + 1, Y)];
				const float NE = SDF[Field.SdfIndex(X + 1, Y + 1)];
				const float NW = SDF[Field.SdfIndex(X, Y + 1)];
				const int32 Mask =
					(SW <= Iso ? 1 : 0)
					| (SE <= Iso ? 2 : 0)
					| (NE <= Iso ? 4 : 0)
					| (NW <= Iso ? 8 : 0);
				if (Mask == 0 || Mask == 15)
				{
					continue;
				}

				const FVector2D PSW = SdfPixelWorld(Field, X, Y);
				const FVector2D PSE = SdfPixelWorld(Field, X + 1, Y);
				const FVector2D PNE = SdfPixelWorld(Field, X + 1, Y + 1);
				const FVector2D PNW = SdfPixelWorld(Field, X, Y + 1);
				const FVector2D Bottom = InterpolateIso(PSW, SW, PSE, SE, Iso);
				const FVector2D Right = InterpolateIso(PSE, SE, PNE, NE, Iso);
				const FVector2D Top = InterpolateIso(PNE, NE, PNW, NW, Iso);
				const FVector2D Left = InterpolateIso(PNW, NW, PSW, SW, Iso);

				switch (Mask)
				{
				case 1:
				case 14:
					AddSeg(Bottom, Left);
					break;
				case 2:
				case 13:
					AddSeg(Right, Bottom);
					break;
				case 3:
				case 12:
					AddSeg(Right, Left);
					break;
				case 4:
				case 11:
					AddSeg(Top, Right);
					break;
				case 6:
				case 9:
					AddSeg(Top, Bottom);
					break;
				case 7:
				case 8:
					AddSeg(Top, Left);
					break;
				case 5:
					AddSeg(Bottom, Left);
					AddSeg(Top, Right);
					break;
				case 10:
					AddSeg(Right, Bottom);
					AddSeg(Left, Top);
					break;
				default:
					break;
				}
			}
		}
	}

	static uint64 QuantizePoint(const FVector2D& Point)
	{
		const int32 X = FMath::RoundToInt(Point.X * 4.0);
		const int32 Y = FMath::RoundToInt(Point.Y * 4.0);
		return (static_cast<uint64>(static_cast<uint32>(X)) << 32)
			| static_cast<uint32>(Y);
	}

	static void StitchLoops(
		const TArray<FGP_FoWContourSegment>& Segments,
		TArray<FGP_FoWContourLoop>& OutLoops)
	{
		OutLoops.Reset();
		TMultiMap<uint64, int32> Adjacency;
		TArray<uint8> Used;
		Used.Init(0, Segments.Num());
		for (int32 Index = 0; Index < Segments.Num(); ++Index)
		{
			Adjacency.Add(QuantizePoint(Segments[Index].Start), Index);
			Adjacency.Add(QuantizePoint(Segments[Index].End), Index);
		}

		for (int32 StartIndex = 0; StartIndex < Segments.Num(); ++StartIndex)
		{
			if (Used[StartIndex])
			{
				continue;
			}

			FGP_FoWContourLoop Loop;
			FVector2D Target = Segments[StartIndex].End;
			Used[StartIndex] = 1;
			Loop.Points.Add(Segments[StartIndex].Start);
			bool bClosed = false;

			for (int32 Guard = 0; Guard < Segments.Num() + 2; ++Guard)
			{
				Loop.Points.Add(Target);
				if (QuantizePoint(Target) == QuantizePoint(Loop.Points[0]) && Loop.Points.Num() > 3)
				{
					Loop.Points.Pop();
					bClosed = true;
					break;
				}

				const uint64 Key = QuantizePoint(Target);
				TArray<int32> Candidates;
				Adjacency.MultiFind(Key, Candidates);
				int32 Next = INDEX_NONE;
				FVector2D NextPoint = Target;
				for (const int32 Candidate : Candidates)
				{
					if (Used[Candidate])
					{
						continue;
					}
					const bool bAtStart = QuantizePoint(Segments[Candidate].Start) == Key;
					Next = Candidate;
					NextPoint = bAtStart ? Segments[Candidate].End : Segments[Candidate].Start;
					break;
				}
				if (Next == INDEX_NONE)
				{
					break;
				}
				Used[Next] = 1;
				Target = NextPoint;
			}

			if (!bClosed
				&& Loop.Points.Num() >= 4
				&& FVector2D::DistSquared(Loop.Points[0], Loop.Points.Last()) <= 4.0f)
			{
				Loop.Points.Pop();
				bClosed = true;
			}
			if (bClosed && Loop.Points.Num() >= 3)
			{
				OutLoops.Add(MoveTemp(Loop));
			}
		}
	}

	static float SignedArea(const TArray<FVector2D>& Points)
	{
		double Area = 0.0;
		const int32 Count = Points.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D& A = Points[Index];
			const FVector2D& B = Points[(Index + 1) % Count];
			Area += static_cast<double>(A.X) * B.Y - static_cast<double>(B.X) * A.Y;
		}
		return static_cast<float>(0.5 * Area);
	}

	static void EnsureWinding(TArray<FVector2D>& Points, bool bCounterClockwise)
	{
		const bool bAlreadyCCW = SignedArea(Points) > 0.0f;
		if (bAlreadyCCW != bCounterClockwise)
		{
			Algo::Reverse(Points);
		}
	}

	static void SmoothChaikinConservative(
		FGP_FoWContourLoop& Loop,
		const TArray<float>& SDF,
		int32 SdfWidth,
		int32 SdfHeight,
		const FVector2D& OriginWorldXY,
		float PixelSizeCm,
		float Iso)
	{
		for (int32 Iteration = 0; Iteration < ChaikinIterations; ++Iteration)
		{
			if (Loop.Points.Num() < 3
				|| Loop.Points.Num() * 2 > MaximumContourVertices)
			{
				break;
			}

			TArray<FVector2D> Next;
			Next.Reserve(Loop.Points.Num() * 2);
			const int32 Count = Loop.Points.Num();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const FVector2D& A = Loop.Points[Index];
				const FVector2D& B = Loop.Points[(Index + 1) % Count];
				Next.Add(A * 0.75f + B * 0.25f);
				Next.Add(A * 0.25f + B * 0.75f);
			}

			for (FVector2D& Point : Next)
			{
				const float Sdf = SampleSignedDistance(
					SDF, SdfWidth, SdfHeight, OriginWorldXY, PixelSizeCm, Point);
				if (Sdf <= Iso)
				{
					continue;
				}

				const float Epsilon = FMath::Max(PixelSizeCm, 1.0f);
				const float Dx = SampleSignedDistance(
					SDF, SdfWidth, SdfHeight, OriginWorldXY, PixelSizeCm,
					Point + FVector2D(Epsilon, 0.0f))
					- SampleSignedDistance(
						SDF, SdfWidth, SdfHeight, OriginWorldXY, PixelSizeCm,
						Point - FVector2D(Epsilon, 0.0f));
				const float Dy = SampleSignedDistance(
					SDF, SdfWidth, SdfHeight, OriginWorldXY, PixelSizeCm,
					Point + FVector2D(0.0f, Epsilon))
					- SampleSignedDistance(
						SDF, SdfWidth, SdfHeight, OriginWorldXY, PixelSizeCm,
						Point - FVector2D(0.0f, Epsilon));
				FVector2D Gradient(Dx, Dy);
				if (!Gradient.Normalize())
				{
					continue;
				}
				Point -= Gradient * (Sdf - Iso + 0.5f);
			}

			Loop.Points = MoveTemp(Next);
		}

		EnsureWinding(Loop.Points, true);
	}

	static float Cross2D(const FVector2D& O, const FVector2D& A, const FVector2D& B)
	{
		return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X);
	}

	static bool PointInTriangle(
		const FVector2D& P,
		const FVector2D& A,
		const FVector2D& B,
		const FVector2D& C)
	{
		const float C1 = Cross2D(A, B, P);
		const float C2 = Cross2D(B, C, P);
		const float C3 = Cross2D(C, A, P);
		const bool bHasNeg = (C1 < 0.0f) || (C2 < 0.0f) || (C3 < 0.0f);
		const bool bHasPos = (C1 > 0.0f) || (C2 > 0.0f) || (C3 > 0.0f);
		return !(bHasNeg && bHasPos);
	}

	static bool SegmentsIntersectProper(
		const FVector2D& A,
		const FVector2D& B,
		const FVector2D& C,
		const FVector2D& D)
	{
		const float D1 = Cross2D(C, D, A);
		const float D2 = Cross2D(C, D, B);
		const float D3 = Cross2D(A, B, C);
		const float D4 = Cross2D(A, B, D);
		return ((D1 > 0.0f && D2 < 0.0f) || (D1 < 0.0f && D2 > 0.0f))
			&& ((D3 > 0.0f && D4 < 0.0f) || (D3 < 0.0f && D4 > 0.0f));
	}

	bool LoopHasSelfIntersection(const FGP_FoWContourLoop& Loop)
	{
		const int32 Count = Loop.Points.Num();
		if (Count < 4)
		{
			return false;
		}
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector2D& A = Loop.Points[I];
			const FVector2D& B = Loop.Points[(I + 1) % Count];
			for (int32 J = I + 2; J < Count; ++J)
			{
				if (I == 0 && J == Count - 1)
				{
					continue;
				}
				const FVector2D& C = Loop.Points[J];
				const FVector2D& D = Loop.Points[(J + 1) % Count];
				if (SegmentsIntersectProper(A, B, C, D))
				{
					return true;
				}
			}
		}
		return false;
	}

	int32 CountLongCellStaircaseSteps(const FGP_FoWContourLoop& Loop, float CellSizeCm)
	{
		const int32 Count = Loop.Points.Num();
		if (Count < 4 || CellSizeCm <= KINDA_SMALL_NUMBER)
		{
			return 0;
		}

		int32 MaxRun = 0;
		int32 Run = 0;
		int32 LastAxis = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D Delta = Loop.Points[(Index + 1) % Count] - Loop.Points[Index];
			const float AbsX = FMath::Abs(Delta.X);
			const float AbsY = FMath::Abs(Delta.Y);
			const float Length = Delta.Size();
			const bool bCellLength =
				Length > 0.7f * CellSizeCm && Length < 1.3f * CellSizeCm;
			const int32 Axis = (AbsX > 0.85f * Length) ? 1 : ((AbsY > 0.85f * Length) ? 2 : 0);
			if (bCellLength && Axis != 0 && LastAxis != 0 && Axis != LastAxis)
			{
				++Run;
			}
			else if (bCellLength && Axis != 0 && LastAxis == 0)
			{
				Run = 1;
			}
			else if (bCellLength && Axis != 0 && Axis == LastAxis)
			{
				MaxRun = FMath::Max(MaxRun, Run);
				Run = 1;
			}
			else
			{
				MaxRun = FMath::Max(MaxRun, Run);
				Run = 0;
			}
			LastAxis = Axis;
		}
		return FMath::Max(MaxRun, Run);
	}

	int32 CountTangentBuckets(const FGP_FoWContourLoop& Loop)
	{
		TSet<int32> Buckets;
		const int32 Count = Loop.Points.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D Delta = Loop.Points[(Index + 1) % Count] - Loop.Points[Index];
			if (Delta.SizeSquared() <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const float Angle = FMath::Atan2(Delta.Y, Delta.X);
			Buckets.Add(FMath::RoundToInt(Angle / (PI / 12.0f)));
		}
		return Buckets.Num();
	}

	void ResampleClosedLoopByArcLength(
		const FGP_FoWContourLoop& Loop,
		int32 SampleCount,
		TArray<FVector2D>& OutPoints)
	{
		OutPoints.Reset();
		const int32 Count = Loop.Points.Num();
		if (Count < 3 || SampleCount < 3)
		{
			return;
		}

		TArray<float> Cumulative;
		Cumulative.SetNum(Count + 1);
		Cumulative[0] = 0.0f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Cumulative[Index + 1] = Cumulative[Index]
				+ FVector2D::Distance(Loop.Points[Index], Loop.Points[(Index + 1) % Count]);
		}
		const float Total = Cumulative.Last();
		if (Total <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		OutPoints.Reserve(SampleCount);
		int32 Edge = 0;
		for (int32 Sample = 0; Sample < SampleCount; ++Sample)
		{
			const float Target = Total * static_cast<float>(Sample) / static_cast<float>(SampleCount);
			while (Edge + 1 < Cumulative.Num() && Cumulative[Edge + 1] < Target)
			{
				++Edge;
			}
			const int32 Next = (Edge + 1) % Count;
			const float Span = Cumulative[Edge + 1] - Cumulative[Edge];
			const float T = Span > KINDA_SMALL_NUMBER ? (Target - Cumulative[Edge]) / Span : 0.0f;
			OutPoints.Add(FMath::Lerp(Loop.Points[Edge], Loop.Points[Next], T));
		}
	}

	static bool CanAddTriangles(const FGP_FoWContourGeometry& Geometry, int32 Additional)
	{
		return Geometry.TriangleCount + Additional <= MaximumOverlayTriangles;
	}

	static void AddWorldTriangle(
		FGP_FoWContourGeometry& Geometry,
		const FVector2D& A,
		const FVector2D& B,
		const FVector2D& C,
		float Obscuration)
	{
		if (!CanAddTriangles(Geometry, 1))
		{
			return;
		}
		FGP_FoWContourVertex Vertex;
		Vertex.Obscuration = Obscuration;
		Vertex.WorldXY = A;
		Geometry.WorldTriangles.Add(Vertex);
		Vertex.WorldXY = B;
		Geometry.WorldTriangles.Add(Vertex);
		Vertex.WorldXY = C;
		Geometry.WorldTriangles.Add(Vertex);
		++Geometry.TriangleCount;
	}

	static void EarClipFill(
		const TArray<FVector2D>& Polygon,
		float Obscuration,
		FGP_FoWContourGeometry& Geometry)
	{
		const int32 Count = Polygon.Num();
		if (Count < 3)
		{
			return;
		}

		TArray<int32> Indices;
		Indices.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Indices.Add(Index);
		}

		int32 Guard = 0;
		while (Indices.Num() >= 3 && Guard++ < Count * Count)
		{
			bool bClipped = false;
			const int32 Remaining = Indices.Num();
			for (int32 I = 0; I < Remaining; ++I)
			{
				const int32 Prev = Indices[(I + Remaining - 1) % Remaining];
				const int32 Curr = Indices[I];
				const int32 Next = Indices[(I + 1) % Remaining];
				const FVector2D& A = Polygon[Prev];
				const FVector2D& B = Polygon[Curr];
				const FVector2D& C = Polygon[Next];
				if (Cross2D(A, B, C) <= 0.0f)
				{
					continue;
				}

				bool bContains = false;
				for (int32 J = 0; J < Remaining; ++J)
				{
					const int32 Other = Indices[J];
					if (Other == Prev || Other == Curr || Other == Next)
					{
						continue;
					}
					if (PointInTriangle(Polygon[Other], A, B, C))
					{
						bContains = true;
						break;
					}
				}
				if (bContains)
				{
					continue;
				}

				AddWorldTriangle(Geometry, A, B, C, Obscuration);
				Indices.RemoveAt(I);
				bClipped = true;
				break;
			}
			if (!bClipped)
			{
				break;
			}
		}
	}

	static bool SegmentHitsPolygon(
		const FVector2D& A,
		const FVector2D& B,
		const TArray<FVector2D>& Polygon,
		int32 IgnoreVertex)
	{
		const int32 Count = Polygon.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Next = (Index + 1) % Count;
			if (Index == IgnoreVertex || Next == IgnoreVertex)
			{
				continue;
			}
			if (SegmentsIntersectProper(A, B, Polygon[Index], Polygon[Next]))
			{
				return true;
			}
		}
		return false;
	}

	static bool InsertHole(TArray<FVector2D>& Outer, const TArray<FVector2D>& Hole)
	{
		if (Outer.Num() < 3 || Hole.Num() < 3)
		{
			return false;
		}

		int32 HoleIndex = 0;
		float MaxX = Hole[0].X;
		for (int32 Index = 1; Index < Hole.Num(); ++Index)
		{
			if (Hole[Index].X > MaxX)
			{
				MaxX = Hole[Index].X;
				HoleIndex = Index;
			}
		}
		const FVector2D HoleVertex = Hole[HoleIndex];

		int32 OuterIndex = INDEX_NONE;
		float BestX = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Outer.Num(); ++Index)
		{
			if (Outer[Index].X <= HoleVertex.X)
			{
				continue;
			}
			if (SegmentHitsPolygon(HoleVertex, Outer[Index], Outer, Index)
				|| SegmentHitsPolygon(HoleVertex, Outer[Index], Hole, HoleIndex))
			{
				continue;
			}
			if (Outer[Index].X < BestX)
			{
				BestX = Outer[Index].X;
				OuterIndex = Index;
			}
		}
		if (OuterIndex == INDEX_NONE)
		{
			BestX = -TNumericLimits<float>::Max();
			for (int32 Index = 0; Index < Outer.Num(); ++Index)
			{
				if (Outer[Index].X < BestX)
				{
					continue;
				}
				if (SegmentHitsPolygon(HoleVertex, Outer[Index], Outer, Index))
				{
					continue;
				}
				BestX = Outer[Index].X;
				OuterIndex = Index;
			}
		}
		if (OuterIndex == INDEX_NONE)
		{
			return false;
		}

		TArray<FVector2D> Combined;
		Combined.Reserve(Outer.Num() + Hole.Num() + 2);
		for (int32 Index = 0; Index <= OuterIndex; ++Index)
		{
			Combined.Add(Outer[Index]);
		}
		for (int32 Offset = 0; Offset <= Hole.Num(); ++Offset)
		{
			Combined.Add(Hole[(HoleIndex + Offset) % Hole.Num()]);
		}
		Combined.Add(Outer[OuterIndex]);
		for (int32 Index = OuterIndex + 1; Index < Outer.Num(); ++Index)
		{
			Combined.Add(Outer[Index]);
		}
		Outer = MoveTemp(Combined);
		return true;
	}

	static void FillPolygonWithHoles(
		const FGP_FoWContourLoop& Outer,
		const TArray<FGP_FoWContourLoop>& Holes,
		float Obscuration,
		FGP_FoWContourGeometry& Geometry)
	{
		TArray<FVector2D> Combined = Outer.Points;
		EnsureWinding(Combined, true);
		for (const FGP_FoWContourLoop& Hole : Holes)
		{
			TArray<FVector2D> HolePoints = Hole.Points;
			EnsureWinding(HolePoints, false);
			InsertHole(Combined, HolePoints);
		}
		EarClipFill(Combined, Obscuration, Geometry);
	}

	static bool PointInLoop(const FVector2D& Point, const FGP_FoWContourLoop& Loop)
	{
		bool bInside = false;
		const int32 Count = Loop.Points.Num();
		for (int32 I = 0, J = Count - 1; I < Count; J = I++)
		{
			const FVector2D& A = Loop.Points[I];
			const FVector2D& B = Loop.Points[J];
			if (((A.Y > Point.Y) != (B.Y > Point.Y))
				&& (Point.X < (B.X - A.X) * (Point.Y - A.Y) / ((B.Y - A.Y) + KINDA_SMALL_NUMBER) + A.X))
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	static FVector2D Rotate90(const FVector2D& Value)
	{
		return FVector2D(-Value.Y, Value.X);
	}

	static void EmitRibbon(
		const FGP_FoWContourLoop& Loop,
		float FeatherCm,
		float OuterObscuration,
		float InnerObscuration,
		FGP_FoWContourGeometry& Geometry)
	{
		const int32 Count = Loop.Points.Num();
		if (Count < 3 || FeatherCm <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		TArray<FVector2D> Inner;
		Inner.SetNum(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D Prev = Loop.Points[(Index + Count - 1) % Count];
			const FVector2D Curr = Loop.Points[Index];
			const FVector2D Next = Loop.Points[(Index + 1) % Count];
			FVector2D Normal = Rotate90(Curr - Prev).GetSafeNormal()
				+ Rotate90(Next - Curr).GetSafeNormal();
			if (!Normal.Normalize())
			{
				Normal = Rotate90(Curr - Prev).GetSafeNormal();
			}
			Inner[Index] = Curr + Normal * FeatherCm;
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Next = (Index + 1) % Count;
			if (!CanAddTriangles(Geometry, 2))
			{
				return;
			}
			FGP_FoWContourVertex A;
			A.WorldXY = Loop.Points[Index];
			A.Obscuration = OuterObscuration;
			FGP_FoWContourVertex B;
			B.WorldXY = Loop.Points[Next];
			B.Obscuration = OuterObscuration;
			FGP_FoWContourVertex C;
			C.WorldXY = Inner[Next];
			C.Obscuration = InnerObscuration;
			FGP_FoWContourVertex D;
			D.WorldXY = Inner[Index];
			D.Obscuration = InnerObscuration;
			Geometry.WorldTriangles.Add(A);
			Geometry.WorldTriangles.Add(B);
			Geometry.WorldTriangles.Add(C);
			Geometry.WorldTriangles.Add(A);
			Geometry.WorldTriangles.Add(C);
			Geometry.WorldTriangles.Add(D);
			Geometry.TriangleCount += 2;
		}
	}

	static void LoopsToSegments(
		const TArray<FGP_FoWContourLoop>& Loops,
		TArray<FGP_FoWContourSegment>& OutSegments)
	{
		OutSegments.Reset();
		for (const FGP_FoWContourLoop& Loop : Loops)
		{
			const int32 Count = Loop.Points.Num();
			for (int32 Index = 0; Index < Count && OutSegments.Num() < MaximumIsoSegments; ++Index)
			{
				FGP_FoWContourSegment Segment;
				Segment.Start = Loop.Points[Index];
				Segment.End = Loop.Points[(Index + 1) % Count];
				OutSegments.Add(Segment);
			}
		}
	}

	static void BuildBinaryMask(
		const FGP_FoWContourField& Field,
		TFunctionRef<bool(EGP_FoWState)> IsInside,
		TArray<uint8>& OutMask)
	{
		OutMask.Init(0, Field.GetSdfCount());
		for (int32 LocalY = 0; LocalY < Field.Height; ++LocalY)
		{
			for (int32 LocalX = 0; LocalX < Field.Width; ++LocalX)
			{
				if (!IsInside(Field.Cells[Field.CellIndex(LocalX, LocalY)]))
				{
					continue;
				}
				const int32 SdfX0 = LocalX * SuperSample;
				const int32 SdfY0 = LocalY * SuperSample;
				for (int32 SY = 0; SY < SuperSample; ++SY)
				{
					for (int32 SX = 0; SX < SuperSample; ++SX)
					{
						OutMask[Field.SdfIndex(SdfX0 + SX, SdfY0 + SY)] = 1;
					}
				}
			}
		}
	}

	bool RebuildPresentation(
		FGP_FoWContourField& Field,
		FGP_FoWContourGeometry& OutGeometry)
	{
		const double StartTime = FPlatformTime::Seconds();
		OutGeometry = FGP_FoWContourGeometry();

		if (Field.Width <= 0 || Field.Height <= 0 || Field.CellSizeCm <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		Field.SdfWidth = Field.Width * SuperSample;
		Field.SdfHeight = Field.Height * SuperSample;
		if (Field.GetSdfCount() <= 0 || Field.GetSdfCount() > MaximumSdfPixels)
		{
			return false;
		}

		Field.SdfPixelSizeCm = Field.CellSizeCm / static_cast<float>(SuperSample);
		Field.SdfOriginWorldXY = FVector2D(
			Field.GridOriginWorldXY.X + static_cast<double>(Field.CellMinX) * Field.CellSizeCm,
			Field.GridOriginWorldXY.Y + static_cast<double>(Field.CellMinY) * Field.CellSizeCm);

		TArray<uint8> VisibleMask;
		TArray<uint8> KnownMask;
		BuildBinaryMask(
			Field,
			[](EGP_FoWState State) { return State == EGP_FoWState::Visible; },
			VisibleMask);
		BuildBinaryMask(
			Field,
			[](EGP_FoWState State)
			{
				return State == EGP_FoWState::Visible || State == EGP_FoWState::Explored;
			},
			KnownMask);

		ComputeSignedDistance(
			VisibleMask, Field.SdfWidth, Field.SdfHeight, Field.SdfPixelSizeCm, Field.VisibleSDF);
		ComputeSignedDistance(
			KnownMask, Field.SdfWidth, Field.SdfHeight, Field.SdfPixelSizeCm, Field.KnownSDF);

		OutGeometry.DistanceFieldDims = FIntPoint(Field.SdfWidth, Field.SdfHeight);
		OutGeometry.DistanceFieldBytes =
			(Field.VisibleSDF.Num() + Field.KnownSDF.Num()) * static_cast<int32>(sizeof(float));

		const float VisibleIso = -VisibleInwardBiasCells * Field.CellSizeCm;
		const float KnownIso = -KnownInwardBiasCells * Field.CellSizeCm;

		TArray<FGP_FoWContourSegment> VisibleRaw;
		TArray<FGP_FoWContourSegment> KnownRaw;
		CollectIsoSegments(Field, Field.VisibleSDF, VisibleIso, VisibleRaw);
		CollectIsoSegments(Field, Field.KnownSDF, KnownIso, KnownRaw);

		StitchLoops(VisibleRaw, OutGeometry.VisibleLoops);
		StitchLoops(KnownRaw, OutGeometry.KnownLoops);

		for (const FGP_FoWContourLoop& Loop : OutGeometry.VisibleLoops)
		{
			OutGeometry.ContourRawVertices += Loop.Points.Num();
		}
		for (const FGP_FoWContourLoop& Loop : OutGeometry.KnownLoops)
		{
			OutGeometry.ContourRawVertices += Loop.Points.Num();
		}

		for (FGP_FoWContourLoop& Loop : OutGeometry.VisibleLoops)
		{
			SmoothChaikinConservative(
				Loop,
				Field.VisibleSDF,
				Field.SdfWidth,
				Field.SdfHeight,
				Field.SdfOriginWorldXY,
				Field.SdfPixelSizeCm,
				VisibleIso);
		}
		for (FGP_FoWContourLoop& Loop : OutGeometry.KnownLoops)
		{
			SmoothChaikinConservative(
				Loop,
				Field.KnownSDF,
				Field.SdfWidth,
				Field.SdfHeight,
				Field.SdfOriginWorldXY,
				Field.SdfPixelSizeCm,
				KnownIso);
		}

		for (const FGP_FoWContourLoop& Loop : OutGeometry.VisibleLoops)
		{
			OutGeometry.ContourSmoothedVertices += Loop.Points.Num();
		}
		for (const FGP_FoWContourLoop& Loop : OutGeometry.KnownLoops)
		{
			OutGeometry.ContourSmoothedVertices += Loop.Points.Num();
		}

		LoopsToSegments(OutGeometry.VisibleLoops, OutGeometry.VisibleIsoSegments);
		LoopsToSegments(OutGeometry.KnownLoops, OutGeometry.KnownIsoSegments);

		const FVector2D SampleMin = Field.SdfOriginWorldXY;
		const FVector2D SampleMax = FVector2D(
			Field.SdfOriginWorldXY.X + Field.SdfWidth * Field.SdfPixelSizeCm,
			Field.SdfOriginWorldXY.Y + Field.SdfHeight * Field.SdfPixelSizeCm);
		FGP_FoWContourLoop SampleRect;
		SampleRect.Points = {
			SampleMin,
			FVector2D(SampleMax.X, SampleMin.Y),
			SampleMax,
			FVector2D(SampleMin.X, SampleMax.Y)
		};

		bool bKnownCoversSample = true;
		for (const float Value : Field.KnownSDF)
		{
			if (Value > KnownIso)
			{
				bKnownCoversSample = false;
				break;
			}
		}

		if (bKnownCoversSample)
		{
			FillPolygonWithHoles(
				SampleRect, OutGeometry.VisibleLoops, ExploredObscuration, OutGeometry);
		}
		else
		{
			FillPolygonWithHoles(
				SampleRect, OutGeometry.KnownLoops, UnexploredObscuration, OutGeometry);
			for (const FGP_FoWContourLoop& KnownLoop : OutGeometry.KnownLoops)
			{
				TArray<FGP_FoWContourLoop> Holes;
				for (const FGP_FoWContourLoop& VisibleLoop : OutGeometry.VisibleLoops)
				{
					if (VisibleLoop.Points.Num() < 3)
					{
						continue;
					}
					const FVector2D Probe =
						(VisibleLoop.Points[0] + VisibleLoop.Points[1] + VisibleLoop.Points[2])
						/ 3.0;
					if (PointInLoop(Probe, KnownLoop))
					{
						Holes.Add(VisibleLoop);
					}
				}
				FillPolygonWithHoles(KnownLoop, Holes, ExploredObscuration, OutGeometry);
			}
		}

		for (const FGP_FoWContourLoop& Loop : OutGeometry.KnownLoops)
		{
			EmitRibbon(
				Loop,
				EdgeFeatherCm,
				UnexploredObscuration,
				ExploredObscuration,
				OutGeometry);
		}
		for (const FGP_FoWContourLoop& Loop : OutGeometry.VisibleLoops)
		{
			EmitRibbon(
				Loop,
				EdgeFeatherCm,
				ExploredObscuration,
				0.0f,
				OutGeometry);
		}

		OutGeometry.MaskRebuildMilliseconds = (FPlatformTime::Seconds() - StartTime) * 1000.0;
		return true;
	}
}
