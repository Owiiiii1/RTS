// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "GPBuildingPlacementGhost.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class ULineBatchComponent;
class UTextRenderComponent;
class UGP_BuildGridSubsystem;
class AGP_BuildingBase;

/**
 * Local-only placement preview (GP-S36G).
 * Primary footprint visual is per-cell filled quads only (no contour lines). Cube slab is never shown.
 */
UCLASS(NotPlaceable)
class GPRUNTIME_API AGP_BuildingPlacementGhost : public AActor
{
	GENERATED_BODY()

public:
	AGP_BuildingPlacementGhost();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void SetGhostVisible(bool bVisible);
	void UpdateGhostTransform(const FTransform& WorldTransform);
	void SetFootprintCells(FIntPoint FootprintCells);
	void SetFootprintLocalOffset(FVector2D LocalCenterOffsetCm);
	void SetPreviewValid(bool bValid);
	void SetBuildingGhostClass(TSubclassOf<AGP_BuildingBase> PayloadClass);

	void UpdateGridPreview(
		const UGP_BuildGridSubsystem* Grid,
		FIntPoint OriginCell,
		FIntPoint FootprintSize,
		float GroundZ,
		bool bValid,
		EGP_BuildingDropRejectReason RejectReason,
		const TArray<EGP_PlacementPreviewCellState>* CellStates = nullptr);
	void ClearGridPreview();

	FVector2D GetPreviewOuterExtentXY() const { return PreviewOuterExtentXY; }
	FVector GetPreviewFillWorldMin() const { return PreviewFillWorldMin; }
	FVector GetPreviewFillWorldMax() const { return PreviewFillWorldMax; }
	int32 GetPreviewCellCount() const { return PreviewCellCount; }
	int32 GetPreviewGridLineCount() const { return PreviewGridLineCount; }
	FString GetPreviewStatusLabel() const { return PreviewStatusLabel; }
	bool HasActiveGridPreview() const { return bGridPreviewActive; }
	bool IsGhostFillHidden() const;
	bool IsBuildingGhostVisible() const { return bBuildingGhostVisible; }
	float GetPreviewGroundZ() const { return PreviewGroundZ; }
	int32 GetPreviewInvalidCellCount() const;
	EGP_PlacementPreviewCellState GetPreviewCellState(int32 Index) const;
	int32 GetPreviewLineWorldCount() const { return PreviewLineWorldStarts.Num(); }
	bool GetPreviewLineWorldSegment(int32 Index, FVector& OutStart, FVector& OutEnd) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Building|Ghost")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Building|Ghost")
	TObjectPtr<UStaticMeshComponent> GhostMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Building|Ghost")
	TObjectPtr<ULineBatchComponent> GridLineBatch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Building|Ghost")
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BuildingGhostMeshes;

	FIntPoint ActiveFootprintCells = FIntPoint(1, 1);
	FVector2D FootprintLocalOffsetCm = FVector2D::ZeroVector;
	FVector2D PreviewOuterExtentXY = FVector2D::ZeroVector;
	FVector PreviewFillWorldMin = FVector::ZeroVector;
	FVector PreviewFillWorldMax = FVector::ZeroVector;
	int32 PreviewCellCount = 0;
	int32 PreviewGridLineCount = 0;
	FString PreviewStatusLabel;
	bool bGridPreviewActive = false;
	bool bBuildingGhostVisible = false;
	float PreviewGroundZ = 0.0f;
	TArray<FVector> PreviewLineWorldStarts;
	TArray<FVector> PreviewLineWorldEnds;
	TArray<EGP_PlacementPreviewCellState> PreviewCellStates;
	TSubclassOf<AGP_BuildingBase> ActivePayloadClass;

	UPROPERTY()
	TObjectPtr<UStaticMesh> FallbackCylinderMesh;

	void HideLegacyCubeFill();
	void RebuildBuildingGhostMeshes();
	void SetBuildingGhostHidden(bool bHideMeshes);
	void FaceStatusTextToCamera();
};
