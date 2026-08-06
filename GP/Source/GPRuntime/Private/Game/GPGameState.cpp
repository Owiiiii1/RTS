// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/GPGameState.h"

#include "Buildings/GPMainBase.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceNode.h"
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
	DOREPLIFETIME_CONDITION_NOTIFY(AGP_GameState, TeamFerroniteThreatValues, COND_None, REPNOTIFY_Always);
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

	RegisteredMainBases.RemoveAll([MainBase](const TWeakObjectPtr<AGP_MainBase>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == MainBase;
	});
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

	const FVector Start = Query.Origin;
	const FVector End = Node->GetActorLocation();
	OutDirectDistanceCm = FVector::Dist(Start, End);
	if (OutDirectDistanceCm > Query.SearchRadiusCm + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = World != nullptr ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (NavSys == nullptr)
	{
		return false;
	}

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (NavData == nullptr)
	{
		return false;
	}

	FPathFindingQuery PathQuery(
		Query.PathfindingActor.Get(),
		*NavData,
		Start,
		End);
	PathQuery.SetAllowPartialPaths(false);

	const FPathFindingResult PathResult = NavSys->FindPathSync(PathQuery);
	if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid() || PathResult.IsPartial())
	{
		return false;
	}

	OutPathLengthCm = PathResult.Path->GetLength();
	if (!FMath::IsFinite(OutPathLengthCm) || OutPathLengthCm > Query.MaxPathLengthCm + KINDA_SMALL_NUMBER)
	{
		return false;
	}

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

	for (const TWeakObjectPtr<AGP_ResourceNode>& Weak : RegisteredResourceNodes)
	{
		AGP_ResourceNode* Node = Weak.Get();
		if (!IsValid(Node)
			|| Node->IsActorBeingDestroyed()
			|| Node == Query.ExcludeNode
			|| Node->HasCompletedDepletionTransition()
			|| Node->IsDestroyPending()
			|| Node->IsDepleted()
			|| Node->IsClearingOccupancy())
		{
			continue;
		}

		if (!Node->CanAcceptMineCommand(true, nullptr))
		{
			continue;
		}

		if (Query.CompatibleDefinition != nullptr)
		{
			const UGP_ResourceDefinition* NodeDef = Node->ResolveResourceDefinition(true);
			const bool bSameAsset = NodeDef == Query.CompatibleDefinition;
			const bool bSameType =
				NodeDef != nullptr
				&& Query.CompatibleDefinition->ResourceType != EGP_ResourceType::None
				&& NodeDef->ResourceType == Query.CompatibleDefinition->ResourceType;
			if (!bSameAsset && !bSameType)
			{
				continue;
			}
		}

		const bool bHasFreeSlot = Node->GetActiveMinerCount() < Node->GetMaxConcurrentMiners();
		if (Query.bRequireFreeSlot && !bHasFreeSlot)
		{
			continue;
		}

		float PathLength = 0.0f;
		float DirectDistance = 0.0f;
		if (!EvaluateResourceNodePath(Query, Node, PathLength, DirectDistance))
		{
			continue;
		}

		FGP_ResourceNodeCandidate Candidate;
		Candidate.Node = Node;
		Candidate.PathLengthCm = PathLength;
		Candidate.DirectDistanceCm = DirectDistance;
		Candidate.bHasFreeSlot = bHasFreeSlot;
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
