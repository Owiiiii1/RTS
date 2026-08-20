// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/GPCombatPresentationTypes.h"
#include "Components/ActorComponent.h"
#include "GPCombatPresentationComponent.generated.h"

class AGP_UnitBase;

DECLARE_LOG_CATEGORY_EXTERN(LogGPCombatPresentation, Log, All);

/**
 * Cosmetic combat presentation channel (GP-S26A).
 * Authority emits Unreliable NetMulticast after AttackHitApplied; receive path owns local viz.
 * No animation/Niagara/sound hard refs. Does not affect damage or cadence.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_CombatPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_CombatPresentationComponent();

	/**
	 * Authority-only. Increments PresentationSequence (skip 0) and fires Unreliable NetMulticast.
	 * Does not invoke local presentation handling inline — multicast Implementation is the sole Play path.
	 */
	void AuthorityEmitAttackHitPresentation(
		uint32 AttackSerial,
		AGP_UnitBase* Target,
		EGP_CombatPresentationEventType EventType,
		float AuthoritativeWorldTime,
		float AppliedDamage,
		bool bBlocked,
		bool bTargetDiedFromHit);

	uint32 GetAuthorityNextPresentationSequence() const;
	uint32 GetLastProcessedPresentationSequence() const;

	/** Local-only gate used by trusted FoW presentation. Authority event sequencing is unchanged. */
	void SetLocalPresentationAllowed(bool bAllowed) { bLocalPresentationAllowed = bAllowed; }
	bool IsLocalPresentationAllowed() const { return bLocalPresentationAllowed; }

protected:
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_CombatPresentationEvent(FGP_CombatPresentationEvent Event);

	void HandleCombatPresentationEvent(const FGP_CombatPresentationEvent& Event);
	void PlayCombatPresentationDebug(const FGP_CombatPresentationEvent& Event, AGP_UnitBase* SourceUnit);

private:
	/** Authority monotonic counter; 0 means none emitted yet; first emit becomes 1. */
	uint32 AuthorityPresentationSequence = 0;

	/** Local last accepted sequence for duplicate/stale suppression (serial arithmetic). */
	uint32 LastProcessedPresentationSequence = 0;

	bool bLocalPresentationAllowed = true;
};
