// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Presentation/GPFoWVisualMask.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "GPFoWWorldPresentationSubsystem.generated.h"

class APlayerController;
class FSceneViewExtensionBase;
class UGP_LocalFoWComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;

/**
 * Local-player owner for source-only world/terrain Fog of War presentation.
 *
 * Builds a Known/Visible runtime mask texture from exactly one trusted UGP_LocalFoWComponent and
 * injects a per-view post-process blendable. It never computes visibility and has no gameplay
 * mutation path.
 */
UCLASS()
class GPUIRUNTIME_API UGP_FoWWorldPresentationSubsystem
	: public ULocalPlayerSubsystem
	, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override;

	static float GetObscurationForState(EGP_FoWState State);
	static FLinearColor ComposeVisualSceneColor(
		const FLinearColor& SceneColor,
		float Known,
		float Visible,
		bool bReady);
	static bool RequiresConservativeFullObscuration(const UGP_LocalFoWComponent* Mirror);
	static bool IsOldSlateRendererActive() { return false; }

	void SetVisualizationEnabled(bool bEnabled);
	bool IsVisualizationEnabled() const { return bVisualizationEnabled; }
	bool IsRendererActive() const;
	UGP_LocalFoWComponent* GetBoundMirror() const { return BoundMirror.Get(); }
	int64 GetLastUpdateRevision() const { return LastUpdateRevision; }

	static const TCHAR* GetRendererName();
	static const TCHAR* GetMaskModelName();
	static const TCHAR* GetSpatialFilterName();
	static const TCHAR* GetMaterialAssetPath();
	static int32 GetMaskTextureResolution();
	static float GetBlendDurationSeconds();
	static int32 GetSpatialBlurRadius();
	static int32 GetSpatialBlurPasses();
	static float GetExploredDimFactor();
	static int32 GetMaskBytesPerTexture();

	UTexture2D* GetPreviousMaskTexture() const { return PreviousMaskTexture; }
	UTexture2D* GetTargetMaskTexture() const { return TargetMaskTexture; }
	UMaterialInstanceDynamic* GetPostProcessMID() const { return PostProcessMID; }
	UMaterialInterface* GetLoadedMaterialTemplate() const { return TemplateMaterial; }
	float GetBlendAlpha() const { return MaskRuntime.BlendAlpha; }
	int64 GetMaskRevision() const { return MaskRuntime.MaskRevision; }
	int64 GetPreviousMaskRevision() const { return MaskRuntime.PreviousRevision; }
	int32 GetMaskBuildCount() const { return MaskRuntime.BuildCount; }
	double GetLastMaskBuildMilliseconds() const { return MaskRuntime.LastBuildMilliseconds; }
	double GetLastMaskUploadMilliseconds() const { return MaskRuntime.LastUploadMilliseconds; }
	bool IsPostProcessBound() const;
	bool IsMaskReady() const { return MaskRuntime.bReady; }
	const FGP_FoWVisualMaskRuntime& GetMaskRuntime() const { return MaskRuntime; }

	void DebugAdvanceBlend(float DeltaSeconds);

#if !UE_BUILD_SHIPPING
	void DebugDumpToLog() const;
#endif

private:
	void BindToPlayerController(APlayerController* NewPlayerController);
	void UnbindMirror();
	void EnsureMaskResources();
	void ReleaseMaskResources();
	void HandleLocalFoWUpdated(UGP_LocalFoWComponent* UpdatedMirror);
	void RebuildMaskFromMirror(const UGP_LocalFoWComponent* Mirror);
	void ApplyConservativeBlackMask();
	void UploadMaskTextures();
	void UpdateMaterialParameters();
	UTexture2D* CreateMaskTexture(const TCHAR* Name) const;
	void UploadTexture(UTexture2D* Texture, const TArray<FColor>& Pixels);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PreviousMaskTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TargetMaskTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> TemplateMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PostProcessMID;

	TWeakObjectPtr<UGP_LocalFoWComponent> BoundMirror;
	FDelegateHandle MirrorUpdatedHandle;
	TSharedPtr<FSceneViewExtensionBase, ESPMode::ThreadSafe> ViewExtension;

	FGP_FoWVisualMaskRuntime MaskRuntime;
	TArray<FColor> PackedPreviousPixels;
	TArray<FColor> PackedTargetPixels;

	bool bVisualizationEnabled = true;
	int64 LastUpdateRevision = -1;
};
