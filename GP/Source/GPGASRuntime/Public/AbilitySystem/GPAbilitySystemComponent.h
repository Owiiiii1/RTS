// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GPAbilitySystemComponent.generated.h"

/**
 * Project-wide AbilitySystemComponent subclass.
 * Per-actor replication mode is configured via SetProjectReplicationMode (call before InitAbilityActorInfo).
 * Does not own AttributeSets; actors grant/init them separately.
 */
UCLASS(ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class GPGASRUNTIME_API UGP_AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UGP_AbilitySystemComponent();

	/**
	 * Sets GAS gameplay-effect replication mode for this ASC.
	 * Call before InitAbilityActorInfo — changing mode after init may invalidate replicated effect state.
	 * Does not store a duplicate replicated field; wraps UAbilitySystemComponent::SetReplicationMode.
	 */
	void SetProjectReplicationMode(EGameplayEffectReplicationMode NewMode);

	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
};
