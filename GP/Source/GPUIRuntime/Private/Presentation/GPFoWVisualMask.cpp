// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPFoWVisualMask.h"

#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/PlatformTime.h"

namespace GPFoWVisualMask
{
	static void BoxBlurSeparable(TArray<float>& Values, int32 Width, int32 Height, int32 Radius)
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

	static float SampleChannelBilinear(
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

	void ResetBuffers(
		FGP_FoWVisualMaskBuffers& Buffers,
		int32 Width,
		int32 Height,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY)
	{
		Buffers.Width = FMath::Max(Width, 0);
		Buffers.Height = FMath::Max(Height, 0);
		Buffers.OriginWorldXY = OriginWorldXY;
		Buffers.ExtentWorldXY = ExtentWorldXY;
		Buffers.Known.Init(0.0f, Buffers.GetCount());
		Buffers.Visible.Init(0.0f, Buffers.GetCount());
	}

	void EncodeFromStates(
		FGP_FoWVisualMaskBuffers& OutBuffers,
		const TArray<EGP_FoWState>& Cells,
		int32 GridWidth,
		int32 GridHeight,
		float CellSizeCm,
		const FVector2D& OriginWorldXY,
		int32 TextureWidth,
		int32 TextureHeight)
	{
		const FVector2D Extent(
			static_cast<double>(GridWidth) * CellSizeCm,
			static_cast<double>(GridHeight) * CellSizeCm);
		ResetBuffers(OutBuffers, TextureWidth, TextureHeight, OriginWorldXY, Extent);
		if (GridWidth <= 0 || GridHeight <= 0 || CellSizeCm <= KINDA_SMALL_NUMBER
			|| Cells.Num() != GridWidth * GridHeight
			|| OutBuffers.GetCount() <= 0)
		{
			return;
		}

		for (int32 TexelY = 0; TexelY < OutBuffers.Height; ++TexelY)
		{
			for (int32 TexelX = 0; TexelX < OutBuffers.Width; ++TexelX)
			{
				const FVector2D UV(
					(static_cast<double>(TexelX) + 0.5) / static_cast<double>(OutBuffers.Width),
					(static_cast<double>(TexelY) + 0.5) / static_cast<double>(OutBuffers.Height));
				const FVector2D WorldXY = UVToWorldXY(UV, OriginWorldXY, Extent);
				const int32 CellX = FMath::Clamp(
					FMath::FloorToInt((WorldXY.X - OriginWorldXY.X) / CellSizeCm),
					0,
					GridWidth - 1);
				const int32 CellY = FMath::Clamp(
					FMath::FloorToInt((WorldXY.Y - OriginWorldXY.Y) / CellSizeCm),
					0,
					GridHeight - 1);
				const EGP_FoWState State = Cells[CellY * GridWidth + CellX];
				const int32 Index = OutBuffers.Index(TexelX, TexelY);
				OutBuffers.Known[Index] =
					(State == EGP_FoWState::Explored || State == EGP_FoWState::Visible) ? 1.0f : 0.0f;
				OutBuffers.Visible[Index] = State == EGP_FoWState::Visible ? 1.0f : 0.0f;
			}
		}
	}

	void EncodeFromLocalFoW(
		FGP_FoWVisualMaskBuffers& OutBuffers,
		const UGP_LocalFoWComponent* Mirror,
		int32 TextureWidth,
		int32 TextureHeight)
	{
		if (Mirror == nullptr || !Mirror->IsReady())
		{
			ResetBuffers(
				OutBuffers,
				TextureWidth,
				TextureHeight,
				FVector2D::ZeroVector,
				FVector2D::ZeroVector);
			return;
		}

		const FIntPoint GridDimensions = Mirror->GetGridDimensions();
		const float CellSizeCm = Mirror->GetCellSizeCm();
		const FVector2D Origin = Mirror->GetGridOriginWorldXY();
		const FVector2D Extent(
			static_cast<double>(GridDimensions.X) * CellSizeCm,
			static_cast<double>(GridDimensions.Y) * CellSizeCm);
		ResetBuffers(OutBuffers, TextureWidth, TextureHeight, Origin, Extent);
		if (GridDimensions.X <= 0 || GridDimensions.Y <= 0 || CellSizeCm <= KINDA_SMALL_NUMBER
			|| OutBuffers.GetCount() <= 0)
		{
			return;
		}

		for (int32 TexelY = 0; TexelY < OutBuffers.Height; ++TexelY)
		{
			for (int32 TexelX = 0; TexelX < OutBuffers.Width; ++TexelX)
			{
				const FVector2D UV(
					(static_cast<double>(TexelX) + 0.5) / static_cast<double>(OutBuffers.Width),
					(static_cast<double>(TexelY) + 0.5) / static_cast<double>(OutBuffers.Height));
				const FVector2D WorldXY = UVToWorldXY(UV, Origin, Extent);
				const EGP_FoWState State = Mirror->GetStateAtWorldLocation(
					FVector(WorldXY.X, WorldXY.Y, 0.0));
				const int32 Index = OutBuffers.Index(TexelX, TexelY);
				OutBuffers.Known[Index] =
					(State == EGP_FoWState::Explored || State == EGP_FoWState::Visible) ? 1.0f : 0.0f;
				OutBuffers.Visible[Index] = State == EGP_FoWState::Visible ? 1.0f : 0.0f;
			}
		}
	}

	void ApplySpatialFilter(FGP_FoWVisualMaskBuffers& Buffers)
	{
		for (int32 Pass = 0; Pass < SpatialBlurPasses; ++Pass)
		{
			BoxBlurSeparable(Buffers.Known, Buffers.Width, Buffers.Height, SpatialBlurRadius);
			BoxBlurSeparable(Buffers.Visible, Buffers.Width, Buffers.Height, SpatialBlurRadius);
		}
	}

	void SampleBilinear(
		const FGP_FoWVisualMaskBuffers& Buffers,
		const FVector2D& WorldXY,
		float& OutKnown,
		float& OutVisible)
	{
		OutKnown = 0.0f;
		OutVisible = 0.0f;
		const FVector2D UV = WorldXYToUV(WorldXY, Buffers.OriginWorldXY, Buffers.ExtentWorldXY);
		if (!IsUVInBounds(UV) || Buffers.Width <= 0 || Buffers.Height <= 0)
		{
			return;
		}

		const float Fx = static_cast<float>(UV.X * Buffers.Width - 0.5);
		const float Fy = static_cast<float>(UV.Y * Buffers.Height - 0.5);
		OutKnown = SampleChannelBilinear(Buffers.Known, Buffers.Width, Buffers.Height, Fx, Fy);
		OutVisible = SampleChannelBilinear(Buffers.Visible, Buffers.Width, Buffers.Height, Fx, Fy);
	}

	void LerpBuffers(
		const FGP_FoWVisualMaskBuffers& From,
		const FGP_FoWVisualMaskBuffers& To,
		float Alpha,
		FGP_FoWVisualMaskBuffers& Out)
	{
		const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		if (From.Width != To.Width || From.Height != To.Height || From.GetCount() != To.GetCount())
		{
			Out = To;
			return;
		}

		Out = To;
		for (int32 Index = 0; Index < Out.GetCount(); ++Index)
		{
			Out.Known[Index] = FMath::Lerp(From.Known[Index], To.Known[Index], ClampedAlpha);
			Out.Visible[Index] = FMath::Lerp(From.Visible[Index], To.Visible[Index], ClampedAlpha);
		}
	}

	void PackRGBA(const FGP_FoWVisualMaskBuffers& Buffers, TArray<FColor>& OutPixels)
	{
		OutPixels.SetNum(Buffers.GetCount());
		for (int32 Index = 0; Index < Buffers.GetCount(); ++Index)
		{
			const uint8 Known = static_cast<uint8>(FMath::Clamp(Buffers.Known[Index], 0.0f, 1.0f) * 255.0f + 0.5f);
			const uint8 Visible = static_cast<uint8>(FMath::Clamp(Buffers.Visible[Index], 0.0f, 1.0f) * 255.0f + 0.5f);
			OutPixels[Index] = FColor(Known, Visible, 0, 255);
		}
	}

	void ResetRuntime(FGP_FoWVisualMaskRuntime& Runtime)
	{
		Runtime = FGP_FoWVisualMaskRuntime();
	}

	void BeginNewTarget(
		FGP_FoWVisualMaskRuntime& Runtime,
		FGP_FoWVisualMaskBuffers&& NewTarget,
		int64 Revision)
	{
		if (Runtime.Target.GetCount() > 0)
		{
			FGP_FoWVisualMaskBuffers Baked;
			LerpBuffers(Runtime.Previous, Runtime.Target, Runtime.BlendAlpha, Baked);
			Runtime.Previous = MoveTemp(Baked);
		}
		else
		{
			ResetBuffers(
				Runtime.Previous,
				NewTarget.Width,
				NewTarget.Height,
				NewTarget.OriginWorldXY,
				NewTarget.ExtentWorldXY);
		}

		Runtime.PreviousRevision = Runtime.MaskRevision;
		Runtime.Target = MoveTemp(NewTarget);
		Runtime.BlendAlpha = 0.0f;
		Runtime.MaskRevision = Revision;
		Runtime.bReady = true;
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

	void SampleVisual(
		const FGP_FoWVisualMaskRuntime& Runtime,
		const FVector2D& WorldXY,
		float& OutKnown,
		float& OutVisible)
	{
		float PreviousKnown = 0.0f;
		float PreviousVisible = 0.0f;
		float TargetKnown = 0.0f;
		float TargetVisible = 0.0f;
		SampleBilinear(Runtime.Previous, WorldXY, PreviousKnown, PreviousVisible);
		SampleBilinear(Runtime.Target, WorldXY, TargetKnown, TargetVisible);
		const float Alpha = FMath::Clamp(Runtime.BlendAlpha, 0.0f, 1.0f);
		OutKnown = FMath::Lerp(PreviousKnown, TargetKnown, Alpha);
		OutVisible = FMath::Lerp(PreviousVisible, TargetVisible, Alpha);
	}
}
