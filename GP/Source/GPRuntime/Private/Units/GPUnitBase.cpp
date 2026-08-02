// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitBase.h"

#include "Command/GPUnitCommand.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GPGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPUnitCommand, Log, All);

namespace GPUnitCommandPrivate
{
	static const TCHAR* NetModeToString(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None:
			return TEXT("None");
		case ROLE_SimulatedProxy:
			return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy:
			return TEXT("AutonomousProxy");
		case ROLE_Authority:
			return TEXT("Authority");
		default:
			return TEXT("Unknown");
		}
	}
}

AGP_UnitBase::AGP_UnitBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
}

void AGP_UnitBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AGP_UnitBase, TeamId, COND_None, REPNOTIFY_OnChanged);
}

int32 AGP_UnitBase::GetTeamId() const
{
	return TeamId;
}

void AGP_UnitBase::SetTeamId(int32 NewTeamId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (TeamId == NewTeamId)
	{
		return;
	}

	TeamId = NewTeamId;
}

bool AGP_UnitBase::IsNeutral() const
{
	return TeamId == 0;
}

bool AGP_UnitBase::HasAssignedTeam() const
{
	return TeamId >= 0;
}

const FGameplayTagContainer& AGP_UnitBase::GetCapabilityTags() const
{
	return CapabilityTags;
}

bool AGP_UnitBase::HasCapabilityTag(FGameplayTag CapabilityTag) const
{
	if (!CapabilityTag.IsValid())
	{
		return false;
	}

	return CapabilityTags.HasTagExact(CapabilityTag);
}

bool AGP_UnitBase::IsGameplaySelectable() const
{
	return HasCapabilityTag(FGPGameplayTags::Get().Capability_Selectable);
}

bool AGP_UnitBase::IsGameplayInspectable() const
{
	return HasCapabilityTag(FGPGameplayTags::Get().Capability_Inspectable);
}

bool AGP_UnitBase::IsSelectionTypeUnit() const
{
	return HasCapabilityTag(FGPGameplayTags::Get().Selection_Type_Unit);
}

bool AGP_UnitBase::IsSelectionTypeBuilding() const
{
	return HasCapabilityTag(FGPGameplayTags::Get().Selection_Type_Building);
}

void AGP_UnitBase::ReceiveCommand(const FGP_UnitCommand& Command)
{
	if (!HasAuthority())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const ENetMode NetMode = World != nullptr ? World->GetNetMode() : NM_MAX;

	UE_LOG(LogGPUnitCommand, Log,
		TEXT("GP UnitCommand Received: Unit=%s Team=%d Tag=%s TargetActor=%s Loc=%s Queue=%s Role=%s NetMode=%s"),
		*GetName(),
		GetTeamId(),
		*Command.CommandTag.ToString(),
		*GetNameSafe(Command.TargetActor),
		*Command.TargetLocation.ToCompactString(),
		Command.bQueue ? TEXT("true") : TEXT("false"),
		GPUnitCommandPrivate::RoleToString(GetLocalRole()),
		GPUnitCommandPrivate::NetModeToString(NetMode));
}

void AGP_UnitBase::OnRep_TeamId()
{
}
