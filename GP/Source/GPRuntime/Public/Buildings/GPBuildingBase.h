// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/GPUnitBase.h"
#include "GPBuildingBase.generated.h"

class UBoxComponent;

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
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintPure, Category = "GP|Navigation")
	UBoxComponent* GetNavigationObstacle() const { return NavigationObstacle; }

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FIntPoint GetGridOriginCell() const { return GridOriginCell; }

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FIntPoint GetGridFootprintSize() const { return GridFootprintSize; }

	FGuid GetGridOccupantId() const { return GridOccupantId; }

	/** Authority/deferred-spawn: set canonical grid facts before BeginPlay when possible. */
	void ConfigureGridPlacement(FIntPoint OriginCell, FIntPoint FootprintSize);

	FIntPoint ResolveFallbackFootprintSize() const;

protected:
	/**
	 * Authored navigation footprint (GP-S33M).
	 * BP children edit Relative Location / Rotation / Box Extent independently of capsule/mesh.
	 * Dynamic NavArea_Null obstacle — not selection, combat, Visibility, or Pawn blocking.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Navigation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> NavigationObstacle;

	/** Attach NavigationObstacle under the current root (Capsule on MainBase / LogisticsHub). */
	void AttachNavigationObstacleToRoot();

	/** Shared nav-obstacle collision / area setup (extent set by derived defaults or BP). */
	void ConfigureNavigationObstacleDefaults();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GP|BuildGrid")
	FIntPoint GridOriginCell = FIntPoint::ZeroValue;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GP|BuildGrid")
	FIntPoint GridFootprintSize = FIntPoint::ZeroValue;

	void TryRegisterWithBuildGrid();
	void TryUnregisterFromBuildGrid();

private:
	FGuid GridOccupantId;
	bool bGridPlacementConfigured = false;
	bool bGridRegistered = false;
};
