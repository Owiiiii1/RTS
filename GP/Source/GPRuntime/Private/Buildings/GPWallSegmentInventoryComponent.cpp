// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPWallSegmentInventoryComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UGP_WallSegmentInventoryComponent::UGP_WallSegmentInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UGP_WallSegmentInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGP_WallSegmentInventoryComponent, WallSegmentCount);
	DOREPLIFETIME(UGP_WallSegmentInventoryComponent, bWallPackagePending);
}

void UGP_WallSegmentInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthorityOwner())
	{
		AuthorityClearForDestruction();
	}
	Super::EndPlay(EndPlayReason);
}

bool UGP_WallSegmentInventoryComponent::HasAuthorityOwner() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->HasAuthority();
}

bool UGP_WallSegmentInventoryComponent::CanPurchaseWallPackage() const
{
	return WallSegmentCount == 0 && !bWallPackagePending;
}

void UGP_WallSegmentInventoryComponent::SetCount(int32 NewCount)
{
	const int32 Clamped = FMath::Clamp(NewCount, 0, DefaultCapacity);
	if (WallSegmentCount == Clamped)
	{
		return;
	}
	WallSegmentCount = Clamped;
	OnWallInventoryChanged.Broadcast(WallSegmentCount);
}

void UGP_WallSegmentInventoryComponent::SetPending(bool bPending)
{
	if (bWallPackagePending == bPending)
	{
		return;
	}
	bWallPackagePending = bPending;
	OnWallPackagePendingChanged.Broadcast(bWallPackagePending);
}

bool UGP_WallSegmentInventoryComponent::AuthorityBeginPackageDelivery()
{
	if (!HasAuthorityOwner() || WallSegmentCount != 0 || bWallPackagePending)
	{
		return false;
	}

	++DeliveryGeneration;
	SetPending(true);
	return true;
}

bool UGP_WallSegmentInventoryComponent::AuthorityCompletePackageDelivery(int32 SegmentCount)
{
	if (!HasAuthorityOwner() || !bWallPackagePending)
	{
		return false;
	}

	if (SegmentCount <= 0
		|| WallSegmentCount != 0
		|| WallSegmentCount + SegmentCount > DefaultCapacity)
	{
		AuthorityCancelPackageDelivery();
		return false;
	}

	SetCount(WallSegmentCount + SegmentCount);
	SetPending(false);
	++DeliveryGeneration;
	return true;
}

void UGP_WallSegmentInventoryComponent::AuthorityCancelPackageDelivery()
{
	if (!HasAuthorityOwner())
	{
		return;
	}

	if (bWallPackagePending)
	{
		SetPending(false);
		++DeliveryGeneration;
	}
}

void UGP_WallSegmentInventoryComponent::AuthorityClearForDestruction()
{
	if (!HasAuthorityOwner())
	{
		return;
	}

	SetCount(0);
	if (bWallPackagePending)
	{
		SetPending(false);
	}
	++DeliveryGeneration;
}

bool UGP_WallSegmentInventoryComponent::AuthorityTryConsumeSegments(int32 Amount)
{
	if (!HasAuthorityOwner() || Amount <= 0 || Amount > WallSegmentCount)
	{
		return false;
	}

	SetCount(WallSegmentCount - Amount);
	return true;
}

void UGP_WallSegmentInventoryComponent::OnRep_WallSegmentCount()
{
	OnWallInventoryChanged.Broadcast(WallSegmentCount);
}

void UGP_WallSegmentInventoryComponent::OnRep_WallPackagePending()
{
	OnWallPackagePendingChanged.Broadcast(bWallPackagePending);
}
