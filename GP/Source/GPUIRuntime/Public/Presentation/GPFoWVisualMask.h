// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"

class UGP_LocalFoWComponent;

struct FGP_FoWVisualMaskRuntime
{
	int32 Width = 0;
	int32 Height = 0;
	FVector2D OriginWorldXY = FVector2D::ZeroVector;
	FVector2D ExtentWorldXY = FVector2D::ZeroVector;
	float BlendAlpha = 1.0f;
	int64 MaskRevision = -1;
	int64 PreviousRevision = -1;
	int32 BuildCount = 0;
	int32 TargetUploadCount = 0;
	double LastEncodeMilliseconds = 0.0;
	double LastUploadMilliseconds = 0.0;
	bool bReady = false;
};

/**
 * Presentation-only Known/Visible FoW mask helpers.
 *
 * Runtime encoding is a packed 1:1 LocalFoW cell dump. Spatial blur and temporal interpolation
 * happen on the GPU. These helpers never mutate gameplay bits.
 */
namespace GPFoWVisualMask
{
	constexpr int32 CanonicalMaskResolution = 1000;
	constexpr float BlendDurationSeconds = 0.20f;
	constexpr float ExploredDimFactor = 0.35f;
	constexpr float BlurRadiusTexels = 1.0f;
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
		return TEXT("GPUBilinear9Tap");
	}

	inline const TCHAR* GetTemporalFilterName()
	{
		return TEXT("GPULerpBlendAlpha");
	}

	inline const TCHAR* GetMaterialAssetPath()
	{
		return TEXT("/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess.M_GP_FoW_PostProcess");
	}

	inline const TCHAR* GetWorldPositionMethodName()
	{
		return TEXT("SceneDepthSvPositionReconstruct");
	}

	inline bool UsesCpuSpatialBlur()
	{
		return false;
	}

	inline bool UsesCpuTemporalLerp()
	{
		return false;
	}

	inline bool UsesWorldLocationQueriesForEncode()
	{
		return false;
	}

	inline int32 BytesPerPackedTexel()
	{
		return 4;
	}

	float ObscurationForState(EGP_FoWState State);
	FLinearColor ComposeSceneColor(
		const FLinearColor& SceneColor,
		float Known,
		float Visible,
		bool bReady);
	FLinearColor ComposeSceneColorFromPacked(const FLinearColor& SceneColor, const FColor& Packed, bool bReady);

	FVector2D WorldXYToUV(
		const FVector2D& WorldXY,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY);
	FVector2D UVToWorldXY(
		const FVector2D& UV,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY);
	bool IsUVInBounds(const FVector2D& UV);

	FColor PackCell(bool bKnown, bool bVisible);
	void EncodePackedFromStates(
		TArray<FColor>& OutPixels,
		int32& OutWidth,
		int32& OutHeight,
		const TArray<EGP_FoWState>& Cells,
		int32 GridWidth,
		int32 GridHeight,
		float CellSizeCm,
		const FVector2D& OriginWorldXY);
	bool EncodePackedFromLocalFoW(
		TArray<FColor>& OutPixels,
		int32& OutWidth,
		int32& OutHeight,
		FVector2D& OutOriginWorldXY,
		FVector2D& OutExtentWorldXY,
		const UGP_LocalFoWComponent* Mirror);

	void SamplePackedBilinear(
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height,
		const FVector2D& WorldXY,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY,
		float& OutKnown,
		float& OutVisible);
	void SamplePacked9Tap(
		const TArray<FColor>& Pixels,
		int32 Width,
		int32 Height,
		const FVector2D& WorldXY,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY,
		float BlurRadiusTexelsIn,
		float& OutKnown,
		float& OutVisible);

	void ResetRuntime(FGP_FoWVisualMaskRuntime& Runtime);
	void BeginNewTarget(
		FGP_FoWVisualMaskRuntime& Runtime,
		int32 Width,
		int32 Height,
		const FVector2D& OriginWorldXY,
		const FVector2D& ExtentWorldXY,
		int64 Revision);
	void AdvanceBlend(FGP_FoWVisualMaskRuntime& Runtime, float DeltaSeconds);
}
