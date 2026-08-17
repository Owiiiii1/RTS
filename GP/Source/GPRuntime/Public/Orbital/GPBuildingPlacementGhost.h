// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "GPBuildingPlacementGhost.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class ULineBatchComponent;
class UTextRenderComponent;
class UGP_BuildGridSubsystem;

/**
 * Local-only placement ghost (GP-S32R / GP-S36G visual feedback).
 * Primary validity feedback is line batch + text, not material parameters.
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
	void SetPreviewValid(bool bValid);

	void UpdateGridPreview(
		const UGP_BuildGridSubsystem* Grid,
		FIntPoint OriginCell,
		FIntPoint FootprintSize,
		float GroundZ,
		bool bValid,
		EGP_BuildingDropRejectReason RejectReason);
	void ClearGridPreview();

	FVector2D GetPreviewOuterExtentXY() const { return PreviewOuterExtentXY; }
	int32 GetPreviewCellCount() const { return PreviewCellCount; }
	int32 GetPreviewGridLineCount() const { return PreviewGridLineCount; }
	FString GetPreviewStatusLabel() const { return PreviewStatusLabel; }
	bool HasActiveGridPreview() const { return bGridPreviewActive; }
	bool IsGhostFillHidden() const;
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
	TObjectPtr<UMaterialInstanceDynamic> GhostMaterial;

	FIntPoint ActiveFootprintCells = FIntPoint(1, 1);
	FVector2D PreviewOuterExtentXY = FVector2D::ZeroVector;
	int32 PreviewCellCount = 0;
	int32 PreviewGridLineCount = 0;
	FString PreviewStatusLabel;
	bool bGridPreviewActive = false;
	TArray<FVector> PreviewLineWorldStarts;
	TArray<FVector> PreviewLineWorldEnds;
};
