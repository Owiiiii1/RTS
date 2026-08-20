// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GPFoWWorldPresentationSubsystem.generated.h"

class APlayerController;
class UGP_FoWWorldOverlayWidget;
class UGP_LocalFoWComponent;

struct FGP_FoWWorldOverlayStats
{
	int32 SampledGameplayCells = 0;
	int32 CellTiles = 0;
	int32 FeatherQuads = 0;
	int32 VisibleCellsSkipped = 0;
	float FeatherCm = 0.0f;
	int32 OverlayVertices = 0;
	int32 OverlayQuads = 0;
	int32 DrawBatches = 0;
	FIntPoint MinCell = FIntPoint::ZeroValue;
	FIntPoint MaxCell = FIntPoint::ZeroValue;
	uint64 ConsumedSerial = 0;
	int64 MaskRevision = -1;
	bool bCameraResample = false;
	bool bFallbackActive = false;
	double RebuildMilliseconds = 0.0;
};

/**
 * Local-player owner for source-only world/terrain Fog of War presentation.
 *
 * The subsystem reads exactly one trusted UGP_LocalFoWComponent. It never computes visibility and has
 * no gameplay mutation path.
 */
UCLASS()
class GPUIRUNTIME_API UGP_FoWWorldPresentationSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	static float GetObscurationForState(EGP_FoWState State);
	static FLinearColor GetOverlayColorForState(EGP_FoWState State);
	static FLinearColor GetOverlayColorForObscuration(float Obscuration);
	static bool RequiresConservativeFullObscuration(const UGP_LocalFoWComponent* Mirror);

	void SetVisualizationEnabled(bool bEnabled);
	bool IsVisualizationEnabled() const { return bVisualizationEnabled; }
	bool IsRendererActive() const;
	UGP_LocalFoWComponent* GetBoundMirror() const { return BoundMirror.Get(); }
	uint64 GetRenderSerial() const { return RenderSerial; }
	int64 GetLastUpdateRevision() const { return LastUpdateRevision; }

	int32 GetLastSampledCellCount() const { return LastStats.SampledGameplayCells; }
	int32 GetLastCellTileCount() const { return LastStats.CellTiles; }
	int32 GetLastOverlayVertexCount() const { return LastStats.OverlayVertices; }
	int32 GetLastOverlayQuadCount() const { return LastStats.OverlayQuads; }
	int32 GetLastDrawBatchCount() const { return LastStats.DrawBatches; }
	FIntPoint GetLastSampledMinCell() const { return LastStats.MinCell; }
	FIntPoint GetLastSampledMaxCell() const { return LastStats.MaxCell; }
	bool DidLastCameraResample() const { return LastStats.bCameraResample; }
	bool WasLastFallbackActive() const { return LastStats.bFallbackActive; }
	int64 GetLastMaskRevision() const { return LastStats.MaskRevision; }
	bool IsVisualDataDirty() const;

	static constexpr int32 GetMaximumSampledCells() { return 16384; }
	static constexpr int32 GetMaximumQuadsPerBatch() { return 8000; }
	static constexpr int32 GetSamplePadCells() { return 1; }
	static constexpr float GetProjectionGroundZ() { return 0.0f; }
	static const TCHAR* GetRendererName();
	static bool IsPostProcessActive() { return false; }
	static bool IsMaskProjectionActive() { return false; }
	static const TCHAR* GetPresentationAlgorithmName();
	static const TCHAR* GetMaskModelName();
	static const TCHAR* GetInterpolationName();
	static const TCHAR* GetBlurName();
	static float GetFeatherFraction();
	static int32 GetMaximumOverlayQuads();

	void RecordOverlayStats(const FGP_FoWWorldOverlayStats& Stats);

#if !UE_BUILD_SHIPPING
	void DebugDumpToLog() const;
#endif

private:
	void BindToPlayerController(APlayerController* NewPlayerController);
	void UnbindMirror();
	void EnsureOverlayWidget(APlayerController* OwningController);
	void RemoveOverlayWidget();
	void HandleLocalFoWUpdated(UGP_LocalFoWComponent* UpdatedMirror);

	UPROPERTY(Transient)
	TObjectPtr<UGP_FoWWorldOverlayWidget> OverlayWidget;

	TWeakObjectPtr<UGP_LocalFoWComponent> BoundMirror;
	FDelegateHandle MirrorUpdatedHandle;

	bool bVisualizationEnabled = true;
	uint64 RenderSerial = 1;
	int64 LastUpdateRevision = -1;
	FGP_FoWWorldOverlayStats LastStats;
	uint64 LastConsumedRenderSerial = 0;
};
