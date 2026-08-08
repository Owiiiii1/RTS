// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class AGP_UnitBase;
class UWorld;

/**
 * Canonical multi-point Visibility LOS helpers (GP-S29R / TDD/04).
 * Server-authoritative fire-gate use only.
 */
namespace GPCombatLOS
{
	struct FGP_LOSTracePoints
	{
		FVector Eye = FVector::ZeroVector;
		FVector Chest = FVector::ZeroVector;
		FVector Feet = FVector::ZeroVector;
		bool bValid = false;
	};

	/** Socket AttackOrigin_* with bounds fallbacks (Top / Center / Bottom+10). */
	GPRUNTIME_API FGP_LOSTracePoints ResolveAttackOriginPoints(const AActor* Attacker);

	/** Socket Hit_* with bounds fallbacks (Top / Center / Bottom+10). */
	GPRUNTIME_API FGP_LOSTracePoints ResolveHitPoints(const AActor* Target);

	/**
	 * ECC_Visibility 3-pair traces (Eye→Head, Chest→Chest, Feet→Feet).
	 * ANY clear pair (!bBlockingHit OR HitActor==Target) ⇒ true.
	 * Ignores Source. Fail-closed on invalid context.
	 */
	GPRUNTIME_API bool HasLineOfSight(
		UWorld* World,
		const AActor* Source,
		const AActor* Target);
}
