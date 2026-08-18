// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/GPUnitBase.h"
#include "GPMobileUnit.generated.h"

class UGP_MovementComponent;

/**
 * Mobile unit composition boundary (GP-S20).
 * Owns UGP_MovementComponent. No command routing or physical move logic.
 * Mobile units never carve NavMesh; buildings keep their own NavigationObstacle.
 */
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_MobileUnit : public AGP_UnitBase
{
	GENERATED_BODY()

public:
	AGP_MobileUnit();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void UpdateNavigationRelevance() override;

	/** GP movement backend. Named to avoid overriding APawn::GetMovementComponent. */
	UGP_MovementComponent* GetUnitMovementComponent() const;

	/**
	 * Actor flag is not enough: Pawn components can still affect generation.
	 * Forces every primitive (native + Blueprint/SCS) off NavMesh generation.
	 */
	void ApplyMobileNavigationGenerationPolicy();

	/** True if any primitive still reports CanEverAffectNavigation. */
	bool HasAnyPrimitiveThatCanAffectNavigation() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_MovementComponent> MovementComponent;
};
