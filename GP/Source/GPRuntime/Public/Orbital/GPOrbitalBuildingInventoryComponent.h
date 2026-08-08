// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orbital/GPOrbitalBuildingType.h"
#include "GPOrbitalBuildingInventoryComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGP_OrbitalBuildingReadyChanged, EGP_OrbitalBuildingType, int32);

/**
 * Owner-only READY inventory for orbital building deploy (GP-S32R).
 * Purchase increments; deploy consumes after pod spawn succeeds.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_OrbitalBuildingInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_OrbitalBuildingInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "GP|Orbital|Building")
	int32 GetReadyCount(EGP_OrbitalBuildingType BuildingType) const;

	/** Authority-only. Increments READY for supported type. */
	bool AuthorityAddReady(EGP_OrbitalBuildingType BuildingType, int32 Amount = 1);

	/** Authority-only. Decrements READY when Amount available. */
	bool AuthorityTryConsumeReady(EGP_OrbitalBuildingType BuildingType, int32 Amount = 1);

	FOnGP_OrbitalBuildingReadyChanged OnReadyChanged;

protected:
	UFUNCTION()
	void OnRep_ReadyLogisticsHubCount(int32 OldCount);

private:
	void BroadcastReadyChanged(EGP_OrbitalBuildingType BuildingType, int32 NewCount);

	UPROPERTY(ReplicatedUsing = OnRep_ReadyLogisticsHubCount)
	int32 ReadyLogisticsHubCount = 0;
};
