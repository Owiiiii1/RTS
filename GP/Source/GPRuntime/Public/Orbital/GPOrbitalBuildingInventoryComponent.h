// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orbital/GPOrbitalBuildingType.h"
#include "GPOrbitalBuildingInventoryComponent.generated.h"

class UGP_OrbitalDropDefinition;

USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_ReadyBuildingEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FPrimaryAssetId DropDefinitionId;

	UPROPERTY()
	int32 ReadyCount = 0;

	bool operator==(const FGP_ReadyBuildingEntry& Other) const
	{
		return DropDefinitionId == Other.DropDefinitionId && ReadyCount == Other.ReadyCount;
	}
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGP_OrbitalBuildingReadyChanged, FPrimaryAssetId, int32);

/**
 * Owner-only READY inventory keyed by stable OrbitalDropDefinition identity (GP-S35B).
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
	int32 GetReadyCount(FPrimaryAssetId DropDefinitionId) const;

	int32 GetReadyCount(const UGP_OrbitalDropDefinition* DropDefinition) const;

	/** Deprecated compatibility: maps LogisticsHub to the native catalog DropDef id. */
	int32 GetReadyCount(EGP_OrbitalBuildingType BuildingType) const;

	bool AuthorityAddReady(FPrimaryAssetId DropDefinitionId, int32 Amount = 1);
	bool AuthorityAddReady(const UGP_OrbitalDropDefinition* DropDefinition, int32 Amount = 1);
	bool AuthorityAddReady(EGP_OrbitalBuildingType BuildingType, int32 Amount = 1);

	bool AuthorityTryConsumeReady(FPrimaryAssetId DropDefinitionId, int32 Amount = 1);
	bool AuthorityTryConsumeReady(const UGP_OrbitalDropDefinition* DropDefinition, int32 Amount = 1);
	bool AuthorityTryConsumeReady(EGP_OrbitalBuildingType BuildingType, int32 Amount = 1);

	const TArray<FGP_ReadyBuildingEntry>& GetReadyEntries() const { return ReadyEntries; }

	FOnGP_OrbitalBuildingReadyChanged OnReadyChanged;

protected:
	UFUNCTION()
	void OnRep_ReadyEntries();

private:
	int32 FindEntryIndex(const FPrimaryAssetId& DropDefinitionId) const;
	void BroadcastReadyChanged(const FPrimaryAssetId& DropDefinitionId, int32 NewCount);
	static FPrimaryAssetId ResolveLegacyTypeId(EGP_OrbitalBuildingType BuildingType);

	UPROPERTY(ReplicatedUsing = OnRep_ReadyEntries)
	TArray<FGP_ReadyBuildingEntry> ReadyEntries;

	TArray<FGP_ReadyBuildingEntry> LastReplicatedEntries;
};
