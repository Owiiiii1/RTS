// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPMatchViewModelAdapter.h"

#include "Game/GPGameState.h"
#include "Game/GPMatchResult.h"
#include "ViewModels/GPMatchViewModel.h"

bool UGP_MatchViewModelAdapter::Initialize(
	UGP_MatchViewModel* InViewModel,
	AGP_GameState* InGameState,
	int32 InLocalTeamId)
{
	Shutdown();
	ViewModel = InViewModel;
	if (ViewModel == nullptr || !IsValid(InGameState))
	{
		return false;
	}

	BoundGameState = InGameState;
	LocalTeamId = InLocalTeamId;
	MatchStateHandle = InGameState->OnMatchStateTagChanged.AddUObject(
		this, &ThisClass::HandleMatchStateChanged);
	MatchTimeHandle = InGameState->OnMatchTimeRemainingChanged.AddUObject(
		this, &ThisClass::HandleMatchTimeChanged);
	TeamThreatHandle = InGameState->OnTeamFerroniteThreatValueChanged.AddUObject(
		this, &ThisClass::HandleTeamThreatChanged);
	MatchResultHandle = InGameState->OnMatchResultChanged.AddUObject(
		this, &ThisClass::HandleMatchResultChanged);
	RefreshSnapshot();
	return true;
}

void UGP_MatchViewModelAdapter::Shutdown()
{
	if (AGP_GameState* GameState = BoundGameState.Get())
	{
		GameState->OnMatchStateTagChanged.Remove(MatchStateHandle);
		GameState->OnMatchTimeRemainingChanged.Remove(MatchTimeHandle);
		GameState->OnTeamFerroniteThreatValueChanged.Remove(TeamThreatHandle);
		GameState->OnMatchResultChanged.Remove(MatchResultHandle);
	}
	BoundGameState.Reset();
	LocalTeamId = -1;
	MatchStateHandle.Reset();
	MatchTimeHandle.Reset();
	TeamThreatHandle.Reset();
	MatchResultHandle.Reset();
}

void UGP_MatchViewModelAdapter::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UGP_MatchViewModelAdapter::RefreshSnapshot()
{
	AGP_GameState* GameState = BoundGameState.Get();
	if (ViewModel == nullptr || GameState == nullptr)
	{
		return;
	}

	const FGP_MatchResult& Result = GameState->GetMatchResult();
	ViewModel->SetMatchTimeRemaining(GameState->GetMatchTimeRemaining());
	ViewModel->SetMatchStateTag(GameState->GetMatchStateTag());
	ViewModel->SetFerroniteThreatValue(
		GameState->GetFerroniteThreatValueForTeam(LocalTeamId));
	ViewModel->SetWinnerTeamId(GameState->GetWinnerTeamId());
	ViewModel->SetWinReasonTag(GameState->GetWinReasonTag());
	ViewModel->SetMatchDuration(Result.MatchDuration);
	ViewModel->SetMatchFinished(GameState->IsMatchFinished());
}

void UGP_MatchViewModelAdapter::HandleMatchStateChanged(
	FGameplayTag OldTag,
	FGameplayTag NewTag)
{
	(void)OldTag;
	if (ViewModel != nullptr)
	{
		ViewModel->SetMatchStateTag(NewTag);
	}
	RefreshSnapshot();
}

void UGP_MatchViewModelAdapter::HandleMatchTimeChanged(float OldTime, float NewTime)
{
	(void)OldTime;
	if (ViewModel != nullptr)
	{
		ViewModel->SetMatchTimeRemaining(NewTime);
	}
}

void UGP_MatchViewModelAdapter::HandleTeamThreatChanged(
	int32 TeamId,
	float OldThreat,
	float NewThreat)
{
	(void)OldThreat;
	if (ViewModel != nullptr && TeamId == LocalTeamId)
	{
		ViewModel->SetFerroniteThreatValue(NewThreat);
	}
}

void UGP_MatchViewModelAdapter::HandleMatchResultChanged(
	int32 OldWinnerTeamId,
	int32 NewWinnerTeamId,
	FGameplayTag OldWinReasonTag,
	FGameplayTag NewWinReasonTag)
{
	(void)OldWinnerTeamId;
	(void)NewWinnerTeamId;
	(void)OldWinReasonTag;
	(void)NewWinReasonTag;
	RefreshSnapshot();
}

int32 UGP_MatchViewModelAdapter::GetBoundDelegateCount() const
{
	return (MatchStateHandle.IsValid() ? 1 : 0)
		+ (MatchTimeHandle.IsValid() ? 1 : 0)
		+ (TeamThreatHandle.IsValid() ? 1 : 0)
		+ (MatchResultHandle.IsValid() ? 1 : 0);
}
