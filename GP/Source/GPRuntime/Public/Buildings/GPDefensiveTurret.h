// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/GPBuildingBase.h"
#include "GPDefensiveTurret.generated.h"

class UCapsuleComponent;
class USceneComponent;

/**
 * Stationary defensive turret (GP-S37T).
 * Thin building glue: identity, 2×2 footprint, combat defaults, CombatOrigin anchor.
 * Auto-acquire / fire / damage reuse UGP_UnitCommandComponent + existing GAS path.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_DefensiveTurret : public AGP_BuildingBase
{
	GENERATED_BODY()

public:
	AGP_DefensiveTurret();

	UFUNCTION(BlueprintPure, Category = "GP|Building")
	UCapsuleComponent* GetCapsuleComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Building|Presentation")
	USceneComponent* GetPresentationRoot() const;

	/** Native/authored LOS / muzzle anchor. Gameplay falls back to actor bounds if missing. */
	UFUNCTION(BlueprintPure, Category = "GP|Combat")
	USceneComponent* GetCombatOrigin() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PresentationRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> CombatOrigin;
};
