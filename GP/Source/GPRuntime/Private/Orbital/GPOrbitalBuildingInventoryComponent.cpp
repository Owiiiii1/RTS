// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPOrbitalBuildingInventoryComponent.h"

#include "Net/UnrealNetwork.h"

UGP_OrbitalBuildingInventoryComponent::UGP_OrbitalBuildingInventoryComponent()
{
	SetIsReplicatedByDefault(true);
}

void UGP_OrbitalBuildingInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(
		UGP_OrbitalBuildingInventoryComponent,
		ReadyLogisticsHubCount,
		COND_OwnerOnly,
		REPNOTIFY_Always);
}

int32 UGP_OrbitalBuildingInventoryComponent::GetReadyCount(EGP_OrbitalBuildingType BuildingType) const
{
	switch (BuildingType)
	{
	case EGP_OrbitalBuildingType::LogisticsHub:
		return FMath::Max(0, ReadyLogisticsHubCount);
	default:
		return 0;
	}
}

bool UGP_OrbitalBuildingInventoryComponent::AuthorityAddReady(EGP_OrbitalBuildingType BuildingType, int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0)
	{
		return false;
	}

	switch (BuildingType)
	{
	case EGP_OrbitalBuildingType::LogisticsHub:
	{
		const int32 Old = ReadyLogisticsHubCount;
		ReadyLogisticsHubCount = FMath::Max(0, ReadyLogisticsHubCount + Amount);
		if (ReadyLogisticsHubCount != Old)
		{
			BroadcastReadyChanged(BuildingType, ReadyLogisticsHubCount);
		}
		return true;
	}
	default:
		return false;
	}
}

bool UGP_OrbitalBuildingInventoryComponent::AuthorityTryConsumeReady(EGP_OrbitalBuildingType BuildingType, int32 Amount)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0)
	{
		return false;
	}

	switch (BuildingType)
	{
	case EGP_OrbitalBuildingType::LogisticsHub:
	{
		if (ReadyLogisticsHubCount < Amount)
		{
			return false;
		}
		ReadyLogisticsHubCount -= Amount;
		BroadcastReadyChanged(BuildingType, ReadyLogisticsHubCount);
		return true;
	}
	default:
		return false;
	}
}

void UGP_OrbitalBuildingInventoryComponent::OnRep_ReadyLogisticsHubCount(int32 OldCount)
{
	(void)OldCount;
	BroadcastReadyChanged(EGP_OrbitalBuildingType::LogisticsHub, ReadyLogisticsHubCount);
}

void UGP_OrbitalBuildingInventoryComponent::BroadcastReadyChanged(EGP_OrbitalBuildingType BuildingType, int32 NewCount)
{
	OnReadyChanged.Broadcast(BuildingType, NewCount);
}
