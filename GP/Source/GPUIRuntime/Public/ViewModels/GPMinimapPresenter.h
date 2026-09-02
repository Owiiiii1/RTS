// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Math/Box.h"
#include "UObject/Object.h"
#include "GPMinimapPresenter.generated.h"

class AGP_CameraPawn;
class AGP_PlayerController;
class UGP_LocalFoWComponent;

/**
 * Presentation snapshot for HUD minimap consumers. Metadata only; never copies FoW cells.
 *
 * FoW grid fields (GridOrigin / WorldSizeCm / GridDimensions / CellSizeCm) describe the
 * technical visibility grid. They are not the displayed map extent.
 *
 * Displayed map fields (MapWorldMin / MapWorldSizeCm) are the playable camera bounds:
 * valid AGP_CameraBoundsVolume, else the same Config.FallbackBounds ClampToBounds uses.
 * If camera bounds are unavailable, displayed fields fall back to the FoW grid.
 *
 * Normalized contract (displayed camera/playable rect, not the full FoW grid):
 * - Normalized.X = 0 at MapWorldMin.X, 1 at MapWorldMin.X + MapWorldSizeCm.X
 * - Normalized.Y = 0 at MapWorldMin.Y, 1 at MapWorldMin.Y + MapWorldSizeCm.Y
 * - World +X maps to minimap +X. World +Y maps to minimap +Y.
 * - No rotation, mirroring, or Slate Y-flip is applied here.
 * - WorldToMinimapNormalized clamps outside the displayed rect onto [0,1].
 * - MinimapNormalizedToWorld clamps input XY onto [0,1] before mapping.
 * - GetMinimapFoWStateNormalized maps through displayed bounds into the trusted FoW query;
 *   XY outside [0,1] is Unexplored (no clamp).
 */
USTRUCT(BlueprintType)
struct GPUIRUNTIME_API FGP_MinimapPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	bool bIsReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	int32 LocalTeamId = -1;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	FVector WorldOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	FVector2D GridOrigin = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	FVector2D WorldSizeCm = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	FVector2D MapWorldMin = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	FVector2D MapWorldSizeCm = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	FIntPoint GridDimensions = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	float CellSizeCm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	int64 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE(FOnGPMinimapPresentationChanged);

/**
 * LocalPlayer-owned, event-driven minimap presentation foundation.
 * Consumes the trusted owning-client UGP_LocalFoWComponent and the possessed
 * AGP_CameraPawn resolved bounds. Does not tick, scan the world, allocate a FoW
 * texture, or copy cell arrays.
 */
UCLASS()
class GPUIRUNTIME_API UGP_MinimapPresenter : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(AGP_PlayerController* InPlayerController);
	bool InitializeWithMirror(UGP_LocalFoWComponent* Mirror);
	void Shutdown();

	bool IsMinimapReady() const { return Presentation.bIsReady; }
	FVector2D WorldToMinimapNormalized(const FVector& WorldLocation) const;
	FVector MinimapNormalizedToWorld(const FVector2D& Normalized, float WorldZ) const;
	EGP_FoWState GetMinimapFoWStateNormalized(const FVector2D& Normalized) const;
	const FGP_MinimapPresentation& GetMinimapPresentation() const { return Presentation; }
	int32 GetBoundDelegateCount() const;
	int32 GetBoundCameraBoundsDelegateCount() const;

#if !UE_BUILD_SHIPPING
	void ContractBindCameraPawn(AGP_CameraPawn* CameraPawn);
	void ContractApplyDisplayedWorldBounds(const FBox& DisplayedBounds);
	void ContractClearDisplayedWorldBounds();
#endif

	FOnGPMinimapPresentationChanged OnMinimapPresentationChanged;

protected:
	virtual void BeginDestroy() override;

private:
	void HandleMirrorUpdated(UGP_LocalFoWComponent* Mirror);
	void HandleResolvedCameraBoundsChanged();
	void BindCameraPawn(AGP_CameraPawn* CameraPawn);
	void UnbindCameraPawn();
	void RebuildPresentation(bool bBroadcast);
	FGP_MinimapPresentation BuildPresentationFromMirror(const UGP_LocalFoWComponent* Mirror) const;
	bool TryResolveDisplayedWorldBounds(FVector2D& OutMin, FVector2D& OutSize) const;
	bool HasUsableBounds() const;
	FVector2D GetDisplayedWorldSizeCm() const;
	FVector ResolveFoWQueryWorldLocation(const FVector2D& Normalized) const;
	void UnbindMirror();

	TWeakObjectPtr<UGP_LocalFoWComponent> BoundMirror;
	FDelegateHandle MirrorUpdatedHandle;
	TWeakObjectPtr<AGP_CameraPawn> BoundCameraPawn;
	FDelegateHandle CameraBoundsChangedHandle;
	bool bHasExplicitDisplayedBounds = false;
	FBox ExplicitDisplayedBounds = FBox(ForceInit);
	FGP_MinimapPresentation Presentation;
};
