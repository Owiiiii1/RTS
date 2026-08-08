// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GPPlayerState.generated.h"

class UGP_AbilitySystemComponent;
class UGP_PlayerAttributeSet;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGP_PlayerTeamIdChanged, int32 /*OldTeamId*/, int32 /*NewTeamId*/);

/**
 * Authoritative replicated player state.
 * Owns player-level ASC + PlayerAttributeSet. Owner/Avatar for ASC = this PlayerState (permanent).
 */
UCLASS()
class GPRUNTIME_API AGP_PlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AGP_PlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "GP|AbilitySystem")
	UGP_AbilitySystemComponent* GetGPAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Attributes")
	const UGP_PlayerAttributeSet* GetPlayerAttributeSet() const;

	UFUNCTION(BlueprintPure, Category = "GP|Team")
	int32 GetTeamId() const;

	/** Authority-only. Silent no-op without authority. */
	void SetTeamId(int32 NewTeamId);

	/** Local/UI: TeamId changed (authority SetTeamId or client OnRep). */
	FOnGP_PlayerTeamIdChanged OnTeamIdChanged;

protected:
	virtual void BeginPlay() override;
	virtual void ClientInitialize(AController* C) override;

	UFUNCTION()
	void OnRep_TeamId(int32 OldTeamId);

private:
	void InitializeAbilitySystemActorInfo();
	void BroadcastTeamIdChanged(int32 OldTeamId, int32 NewTeamId);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|AbilitySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Attributes", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_PlayerAttributeSet> PlayerAttributeSet;

	/** -1 unassigned, 0 neutral, 1+ playable teams. */
	UPROPERTY(ReplicatedUsing = OnRep_TeamId, VisibleInstanceOnly, Category = "GP|Team")
	int32 TeamId = -1;
};
