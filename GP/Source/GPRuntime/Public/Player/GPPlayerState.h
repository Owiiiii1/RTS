// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GPPlayerState.generated.h"

class AGP_UnitBase;
class UWorld;
class UGP_AbilitySystemComponent;
class UGP_OrbitalBuildingInventoryComponent;
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

	UFUNCTION(BlueprintPure, Category = "GP|Orbital|Building")
	UGP_OrbitalBuildingInventoryComponent* GetOrbitalBuildingInventoryComponent() const;

	/** Authority-only. Silent no-op without authority. */
	void SetTeamId(int32 NewTeamId);

	/** Local/UI: TeamId changed (authority SetTeamId or client OnRep). */
	FOnGP_PlayerTeamIdChanged OnTeamIdChanged;

	/** CurrentUnits + PendingOrbitalUnitCount. Authority accounting. */
	int32 GetCommittedUnitCount() const;

	int32 GetPendingOrbitalUnitCount() const { return PendingOrbitalUnitCount; }

	/** True when Committed + ManifestCount <= MaxUnits. MaxUnits == 0 is not unlimited. */
	bool CanAcceptManifestUnitCount(int32 ManifestUnitCount) const;

	/** Authority: reserve manifest entity count exactly once on accepted unit order. */
	bool TryReserveOrbitalUnits(int32 Count);

	/** Authority: release leftover reservation (failed/incomplete payload). */
	void ReleaseOrbitalUnitReservation(int32 Count);

	/** Authority: living Worker / Salvage Walker entered ownership. */
	void NotifyPlayerUnitBecameLive(AGP_UnitBase* Unit);

	/** Authority: counted unit died or left play. */
	void NotifyPlayerUnitDied(AGP_UnitBase* Unit);

	/** One-shot team lookup (PlayerArray). No tick scans. */
	static AGP_PlayerState* FindAuthoritativeForTeam(const UWorld* World, int32 InTeamId);

protected:
	virtual void BeginPlay() override;
	virtual void ClientInitialize(AController* C) override;

	UFUNCTION()
	void OnRep_TeamId(int32 OldTeamId);

private:
	void InitializeAbilitySystemActorInfo();
	void BroadcastTeamIdChanged(int32 OldTeamId, int32 NewTeamId);
	void ApplyBaseUnitCapIfNeeded();
	void AuthorityCatchUpExistingUnits();
	void AuthorityAdjustCurrentUnits(int32 Delta);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|AbilitySystem", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_AbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Attributes", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_PlayerAttributeSet> PlayerAttributeSet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Orbital|Building", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_OrbitalBuildingInventoryComponent> OrbitalBuildingInventoryComponent;

	/** -1 unassigned, 0 neutral, 1+ playable teams. */
	UPROPERTY(ReplicatedUsing = OnRep_TeamId, VisibleInstanceOnly, Category = "GP|Team")
	int32 TeamId = -1;

	/** Server-authoritative in-flight orbital unit reservations (entity count, not transport slots). */
	int32 PendingOrbitalUnitCount = 0;

	bool bBaseUnitCapApplied = false;
};
