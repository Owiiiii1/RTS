// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTagContainer.h"
#include "GPGameState.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGP_MatchStateTagChanged, FGameplayTag /*OldTag*/, FGameplayTag /*NewTag*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGP_MatchTimeRemainingChanged, float /*OldTime*/, float /*NewTime*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGP_FerroniteThreatValueChanged, float /*OldThreat*/, float /*NewThreat*/);
DECLARE_MULTICAST_DELEGATE_FourParams(
	FOnGP_MatchResultChanged,
	int32 /*OldWinnerTeamId*/,
	int32 /*NewWinnerTeamId*/,
	FGameplayTag /*OldWinReasonTag*/,
	FGameplayTag /*NewWinReasonTag*/);

/**
 * Server-authoritative match flow state (storage + replication).
 * Does not own the match timer, win evaluation, or EndMatch — those belong to future AGP_GameMode.
 */
UCLASS()
class GPRUNTIME_API AGP_GameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AGP_GameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Getters ---

	UFUNCTION(BlueprintPure, Category = "GP|Match")
	FGameplayTag GetMatchStateTag() const { return MatchStateTag; }

	UFUNCTION(BlueprintPure, Category = "GP|Match")
	float GetMatchTimeRemaining() const { return MatchTimeRemaining; }

	UFUNCTION(BlueprintPure, Category = "GP|Match")
	float GetFerroniteThreatValue() const { return FerroniteThreatValue; }

	UFUNCTION(BlueprintPure, Category = "GP|Match")
	int32 GetWinnerTeamId() const { return WinnerTeamId; }

	UFUNCTION(BlueprintPure, Category = "GP|Match")
	FGameplayTag GetWinReasonTag() const { return WinReasonTag; }

	// --- Authority-only mutation (no RPCs) ---

	void SetMatchStateTag(FGameplayTag NewStateTag);
	void SetMatchTimeRemaining(float NewTimeRemaining);
	void SetFerroniteThreatValue(float NewThreatValue);
	void SetMatchResult(int32 InWinnerTeamId, FGameplayTag InWinReasonTag);
	void ClearMatchResult();

	/** C++ subscription: old/new MatchStateTag. */
	FOnGP_MatchStateTagChanged OnMatchStateTagChanged;

	/** C++ subscription: old/new MatchTimeRemaining. */
	FOnGP_MatchTimeRemainingChanged OnMatchTimeRemainingChanged;

	/** C++ subscription: old/new FerroniteThreatValue. */
	FOnGP_FerroniteThreatValueChanged OnFerroniteThreatValueChanged;

	/**
	 * C++ subscription: match result field refresh.
	 * On clients, WinnerTeamId and WinReasonTag may replicate in separate frames —
	 * each OnRep broadcasts with accurate old/new for the changed field; the other
	 * pair uses the current getter values (field-level refresh). UI should read both getters.
	 */
	FOnGP_MatchResultChanged OnMatchResultChanged;

protected:
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MatchStateTag, Category = "GP|Match")
	FGameplayTag MatchStateTag;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MatchTimeRemaining, Category = "GP|Match")
	float MatchTimeRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FerroniteThreatValue, Category = "GP|Match")
	float FerroniteThreatValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WinnerTeamId, Category = "GP|Match")
	int32 WinnerTeamId = -1;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WinReasonTag, Category = "GP|Match")
	FGameplayTag WinReasonTag;

	UFUNCTION()
	void OnRep_MatchStateTag(FGameplayTag OldMatchStateTag);

	UFUNCTION()
	void OnRep_MatchTimeRemaining(float OldMatchTimeRemaining);

	UFUNCTION()
	void OnRep_FerroniteThreatValue(float OldFerroniteThreatValue);

	UFUNCTION()
	void OnRep_WinnerTeamId(int32 OldWinnerTeamId);

	UFUNCTION()
	void OnRep_WinReasonTag(FGameplayTag OldWinReasonTag);

	void BroadcastMatchResultChanged(
		int32 OldWinnerTeamId,
		int32 NewWinnerTeamId,
		FGameplayTag OldWinReasonTag,
		FGameplayTag NewWinReasonTag);

	static bool IsMatchStateBranchTag(const FGameplayTag& Tag);
	static bool IsWinReasonBranchTag(const FGameplayTag& Tag);
};
