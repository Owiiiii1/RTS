// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayTagContainer.h"
#include "Resources/GPResourceNodeSearch.h"
#include "GPGameState.generated.h"

class AGP_MainBase;
class AGP_ResourceNode;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGP_MatchStateTagChanged, FGameplayTag /*OldTag*/, FGameplayTag /*NewTag*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGP_MatchTimeRemainingChanged, float /*OldTime*/, float /*NewTime*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGP_FerroniteThreatValueChanged, float /*OldThreat*/, float /*NewThreat*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnGP_TeamFerroniteThreatValueChanged,
	int32 /*TeamId*/,
	float /*OldThreat*/,
	float /*NewThreat*/);
DECLARE_MULTICAST_DELEGATE_FourParams(
	FOnGP_MatchResultChanged,
	int32 /*OldWinnerTeamId*/,
	int32 /*NewWinnerTeamId*/,
	FGameplayTag /*OldWinReasonTag*/,
	FGameplayTag /*NewWinReasonTag*/);

/** Authority-only ResourceNode registry lifecycle (GP-S28P2). Not replicated. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGP_ResourceNodeRegistered, AGP_ResourceNode* /*Node*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGP_ResourceNodeUnregistered, AGP_ResourceNode* /*Node*/);

/** Per-team fluctuating Planetary Ferronite threat stock (GP-S28). */
USTRUCT(BlueprintType)
struct FGP_TeamFerroniteThreat
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	int32 TeamId = -1;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	float ThreatValue = 0.0f;
};

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

	/**
	 * Legacy/global scalar mirror (pre-S28).
	 * Synced from TeamId==1 threat when present; otherwise first registered team entry.
	 * Prefer GetFerroniteThreatValueForTeam for multi-base play.
	 */
	UFUNCTION(BlueprintPure, Category = "GP|Match")
	float GetFerroniteThreatValue() const { return FerroniteThreatValue; }

	UFUNCTION(BlueprintPure, Category = "GP|Match|Threat")
	float GetFerroniteThreatValueForTeam(int32 InTeamId) const;

	UFUNCTION(BlueprintPure, Category = "GP|Match|Threat")
	const TArray<FGP_TeamFerroniteThreat>& GetTeamFerroniteThreatValues() const
	{
		return TeamFerroniteThreatValues;
	}

	UFUNCTION(BlueprintPure, Category = "GP|Match")
	int32 GetWinnerTeamId() const { return WinnerTeamId; }

	UFUNCTION(BlueprintPure, Category = "GP|Match")
	FGameplayTag GetWinReasonTag() const { return WinReasonTag; }

	// --- Authority-only mutation (no RPCs) ---

	void SetMatchStateTag(FGameplayTag NewStateTag);
	void SetMatchTimeRemaining(float NewTimeRemaining);

	/** Legacy scalar setter — also writes TeamId 1 entry for compatibility. */
	void SetFerroniteThreatValue(float NewThreatValue);

	/** Authority-only per-team SoT write (clamped >= 0). */
	void SetFerroniteThreatValueForTeam(int32 InTeamId, float NewThreatValue);

	/** Authority-only delta apply (clamped result >= 0). Returns applied delta after clamp. */
	float AddFerroniteThreatValueForTeam(int32 InTeamId, float Delta);

	void SetMatchResult(int32 InWinnerTeamId, FGameplayTag InWinReasonTag);
	void ClearMatchResult();

	/** Result of authority MainBase registry mutation (GP-S28). */
	enum class EGP_MainBaseRegisterResult : uint8
	{
		Registered,
		AlreadyRegistered,
		RejectedNoAuthority,
		RejectedInvalidActor,
		RejectedInvalidTeam,
		RejectedDuplicate
	};

	/** Authority-only MainBase registry (team-scoped; no GetActorOfClass lookups). Exactly one MainBase per playable TeamId. */
	EGP_MainBaseRegisterResult RegisterMainBase(AGP_MainBase* MainBase);
	void UnregisterMainBase(AGP_MainBase* MainBase);
	AGP_MainBase* FindMainBaseForTeam(int32 InTeamId) const;
	int32 CountRegisteredMainBasesForTeam(int32 InTeamId) const;
	bool IsMainBaseRegistryUniqueForTeam(int32 InTeamId) const;
	void PruneInvalidMainBaseRegistrations();

	/** Result of authority ResourceNode registry mutation (GP-S28P2). Multi-entry; not unique per team. */
	enum class EGP_ResourceNodeRegisterResult : uint8
	{
		Registered,
		AlreadyRegistered,
		RejectedNoAuthority,
		RejectedInvalidActor,
		RejectedDepletedOrPendingDestroy
	};

	/**
	 * Authority-only ResourceNode registry (no GetAllActorsOfClass).
	 * Depleted / destroy-pending nodes are not valid search candidates.
	 */
	EGP_ResourceNodeRegisterResult RegisterResourceNode(AGP_ResourceNode* ResourceNode);
	void UnregisterResourceNode(AGP_ResourceNode* ResourceNode);
	void PruneInvalidResourceNodeRegistrations();
	int32 GetRegisteredResourceNodeCount() const;

	/**
	 * Authority path-aware search over registry.
	 * Sorted: free slot (optional prefer) → path length → direct distance → actor name.
	 */
	void FindResourceCandidates(
		const FGP_ResourceNodeSearchQuery& Query,
		TArray<FGP_ResourceNodeCandidate>& OutCandidates) const;

	AGP_ResourceNode* FindBestResourceCandidate(const FGP_ResourceNodeSearchQuery& Query) const;

	/** C++ subscription: ResourceNode registered (authority). */
	FOnGP_ResourceNodeRegistered OnResourceNodeRegistered;

	/** C++ subscription: ResourceNode unregistered (authority). */
	FOnGP_ResourceNodeUnregistered OnResourceNodeUnregistered;

	/** C++ subscription: old/new MatchStateTag. */
	FOnGP_MatchStateTagChanged OnMatchStateTagChanged;

	/** C++ subscription: old/new MatchTimeRemaining. */
	FOnGP_MatchTimeRemainingChanged OnMatchTimeRemainingChanged;

	/** C++ subscription: old/new legacy FerroniteThreatValue scalar. */
	FOnGP_FerroniteThreatValueChanged OnFerroniteThreatValueChanged;

	/** C++ subscription: per-team threat change. */
	FOnGP_TeamFerroniteThreatValueChanged OnTeamFerroniteThreatValueChanged;

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

	/**
	 * Legacy scalar (COND_None). Synced from per-team SoT for Team 1 / first entry.
	 * Per-team SoT: TeamFerroniteThreatValues.
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FerroniteThreatValue, Category = "GP|Match")
	float FerroniteThreatValue = 0.0f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_TeamFerroniteThreatValues, Category = "GP|Match|Threat")
	TArray<FGP_TeamFerroniteThreat> TeamFerroniteThreatValues;

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
	void OnRep_TeamFerroniteThreatValues();

	UFUNCTION()
	void OnRep_WinnerTeamId(int32 OldWinnerTeamId);

	UFUNCTION()
	void OnRep_WinReasonTag(FGameplayTag OldWinReasonTag);

	void BroadcastMatchResultChanged(
		int32 OldWinnerTeamId,
		int32 NewWinnerTeamId,
		FGameplayTag OldWinReasonTag,
		FGameplayTag NewWinReasonTag);

	void SyncLegacyFerroniteThreatScalar();
	int32 FindTeamThreatIndex(int32 InTeamId) const;

	static bool IsMatchStateBranchTag(const FGameplayTag& Tag);
	static bool IsWinReasonBranchTag(const FGameplayTag& Tag);

private:
	/** Authority-only weak registry; not replicated. */
	TArray<TWeakObjectPtr<AGP_MainBase>> RegisteredMainBases;

	/** Authority-only weak ResourceNode registry; not replicated. */
	TArray<TWeakObjectPtr<AGP_ResourceNode>> RegisteredResourceNodes;

	bool EvaluateResourceNodePath(
		const FGP_ResourceNodeSearchQuery& Query,
		AGP_ResourceNode* Node,
		float& OutPathLengthCm,
		float& OutDirectDistanceCm) const;
};
