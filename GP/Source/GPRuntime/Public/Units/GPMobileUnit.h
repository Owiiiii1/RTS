// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/GPUnitBase.h"
#include "GPMobileUnit.generated.h"

class UGP_MovementComponent;

/**
 * Mobile unit composition boundary (GP-S20).
 * Owns UGP_MovementComponent. No command routing or physical move logic.
 */
UCLASS(Abstract, Blueprintable)
class GPRUNTIME_API AGP_MobileUnit : public AGP_UnitBase
{
	GENERATED_BODY()

public:
	AGP_MobileUnit();

	/** GP movement backend. Named to avoid overriding APawn::GetMovementComponent. */
	UGP_MovementComponent* GetUnitMovementComponent() const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components|Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_MovementComponent> MovementComponent;
};
