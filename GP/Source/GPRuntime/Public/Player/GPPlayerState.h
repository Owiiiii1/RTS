// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GPPlayerState.generated.h"

class UGP_AbilitySystemComponent;
class UGP_PlayerAttributeSet;

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

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "GP|AbilitySystem")
	UGP_AbilitySystemComponent* GetGPAbilitySystemComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Attributes")
	const UGP_PlayerAttributeSet* GetPlayerAttributeSet() const;

protected:
	virtual void BeginPlay() override;
	virtual void ClientInitialize(AController* C) override;

private:
	void InitializeAbilitySystemActorInfo();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|AbilitySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Attributes", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_PlayerAttributeSet> PlayerAttributeSet;
};
