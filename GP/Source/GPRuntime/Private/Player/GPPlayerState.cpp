// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GPPlayerState.h"
#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"

AGP_PlayerState::AGP_PlayerState()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UGP_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetProjectReplicationMode(EGameplayEffectReplicationMode::Mixed);

	PlayerAttributeSet = CreateDefaultSubobject<UGP_PlayerAttributeSet>(TEXT("PlayerAttributeSet"));
}

void AGP_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AGP_PlayerState, TeamId, COND_None, REPNOTIFY_OnChanged);
}

UAbilitySystemComponent* AGP_PlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UGP_AbilitySystemComponent* AGP_PlayerState::GetGPAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UGP_PlayerAttributeSet* AGP_PlayerState::GetPlayerAttributeSet() const
{
	return PlayerAttributeSet;
}

int32 AGP_PlayerState::GetTeamId() const
{
	return TeamId;
}

void AGP_PlayerState::SetTeamId(int32 NewTeamId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (TeamId == NewTeamId)
	{
		return;
	}

	const int32 OldTeamId = TeamId;
	TeamId = NewTeamId;
	BroadcastTeamIdChanged(OldTeamId, TeamId);
}

void AGP_PlayerState::OnRep_TeamId(int32 OldTeamId)
{
	BroadcastTeamIdChanged(OldTeamId, TeamId);
}

void AGP_PlayerState::BroadcastTeamIdChanged(int32 OldTeamId, int32 NewTeamId)
{
	OnTeamIdChanged.Broadcast(OldTeamId, NewTeamId);
}

void AGP_PlayerState::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystemActorInfo();
}

void AGP_PlayerState::ClientInitialize(AController* C)
{
	Super::ClientInitialize(C);
	InitializeAbilitySystemActorInfo();
}

void AGP_PlayerState::InitializeAbilitySystemActorInfo()
{
	if (AbilitySystemComponent == nullptr)
	{
		ensureMsgf(false, TEXT("AGP_PlayerState::InitializeAbilitySystemActorInfo: AbilitySystemComponent is null."));
		UE_LOG(LogTemp, Error, TEXT("AGP_PlayerState::InitializeAbilitySystemActorInfo: AbilitySystemComponent is null on %s."),
			*GetNameSafe(this));
		return;
	}

	const AActor* CurrentOwner = AbilitySystemComponent->GetOwnerActor();
	const AActor* CurrentAvatar = AbilitySystemComponent->GetAvatarActor();
	if (CurrentOwner == this && CurrentAvatar == this)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	UE_LOG(LogTemp, Verbose,
		TEXT("AGP_PlayerState::InitializeAbilitySystemActorInfo: InitAbilityActorInfo(this, this) on %s."),
		*GetName());
}
