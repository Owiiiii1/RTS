// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/GPUnitBase.h"
#include "GPBuildingBase.generated.h"

class UBoxComponent;
class UGP_BuildingDefinition;
class USceneComponent;
struct FStreamableHandle;

/**
 * Minimal static unit ancestor for buildings (GP-S28 adaptation ahead of full GP-S34).
 * No Production / Construction component. No permanent Tick.
 * GP-S33M: owns inherited NavigationObstacle footprint (independent of capsule/mesh).
 * GP-S36G: replicated grid OriginCell + FootprintSize; occupancy via BuildGridSubsystem.
 */
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_BuildingBase : public AGP_UnitBase
{
	GENERATED_BODY()

public:
	AGP_BuildingBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintPure, Category = "GP|Navigation")
	UBoxComponent* GetNavigationObstacle() const { return NavigationObstacle; }

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	UBoxComponent* GetPlacementFootprintBounds() const { return PlacementFootprintBounds; }

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FIntPoint GetGridOriginCell() const { return GridOriginCell; }

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FIntPoint GetGridFootprintSize() const { return GridFootprintSize; }

	FGuid GetGridOccupantId() const { return GridOccupantId; }

	/** Attach deferred scene boxes under the current root. Safe before or after registration. */
	void AttachDeferredSceneComponentsToRoot();

	/** Authority/deferred-spawn: set canonical grid facts before BeginPlay when possible. */
	void ConfigureGridPlacement(FIntPoint OriginCell, FIntPoint FootprintSize);

	/**
	 * GP-S36G: PlacementFootprintBounds is Blueprint-class design data, not per-level-instance.
	 * Copies BoxExtent / RelativeLocation / RelativeRotation / RelativeScale3D from the class CDO
	 * onto this live component and re-applies scale-only isolation. Production calls this only
	 * for net-startup actors.
	 */
	void ApplyClassDesignToLivePlacementFootprintBounds();

	/**
	 * UE 5.8: SetAbsolute(false, false, true) — inherit parent location and rotation;
	 * RelativeScale3D is the component world scale (no actor/root scale).
	 */
	void ApplyPlacementFootprintParentScaleIsolation();

	FIntPoint ResolveFallbackFootprintSize() const;

#if !UE_BUILD_SHIPPING
	/** Actor name, resolved cells/offset, origin, registered size. */
	FString GetBuildGridOccupancyDebugString() const;
#endif

	/**
	 * Designer-facing building identity / storage / unit-cap. Soft only — no LoadSynchronous.
	 * Empty = immediate fallback. Loaded = apply now. Unloaded = async. Failure = fallback.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Definition")
	TSoftObjectPtr<UGP_BuildingDefinition> BuildingDefinitionAsset;

	UFUNCTION(BlueprintPure, Category = "GP|Definition")
	const UGP_BuildingDefinition* ResolveLoadedBuildingDefinition() const;

	UFUNCTION(BlueprintPure, Category = "GP|Definition")
	bool IsBuildingDefinitionReady() const { return bBuildingDefinitionReady; }

	UFUNCTION(BlueprintPure, Category = "GP|Definition")
	bool IsBuildingDefinitionLoadPending() const { return bBuildingDefinitionLoadPending; }

#if !UE_BUILD_SHIPPING
	void DebugForceUnresolvedSoftBuildingDefinitionLoad(UGP_BuildingDefinition* InjectedDefinition, bool bHoldCompletion);
	bool DebugDidRequestAsyncBuildingDefinitionLoad() const { return bDebugDidRequestAsyncBuildingDefinitionLoad; }
	void DebugCompletePendingBuildingDefinitionLoad();
#endif

protected:
	/**
	 * Authored navigation footprint (GP-S33M).
	 * Native inherited component: pointer is not replaceable (VisibleAnywhere / BlueprintReadOnly).
	 * BoxExtent / RelativeTransform are editable on Blueprint children via bEditableWhenInherited.
	 * Dynamic NavArea_Null obstacle — not selection, combat, Visibility, or Pawn blocking.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Navigation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> NavigationObstacle;

	/**
	 * Authorable BuildGrid placement footprint (GP-S36G).
	 * Native inherited component: pointer is not replaceable (VisibleAnywhere / BlueprintReadOnly).
	 * Blueprint children edit this component's BoxExtent / RelativeScale3D / RelativeLocation.
	 * This live component is the single occupied-ground source after init (no hidden CDO path).
	 * Pre-placed instances are synchronized from the class CDO so stale level snapshots cannot diverge.
	 * Parent/actor scale is isolated (absolute scale); own authored RelativeScale3D remains the size.
	 * Location and rotation follow Capsule/root. Occupancy is the oriented cell set of this box.
	 * GridOriginCell / GridFootprintSize are the occupied-cell AABB (debug/legacy), not the SoT.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|BuildGrid", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> PlacementFootprintBounds;

	/** Attach NavigationObstacle under the current root (Capsule on MainBase / LogisticsHub). */
	void AttachNavigationObstacleToRoot();

	/** Shared nav-obstacle collision / area setup (extent set by derived defaults or BP). */
	void ConfigureNavigationObstacleDefaults();

	void AttachPlacementFootprintBoundsToRoot();
	void ConfigurePlacementFootprintBoundsDefaults();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GP|BuildGrid")
	FIntPoint GridOriginCell = FIntPoint::ZeroValue;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GP|BuildGrid")
	FIntPoint GridFootprintSize = FIntPoint::ZeroValue;

	void TryRegisterWithBuildGrid();
	void TryUnregisterFromBuildGrid();
	void TryApplyClassDesignToLivePlacementFootprintBounds();

	void BeginBuildingDefinitionInitialization();
	void RequestAsyncBuildingDefinitionLoad();
	void HandleBuildingDefinitionLoaded();
	void FinishBuildingDefinitionLoadResolve();
	void CompleteBuildingDefinitionInitialization(const UGP_BuildingDefinition* DefinitionOrNull);
	void CancelPendingBuildingDefinitionLoad();
	virtual void NotifyBuildingDefinitionReady();

private:
	void AttachDeferredComponentToRoot(USceneComponent* Component);
	FGuid GridOccupantId;
	bool bGridPlacementConfigured = false;
	bool bGridRegistered = false;

	TSharedPtr<FStreamableHandle> BuildingDefinitionLoadHandle;
	bool bBuildingDefinitionReady = false;
	bool bBuildingDefinitionLoadPending = false;
	bool bBuildingDefinitionLoadAbandoned = false;

	UPROPERTY(Transient)
	TObjectPtr<UGP_BuildingDefinition> DebugInjectedBuildingDefinition;

#if !UE_BUILD_SHIPPING
	bool bDebugForceUnresolvedSoftBuildingPath = false;
	bool bDebugHoldAsyncBuildingCompletion = false;
	bool bDebugDidRequestAsyncBuildingDefinitionLoad = false;
#endif
};
