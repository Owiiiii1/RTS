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
 */
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_BuildingBase : public AGP_UnitBase
{
	GENERATED_BODY()

public:
	AGP_BuildingBase();

	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintPure, Category = "GP|Navigation")
	UBoxComponent* GetNavigationObstacle() const { return NavigationObstacle; }

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
};
