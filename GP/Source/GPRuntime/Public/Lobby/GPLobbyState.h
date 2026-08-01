// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "GPLobbyState.generated.h"

USTRUCT()
struct GPRUNTIME_API FGP_LobbyPlayer
{
	GENERATED_BODY()

	UPROPERTY()
	int32 PlayerId = INDEX_NONE;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	bool bIsReady = false;
};

DECLARE_MULTICAST_DELEGATE(FGPOnLobbyPlayersChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FGPOnLobbyAllReadyChanged, bool /*bAllReady*/);

/**
 * Authoritative replicated lobby participant snapshot.
 * Server-only mutations. Does not own sessions, travel, ready RPC, or match start.
 * FindLobbyPlayer pointer is valid only until the next LobbyPlayers mutation.
 */
UCLASS()
class GPRUNTIME_API AGP_LobbyState : public AInfo
{
	GENERATED_BODY()

public:
	AGP_LobbyState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	const TArray<FGP_LobbyPlayer>& GetLobbyPlayers() const;
	const FGP_LobbyPlayer* FindLobbyPlayer(int32 PlayerId) const;
	bool IsAllReady() const;

	bool AddOrUpdatePlayer(int32 PlayerId, const FString& DisplayName);
	bool RemovePlayer(int32 PlayerId);
	bool SetPlayerReady(int32 PlayerId, bool bReady);
	bool ClearLobbyPlayers();

	FGPOnLobbyPlayersChanged& OnLobbyPlayersChanged();
	FGPOnLobbyAllReadyChanged& OnLobbyAllReadyChanged();

private:
	UFUNCTION()
	void OnRep_LobbyPlayers();

	UFUNCTION()
	void OnRep_AllReady(bool bOldAllReady);

	int32 FindLobbyPlayerIndex(int32 PlayerId) const;
	bool RecalculateAllReady();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyPlayers)
	TArray<FGP_LobbyPlayer> LobbyPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_AllReady)
	bool bAllReady = false;

	FGPOnLobbyPlayersChanged LobbyPlayersChanged;
	FGPOnLobbyAllReadyChanged LobbyAllReadyChanged;
};
