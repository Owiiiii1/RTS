// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Presentation/GPFoWVisualMask.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "GPFoWWorldPresentationSubsystem.generated.h"

class AActor;
class APlayerController;
class FSceneViewExtensionBase;
class UCameraComponent;
class UGP_LocalFoWComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;
struct FPostProcessSettings;

/**
 * Local-player owner for source-only world/terrain Fog of War presentation.
 *
 * Builds a packed Known/Visible runtime mask from exactly one trusted UGP_LocalFoWComponent and
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
	static const TCHAR* GetTemporalFilterName();
	static const TCHAR* GetMaterialAssetPath();
	static const TCHAR* GetWorldPositionMethodName();
	static int32 GetCanonicalMaskResolution();
	static float GetBlendDurationSeconds();
	static float GetBlurRadiusTexels();
	static float GetExploredDimFactor();
	static bool UsesCpuSpatialBlur();
	static bool UsesCpuTemporalLerp();
	static bool UsesWorldLocationQueriesForEncode();

	int32 GetMaskTextureResolution() const;
	int32 GetMaskTextureWidth() const { return MaskWidth; }
	int32 GetMaskTextureHeight() const { return MaskHeight; }
	int32 GetMaskBytesPerTexture() const;
	int32 GetTargetUploadCount() const { return MaskRuntime.TargetUploadCount; }

	UTexture2D* GetPreviousMaskTexture() const { return PreviousMaskTexture; }
	UTexture2D* GetTargetMaskTexture() const { return TargetMaskTexture; }
	UMaterialInstanceDynamic* GetPostProcessMID() const { return PostProcessMID; }
	UMaterialInterface* GetLoadedMaterialTemplate() const { return TemplateMaterial; }
	float GetBlendAlpha() const { return MaskRuntime.BlendAlpha; }
	int64 GetMaskRevision() const { return MaskRuntime.MaskRevision; }
	int64 GetPreviousMaskRevision() const { return MaskRuntime.PreviousRevision; }
	int32 GetMaskBuildCount() const { return MaskRuntime.BuildCount; }
	double GetLastMaskBuildMilliseconds() const { return MaskRuntime.LastEncodeMilliseconds; }
	double GetLastMaskEncodeMilliseconds() const { return MaskRuntime.LastEncodeMilliseconds; }
	double GetLastMaskUploadMilliseconds() const { return MaskRuntime.LastUploadMilliseconds; }
	bool IsPostProcessBound() const;
	bool IsMaskReady() const { return MaskRuntime.bReady; }
	const FGP_FoWVisualMaskRuntime& GetMaskRuntime() const { return MaskRuntime; }

	bool OwnsLocalGameView(
		int32 ViewPlayerIndex,
		const AActor* ViewActor,
		bool bIsGameView,
		bool bIsSceneCapture,
		bool bIsReflectionCapture) const;
	bool TryInjectOwnedView(
		FPostProcessSettings& Settings,
		int32 ViewPlayerIndex,
		const AActor* ViewActor,
		bool bIsGameView,
		bool bIsSceneCapture,
		bool bIsReflectionCapture,
		uint64 FrameNumber,
		const TCHAR* ViewDebugName);
	bool SettingsContainLocalBlendable(const FPostProcessSettings& Settings) const;
	bool IsBlendableBoundToLocalCamera() const;
	int32 GetActualViewsSeen() const { return ActualViewsSeen; }
	int32 GetBlendableInjectionCount() const { return BlendableInjectionCount; }
	uint64 GetLastInjectedFrame() const { return LastInjectedFrame; }
	FString GetLastInjectedView() const { return LastInjectedView; }
	int32 GetVisualDebugMode() const;
	float GetMidDebugModeValue() const;

	void DebugAdvanceBlend(float DeltaSeconds);
	void DebugSetVisualDebugMode(int32 Mode);
	bool DebugPingPongUploadPackedMask(const TArray<FColor>& Pixels, int32 Width, int32 Height);

#if !UE_BUILD_SHIPPING
	void DebugDumpToLog() const;
#endif

private:
	void BindToPlayerController(APlayerController* NewPlayerController);
	void UnbindMirror();
	void EnsureMaskResources(int32 Width, int32 Height);
	void ReleaseMaskResources();
	void HandleLocalFoWUpdated(UGP_LocalFoWComponent* UpdatedMirror);
	void RebuildMaskFromMirror(const UGP_LocalFoWComponent* Mirror);
	void ApplyConservativeBlackMask();
	void UploadTargetMask(const TArray<FColor>& Pixels);
	void FillTextureBlack(UTexture2D* Texture, int32 Width, int32 Height);
	void UpdateMaterialParameters();
	void RefreshLocalCameraBlendable();
	void UnbindLocalCameraBlendable();
	UCameraComponent* ResolveLocalCamera() const;
	UTexture2D* CreateMaskTexture(const TCHAR* Name, int32 Width, int32 Height) const;
	void UploadTexture(UTexture2D* Texture, const TArray<FColor>& Pixels, int32 Width, int32 Height);
	void RecordViewSeen(const TCHAR* ViewDebugName);
	void RecordSuccessfulInjection(uint64 FrameNumber, const TCHAR* ViewDebugName);

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PreviousMaskTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TargetMaskTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> TemplateMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PostProcessMID;

	UPROPERTY(Transient)
	TWeakObjectPtr<UCameraComponent> BoundCamera;

	TWeakObjectPtr<UGP_LocalFoWComponent> BoundMirror;
	FDelegateHandle MirrorUpdatedHandle;
	TSharedPtr<FSceneViewExtensionBase, ESPMode::ThreadSafe> ViewExtension;

	FGP_FoWVisualMaskRuntime MaskRuntime;
	TArray<FColor> PackedTargetPixels;

	int32 MaskWidth = GPFoWVisualMask::CanonicalMaskResolution;
	int32 MaskHeight = GPFoWVisualMask::CanonicalMaskResolution;
	bool bVisualizationEnabled = true;
	int64 LastUpdateRevision = -1;
	int32 ForcedVisualDebugMode = 0;
	bool bHasForcedVisualDebugMode = false;

	int32 ActualViewsSeen = 0;
	int32 BlendableInjectionCount = 0;
	uint64 LastInjectedFrame = 0;
	FString LastInjectedView;
};
