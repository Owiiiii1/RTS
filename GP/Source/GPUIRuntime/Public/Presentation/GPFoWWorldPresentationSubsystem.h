// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GPFoWWorldPresentationSubsystem.generated.h"

class APlayerController;
class UGP_FoWWorldOverlayWidget;
class UGP_LocalFoWComponent;

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

	int32 GetLastSampledCellCount() const { return LastSampledCellCount; }
	int32 GetLastPaddedCellCount() const { return LastPaddedCellCount; }
	int32 GetLastContourSegmentCount() const { return LastContourSegmentCount; }
	int32 GetLastOverlayVertexCount() const { return LastOverlayVertexCount; }
	int32 GetLastOverlayTriangleCount() const { return LastOverlayTriangleCount; }
	int32 GetLastMixedCellCount() const { return LastMixedCellCount; }
	int32 GetLastCoalescedQuadCount() const { return LastCoalescedQuadCount; }
	int32 GetLastDrawBatchCount() const { return LastDrawBatchCount; }
	FIntPoint GetLastSampledMinCell() const { return LastSampledMinCell; }
	FIntPoint GetLastSampledMaxCell() const { return LastSampledMaxCell; }
	bool IsVisualDataDirty() const;

	static constexpr int32 GetMaximumSampledCells() { return 65536; }
	static constexpr int32 GetMaximumQuadsPerBatch() { return 8000; }
	static constexpr int32 GetSamplePadCells() { return 1; }
	static constexpr float GetProjectionGroundZ() { return 0.0f; }
	static const TCHAR* GetContourAlgorithmName();
	static float GetConservativeBoundaryT();
	static int32 GetSubcellsPerCell();
	static int32 GetMaximumOverlayTriangles();
	static int32 GetMaximumIsoSegments();

	void RecordOverlayStats(
		int32 SampledCells,
		int32 PaddedCells,
		int32 ContourSegments,
		int32 OverlayVertices,
		int32 OverlayTriangles,
		int32 MixedCells,
		int32 CoalescedQuads,
		int32 DrawBatches,
		const FIntPoint& MinCell,
		const FIntPoint& MaxCell,
		uint64 ConsumedSerial);

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
	int32 LastSampledCellCount = 0;
	int32 LastPaddedCellCount = 0;
	int32 LastContourSegmentCount = 0;
	int32 LastOverlayVertexCount = 0;
	int32 LastOverlayTriangleCount = 0;
	int32 LastMixedCellCount = 0;
	int32 LastCoalescedQuadCount = 0;
	int32 LastDrawBatchCount = 0;
	FIntPoint LastSampledMinCell = FIntPoint::ZeroValue;
	FIntPoint LastSampledMaxCell = FIntPoint::ZeroValue;
	uint64 LastConsumedRenderSerial = 0;
};
