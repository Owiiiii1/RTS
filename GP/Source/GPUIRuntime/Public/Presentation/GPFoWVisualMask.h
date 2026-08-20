// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"

class UGP_LocalFoWComponent;

struct FGP_FoWVisualMaskBuffers
{
	TArray<float> Known;
	TArray<float> Visible;
	int32 Width = 0;
	int32 Height = 0;
	FVector2D OriginWorldXY = FVector2D::ZeroVector;
	FVector2D ExtentWorldXY = FVector2D::ZeroVector;

	int32 GetCount() const { return Width * Height; }

	int32 Index(int32 X, int32 Y) const
	{
		return Y * Width + X;
	}
};

struct FGP_FoWVisualMaskRuntime
{
	FGP_FoWVisualMaskBuffers Previous;
	FGP_FoWVisualMaskBuffers Target;
	float BlendAlpha = 1.0f;
	int64 MaskRevision = -1;
	int64 PreviousRevision = -1;
	int32 BuildCount = 0;
	double LastBuildMilliseconds = 0.0;
	double LastUploadMilliseconds = 0.0;
	bool bReady = false;
};

/**
 * Presentation-only Known/Visible FoW mask helpers.
 *
 * Gameplay LocalFoW bits are never mutated. Texture resolution is independent of gameplay CellSize.
 */
namespace GPFoWVisualMask
{
	constexpr int32 TextureResolution = 1024;
	constexpr int32 SpatialBlurRadius = 1;
	constexpr int32 SpatialBlurPasses = 2;
	constexpr float BlendDurationSeconds = 0.20f;
	constexpr float ExploredDimFactor = 0.35f;
	constexpr float UnexploredObscuration = 1.0f;
	constexpr float ExploredObscuration = 1.0f - ExploredDimFactor;
	constexpr float VisibleObscuration = 0.0f;

	inline const TCHAR* GetRendererName()
	{
		return TEXT("PostProcessTextureMask");
	}

	inline const TCHAR* GetMaskModelName()
	{
		return TEXT("Known+Visible");
	}

	inline const TCHAR* GetSpatialFilterName()
	{
		return TEXT("SeparableBox");
	}

	inline const TCHAR* GetMaterialAssetPath()
	{
		return TEXT("/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess.M_GP_FoW_PostProcess");
	}

	float ObscurationForState(EGP_FoWState State);
	FLinearColor ComposeSceneColor(
		const FLinearColor& SceneColor,
		float Known,
		float Visible,
		bool bReady);

	FVector2D WorldXYToUV(
		const FVector2D& WorldXY,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY);
	FVector2D UVToWorldXY(
		const FVector2D& UV,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY);
	bool IsUVInBounds(const FVector2D& UV);

	void ResetBuffers(
		FGP_FoWVisualMaskBuffers& Buffers,
		int32 Width,
		int32 Height,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY);
	void EncodeFromStates(
		FGP_FoWVisualMaskBuffers& OutBuffers,
		const TArray<EGP_FoWState>& Cells,
		int32 GridWidth,
		int32 GridHeight,
		float CellSizeCm,
		const FVector2D& OriginWorldXY,
		int32 TextureWidth,
		int32 TextureHeight);
	void EncodeFromLocalFoW(
		FGP_FoWVisualMaskBuffers& OutBuffers,
		const UGP_LocalFoWComponent* Mirror,
		int32 TextureWidth,
		int32 TextureHeight);
	void ApplySpatialFilter(FGP_FoWVisualMaskBuffers& Buffers);
	void SampleBilinear(
		const FGP_FoWVisualMaskBuffers& Buffers,
		const FVector2D& WorldXY,
		float& OutKnown,
		float& OutVisible);
	void LerpBuffers(
		const FGP_FoWVisualMaskBuffers& From,
		const FGP_FoWVisualMaskBuffers& To,
		float Alpha,
		FGP_FoWVisualMaskBuffers& Out);
	void PackRGBA(const FGP_FoWVisualMaskBuffers& Buffers, TArray<FColor>& OutPixels);

	void ResetRuntime(FGP_FoWVisualMaskRuntime& Runtime);
	void BeginNewTarget(FGP_FoWVisualMaskRuntime& Runtime, FGP_FoWVisualMaskBuffers&& NewTarget, int64 Revision);
	void AdvanceBlend(FGP_FoWVisualMaskRuntime& Runtime, float DeltaSeconds);
	void SampleVisual(
		const FGP_FoWVisualMaskRuntime& Runtime,
		const FVector2D& WorldXY,
		float& OutKnown,
		float& OutVisible);
}
