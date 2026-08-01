// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lobby/GPLobbyState.h"

#include "Net/UnrealNetwork.h"

AGP_LobbyState::AGP_LobbyState()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	PrimaryActorTick.bCanEverTick = false;
}

void AGP_LobbyState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AGP_LobbyState, LobbyPlayers, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_LobbyState, bAllReady, COND_None, REPNOTIFY_Always);
}

const TArray<FGP_LobbyPlayer>& AGP_LobbyState::GetLobbyPlayers() const
{
	return LobbyPlayers;
}

const FGP_LobbyPlayer* AGP_LobbyState::FindLobbyPlayer(int32 PlayerId) const
{
	const int32 Index = FindLobbyPlayerIndex(PlayerId);
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	return &LobbyPlayers[Index];
}

bool AGP_LobbyState::IsAllReady() const
{
	return bAllReady;
}

bool AGP_LobbyState::AddOrUpdatePlayer(int32 PlayerId, const FString& DisplayName)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_LobbyState::AddOrUpdatePlayer denied without authority."));
		return false;
	}

	if (PlayerId == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_LobbyState::AddOrUpdatePlayer rejected INDEX_NONE PlayerId."));
		return false;
	}

	const int32 ExistingIndex = FindLobbyPlayerIndex(PlayerId);
	if (ExistingIndex != INDEX_NONE)
	{
		FGP_LobbyPlayer& ExistingPlayer = LobbyPlayers[ExistingIndex];
		if (ExistingPlayer.DisplayName == DisplayName)
		{
			return false;
		}

		ExistingPlayer.DisplayName = DisplayName;

		const bool bAllReadyChanged = RecalculateAllReady();
		LobbyPlayersChanged.Broadcast();
		if (bAllReadyChanged)
		{
			LobbyAllReadyChanged.Broadcast(bAllReady);
		}

		ForceNetUpdate();
		return true;
	}

	FGP_LobbyPlayer NewPlayer;
	NewPlayer.PlayerId = PlayerId;
	NewPlayer.DisplayName = DisplayName;
	NewPlayer.bIsReady = false;
	LobbyPlayers.Add(NewPlayer);

	LobbyPlayers.Sort(
		[](const FGP_LobbyPlayer& A, const FGP_LobbyPlayer& B)
		{
			return A.PlayerId < B.PlayerId;
		});

	const bool bAllReadyChanged = RecalculateAllReady();
	LobbyPlayersChanged.Broadcast();
	if (bAllReadyChanged)
	{
		LobbyAllReadyChanged.Broadcast(bAllReady);
	}

	ForceNetUpdate();
	return true;
}

bool AGP_LobbyState::RemovePlayer(int32 PlayerId)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_LobbyState::RemovePlayer denied without authority."));
		return false;
	}

	if (PlayerId == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_LobbyState::RemovePlayer rejected INDEX_NONE PlayerId."));
		return false;
	}

	const int32 ExistingIndex = FindLobbyPlayerIndex(PlayerId);
	if (ExistingIndex == INDEX_NONE)
	{
		return false;
	}

	LobbyPlayers.RemoveAt(ExistingIndex);

	const bool bAllReadyChanged = RecalculateAllReady();
	LobbyPlayersChanged.Broadcast();
	if (bAllReadyChanged)
	{
		LobbyAllReadyChanged.Broadcast(bAllReady);
	}

	ForceNetUpdate();
	return true;
}

bool AGP_LobbyState::SetPlayerReady(int32 PlayerId, bool bReady)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_LobbyState::SetPlayerReady denied without authority."));
		return false;
	}

	if (PlayerId == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_LobbyState::SetPlayerReady rejected INDEX_NONE PlayerId."));
		return false;
	}

	const int32 ExistingIndex = FindLobbyPlayerIndex(PlayerId);
	if (ExistingIndex == INDEX_NONE)
	{
		return false;
	}

	FGP_LobbyPlayer& ExistingPlayer = LobbyPlayers[ExistingIndex];
	if (ExistingPlayer.bIsReady == bReady)
	{
		return false;
	}

	ExistingPlayer.bIsReady = bReady;

	const bool bAllReadyChanged = RecalculateAllReady();
	LobbyPlayersChanged.Broadcast();
	if (bAllReadyChanged)
	{
		LobbyAllReadyChanged.Broadcast(bAllReady);
	}

	ForceNetUpdate();
	return true;
}

bool AGP_LobbyState::ClearLobbyPlayers()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_LobbyState::ClearLobbyPlayers denied without authority."));
		return false;
	}

	if (LobbyPlayers.Num() == 0 && !bAllReady)
	{
		return false;
	}

	const bool bHadPlayers = LobbyPlayers.Num() > 0;
	const bool bWasAllReady = bAllReady;

	LobbyPlayers.Reset();
	bAllReady = false;

	if (bHadPlayers)
	{
		LobbyPlayersChanged.Broadcast();
	}

	if (bWasAllReady)
	{
		LobbyAllReadyChanged.Broadcast(false);
	}

	ForceNetUpdate();
	return true;
}

FGPOnLobbyPlayersChanged& AGP_LobbyState::OnLobbyPlayersChanged()
{
	return LobbyPlayersChanged;
}

FGPOnLobbyAllReadyChanged& AGP_LobbyState::OnLobbyAllReadyChanged()
{
	return LobbyAllReadyChanged;
}

void AGP_LobbyState::OnRep_LobbyPlayers()
{
	LobbyPlayersChanged.Broadcast();
}

void AGP_LobbyState::OnRep_AllReady(bool bOldAllReady)
{
	if (bOldAllReady != bAllReady)
	{
		LobbyAllReadyChanged.Broadcast(bAllReady);
	}
}

int32 AGP_LobbyState::FindLobbyPlayerIndex(int32 PlayerId) const
{
	if (PlayerId == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < LobbyPlayers.Num(); ++Index)
	{
		if (LobbyPlayers[Index].PlayerId == PlayerId)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool AGP_LobbyState::RecalculateAllReady()
{
	const bool bPreviousAllReady = bAllReady;

	bool bNewAllReady = LobbyPlayers.Num() > 0;
	if (bNewAllReady)
	{
		for (const FGP_LobbyPlayer& Player : LobbyPlayers)
		{
			if (!Player.bIsReady)
			{
				bNewAllReady = false;
				break;
			}
		}
	}

	bAllReady = bNewAllReady;
	return bAllReady != bPreviousAllReady;
}
