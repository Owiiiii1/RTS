// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Math/Box2D.h"
#include "GPMinimapWidget.generated.h"

struct FStreamableHandle;
class SGPMinimapSurface;
class UGP_MinimapPresenter;
class UTexture2D;

/**
 * Native UMG minimap surface: static authored background + trusted FoW overlay.
 *
 * Presenter coordinates are unchanged (world +X/+Y → normalized +X/+Y).
 * Widget layer only: ScreenY = 1 - NormalizedY so NormalizedY = 1 is the top of the square.
 * Background and FoW share that transform. Not SceneCapture. No Tick / polling / world scan.
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
	static int32 ClampFoWPresentationResolution(int32 Requested);
	static FBox2D ComputeSharedMapDestLocal(
		const FVector2D& AllottedSize,
		bool bHasTexture,
		const FVector2D& TextureSize);

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
	void RequestBackgroundLoad();
	void StartBackgroundLoad(const FSoftObjectPath& Path);
	void CancelBackgroundLoad();
	void HandleBackgroundLoaded();
	void ApplyResidentBackground(UTexture2D* Texture);
	void RebuildFoWOverlay();
	void EnsureFoWTexture();
	void UploadFoWTexture();
	void PushBrushesToSlate();
	FBox2D ComputeMapDestLocal(const FVector2D& AllottedSize) const;
	int32 ResolveConfiguredFoWResolution() const;
	FSoftObjectPath GetConfiguredBackgroundPath() const;

	TSharedPtr<SGPMinimapSurface> MySurface;

	TWeakObjectPtr<UGP_MinimapPresenter> BoundPresenter;
	FDelegateHandle PresentationChangedHandle;

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
