// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Native GP.* gameplay tag registry.
 * Access via FGPGameplayTags::Get(). Do not use magic-string RequestGameplayTag in project code.
 */
struct GPGASRUNTIME_API FGPGameplayTags
{
public:
	static const FGPGameplayTags& Get();
	static void InitializeNativeTags();

	// Match.State
	FGameplayTag Match_State_Loading;
	FGameplayTag Match_State_WaitingForPlayers;
	FGameplayTag Match_State_Playing;
	FGameplayTag Match_State_Paused;
	FGameplayTag Match_State_Finished;

	// Match.WinReason
	FGameplayTag Match_WinReason_DeliveryQuota;
	FGameplayTag Match_WinReason_TimerScore;
	FGameplayTag Match_WinReason_Annihilation;
	FGameplayTag Match_WinReason_OpponentDisconnect;

	// Unit.Type
	FGameplayTag Unit_Type_Worker;
	FGameplayTag Unit_Type_SalvageWalker;
	FGameplayTag Unit_Type_Combat;
	FGameplayTag Unit_Type_Support;
	FGameplayTag Unit_Type_Building;

	// Unit.State
	FGameplayTag Unit_State_Moving;
	FGameplayTag Unit_State_Mining;
	FGameplayTag Unit_State_Repairing;
	FGameplayTag Unit_State_Attacking;
	FGameplayTag Unit_State_AttackCooldown;
	FGameplayTag Unit_State_Dead;
	FGameplayTag Unit_State_Stunned;

	// Building.Type
	FGameplayTag Building_Type_MainBase;
	FGameplayTag Building_Type_LogisticsHub;
	FGameplayTag Building_Type_DefensiveTurret;
	FGameplayTag Building_Type_Wall;
	FGameplayTag Building_Type_WallTurret;
	FGameplayTag Building_Type_FerroniteDeposit;

	// Building.Role
	FGameplayTag Building_Role_Command;
	FGameplayTag Building_Role_Logistics;
	FGameplayTag Building_Role_Defense;
	FGameplayTag Building_Role_Resource;

	// Command
	FGameplayTag Command_Move;
	FGameplayTag Command_Stop;
	FGameplayTag Command_Attack;
	FGameplayTag Command_AttackMove;
	FGameplayTag Command_Mine;
	FGameplayTag Command_Repair;
	FGameplayTag Command_Sell;
	FGameplayTag Command_Demolish;
	FGameplayTag Command_OrderDrop;
	FGameplayTag Command_CancelOrder;

	// Drop.Type
	FGameplayTag Drop_Type_Unit;
	FGameplayTag Drop_Type_Building;
	FGameplayTag Drop_Type_Wall;
	FGameplayTag Drop_Type_WallPackage;
	FGameplayTag Drop_Type_Module;

	// State
	FGameplayTag State_PodInFlight;

	// Resource
	FGameplayTag Resource_Type_Ferronite;
	FGameplayTag Resource_Node;

	// Ability
	FGameplayTag Ability_Repair;
	FGameplayTag Ability_State_Channeling;

	// Capability / Selection
	FGameplayTag Capability_Selectable;
	FGameplayTag Capability_Inspectable;
	FGameplayTag Selection_Type_Unit;
	FGameplayTag Selection_Type_Building;

	// Faction / Team
	FGameplayTag Faction_Corporate;
	FGameplayTag Team_Neutral;
	FGameplayTag Team_Player_One;
	FGameplayTag Team_Player_Two;

	// Effect.Source
	FGameplayTag Effect_Source_UnitCapBuilding;

	// Notify
	FGameplayTag Notify_InsufficientOrbitalFerronite;
	FGameplayTag Notify_UnitCapReached;
	FGameplayTag Notify_WorkerIdle;
	FGameplayTag Notify_BaseUnderAttack;
	FGameplayTag Notify_DropRejected;
	FGameplayTag Notify_MatchEndingSoon;
	FGameplayTag Notify_CommandRejected;

private:
	static FGPGameplayTags GameplayTags;

	void AddAllTags();
	static void AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);
};
