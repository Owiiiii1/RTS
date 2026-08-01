// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GPPlayerController.h"
#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

AGP_PlayerController::AGP_PlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
}

UGP_AbilitySystemComponent* AGP_PlayerController::GetGPAbilitySystemComponent() const
{
	APlayerState* PS = GetPlayerState<APlayerState>();
	if (PS == nullptr)
	{
		return nullptr;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PS);
	if (ASI == nullptr)
	{
		return nullptr;
	}

	return Cast<UGP_AbilitySystemComponent>(ASI->GetAbilitySystemComponent());
}

void AGP_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			TryInitializeLocalPawn(ControlledPawn);
		}
		TryInitializePlayerStateLink();
	}
}

void AGP_PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Verbose,
		TEXT("AGP_PlayerController::OnPossess: Pawn=%s Local=%s"),
		*GetNameSafe(InPawn),
		IsLocalController() ? TEXT("true") : TEXT("false"));

	if (IsLocalController())
	{
		TryInitializeLocalPawn(InPawn);
	}

	TryInitializePlayerStateLink();
}

void AGP_PlayerController::OnUnPossess()
{
	const APawn* CurrentPawn = GetPawn();
	if (LastInitializedLocalPawn.Get() == CurrentPawn)
	{
		LastInitializedLocalPawn.Reset();
	}

	Super::OnUnPossess();
}

void AGP_PlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);

	if (!IsLocalController())
	{
		return;
	}

	TryInitializeLocalPawn(InPawn);
	TryInitializePlayerStateLink();
}

void AGP_PlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	TryInitializePlayerStateLink();
}

void AGP_PlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();

	if (IsLocalController())
	{
		TryInitializeLocalPawn(GetPawn());
	}

	TryInitializePlayerStateLink();
}

void AGP_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void AGP_PlayerController::TryInitializeLocalPawn(APawn* InPawn)
{
	if (!IsLocalController() || InPawn == nullptr)
	{
		return;
	}

	if (LastInitializedLocalPawn.Get() == InPawn)
	{
		return;
	}

	LastInitializedLocalPawn = InPawn;
	OnLocalPawnReady(InPawn);
}

void AGP_PlayerController::TryInitializePlayerStateLink()
{
	APlayerState* PS = GetPlayerState<APlayerState>();
	if (PS == nullptr)
	{
		return;
	}

	if (LastInitializedPlayerState.Get() != PS)
	{
		LastInitializedPlayerState = PS;
		LastNotifiedAbilitySystemComponent.Reset();
		OnPlayerStateReady(PS);
	}

	UGP_AbilitySystemComponent* ASC = GetGPAbilitySystemComponent();
	if (ASC == nullptr)
	{
		// Expected until GP-S09 PlayerState owns UGP_AbilitySystemComponent.
		return;
	}

	if (LastNotifiedAbilitySystemComponent.Get() == ASC)
	{
		return;
	}

	LastNotifiedAbilitySystemComponent = ASC;
	OnAbilitySystemLinkReady(ASC);
}

void AGP_PlayerController::OnLocalPawnReady(APawn* InPawn)
{
	UE_LOG(LogTemp, Log,
		TEXT("AGP_PlayerController::OnLocalPawnReady: local pawn ready (%s). CameraPawn class validation deferred."),
		*GetNameSafe(InPawn));
}

void AGP_PlayerController::OnPlayerStateReady(APlayerState* InPlayerState)
{
	UE_LOG(LogTemp, Log,
		TEXT("AGP_PlayerController::OnPlayerStateReady: PlayerState ready (%s). AGP_PlayerState deferred to GP-S09."),
		*GetNameSafe(InPlayerState));
}

void AGP_PlayerController::OnAbilitySystemLinkReady(UGP_AbilitySystemComponent* InAbilitySystemComponent)
{
	UE_LOG(LogTemp, Log,
		TEXT("AGP_PlayerController::OnAbilitySystemLinkReady: ASC linked (%s). No InitAbilityActorInfo / grants in GP-S08."),
		*GetNameSafe(InAbilitySystemComponent));
}
