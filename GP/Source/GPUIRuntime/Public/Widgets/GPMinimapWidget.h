// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "InputCoreTypes.h"
#include "Math/Box2D.h"
#include "GPMinimapWidget.generated.h"

struct FStreamableHandle;
class SGPMinimapSurface;
class UGP_MinimapPresenter;
class UTexture2D;

enum class EGP_MinimapPointerResult : uint8
{
	Ignored,
	ConsumedNoPan,
	PanRequested
};

/**
 * Native UMG minimap surface: static authored background + trusted FoW overlay.
 *
 * Presenter normalized XY is the displayed camera/playable rect, not the full FoW grid.
 * Presenter mapping stays world +X/+Y → normalized +X/+Y (no flip there).
 * Widget surface transform only: ScreenX = 1 - NormalizedX, ScreenY = 1 - NormalizedY.
 * Background, FoW, blips, and camera footprint share that transform. LMB inside MapDest
 * click-to-pans the camera pawn XY-anchor (zoom/yaw/pitch preserved). Letterbox clicks do
 * not map to world. Blip color is canonical team color; unit vs building differs by size.
 */
UCLASS(meta = (DisplayName = "GP Minimap", ShortTooltip = "Static map image plus FoW overlay"))
class GPUIRUNTIME_API UGP_MinimapWidget : public UWidget
{
	GENERATED_BODY()

public:
	UGP_MinimapWidget(const FObjectInitializer& ObjectInitializer);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

	static FVector2D PresenterNormalizedToSurfaceUV(const FVector2D& Normalized);
	static FVector2D SurfaceUVToPresenterNormalized(const FVector2D& SurfaceUV);
	static FLinearColor ResolveBlipColor(int32 TeamId);
	static float GetUnitBlipHalfExtentPx();
	static float GetBuildingBlipHalfExtentPx();
	static int32 ClampFoWPresentationResolution(int32 Requested);
	static FBox2D ComputeSharedMapDestLocal(
		const FVector2D& AllottedSize,
		bool bHasTexture,
		const FVector2D& TextureSize);
	static bool BuildClosedCameraFootprintOutlinePoints(
		const TArray<FVector2D>& PresenterNormalizedCorners,
		const FBox2D& MapDest,
		TArray<FVector2f>& OutOutlinePoints);
	static bool TryConvertLocalPointToSurfaceUV(
		const FVector2D& LocalPoint,
		const FBox2D& MapDest,
		FVector2D& OutSurfaceUV);
	static EGP_MinimapPointerResult ResolvePointerOnMap(
		const FKey& Button,
		const FVector2D& LocalPoint,
		const FBox2D& MapDest,
		FVector2D& OutSurfaceUV);

#if !UE_BUILD_SHIPPING
	void ContractBindPresenter(UGP_MinimapPresenter* Presenter);
	void ContractUnbindPresenter();
	void ContractRequestBackgroundLoad();
	void ContractRequestBackgroundLoadForPath(const FSoftObjectPath& Path);
	int32 GetBoundPresenterListenerCount() const;
	int32 GetFoWPresentationResolution() const { return FoWResolution; }
	int64 GetConsumedFoWRevision() const { return ConsumedRevision; }
	bool IsUsingFallbackBackground() const;
	bool HasResidentBackgroundTexture() const { return BackgroundTexture != nullptr; }
	bool DebugDidRequestAsyncBackgroundLoad() const { return bDebugRequestedAsyncLoad; }
	bool DebugDidCallLoadSynchronous() const { return bDebugCalledLoadSynchronous; }
	EGP_FoWState GetFoWPresentationSample(int32 SurfaceX, int32 SurfaceY) const;
	int32 GetFoWPresentationSampleCount() const { return FoWSamples.Num(); }
	FBox2D ContractComputeMapDestLocal(const FVector2D& AllottedSize) const;
	int32 GetFriendlyBlipDrawCount() const { return BlipDrawList.Num(); }
	int32 GetBlipDrawCount() const { return BlipDrawList.Num(); }
	int32 ContractGetBlipDrawTeamId(int32 Index) const;
	bool ContractGetBlipDrawIsBuilding(int32 Index) const;
	int32 GetCameraFootprintDrawCount() const { return CameraFootprintDrawCorners.Num(); }
	bool ContractHasCameraFootprintDraw() const { return bHasCameraFootprintDraw; }
	FVector2D ContractGetCameraFootprintDrawCorner(int32 Index) const;
	FVector2D ContractWorldToSurfaceUV(const FVector& WorldLocation) const;
	bool ContractHandleMapLeftClick(const FVector2D& SurfaceUV);
	EGP_MinimapPointerResult ContractResolvePointerOnMap(
		const FKey& Button,
		const FVector2D& LocalPoint,
		const FVector2D& AllottedSize,
		FVector2D& OutSurfaceUV) const;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void BeginDestroy() override;

private:
	ULocalPlayer* ResolveOwningLocalPlayer() const;
	UGP_MinimapPresenter* ResolvePresenter() const;
	void BindPresenter(UGP_MinimapPresenter* Presenter);
	void UnbindPresenter();
	void HandleMinimapPresentationChanged();
	void HandleMinimapBlipsChanged();
	void HandleMinimapCameraFootprintChanged();
	void HandleMapLeftClick(FVector2D SurfaceUV);
	void RequestBackgroundLoad();
	void StartBackgroundLoad(const FSoftObjectPath& Path);
	void CancelBackgroundLoad();
	void HandleBackgroundLoaded();
	void ApplyResidentBackground(UTexture2D* Texture);
	void RebuildFoWOverlay();
	void RebuildBlipDrawCache();
	void RebuildCameraFootprintDrawCache();
	void EnsureFoWTexture();
	void UploadFoWTexture();
	void PushBrushesToSlate();
	FBox2D ComputeMapDestLocal(const FVector2D& AllottedSize) const;
	int32 ResolveConfiguredFoWResolution() const;
	FSoftObjectPath GetConfiguredBackgroundPath() const;

	TSharedPtr<SGPMinimapSurface> MySurface;

	TWeakObjectPtr<UGP_MinimapPresenter> BoundPresenter;
	FDelegateHandle PresentationChangedHandle;
	FDelegateHandle BlipsChangedHandle;
	FDelegateHandle CameraFootprintChangedHandle;

	struct FGPMinimapBlipDraw
	{
		FVector2D PresenterNormalized = FVector2D::ZeroVector;
		int32 TeamId = -1;
		bool bIsBuilding = false;
	};

	TArray<FGPMinimapBlipDraw> BlipDrawList;
	TArray<FVector2D> CameraFootprintDrawCorners;
	bool bHasCameraFootprintDraw = false;

	TSharedPtr<FStreamableHandle> BackgroundLoadHandle;
	FSoftObjectPath PendingBackgroundPath;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BackgroundTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FoWTexture;

	TArray<EGP_FoWState> FoWSamples;
	TArray<FColor> FoWPixels;

	FSlateBrush BackgroundBrush;
	FSlateBrush FoWBrush;

	int32 FoWResolution = 128;
	int64 ConsumedRevision = -1;
	bool bFoWReady = false;
	bool bDebugRequestedAsyncLoad = false;
	bool bDebugCalledLoadSynchronous = false;
};
