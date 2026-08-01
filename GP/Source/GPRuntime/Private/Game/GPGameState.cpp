// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/GPGameState.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GPGameplayTags.h"

AGP_GameState::AGP_GameState()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	// Native tags are registered in FGPGASRuntimeModule::StartupModule before worlds/GameState spawn.
	const FGameplayTag LoadingTag = FGPGameplayTags::Get().Match_State_Loading;
	if (LoadingTag.IsValid())
	{
		MatchStateTag = LoadingTag;
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameState: Match_State_Loading invalid at construction; MatchStateTag left unset until SetMatchStateTag."));
		MatchStateTag = FGameplayTag();
	}

	MatchTimeRemaining = 0.0f;
	FerroniteThreatValue = 0.0f;
	WinnerTeamId = -1;
	WinReasonTag = FGameplayTag();
}

void AGP_GameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, MatchStateTag, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, MatchTimeRemaining, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, FerroniteThreatValue, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, WinnerTeamId, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, WinReasonTag, COND_None, REPNOTIFY_Always);
}

bool AGP_GameState::IsMatchStateBranchTag(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return false;
	}

	const FGameplayTag MatchStateRoot = FGPGameplayTags::Get().Match_State_Loading.RequestDirectParent();
	return MatchStateRoot.IsValid() && Tag.MatchesTag(MatchStateRoot);
}

bool AGP_GameState::IsWinReasonBranchTag(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return false;
	}

	const FGameplayTag WinReasonRoot = FGPGameplayTags::Get().Match_WinReason_DeliveryQuota.RequestDirectParent();
	return WinReasonRoot.IsValid() && Tag.MatchesTag(WinReasonRoot);
}

void AGP_GameState::SetMatchStateTag(FGameplayTag NewStateTag)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetMatchStateTag denied without authority."));
		return;
	}

	if (!IsMatchStateBranchTag(NewStateTag))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameState::SetMatchStateTag rejected invalid or non-GP.Match.State tag '%s'."),
			*NewStateTag.ToString());
		return;
	}

	if (MatchStateTag == NewStateTag)
	{
		return;
	}

	const FGameplayTag OldTag = MatchStateTag;
	MatchStateTag = NewStateTag;
	OnMatchStateTagChanged.Broadcast(OldTag, MatchStateTag);
}

void AGP_GameState::SetMatchTimeRemaining(float NewTimeRemaining)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetMatchTimeRemaining denied without authority."));
		return;
	}

	const float ClampedTime = FMath::Max(0.0f, NewTimeRemaining);
	if (FMath::IsNearlyEqual(MatchTimeRemaining, ClampedTime))
	{
		return;
	}

	const float OldTime = MatchTimeRemaining;
	MatchTimeRemaining = ClampedTime;
	OnMatchTimeRemainingChanged.Broadcast(OldTime, MatchTimeRemaining);
}

void AGP_GameState::SetFerroniteThreatValue(float NewThreatValue)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetFerroniteThreatValue denied without authority."));
		return;
	}

	const float ClampedThreat = FMath::Max(0.0f, NewThreatValue);
	if (FMath::IsNearlyEqual(FerroniteThreatValue, ClampedThreat))
	{
		return;
	}

	const float OldThreat = FerroniteThreatValue;
	FerroniteThreatValue = ClampedThreat;
	OnFerroniteThreatValueChanged.Broadcast(OldThreat, FerroniteThreatValue);
}

void AGP_GameState::SetMatchResult(int32 InWinnerTeamId, FGameplayTag InWinReasonTag)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetMatchResult denied without authority."));
		return;
	}

	if (InWinnerTeamId < -1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameState::SetMatchResult rejected WinnerTeamId=%d (must be >= -1)."),
			InWinnerTeamId);
		return;
	}

	if (!IsWinReasonBranchTag(InWinReasonTag))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameState::SetMatchResult rejected invalid or non-GP.Match.WinReason tag '%s'."),
			*InWinReasonTag.ToString());
		return;
	}

	if (WinnerTeamId == InWinnerTeamId && WinReasonTag == InWinReasonTag)
	{
		return;
	}

	const int32 OldWinner = WinnerTeamId;
	const FGameplayTag OldReason = WinReasonTag;
	WinnerTeamId = InWinnerTeamId;
	WinReasonTag = InWinReasonTag;
	BroadcastMatchResultChanged(OldWinner, WinnerTeamId, OldReason, WinReasonTag);
}

void AGP_GameState::ClearMatchResult()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::ClearMatchResult denied without authority."));
		return;
	}

	if (WinnerTeamId == -1 && !WinReasonTag.IsValid())
	{
		return;
	}

	const int32 OldWinner = WinnerTeamId;
	const FGameplayTag OldReason = WinReasonTag;
	WinnerTeamId = -1;
	WinReasonTag = FGameplayTag();
	BroadcastMatchResultChanged(OldWinner, WinnerTeamId, OldReason, WinReasonTag);
}

void AGP_GameState::BroadcastMatchResultChanged(
	int32 OldWinnerTeamIdValue,
	int32 NewWinnerTeamIdValue,
	FGameplayTag OldWinReasonTagValue,
	FGameplayTag NewWinReasonTagValue)
{
	OnMatchResultChanged.Broadcast(
		OldWinnerTeamIdValue,
		NewWinnerTeamIdValue,
		OldWinReasonTagValue,
		NewWinReasonTagValue);
}

void AGP_GameState::OnRep_MatchStateTag(FGameplayTag OldMatchStateTag)
{
	OnMatchStateTagChanged.Broadcast(OldMatchStateTag, MatchStateTag);
}

void AGP_GameState::OnRep_MatchTimeRemaining(float OldMatchTimeRemaining)
{
	OnMatchTimeRemainingChanged.Broadcast(OldMatchTimeRemaining, MatchTimeRemaining);
}

void AGP_GameState::OnRep_FerroniteThreatValue(float OldFerroniteThreatValue)
{
	OnFerroniteThreatValueChanged.Broadcast(OldFerroniteThreatValue, FerroniteThreatValue);
}

void AGP_GameState::OnRep_WinnerTeamId(int32 OldWinnerTeamId)
{
	// Field-level refresh: WinReason old/new both current (may already be updated or not).
	BroadcastMatchResultChanged(OldWinnerTeamId, WinnerTeamId, WinReasonTag, WinReasonTag);
}

void AGP_GameState::OnRep_WinReasonTag(FGameplayTag OldWinReasonTag)
{
	// Field-level refresh: Winner old/new both current (may already be updated or not).
	BroadcastMatchResultChanged(WinnerTeamId, WinnerTeamId, OldWinReasonTag, WinReasonTag);
}
