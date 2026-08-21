// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/GPGameState.h"

#include "Buildings/GPMainBase.h"
#include "Engine/World.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "GameFramework/PlayerState.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Resources/GPResourceApproach.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceNode.h"
#include "Resources/GPResourceNodeSearch.h"
#include "Tags/GPGameplayTags.h"

AGP_GameState::AGP_GameState()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	FogOfWarComponent = CreateDefaultSubobject<UGP_FogOfWarComponent>(TEXT("FogOfWarComponent"));

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
	DeliveryQuotaFerroniteScore = 5000.0f;
	bAnnihilationCountsAsWin = true;
	MatchSeed = 0;
	MatchResult = FGP_MatchResult();
}

void AGP_GameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, MatchStateTag, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, MatchTimeRemaining, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, FerroniteThreatValue, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, TeamFerroniteThreatValues, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, WinnerTeamId, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, WinReasonTag, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(AGP_GameState, DeliveryQuotaFerroniteScore);
	DOREPLIFETIME(AGP_GameState, bAnnihilationCountsAsWin);
	DOREPLIFETIME(AGP_GameState, MatchSeed);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, MatchResult, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, ReplicatedMainBases, COND_None, REPNOTIFY_Always);
}

void AGP_GameState::AddPlayerState(APlayerState* PlayerState)
{
	const bool bAlreadyPresent = PlayerArray.Contains(PlayerState);
	Super::AddPlayerState(PlayerState);
	if (!bAlreadyPresent && PlayerArray.Contains(PlayerState))
	{
		OnPlayerStateRosterChanged.Broadcast(PlayerState, true);
	}
}

void AGP_GameState::RemovePlayerState(APlayerState* PlayerState)
{
	const bool bWasPresent = PlayerArray.Contains(PlayerState);
	Super::RemovePlayerState(PlayerState);
	if (bWasPresent)
	{
		OnPlayerStateRosterChanged.Broadcast(PlayerState, false);
	}
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

int32 AGP_GameState::FindTeamThreatIndex(int32 InTeamId) const
{
	for (int32 Index = 0; Index < TeamFerroniteThreatValues.Num(); ++Index)
	{
		if (TeamFerroniteThreatValues[Index].TeamId == InTeamId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

float AGP_GameState::GetFerroniteThreatValueForTeam(int32 InTeamId) const
{
	const int32 Index = FindTeamThreatIndex(InTeamId);
	return Index != INDEX_NONE ? TeamFerroniteThreatValues[Index].ThreatValue : 0.0f;
}

void AGP_GameState::SyncLegacyFerroniteThreatScalar()
{
	float Mirrored = 0.0f;
	const int32 Team1Index = FindTeamThreatIndex(1);
	if (Team1Index != INDEX_NONE)
	{
		Mirrored = TeamFerroniteThreatValues[Team1Index].ThreatValue;
	}
	else if (TeamFerroniteThreatValues.Num() > 0)
	{
		Mirrored = TeamFerroniteThreatValues[0].ThreatValue;
	}

	if (FMath::IsNearlyEqual(FerroniteThreatValue, Mirrored))
	{
		return;
	}

	const float OldThreat = FerroniteThreatValue;
	FerroniteThreatValue = Mirrored;
	OnFerroniteThreatValueChanged.Broadcast(OldThreat, FerroniteThreatValue);
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
	SetFerroniteThreatValueForTeam(1, NewThreatValue);
}

void AGP_GameState::SetFerroniteThreatValueForTeam(int32 InTeamId, float NewThreatValue)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetFerroniteThreatValueForTeam denied without authority."));
		return;
	}

	if (InTeamId < 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameState::SetFerroniteThreatValueForTeam rejected TeamId=%d."),
			InTeamId);
		return;
	}

	if (!FMath::IsFinite(NewThreatValue))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameState::SetFerroniteThreatValueForTeam rejected non-finite threat for TeamId=%d."),
			InTeamId);
		return;
	}

	const float ClampedThreat = FMath::Max(0.0f, NewThreatValue);
	int32 Index = FindTeamThreatIndex(InTeamId);
	float OldThreat = 0.0f;
	if (Index == INDEX_NONE)
	{
		FGP_TeamFerroniteThreat Entry;
		Entry.TeamId = InTeamId;
		Entry.ThreatValue = ClampedThreat;
		TeamFerroniteThreatValues.Add(Entry);
		Index = TeamFerroniteThreatValues.Num() - 1;
	}
	else
	{
		OldThreat = TeamFerroniteThreatValues[Index].ThreatValue;
		if (FMath::IsNearlyEqual(OldThreat, ClampedThreat))
		{
			SyncLegacyFerroniteThreatScalar();
			return;
		}
		TeamFerroniteThreatValues[Index].ThreatValue = ClampedThreat;
	}

	OnTeamFerroniteThreatValueChanged.Broadcast(InTeamId, OldThreat, ClampedThreat);
	SyncLegacyFerroniteThreatScalar();
}

float AGP_GameState::AddFerroniteThreatValueForTeam(int32 InTeamId, float Delta)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::AddFerroniteThreatValueForTeam denied without authority."));
		return 0.0f;
	}

	if (!FMath::IsFinite(Delta) || FMath::IsNearlyZero(Delta))
	{
		return 0.0f;
	}

	const float Previous = GetFerroniteThreatValueForTeam(InTeamId);
	const float Next = FMath::Max(0.0f, Previous + Delta);
	SetFerroniteThreatValueForTeam(InTeamId, Next);
	return Next - Previous;
}

bool AGP_GameState::IsMatchFinished() const
{
	return MatchStateTag == FGPGameplayTags::Get().Match_State_Finished;
}

bool AGP_GameState::AreEconomicOrdersAllowed() const
{
	return !IsMatchFinished();
}

bool AGP_GameState::AreEconomicOrdersAllowedInWorld(const UWorld* World)
{
	if (World == nullptr)
	{
		return true;
	}

	const AGP_GameState* GPGameState = World->GetGameState<AGP_GameState>();
	if (GPGameState == nullptr)
	{
		return true;
	}

	return GPGameState->AreEconomicOrdersAllowed();
}

void AGP_GameState::SetDeliveryQuotaFerroniteScore(float InQuota)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetDeliveryQuotaFerroniteScore denied without authority."));
		return;
	}

	if (!FMath::IsFinite(InQuota))
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetDeliveryQuotaFerroniteScore rejected non-finite quota."));
		return;
	}

	DeliveryQuotaFerroniteScore = FMath::Max(0.0f, InQuota);
}

void AGP_GameState::SetAnnihilationCountsAsWin(bool bInAnnihilationCountsAsWin)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetAnnihilationCountsAsWin denied without authority."));
		return;
	}

	bAnnihilationCountsAsWin = bInAnnihilationCountsAsWin;
}

void AGP_GameState::SetMatchSeed(int32 InMatchSeed)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetMatchSeed denied without authority."));
		return;
	}

	MatchSeed = InMatchSeed;
}

void AGP_GameState::SetMatchResult(int32 InWinnerTeamId, FGameplayTag InWinReasonTag)
{
	FGP_MatchResult Result = MatchResult;
	Result.WinnerTeamId = InWinnerTeamId;
	Result.WinnerReason = InWinReasonTag;
	SetMatchResult(Result);
}

void AGP_GameState::SetMatchResult(const FGP_MatchResult& InResult)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::SetMatchResult denied without authority."));
		return;
	}

	if (IsMatchFinished())
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("AGP_GameState::SetMatchResult ignored — match already Finished (WinnerTeamId=%d)."),
			WinnerTeamId);
		return;
	}

	if (InResult.WinnerTeamId < -1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameState::SetMatchResult rejected WinnerTeamId=%d (must be >= -1)."),
			InResult.WinnerTeamId);
		return;
	}

	if (!IsWinReasonBranchTag(InResult.WinnerReason))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameState::SetMatchResult rejected invalid or non-GP.Match.WinReason tag '%s'."),
			*InResult.WinnerReason.ToString());
		return;
	}

	if (WinnerTeamId == InResult.WinnerTeamId
		&& WinReasonTag == InResult.WinnerReason
		&& MatchResult.MatchDuration == InResult.MatchDuration
		&& MatchResult.FinalScores.Num() == InResult.FinalScores.Num())
	{
		return;
	}

	const int32 OldWinner = WinnerTeamId;
	const FGameplayTag OldReason = WinReasonTag;
	MatchResult = InResult;
	WinnerTeamId = InResult.WinnerTeamId;
	WinReasonTag = InResult.WinnerReason;
	BroadcastMatchResultChanged(OldWinner, WinnerTeamId, OldReason, WinReasonTag);
}

void AGP_GameState::ClearMatchResult()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameState::ClearMatchResult denied without authority."));
		return;
	}

	if (WinnerTeamId == -1 && !WinReasonTag.IsValid() && !MatchResult.HasWinner() && MatchResult.FinalScores.Num() == 0)
	{
		MatchResult = FGP_MatchResult();
		return;
	}

	const int32 OldWinner = WinnerTeamId;
	const FGameplayTag OldReason = WinReasonTag;
	WinnerTeamId = -1;
	WinReasonTag = FGameplayTag();
	MatchResult = FGP_MatchResult();
	BroadcastMatchResultChanged(OldWinner, WinnerTeamId, OldReason, WinReasonTag);
}

void AGP_GameState::PruneInvalidMainBaseRegistrations()
{
	RegisteredMainBases.RemoveAll([](const TWeakObjectPtr<AGP_MainBase>& Weak)
	{
		return !Weak.IsValid();
	});
}

AGP_GameState::EGP_MainBaseRegisterResult AGP_GameState::RegisterMainBase(AGP_MainBase* MainBase)
{
	if (!HasAuthority())
	{
		return EGP_MainBaseRegisterResult::RejectedNoAuthority;
	}
	if (!IsValid(MainBase))
	{
		return EGP_MainBaseRegisterResult::RejectedInvalidActor;
	}

	PruneInvalidMainBaseRegistrations();

	const int32 TeamId = MainBase->GetTeamId();
	if (TeamId < 1)
	{
		return EGP_MainBaseRegisterResult::RejectedInvalidTeam;
	}

	AGP_MainBase* ExistingForTeam = nullptr;
	for (const TWeakObjectPtr<AGP_MainBase>& ExistingWeak : RegisteredMainBases)
	{
		AGP_MainBase* Existing = ExistingWeak.Get();
		if (!IsValid(Existing))
		{
			continue;
		}
		if (Existing == MainBase)
		{
			UE_LOG(LogTemp, Log,
				TEXT("AGP_GameState::RegisterMainBase: AlreadyRegistered MainBase=%s TeamId=%d CountForTeam=%d"),
				*GetNameSafe(MainBase),
				TeamId,
				CountRegisteredMainBasesForTeam(TeamId));
			return EGP_MainBaseRegisterResult::AlreadyRegistered;
		}
		if (Existing->GetTeamId() == TeamId)
		{
			ExistingForTeam = Existing;
		}
	}

	if (ExistingForTeam != nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_GameState::RegisterMainBase: Rejected DuplicateMainBaseForTeam TeamId=%d Existing=%s New=%s Registered=false CountForTeam=%d ResolvedMainBase=%s"),
			TeamId,
			*GetNameSafe(ExistingForTeam),
			*GetNameSafe(MainBase),
			CountRegisteredMainBasesForTeam(TeamId),
			*GetNameSafe(ExistingForTeam));
		return EGP_MainBaseRegisterResult::RejectedDuplicate;
	}

	RegisteredMainBases.Add(MainBase);
	OnMainBaseRegistered.Broadcast(MainBase);
	SetReplicatedMainBaseForTeam(TeamId, MainBase);
	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameState::RegisterMainBase: Registered=true MainBase=%s TeamId=%d CountForTeam=%d RegistrySize=%d"),
		*GetNameSafe(MainBase),
		TeamId,
		CountRegisteredMainBasesForTeam(TeamId),
		RegisteredMainBases.Num());
	return EGP_MainBaseRegisterResult::Registered;
}

void AGP_GameState::UnregisterMainBase(AGP_MainBase* MainBase)
{
	PruneInvalidMainBaseRegistrations();

	if (MainBase == nullptr)
	{
		return;
	}

	// Resolve team from replicated handle by actor pointer (TeamId on the actor may already have changed).
	int32 ResolvedTeamId = INDEX_NONE;
	for (const FGP_ReplicatedMainBaseEntry& Entry : ReplicatedMainBases)
	{
		if (Entry.MainBase == MainBase)
		{
			ResolvedTeamId = Entry.TeamId;
			break;
		}
	}

	const int32 Removed = RegisteredMainBases.RemoveAll([MainBase](const TWeakObjectPtr<AGP_MainBase>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == MainBase;
	});
	if (Removed > 0)
	{
		OnMainBaseUnregistered.Broadcast(MainBase);
		if (ResolvedTeamId >= 1)
		{
			SetReplicatedMainBaseForTeam(ResolvedTeamId, nullptr);
		}
	}
}

AGP_MainBase* AGP_GameState::FindMainBaseForTeam(int32 InTeamId) const
{
	if (InTeamId < 1)
	{
		return nullptr;
	}

	AGP_MainBase* Found = nullptr;
	int32 Count = 0;
	for (const TWeakObjectPtr<AGP_MainBase>& Weak : RegisteredMainBases)
	{
		AGP_MainBase* Candidate = Weak.Get();
		if (!IsValid(Candidate) || Candidate->GetTeamId() != InTeamId)
		{
			continue;
		}
		++Count;
		if (Found == nullptr)
		{
			Found = Candidate;
		}
	}

	if (Count > 1)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_GameState::FindMainBaseForTeam: invariant broken TeamId=%d Count=%d using first=%s"),
			InTeamId,
			Count,
			*GetNameSafe(Found));
	}
	return Found;
}

int32 AGP_GameState::CountRegisteredMainBasesForTeam(int32 InTeamId) const
{
	if (InTeamId < 1)
	{
		return 0;
	}

	int32 Count = 0;
	for (const TWeakObjectPtr<AGP_MainBase>& Weak : RegisteredMainBases)
	{
		AGP_MainBase* Candidate = Weak.Get();
		if (IsValid(Candidate) && Candidate->GetTeamId() == InTeamId)
		{
			++Count;
		}
	}
	return Count;
}

bool AGP_GameState::IsMainBaseRegistryUniqueForTeam(int32 InTeamId) const
{
	return CountRegisteredMainBasesForTeam(InTeamId) <= 1;
}

AGP_MainBase* AGP_GameState::FindMainBaseForTeamClientSafe(int32 InTeamId) const
{
	if (InTeamId < 1)
	{
		return nullptr;
	}

	for (const FGP_ReplicatedMainBaseEntry& Entry : ReplicatedMainBases)
	{
		if (Entry.TeamId != InTeamId)
		{
			continue;
		}
		return IsValid(Entry.MainBase) ? Entry.MainBase.Get() : nullptr;
	}
	return nullptr;
}

int32 AGP_GameState::FindReplicatedMainBaseIndex(int32 TeamId) const
{
	for (int32 Index = 0; Index < ReplicatedMainBases.Num(); ++Index)
	{
		if (ReplicatedMainBases[Index].TeamId == TeamId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void AGP_GameState::NotifyResolvedMainBaseChanges(
	const TArray<FGP_ReplicatedMainBaseEntry>& PreviousEntries,
	const TArray<FGP_ReplicatedMainBaseEntry>& NewEntries)
{
	TSet<int32> TeamIds;
	for (const FGP_ReplicatedMainBaseEntry& Entry : PreviousEntries)
	{
		if (Entry.TeamId >= 1)
		{
			TeamIds.Add(Entry.TeamId);
		}
	}
	for (const FGP_ReplicatedMainBaseEntry& Entry : NewEntries)
	{
		if (Entry.TeamId >= 1)
		{
			TeamIds.Add(Entry.TeamId);
		}
	}

	auto Resolve = [](const TArray<FGP_ReplicatedMainBaseEntry>& Entries, int32 TeamId) -> AGP_MainBase*
	{
		for (const FGP_ReplicatedMainBaseEntry& Entry : Entries)
		{
			if (Entry.TeamId == TeamId)
			{
				return IsValid(Entry.MainBase) ? Entry.MainBase.Get() : nullptr;
			}
		}
		return nullptr;
	};

	for (const int32 TeamId : TeamIds)
	{
		AGP_MainBase* Previous = Resolve(PreviousEntries, TeamId);
		AGP_MainBase* NewBase = Resolve(NewEntries, TeamId);
		if (Previous != NewBase)
		{
			OnResolvedMainBaseChanged.Broadcast(TeamId, Previous, NewBase);
		}
	}
}

void AGP_GameState::SetReplicatedMainBaseForTeam(int32 TeamId, AGP_MainBase* NewMainBase)
{
	if (TeamId < 1)
	{
		return;
	}

	const TArray<FGP_ReplicatedMainBaseEntry> Previous = ReplicatedMainBases;
	const int32 Index = FindReplicatedMainBaseIndex(TeamId);
	if (IsValid(NewMainBase))
	{
		if (Index == INDEX_NONE)
		{
			FGP_ReplicatedMainBaseEntry Entry;
			Entry.TeamId = TeamId;
			Entry.MainBase = NewMainBase;
			ReplicatedMainBases.Add(Entry);
		}
		else
		{
			ReplicatedMainBases[Index].MainBase = NewMainBase;
		}
	}
	else if (Index != INDEX_NONE)
	{
		ReplicatedMainBases.RemoveAt(Index);
	}

	PreviousReplicatedMainBases = ReplicatedMainBases;
	NotifyResolvedMainBaseChanges(Previous, ReplicatedMainBases);
}

void AGP_GameState::OnRep_ReplicatedMainBases()
{
	NotifyResolvedMainBaseChanges(PreviousReplicatedMainBases, ReplicatedMainBases);
	PreviousReplicatedMainBases = ReplicatedMainBases;
}

void AGP_GameState::PruneInvalidResourceNodeRegistrations()
{
	RegisteredResourceNodes.RemoveAll([](const TWeakObjectPtr<AGP_ResourceNode>& Weak)
	{
		const AGP_ResourceNode* Node = Weak.Get();
		return !IsValid(Node) || Node->IsActorBeingDestroyed();
	});
}

AGP_GameState::EGP_ResourceNodeRegisterResult AGP_GameState::RegisterResourceNode(AGP_ResourceNode* ResourceNode)
{
	if (!HasAuthority())
	{
		return EGP_ResourceNodeRegisterResult::RejectedNoAuthority;
	}
	if (!IsValid(ResourceNode) || ResourceNode->IsActorBeingDestroyed())
	{
		return EGP_ResourceNodeRegisterResult::RejectedInvalidActor;
	}
	if (ResourceNode->HasCompletedDepletionTransition() || ResourceNode->IsDestroyPending())
	{
		return EGP_ResourceNodeRegisterResult::RejectedDepletedOrPendingDestroy;
	}

	PruneInvalidResourceNodeRegistrations();

	for (const TWeakObjectPtr<AGP_ResourceNode>& Weak : RegisteredResourceNodes)
	{
		if (Weak.Get() == ResourceNode)
		{
			return EGP_ResourceNodeRegisterResult::AlreadyRegistered;
		}
	}

	RegisteredResourceNodes.Add(ResourceNode);
	OnResourceNodeRegistered.Broadcast(ResourceNode);
	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameState::RegisterResourceNode: Registered=%s RegistrySize=%d"),
		*GetNameSafe(ResourceNode),
		RegisteredResourceNodes.Num());
	return EGP_ResourceNodeRegisterResult::Registered;
}

void AGP_GameState::UnregisterResourceNode(AGP_ResourceNode* ResourceNode)
{
	PruneInvalidResourceNodeRegistrations();
	if (ResourceNode == nullptr)
	{
		return;
	}

	const int32 Removed = RegisteredResourceNodes.RemoveAll([ResourceNode](const TWeakObjectPtr<AGP_ResourceNode>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == ResourceNode;
	});
	if (Removed > 0)
	{
		OnResourceNodeUnregistered.Broadcast(ResourceNode);
	}
}

int32 AGP_GameState::GetRegisteredResourceNodeCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AGP_ResourceNode>& Weak : RegisteredResourceNodes)
	{
		if (Weak.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

bool AGP_GameState::EvaluateResourceNodePath(
	const FGP_ResourceNodeSearchQuery& Query,
	AGP_ResourceNode* Node,
	float& OutPathLengthCm,
	float& OutDirectDistanceCm) const
{
	OutPathLengthCm = 0.0f;
	OutDirectDistanceCm = 0.0f;
	if (!IsValid(Node) || Query.PathfindingActor == nullptr)
	{
		return false;
	}

	OutDirectDistanceCm = FVector::Dist(Query.SearchCenter, Node->GetActorLocation());
	if (OutDirectDistanceCm > Query.SearchRadiusCm + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	GPResourceApproach::FEvaluateParams Params;
	Params.PathStart = Query.PathStart;
	Params.InteractionRangeCm = Query.InteractionRangeCm;
	Params.AcceptanceRadiusCm = Query.AcceptanceRadiusCm;
	Params.SafetyMarginCm = Query.ApproachSafetyMarginCm;
	Params.MaxPathLengthCm = Query.MaxPathLengthCm;
	Params.DirectionCount = Query.ApproachDirectionCount;
	Params.PathfindingActor = Query.PathfindingActor;

	const GPResourceApproach::FEvaluateResult Eval =
		GPResourceApproach::EvaluateNodeApproachPath(GetWorld(), Node, Params);
	if (!Eval.bReachable)
	{
		return false;
	}

	OutPathLengthCm = Eval.PathLengthCm;
	return true;
}

void AGP_GameState::FindResourceCandidates(
	const FGP_ResourceNodeSearchQuery& Query,
	TArray<FGP_ResourceNodeCandidate>& OutCandidates) const
{
	OutCandidates.Reset();
	if (!HasAuthority())
	{
		return;
	}

	int32 RegistryCount = 0;
	for (const TWeakObjectPtr<AGP_ResourceNode>& Weak : RegisteredResourceNodes)
	{
		if (Weak.IsValid())
		{
			++RegistryCount;
		}
	}

#if !UE_BUILD_SHIPPING
	const bool bLog = Query.bLogDiagnostics;
	if (bLog)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("GP ResourceReassignmentSearch: Reason=%s SearchCenter=(%.0f,%.0f,%.0f) PathStart=(%.0f,%.0f,%.0f) Radius=%.0f MaxPath=%.0f RegistryCount=%d RequireFreeSlot=%s Exclude=%s"),
			*Query.SearchReason.ToString(),
			Query.SearchCenter.X, Query.SearchCenter.Y, Query.SearchCenter.Z,
			Query.PathStart.X, Query.PathStart.Y, Query.PathStart.Z,
			Query.SearchRadiusCm,
			Query.MaxPathLengthCm,
			RegistryCount,
			Query.bRequireFreeSlot ? TEXT("true") : TEXT("false"),
			*GetNameSafe(Query.ExcludeNode));
	}

	auto ResourceTypeLabel = [](const UGP_ResourceDefinition* Def) -> FString
	{
		if (Def == nullptr)
		{
			return FString(TEXT("None"));
		}
		switch (Def->ResourceType)
		{
		case EGP_ResourceType::None: return FString(TEXT("None"));
		case EGP_ResourceType::Ore: return FString(TEXT("Ore"));
		default: return FString(TEXT("Other"));
		}
	};
#else
	(void)RegistryCount;
#endif

	for (const TWeakObjectPtr<AGP_ResourceNode>& Weak : RegisteredResourceNodes)
	{
		AGP_ResourceNode* Node = Weak.Get();
		if (!IsValid(Node) || Node->IsActorBeingDestroyed())
		{
			continue;
		}

		const FVector NodeLoc = Node->GetActorLocation();
		const UGP_ResourceDefinition* NodeDef = Node->ResolveResourceDefinition(true);
		const int32 ActiveCount = Node->GetActiveMinerCount();
		const int32 WaitingCount = Node->GetWaitingMinerCount();
		const int32 MaxMiners = Node->GetMaxConcurrentMiners();
		const bool bHasFreeSlot = ActiveCount < MaxMiners;
		const float DistFromCenter = FVector::Dist(Query.SearchCenter, NodeLoc);

		FVector Approach = FVector::ZeroVector;
		float PathLength = -1.0f;

		auto FinalizeReject = [&](EGP_ResourceCandidateRejectReason Reason)
		{
#if !UE_BUILD_SHIPPING
			if (bLog)
			{
				UE_LOG(LogTemp, Log,
					TEXT("GP ResourceCandidate Rejected: Candidate=%s Loc=%s Type=%s Depleted=%s DestroyPending=%s Amount=%d Active=%d Waiting=%d Max=%d HasFreeSlot=%s DistFromCenter=%.1f PathStart=(%.0f,%.0f,%.0f) Approach=%s PathLength=%.1f Reason=%s"),
					*GetNameSafe(Node),
					*NodeLoc.ToCompactString(),
					*ResourceTypeLabel(NodeDef),
					Node->IsDepleted() || Node->HasCompletedDepletionTransition() ? TEXT("true") : TEXT("false"),
					Node->IsDestroyPending() ? TEXT("true") : TEXT("false"),
					Node->GetCurrentAmount(),
					ActiveCount,
					WaitingCount,
					MaxMiners,
					bHasFreeSlot ? TEXT("true") : TEXT("false"),
					DistFromCenter,
					Query.PathStart.X, Query.PathStart.Y, Query.PathStart.Z,
					*Approach.ToCompactString(),
					PathLength,
					GPResourceApproach::RejectReasonToString(Reason));
			}
#else
			(void)Reason;
#endif
		};

		if (Node == Query.ExcludeNode)
		{
			FinalizeReject(EGP_ResourceCandidateRejectReason::ExcludedNode);
			continue;
		}
		if (Node->IsDestroyPending())
		{
			FinalizeReject(EGP_ResourceCandidateRejectReason::DestroyPending);
			continue;
		}
		if (Node->HasCompletedDepletionTransition() || Node->IsDepleted())
		{
			FinalizeReject(EGP_ResourceCandidateRejectReason::Depleted);
			continue;
		}
		if (Node->IsClearingOccupancy())
		{
			FinalizeReject(EGP_ResourceCandidateRejectReason::ClearingOccupancy);
			continue;
		}
		if (!Node->CanAcceptMineCommand(true, nullptr))
		{
			FinalizeReject(EGP_ResourceCandidateRejectReason::MineRejected);
			continue;
		}
		if (NodeDef == nullptr && Query.CompatibleDefinition != nullptr)
		{
			FinalizeReject(EGP_ResourceCandidateRejectReason::UnresolvedDefinition);
			continue;
		}
		if (Query.CompatibleDefinition != nullptr)
		{
			const bool bSameAsset = NodeDef == Query.CompatibleDefinition;
			const bool bSameType =
				NodeDef != nullptr
				&& Query.CompatibleDefinition->ResourceType != EGP_ResourceType::None
				&& NodeDef->ResourceType == Query.CompatibleDefinition->ResourceType;
			if (!bSameAsset && !bSameType)
			{
				FinalizeReject(EGP_ResourceCandidateRejectReason::IncompatibleResourceType);
				continue;
			}
		}
		if (Query.bRequireFreeSlot && !bHasFreeSlot)
		{
			FinalizeReject(EGP_ResourceCandidateRejectReason::NoFreeSlot);
			continue;
		}
		if (DistFromCenter > Query.SearchRadiusCm + KINDA_SMALL_NUMBER)
		{
			FinalizeReject(EGP_ResourceCandidateRejectReason::OutsideSearchRadius);
			continue;
		}

		GPResourceApproach::FEvaluateParams Params;
		Params.PathStart = Query.PathStart;
		Params.InteractionRangeCm = Query.InteractionRangeCm;
		Params.AcceptanceRadiusCm = Query.AcceptanceRadiusCm;
		Params.SafetyMarginCm = Query.ApproachSafetyMarginCm;
		Params.MaxPathLengthCm = Query.MaxPathLengthCm;
		Params.DirectionCount = Query.ApproachDirectionCount;
		Params.PathfindingActor = Query.PathfindingActor;

		const GPResourceApproach::FEvaluateResult Eval =
			GPResourceApproach::EvaluateNodeApproachPath(GetWorld(), Node, Params);
		Approach = Eval.BestApproachLocation;
		PathLength = Eval.PathLengthCm;
		if (!Eval.bReachable)
		{
			FinalizeReject(Eval.RejectReason != EGP_ResourceCandidateRejectReason::None
				? Eval.RejectReason
				: EGP_ResourceCandidateRejectReason::CandidateProjectionFailed);
			continue;
		}

		FGP_ResourceNodeCandidate Candidate;
		Candidate.Node = Node;
		Candidate.BestApproachLocation = Eval.BestApproachLocation;
		Candidate.PathLengthCm = Eval.PathLengthCm;
		Candidate.DirectDistanceCm = DistFromCenter;
		Candidate.bHasFreeSlot = bHasFreeSlot;
#if !UE_BUILD_SHIPPING
		Candidate.RejectReason = EGP_ResourceCandidateRejectReason::Accepted;
		if (bLog)
		{
			UE_LOG(LogTemp, Log,
				TEXT("GP ResourceCandidate Accepted: Candidate=%s Loc=%s Type=%s Depleted=false DestroyPending=false Amount=%d Active=%d Waiting=%d Max=%d HasFreeSlot=%s DistFromCenter=%.1f PathStart=(%.0f,%.0f,%.0f) Approach=%s PathLength=%.1f Reason=Accepted"),
				*GetNameSafe(Node),
				*NodeLoc.ToCompactString(),
				*ResourceTypeLabel(NodeDef),
				Node->GetCurrentAmount(),
				ActiveCount,
				WaitingCount,
				MaxMiners,
				bHasFreeSlot ? TEXT("true") : TEXT("false"),
				DistFromCenter,
				Query.PathStart.X, Query.PathStart.Y, Query.PathStart.Z,
				*Eval.BestApproachLocation.ToCompactString(),
				Eval.PathLengthCm);
		}
#endif
		OutCandidates.Add(Candidate);
	}

	OutCandidates.Sort([PreferFree = Query.bPreferFreeSlot](const FGP_ResourceNodeCandidate& A, const FGP_ResourceNodeCandidate& B)
	{
		if (PreferFree && A.bHasFreeSlot != B.bHasFreeSlot)
		{
			return A.bHasFreeSlot && !B.bHasFreeSlot;
		}
		if (!FMath::IsNearlyEqual(A.PathLengthCm, B.PathLengthCm))
		{
			return A.PathLengthCm < B.PathLengthCm;
		}
		if (!FMath::IsNearlyEqual(A.DirectDistanceCm, B.DirectDistanceCm))
		{
			return A.DirectDistanceCm < B.DirectDistanceCm;
		}
		const FString NameA = GetNameSafe(A.Node);
		const FString NameB = GetNameSafe(B.Node);
		return NameA < NameB;
	});

#if !UE_BUILD_SHIPPING
	if (bLog)
	{
		if (OutCandidates.Num() == 0)
		{
			UE_LOG(LogTemp, Log,
				TEXT("GP ResourceReassignmentNoCandidate: Reason=%s SearchCenter=(%.0f,%.0f,%.0f) PathStart=(%.0f,%.0f,%.0f) Radius=%.0f MaxPath=%.0f RegistryCount=%d"),
				*Query.SearchReason.ToString(),
				Query.SearchCenter.X, Query.SearchCenter.Y, Query.SearchCenter.Z,
				Query.PathStart.X, Query.PathStart.Y, Query.PathStart.Z,
				Query.SearchRadiusCm,
				Query.MaxPathLengthCm,
				RegistryCount);
		}
		else
		{
			const FGP_ResourceNodeCandidate& Best = OutCandidates[0];
			UE_LOG(LogTemp, Log,
				TEXT("GP ResourceReassignmentSelected: Reason=%s Node=%s Approach=%s DistFromCenter=%.1f PathLen=%.1f FreeSlot=%s Candidates=%d"),
				*Query.SearchReason.ToString(),
				*GetNameSafe(Best.Node),
				*Best.BestApproachLocation.ToCompactString(),
				Best.DirectDistanceCm,
				Best.PathLengthCm,
				Best.bHasFreeSlot ? TEXT("true") : TEXT("false"),
				OutCandidates.Num());
		}
	}
#else
	(void)RegistryCount;
#endif
}

AGP_ResourceNode* AGP_GameState::FindBestResourceCandidate(const FGP_ResourceNodeSearchQuery& Query) const
{
	TArray<FGP_ResourceNodeCandidate> Candidates;
	FindResourceCandidates(Query, Candidates);
	return Candidates.Num() > 0 ? Candidates[0].Node.Get() : nullptr;
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

void AGP_GameState::OnRep_TeamFerroniteThreatValues()
{
	for (const FGP_TeamFerroniteThreat& Entry : TeamFerroniteThreatValues)
	{
		OnTeamFerroniteThreatValueChanged.Broadcast(Entry.TeamId, Entry.ThreatValue, Entry.ThreatValue);
	}
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

void AGP_GameState::OnRep_MatchResult(const FGP_MatchResult& OldMatchResult)
{
	BroadcastMatchResultChanged(
		OldMatchResult.WinnerTeamId,
		MatchResult.WinnerTeamId,
		OldMatchResult.WinnerReason,
		MatchResult.WinnerReason);
}
