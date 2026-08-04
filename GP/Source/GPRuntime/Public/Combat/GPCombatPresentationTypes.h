// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPCombatPresentationTypes.generated.h"

class AGP_UnitBase;

/** Cosmetic combat presentation beat (GP-S26A). Does not affect gameplay cadence/damage. */
UENUM()
enum class EGP_CombatPresentationEventType : uint8
{
	MeleeImpact UMETA(DisplayName = "MeleeImpact")
	// Future (not implemented in S26A): RangedInstant, ProjectileLaunch, ProjectileImpact, HitReaction, AttackCancel
};

/**
 * Transient cosmetic presentation payload (GP-S26A).
 * Source is implied by the owning presentation component's actor.
 * Not persisted / not late-join replayed.
 */
USTRUCT()
struct GPRUNTIME_API FGP_CombatPresentationEvent
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 PresentationSequence = 0;

	UPROPERTY()
	uint32 AttackSerial = 0;

	UPROPERTY()
	TObjectPtr<AGP_UnitBase> Target = nullptr;

	UPROPERTY()
	EGP_CombatPresentationEventType EventType = EGP_CombatPresentationEventType::MeleeImpact;

	UPROPERTY()
	float AuthoritativeWorldTime = 0.0f;

	UPROPERTY()
	float AppliedDamage = 0.0f;

	UPROPERTY()
	bool bBlocked = false;

	UPROPERTY()
	bool bTargetDiedFromHit = false;
};
