// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tags/GPGameplayTags.h"
#include "GameplayTagsManager.h"

FGPGameplayTags FGPGameplayTags::GameplayTags;

const FGPGameplayTags& FGPGameplayTags::Get()
{
	return GameplayTags;
}

void FGPGameplayTags::InitializeNativeTags()
{
	static bool bNativeTagsRegistered = false;
	if (bNativeTagsRegistered)
	{
		return;
	}

	GameplayTags.AddAllTags();
	bNativeTagsRegistered = true;
}

void FGPGameplayTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName), FString(TagComment));
}

void FGPGameplayTags::AddAllTags()
{
	// Match.State
	AddTag(Match_State_Loading, "GP.Match.State.Loading", "Match is loading assets and initializing session state.");
	AddTag(Match_State_WaitingForPlayers, "GP.Match.State.WaitingForPlayers", "Match lobby / waiting for required players before start.");
	AddTag(Match_State_Playing, "GP.Match.State.Playing", "Active match gameplay is running.");
	AddTag(Match_State_Paused, "GP.Match.State.Paused", "Match is paused.");
	AddTag(Match_State_Finished, "GP.Match.State.Finished", "Match has ended; post-match / results state.");

	// Match.WinReason
	AddTag(Match_WinReason_DeliveryQuota, "GP.Match.WinReason.DeliveryQuota", "Victory by reaching DeliveryQuotaFerroniteScore first.");
	AddTag(Match_WinReason_TimerScore, "GP.Match.WinReason.TimerScore", "Victory by highest FerroniteScore when the match timer expires.");
	AddTag(Match_WinReason_Annihilation, "GP.Match.WinReason.Annihilation", "Victory because the opponent MainBase was destroyed.");
	AddTag(Match_WinReason_OpponentDisconnect, "GP.Match.WinReason.OpponentDisconnect", "Victory because the opponent disconnected.");

	// Unit.Type
	AddTag(Unit_Type_Worker, "GP.Unit.Type.Worker", "Worker unit identity.");
	AddTag(Unit_Type_SalvageWalker, "GP.Unit.Type.SalvageWalker", "Salvage Walker combat unit identity.");
	AddTag(Unit_Type_Combat, "GP.Unit.Type.Combat", "Generic combat unit identity category.");
	AddTag(Unit_Type_Support, "GP.Unit.Type.Support", "Support unit identity category.");
	AddTag(Unit_Type_Building, "GP.Unit.Type.Building", "Building treated as a unit-identity category for shared systems.");

	// Unit.State
	AddTag(Unit_State_Moving, "GP.Unit.State.Moving", "Unit is currently moving.");
	AddTag(Unit_State_Mining, "GP.Unit.State.Mining", "Unit is mining a resource node.");
	AddTag(Unit_State_Repairing, "GP.Unit.State.Repairing", "Unit is repairing a target.");
	AddTag(Unit_State_Attacking, "GP.Unit.State.Attacking", "Unit is actively attacking.");
	AddTag(Unit_State_AttackCooldown, "GP.Unit.State.AttackCooldown", "Unit is waiting on attack cooldown.");
	AddTag(Unit_State_Dead, "GP.Unit.State.Dead", "Unit is dead / pending destruction.");
	AddTag(Unit_State_Stunned, "GP.Unit.State.Stunned", "Unit is stunned and cannot act.");

	// Building.Type
	AddTag(Building_Type_MainBase, "GP.Building.Type.MainBase", "Main Base building identity.");
	AddTag(Building_Type_LogisticsHub, "GP.Building.Type.LogisticsHub", "Logistics Hub building identity (orbital order surface).");
	AddTag(Building_Type_DefensiveTurret, "GP.Building.Type.DefensiveTurret", "Defensive turret building identity.");
	AddTag(Building_Type_Wall, "GP.Building.Type.Wall", "Wall building identity.");
	AddTag(Building_Type_WallTurret, "GP.Building.Type.WallTurret", "Wall-mounted turret building identity.");
	AddTag(Building_Type_FerroniteDeposit, "GP.Building.Type.FerroniteDeposit", "Map-placed Ferronite deposit identity.");

	// Building.Role
	AddTag(Building_Role_Command, "GP.Building.Role.Command", "Command / headquarters building role.");
	AddTag(Building_Role_Logistics, "GP.Building.Role.Logistics", "Logistics / orbital-order building role.");
	AddTag(Building_Role_Defense, "GP.Building.Role.Defense", "Defensive building role.");
	AddTag(Building_Role_Resource, "GP.Building.Role.Resource", "Resource-related building or deposit role.");

	// Command
	AddTag(Command_Move, "GP.Command.Move", "Move selected units to a destination.");
	AddTag(Command_Stop, "GP.Command.Stop", "Stop current unit orders.");
	AddTag(Command_Attack, "GP.Command.Attack", "Attack a specific target.");
	AddTag(Command_AttackMove, "GP.Command.AttackMove", "Attack-move to a destination with auto-acquire.");
	AddTag(Command_Patrol, "GP.Command.Patrol", "Patrol between the unit's start location and a destination.");
	AddTag(Command_Mine, "GP.Command.Mine", "Mine a resource node.");
	AddTag(Command_Repair, "GP.Command.Repair", "Worker repair command.");
	AddTag(Command_Sell, "GP.Command.Sell", "Sell a building for partial OrbitalFerronite refund.");
	AddTag(Command_Demolish, "GP.Command.Demolish", "Demolish a wall permanently with no refund.");
	AddTag(Command_OrderDrop, "GP.Command.OrderDrop", "Order an orbital drop via Logistics Hub / Order Menu.");
	AddTag(Command_CancelOrder, "GP.Command.CancelOrder", "Cancel a pending or in-flight orbital order.");

	// Drop.Type
	AddTag(Drop_Type_Unit, "GP.Drop.Type.Unit", "Orbital drop payload is a unit.");
	AddTag(Drop_Type_Building, "GP.Drop.Type.Building", "Orbital drop payload is a building.");
	AddTag(Drop_Type_Wall, "GP.Drop.Type.Wall", "Superseded per-segment wall drop classification.");
	AddTag(Drop_Type_WallPackage, "GP.Drop.Type.WallPackage", "Wall Package delivered to MainBase inventory.");
	AddTag(Drop_Type_Module, "GP.Drop.Type.Module", "Orbital drop payload kind reserved for modules.");

	// State
	AddTag(State_PodInFlight, "GP.State.PodInFlight", "Drop pod is descending / in flight.");

	// Resource
	AddTag(Resource_Type_Ferronite, "GP.Resource.Type.Ferronite", "Primary Ferronite resource identity.");
	AddTag(Resource_Node, "GP.Resource.Node", "Resource node / deposit interaction tag.");

	// Ability
	AddTag(Ability_Repair, "GP.Ability.Repair", "GAS repair ability identity.");
	AddTag(Ability_State_Channeling, "GP.Ability.State.Channeling", "Ability is currently channeling.");

	// Capability / Selection
	AddTag(Capability_Selectable, "GP.Capability.Selectable", "Actor can be selected by the player.");
	AddTag(Capability_Inspectable, "GP.Capability.Inspectable", "Actor can be inspected in the HUD.");
	AddTag(Selection_Type_Unit, "GP.Selection.Type.Unit", "Selection classification for units.");
	AddTag(Selection_Type_Building, "GP.Selection.Type.Building", "Selection classification for buildings.");

	// Faction / Team
	AddTag(Faction_Corporate, "GP.Faction.Corporate", "Corporate player faction identity.");
	AddTag(Team_Neutral, "GP.Team.Neutral", "Neutral team identity.");
	AddTag(Team_Player_One, "GP.Team.Player.One", "Player one team identity.");
	AddTag(Team_Player_Two, "GP.Team.Player.Two", "Player two team identity.");

	// Effect.Source
	AddTag(Effect_Source_UnitCapBuilding, "GP.Effect.Source.UnitCapBuilding", "Gameplay effect sourced from a unit-cap building bonus.");

	// Notify
	AddTag(Notify_InsufficientOrbitalFerronite, "GP.Notify.InsufficientOrbitalFerronite", "HUD notify: insufficient OrbitalFerronite for an order.");
	AddTag(Notify_UnitCapReached, "GP.Notify.UnitCapReached", "HUD notify: unit cap reached; orbital order rejected.");
	AddTag(Notify_WorkerIdle, "GP.Notify.WorkerIdle", "HUD notify: worker is idle with no deposit.");
	AddTag(Notify_BaseUnderAttack, "GP.Notify.BaseUnderAttack", "HUD notify: base is under attack.");
	AddTag(Notify_DropRejected, "GP.Notify.DropRejected", "HUD notify: orbital drop order was rejected.");
	AddTag(Notify_MatchEndingSoon, "GP.Notify.MatchEndingSoon", "HUD notify: match timer is nearly expired.");
	AddTag(Notify_CommandRejected, "GP.Notify.CommandRejected", "HUD notify: a command request was rejected.");
}
