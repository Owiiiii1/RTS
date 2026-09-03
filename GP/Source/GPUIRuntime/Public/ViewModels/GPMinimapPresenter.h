// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Math/Box.h"
#include "UObject/Object.h"
#include "GPMinimapPresenter.generated.h"

class AGP_CameraPawn;
class AGP_PlayerController;
class AGP_UnitBase;
class UGP_LocalFoWComponent;
class UGP_LocalFoWUnitPresentationSubsystem;

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

UENUM(BlueprintType)
enum class EGP_MinimapBlipKind : uint8
{
	Unit,
	Building
};

/** Presentation-only minimap blip. Color is resolved from TeamId at paint time. */
USTRUCT(BlueprintType)
struct GPUIRUNTIME_API FGP_MinimapBlip
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	FVector2D NormalizedPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	EGP_MinimapBlipKind Kind = EGP_MinimapBlipKind::Unit;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	int32 TeamId = -1;

	TWeakObjectPtr<const AGP_UnitBase> SourceActor;
};

/**
 * Projected viewport footprint in presenter-normalized coordinates (no surface flip).
 * Corners are the convex clip of the four deprojected ground hits against the unit square.
 * Independent per-corner clamp is not used (that can self-cross). Invalid if clip has < 3 points.
 */
USTRUCT(BlueprintType)
struct GPUIRUNTIME_API FGP_MinimapCameraFootprint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	TArray<FVector2D> NormalizedCorners;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Minimap")
	int32 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE(FOnGPMinimapPresentationChanged);
DECLARE_MULTICAST_DELEGATE(FOnGPMinimapBlipsChanged);
DECLARE_MULTICAST_DELEGATE(FOnGPMinimapCameraFootprintChanged);

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
	bool TryWorldToMinimapNormalizedUnclamped(const FVector& WorldLocation, FVector2D& OutNormalized) const;
	FVector MinimapNormalizedToWorld(const FVector2D& Normalized, float WorldZ) const;
	bool PanCameraToMinimapNormalized(const FVector2D& PresenterNormalized);
	EGP_FoWState GetMinimapFoWStateNormalized(const FVector2D& Normalized) const;
	const FGP_MinimapPresentation& GetMinimapPresentation() const { return Presentation; }
	const TArray<FGP_MinimapBlip>& GetBlips() const { return Blips; }
	const TArray<FGP_MinimapBlip>& GetFriendlyBlips() const { return Blips; }
	int64 GetBlipRevision() const { return BlipRevision; }
	int64 GetFriendlyBlipRevision() const { return BlipRevision; }
	const FGP_MinimapCameraFootprint& GetCameraFootprint() const { return CameraFootprint; }
	int32 GetBoundDelegateCount() const;
	int32 GetBoundCameraBoundsDelegateCount() const;
	int32 GetBoundCameraPresentationDelegateCount() const;
	int32 GetBoundUnitRegistryDelegateCount() const;

#if !UE_BUILD_SHIPPING
	void ContractBindCameraPawn(AGP_CameraPawn* CameraPawn);
	void ContractApplyDisplayedWorldBounds(const FBox& DisplayedBounds);
	void ContractClearDisplayedWorldBounds();
	void ContractRebuildFriendlyBlips();
	void ContractBindUnitRegistry(UWorld* World);
	void ContractRebuildCameraFootprint();
	void ContractSetViewportSizeOverride(int32 SizeX, int32 SizeY);
	void ContractClearViewportSizeOverride();
	bool ContractTryGetUnclampedNormalizedCorners(TArray<FVector2D>& OutCorners) const;
	const FGP_MinimapBlip* ContractFindFriendlyBlipForActor(const AGP_UnitBase* Unit) const;
	const FGP_MinimapBlip* ContractFindBlipForActor(const AGP_UnitBase* Unit) const;
#endif

	FOnGPMinimapPresentationChanged OnMinimapPresentationChanged;
	FOnGPMinimapBlipsChanged OnMinimapBlipsChanged;
	FOnGPMinimapCameraFootprintChanged OnMinimapCameraFootprintChanged;

protected:
	virtual void BeginDestroy() override;

private:
	void HandleMirrorUpdated(UGP_LocalFoWComponent* Mirror);
	void HandleResolvedCameraBoundsChanged();
	void HandleCameraPresentationChanged();
	void HandleUnitRegistryChanged();
	void HandleRegisteredUnitsEvaluated();
	void BindCameraPawn(AGP_CameraPawn* CameraPawn);
	void UnbindCameraPawn();
	void BindPlayerController(AGP_PlayerController* InPlayerController);
	void UnbindPlayerController();
	void BindUnitRegistry(UWorld* World);
	void UnbindUnitRegistry();
	void RebuildPresentation(bool bBroadcast);
	void RebuildBlips(bool bBroadcast);
	void RebuildCameraFootprint(bool bBroadcast);
	bool TryBuildCameraFootprint(FGP_MinimapCameraFootprint& OutFootprint) const;
	bool TryCollectUnclampedNormalizedCorners(TArray<FVector2D>& OutCorners) const;
	bool TryDeprojectViewportCornerToGround(
		float ScreenX,
		float ScreenY,
		int32 SizeX,
		int32 SizeY,
		float PlaneZ,
		FVector& OutWorld) const;
	FVector2D WorldToMinimapNormalizedUnclamped(const FVector& WorldLocation) const;
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
	FDelegateHandle CameraPresentationChangedHandle;
	TWeakObjectPtr<AGP_PlayerController> BoundPlayerController;
	TWeakObjectPtr<UGP_LocalFoWUnitPresentationSubsystem> BoundUnitRegistry;
	FDelegateHandle UnitRegistryChangedHandle;
	FDelegateHandle RegisteredUnitsEvaluatedHandle;
	bool bHasExplicitDisplayedBounds = false;
	FBox ExplicitDisplayedBounds = FBox(ForceInit);
	FGP_MinimapPresentation Presentation;
	TArray<FGP_MinimapBlip> Blips;
	int64 BlipRevision = 0;
	FGP_MinimapCameraFootprint CameraFootprint;
	bool bHasViewportSizeOverride = false;
	FIntPoint ViewportSizeOverride = FIntPoint::ZeroValue;
};
