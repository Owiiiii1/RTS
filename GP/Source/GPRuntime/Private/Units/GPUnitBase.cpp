// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitBase.h"

#include "Net/UnrealNetwork.h"
#include "Tags/GPGameplayTags.h"

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

void AGP_UnitBase::OnRep_TeamId()
{
}
