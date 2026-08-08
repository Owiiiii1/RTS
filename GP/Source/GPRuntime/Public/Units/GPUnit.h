// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/GPMobileUnit.h"
#include "GPUnit.generated.h"

class UCapsuleComponent;
class UGP_UnitVisualComponent;

/**
 * First production-oriented concrete unit actor.
 * Capsule root for selection/collision; composite primitive visuals via UGP_UnitVisualComponent (GP-S26B1).
 * Inherits mobility composition from AGP_MobileUnit (GP-S20).
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_Unit : public AGP_MobileUnit
{
	GENERATED_BODY()

public:
	AGP_Unit();

	UGP_UnitVisualComponent* GetUnitVisualComponent() const;

	/** Always false after GP-S26B1 migration (legacy single Cylinder VisualMesh removed). */
	bool HasLegacyVisualMesh() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_UnitVisualComponent> UnitVisualComponent;
};
