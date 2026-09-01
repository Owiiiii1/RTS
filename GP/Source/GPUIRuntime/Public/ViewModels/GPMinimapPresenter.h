// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "UObject/Object.h"
#include "GPMinimapPresenter.generated.h"

class AGP_PlayerController;
class UGP_LocalFoWComponent;

/**
 * Presentation snapshot of the trusted local FoW grid for HUD minimap consumers.
 *
 * This is metadata only. It never copies the FoW cell arrays.
 *
 * Normalized coordinate contract (axis-aligned to the current trusted FoW grid):
 * - Normalized.X = 0 at GridOrigin.X, 1 at GridOrigin.X + WorldSizeCm.X
 * - Normalized.Y = 0 at GridOrigin.Y, 1 at GridOrigin.Y + WorldSizeCm.Y
 * - World +X maps to minimap +X. World +Y maps to minimap +Y.
 * - No rotation, mirroring, or Slate Y-flip is applied here.
 * - WorldToMinimapNormalized clamps out-of-grid world XY onto [0,1].
 * - MinimapNormalizedToWorld clamps input XY onto [0,1] before mapping.
 * - GetMinimapFoWStateNormalized treats XY outside [0,1] as Unexplored (no clamp).
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
	FIntPoint GridDimensions = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	float CellSizeCm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	int64 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE(FOnGPMinimapPresentationChanged);

/**
 * LocalPlayer-owned, event-driven minimap presentation foundation.
 * Consumes the trusted owning-client UGP_LocalFoWComponent. Does not tick,
 * scan the world, allocate a FoW texture, or copy cell arrays.
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

	FOnGPMinimapPresentationChanged OnMinimapPresentationChanged;

protected:
	virtual void BeginDestroy() override;

private:
	void HandleMirrorUpdated(UGP_LocalFoWComponent* Mirror);
	void RebuildPresentation(bool bBroadcast);
	FGP_MinimapPresentation BuildPresentationFromMirror(const UGP_LocalFoWComponent* Mirror) const;
	bool HasUsableBounds() const;
	FVector2D GetWorldSizeCm() const;
	FVector ResolveFoWQueryWorldLocation(const FVector2D& Normalized) const;
	void UnbindMirror();

	TWeakObjectPtr<UGP_LocalFoWComponent> BoundMirror;
	FDelegateHandle MirrorUpdatedHandle;
	FGP_MinimapPresentation Presentation;
};
