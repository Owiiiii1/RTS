// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/GPAbilitySystemComponent.h"

UGP_AbilitySystemComponent::UGP_AbilitySystemComponent()
{
}

void UGP_AbilitySystemComponent::SetProjectReplicationMode(EGameplayEffectReplicationMode NewMode)
{
	// Must be called before InitAbilityActorInfo (see class comment / TDD replication discipline).
	SetReplicationMode(NewMode);

	UE_LOG(LogTemp, Log, TEXT("UGP_AbilitySystemComponent::SetProjectReplicationMode: %s on %s"),
		*UEnum::GetValueAsString(NewMode),
		*GetNameSafe(GetOwner()));
}

void UGP_AbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	if (InOwnerActor == nullptr || InAvatarActor == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UGP_AbilitySystemComponent::InitAbilityActorInfo: OwnerActor or AvatarActor is null (Owner=%s, Avatar=%s)."),
			*GetNameSafe(InOwnerActor),
			*GetNameSafe(InAvatarActor));
		return;
	}

	if (InOwnerActor != InAvatarActor)
	{
		// Diagnostic only: PlayerState Owner + Pawn Avatar is a normal GAS pattern.
		UE_LOG(LogTemp, Warning,
			TEXT("UGP_AbilitySystemComponent::InitAbilityActorInfo: OwnerActor and AvatarActor differ (Owner=%s, Avatar=%s); verify this is intentional."),
			*InOwnerActor->GetName(),
			*InAvatarActor->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("UGP_AbilitySystemComponent::InitAbilityActorInfo: OwnerActor == AvatarActor (%s)."),
			*InOwnerActor->GetName());
	}
}
