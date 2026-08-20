// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPLocalFoWUnitPresentationSubsystem.h"

#include "Engine/World.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "TimerManager.h"
#include "Units/GPUnitBase.h"

void UGP_LocalFoWUnitPresentationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	RefreshLocalMirrorBinding();
	InWorld.GetTimerManager().SetTimer(
		EvaluationTimerHandle,
		this,
		&ThisClass::EvaluateRegisteredUnits,
		GetEvaluationIntervalSeconds(),
		true);
	EvaluateRegisteredUnits();
}

void UGP_LocalFoWUnitPresentationSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluationTimerHandle);
	}

	UnbindLocalMirror();
	for (const TWeakObjectPtr<AGP_UnitBase>& UnitWeak : RegisteredUnits)
	{
		if (AGP_UnitBase* Unit = UnitWeak.Get())
		{
			Unit->SetLocalFoWPresentationVisible(true);
		}
	}
	RegisteredUnits.Reset();

	Super::Deinitialize();
}

void UGP_LocalFoWUnitPresentationSubsystem::RegisterUnit(AGP_UnitBase* Unit)
{
	if (!IsValid(Unit))
	{
		return;
	}

	RegisteredUnits.AddUnique(Unit);
	RefreshLocalMirrorBinding();
	EvaluateUnit(Unit);
}

void UGP_LocalFoWUnitPresentationSubsystem::UnregisterUnit(AGP_UnitBase* Unit)
{
	if (Unit == nullptr)
	{
		return;
	}

	RegisteredUnits.RemoveAllSwap(
		[Unit](const TWeakObjectPtr<AGP_UnitBase>& Candidate)
		{
			return !Candidate.IsValid() || Candidate.Get() == Unit;
		},
		EAllowShrinking::No);
}

void UGP_LocalFoWUnitPresentationSubsystem::NotifyUnitTeamChanged(AGP_UnitBase* Unit)
{
	RefreshLocalMirrorBinding();
	EvaluateUnit(Unit);
}

bool UGP_LocalFoWUnitPresentationSubsystem::ShouldPresentUnitForLocalPlayer(
	const AGP_UnitBase* Unit,
	int32 LocalTeamId,
	const UGP_LocalFoWComponent* LocalFoW)
{
	if (!IsValid(Unit))
	{
		return false;
	}

	const int32 UnitTeamId = Unit->GetTeamId();
	if (UnitTeamId <= 0)
	{
		return true;
	}

	if (LocalTeamId >= 1 && UnitTeamId == LocalTeamId)
	{
		return true;
	}

	return LocalFoW != nullptr
		&& LocalFoW->IsReady()
		&& LocalFoW->GetLocalTeamId() == LocalTeamId
		&& LocalFoW->IsVisible(Unit->GetActorLocation());
}

void UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
	AGP_UnitBase* Unit,
	int32 LocalTeamId,
	const UGP_LocalFoWComponent* LocalFoW)
{
	if (!IsValid(Unit))
	{
		return;
	}

	Unit->SetLocalFoWPresentationVisible(
		ShouldPresentUnitForLocalPlayer(Unit, LocalTeamId, LocalFoW));
}

void UGP_LocalFoWUnitPresentationSubsystem::EvaluateRegisteredUnits()
{
	RefreshLocalMirrorBinding();
	RegisteredUnits.RemoveAllSwap(
		[](const TWeakObjectPtr<AGP_UnitBase>& UnitWeak)
		{
			return !UnitWeak.IsValid();
		},
		EAllowShrinking::No);

	for (const TWeakObjectPtr<AGP_UnitBase>& UnitWeak : RegisteredUnits)
	{
		EvaluateUnit(UnitWeak.Get());
	}
}

void UGP_LocalFoWUnitPresentationSubsystem::EvaluateUnit(AGP_UnitBase* Unit)
{
	if (!IsValid(Unit))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	ApplyUnitPresentationForLocalPlayer(
		Unit,
		ResolveLocalTeamId(),
		BoundLocalFoW.Get());
}

void UGP_LocalFoWUnitPresentationSubsystem::RefreshLocalMirrorBinding()
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer)
	{
		UnbindLocalMirror();
		return;
	}

	UGP_LocalFoWComponent* ResolvedMirror = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		AGP_PlayerController* PlayerController = Cast<AGP_PlayerController>(It->Get());
		if (PlayerController != nullptr && PlayerController->IsLocalController())
		{
			ResolvedMirror = PlayerController->GetLocalFogOfWarComponent();
			break;
		}
	}

	if (ResolvedMirror == BoundLocalFoW.Get())
	{
		return;
	}

	UnbindLocalMirror();
	BoundLocalFoW = ResolvedMirror;
	if (ResolvedMirror != nullptr)
	{
		LocalFoWUpdatedHandle = ResolvedMirror->OnLocalFoWUpdated.AddUObject(
			this,
			&ThisClass::HandleLocalFoWUpdated);
	}
}

void UGP_LocalFoWUnitPresentationSubsystem::UnbindLocalMirror()
{
	if (BoundLocalFoW.IsValid() && LocalFoWUpdatedHandle.IsValid())
	{
		BoundLocalFoW->OnLocalFoWUpdated.Remove(LocalFoWUpdatedHandle);
	}
	LocalFoWUpdatedHandle.Reset();
	BoundLocalFoW.Reset();
}

void UGP_LocalFoWUnitPresentationSubsystem::HandleLocalFoWUpdated(
	UGP_LocalFoWComponent* UpdatedMirror)
{
	if (UpdatedMirror == nullptr || UpdatedMirror != BoundLocalFoW.Get())
	{
		return;
	}
	EvaluateRegisteredUnits();
}

int32 UGP_LocalFoWUnitPresentationSubsystem::ResolveLocalTeamId() const
{
	if (const UGP_LocalFoWComponent* LocalFoW = BoundLocalFoW.Get())
	{
		if (LocalFoW->IsReady())
		{
			return LocalFoW->GetLocalTeamId();
		}
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return -1;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController == nullptr || !PlayerController->IsLocalController())
		{
			continue;
		}

		const AGP_PlayerState* PlayerState =
			PlayerController->GetPlayerState<AGP_PlayerState>();
		return PlayerState != nullptr ? PlayerState->GetTeamId() : -1;
	}

	return -1;
}
