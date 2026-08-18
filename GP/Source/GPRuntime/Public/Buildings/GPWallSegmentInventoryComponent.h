// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GPWallSegmentInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGP_WallInventoryChanged, int32, NewCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGP_WallPackagePendingChanged, bool, bPending);

/**
 * Authoritative MainBase Wall Segment stock (GP-S42A).
 * Not Ferronite storage. Not Building READY. Not a generic inventory.
 * Replicated to all (depot presentation is world-visible on MainBase).
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_WallSegmentInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_WallSegmentInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	static constexpr int32 DefaultCapacity = 5;

	UFUNCTION(BlueprintPure, Category = "GP|Wall|Inventory")
	int32 GetWallSegmentCount() const { return WallSegmentCount; }

	UFUNCTION(BlueprintPure, Category = "GP|Wall|Inventory")
	int32 GetWallSegmentCapacity() const { return DefaultCapacity; }

	UFUNCTION(BlueprintPure, Category = "GP|Wall|Inventory")
	bool IsWallPackagePending() const { return bWallPackagePending; }

	UFUNCTION(BlueprintPure, Category = "GP|Wall|Inventory")
	bool CanPurchaseWallPackage() const;

	UFUNCTION(BlueprintPure, Category = "GP|Wall|Inventory")
	bool CanBuildWall() const { return WallSegmentCount > 0; }

	int32 GetDeliveryGeneration() const { return DeliveryGeneration; }

	bool AuthorityBeginPackageDelivery();
	bool AuthorityCompletePackageDelivery(int32 SegmentCount);
	void AuthorityCancelPackageDelivery();
	void AuthorityClearForDestruction();

	/** Future GP-S42C consume. Not wired to gameplay in this slice. */
	bool AuthorityTryConsumeSegments(int32 Amount);

	UPROPERTY(BlueprintAssignable, Category = "GP|Wall|Inventory")
	FOnGP_WallInventoryChanged OnWallInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "GP|Wall|Inventory")
	FOnGP_WallPackagePendingChanged OnWallPackagePendingChanged;

protected:
	UFUNCTION()
	void OnRep_WallSegmentCount();

	UFUNCTION()
	void OnRep_WallPackagePending();

private:
	bool HasAuthorityOwner() const;
	void SetCount(int32 NewCount);
	void SetPending(bool bPending);

	UPROPERTY(ReplicatedUsing = OnRep_WallSegmentCount)
	int32 WallSegmentCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_WallPackagePending)
	bool bWallPackagePending = false;

	int32 DeliveryGeneration = 0;
};
