// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPOrbitalBuildingInventoryComponent.h"

#include "Net/UnrealNetwork.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPOrbitalDropDefinition.h"

UGP_OrbitalBuildingInventoryComponent::UGP_OrbitalBuildingInventoryComponent()
{
	SetIsReplicatedByDefault(true);
}

void UGP_OrbitalBuildingInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(
		UGP_OrbitalBuildingInventoryComponent,
		ReadyEntries,
		COND_OwnerOnly,
		REPNOTIFY_Always);
}

FPrimaryAssetId UGP_OrbitalBuildingInventoryComponent::ResolveLegacyTypeId(EGP_OrbitalBuildingType BuildingType)
{
	if (BuildingType != EGP_OrbitalBuildingType::LogisticsHub)
	{
		return FPrimaryAssetId();
	}
	return UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDropId();
}

int32 UGP_OrbitalBuildingInventoryComponent::FindEntryIndex(const FPrimaryAssetId& DropDefinitionId) const
{
	if (!DropDefinitionId.IsValid())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < ReadyEntries.Num(); ++Index)
	{
		if (ReadyEntries[Index].DropDefinitionId == DropDefinitionId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 UGP_OrbitalBuildingInventoryComponent::GetReadyCount(FPrimaryAssetId DropDefinitionId) const
{
	const int32 Index = FindEntryIndex(DropDefinitionId);
	if (Index == INDEX_NONE)
	{
		return 0;
	}
	return FMath::Max(0, ReadyEntries[Index].ReadyCount);
}

int32 UGP_OrbitalBuildingInventoryComponent::GetReadyCount(const UGP_OrbitalDropDefinition* DropDefinition) const
{
	return IsValid(DropDefinition) ? GetReadyCount(DropDefinition->GetPrimaryAssetId()) : 0;
}

int32 UGP_OrbitalBuildingInventoryComponent::GetReadyCount(EGP_OrbitalBuildingType BuildingType) const
{
	return GetReadyCount(ResolveLegacyTypeId(BuildingType));
}

bool UGP_OrbitalBuildingInventoryComponent::AuthorityAddReady(FPrimaryAssetId DropDefinitionId, int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0 || !DropDefinitionId.IsValid())
	{
		return false;
	}

	int32 Index = FindEntryIndex(DropDefinitionId);
	if (Index == INDEX_NONE)
	{
		FGP_ReadyBuildingEntry Entry;
		Entry.DropDefinitionId = DropDefinitionId;
		Entry.ReadyCount = Amount;
		ReadyEntries.Add(Entry);
		BroadcastReadyChanged(DropDefinitionId, Amount);
		return true;
	}

	const int32 Old = ReadyEntries[Index].ReadyCount;
	ReadyEntries[Index].ReadyCount = FMath::Max(0, ReadyEntries[Index].ReadyCount + Amount);
	if (ReadyEntries[Index].ReadyCount != Old)
	{
		BroadcastReadyChanged(DropDefinitionId, ReadyEntries[Index].ReadyCount);
	}
	return true;
}

bool UGP_OrbitalBuildingInventoryComponent::AuthorityAddReady(
	const UGP_OrbitalDropDefinition* DropDefinition,
	int32 Amount)
{
	return IsValid(DropDefinition) ? AuthorityAddReady(DropDefinition->GetPrimaryAssetId(), Amount) : false;
}

bool UGP_OrbitalBuildingInventoryComponent::AuthorityAddReady(EGP_OrbitalBuildingType BuildingType, int32 Amount)
{
	return AuthorityAddReady(ResolveLegacyTypeId(BuildingType), Amount);
}

bool UGP_OrbitalBuildingInventoryComponent::AuthorityTryConsumeReady(FPrimaryAssetId DropDefinitionId, int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0 || !DropDefinitionId.IsValid())
	{
		return false;
	}

	const int32 Index = FindEntryIndex(DropDefinitionId);
	if (Index == INDEX_NONE || ReadyEntries[Index].ReadyCount < Amount)
	{
		return false;
	}

	ReadyEntries[Index].ReadyCount -= Amount;
	const int32 NewCount = ReadyEntries[Index].ReadyCount;
	if (NewCount <= 0)
	{
		ReadyEntries.RemoveAt(Index);
	}

	BroadcastReadyChanged(DropDefinitionId, FMath::Max(0, NewCount));
	return true;
}

bool UGP_OrbitalBuildingInventoryComponent::AuthorityTryConsumeReady(
	const UGP_OrbitalDropDefinition* DropDefinition,
	int32 Amount)
{
	return IsValid(DropDefinition) ? AuthorityTryConsumeReady(DropDefinition->GetPrimaryAssetId(), Amount) : false;
}

bool UGP_OrbitalBuildingInventoryComponent::AuthorityTryConsumeReady(EGP_OrbitalBuildingType BuildingType, int32 Amount)
{
	return AuthorityTryConsumeReady(ResolveLegacyTypeId(BuildingType), Amount);
}

void UGP_OrbitalBuildingInventoryComponent::OnRep_ReadyEntries()
{
	TSet<FPrimaryAssetId> Touched;
	for (const FGP_ReadyBuildingEntry& Entry : ReadyEntries)
	{
		if (Entry.DropDefinitionId.IsValid())
		{
			Touched.Add(Entry.DropDefinitionId);
			BroadcastReadyChanged(Entry.DropDefinitionId, FMath::Max(0, Entry.ReadyCount));
		}
	}
	for (const FGP_ReadyBuildingEntry& Previous : LastReplicatedEntries)
	{
		if (Previous.DropDefinitionId.IsValid() && !Touched.Contains(Previous.DropDefinitionId))
		{
			BroadcastReadyChanged(Previous.DropDefinitionId, 0);
		}
	}
	LastReplicatedEntries = ReadyEntries;
}

void UGP_OrbitalBuildingInventoryComponent::BroadcastReadyChanged(
	const FPrimaryAssetId& DropDefinitionId,
	int32 NewCount)
{
	OnReadyChanged.Broadcast(DropDefinitionId, NewCount);
}
