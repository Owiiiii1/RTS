// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "GPBuildingDropCatalog.generated.h"

class AGP_BuildingBase;
class UGP_BuildingDefinition;
class UGP_OrbitalDropDefinition;

/**
 * Runtime building acquisition catalog (GP-S35B).
 * Not a GameInstance subsystem. Native bootstrap + optional registered test definitions.
 * Does not own faction/session preload.
 *
 * Lifetime: static TStrongObjectPtr is the only owner. Native definitions are UPROPERTY
 * children of this object. No AddToRoot. Released on EnginePreExit while UObject is valid.
 */
UCLASS()
class GPRUNTIME_API UGP_BuildingDropCatalog : public UObject
{
	GENERATED_BODY()

public:
	static UGP_BuildingDropCatalog& Get();
	/** Idempotent. Safe if never created. Later Get() recreates a fresh native catalog. */
	static void ShutdownCatalog();
	static void BindEngineLifecycle();
	static void UnbindEngineLifecycle();

	void EnsureNativeCatalog();

	UGP_OrbitalDropDefinition* FindDropDefinition(const FPrimaryAssetId& DropDefinitionId) const;
	UGP_BuildingDefinition* FindBuildingDefinition(const FPrimaryAssetId& BuildingDefinitionId) const;

	UGP_OrbitalDropDefinition* GetLegacyLogisticsHubDrop() const { return LegacyLogisticsHubDrop; }
	UGP_BuildingDefinition* GetMainBaseBuilding() const { return MainBaseBuilding; }
	FPrimaryAssetId GetLegacyLogisticsHubDropId() const;

	void ResolveDeliveryTiming(
		const UGP_OrbitalDropDefinition* DropDefinition,
		float& OutDescentSeconds,
		float& OutPayloadDeployDelaySeconds) const;

	void OverrideDeliveryTiming(float DescentSeconds, float PayloadDeployDelaySeconds);

	/** Sync Cost from deprecated settings onto the native Logistics Hub drop (operator DefaultGame.ini bridge). */
	void SyncLegacyLogisticsHubCompatibility();

	void RegisterDropDefinition(UGP_OrbitalDropDefinition* DropDefinition);
	void RegisterBuildingDefinition(UGP_BuildingDefinition* BuildingDefinition);

	void GetOperatorVisibleDrops(TArray<UGP_OrbitalDropDefinition*>& OutDrops) const;

	TSubclassOf<AGP_BuildingBase> ResolvePayloadClass(const UGP_OrbitalDropDefinition* DropDefinition) const;
	float GetPurchaseCost(const UGP_OrbitalDropDefinition* DropDefinition) const;

private:
	UGP_BuildingDefinition* CreateNativeBuilding(
		FName AssetName,
		const FText& DisplayName,
		const FGameplayTag& BuildingTypeTag,
		FIntPoint FootprintCells,
		float MaxHealth);
	UGP_OrbitalDropDefinition* CreateNativeDrop(
		FName AssetName,
		UGP_BuildingDefinition* BuildingDefinition,
		const FGameplayTag& DropTypeTag,
		float Cost);

	UPROPERTY()
	TArray<TObjectPtr<UGP_BuildingDefinition>> NativeBuildings;

	UPROPERTY()
	TArray<TObjectPtr<UGP_OrbitalDropDefinition>> NativeDrops;

	UPROPERTY()
	TArray<TObjectPtr<UGP_BuildingDefinition>> RegisteredBuildings;

	UPROPERTY()
	TArray<TObjectPtr<UGP_OrbitalDropDefinition>> RegisteredDrops;

	UPROPERTY()
	TObjectPtr<UGP_OrbitalDropDefinition> LegacyLogisticsHubDrop;

	UPROPERTY()
	TObjectPtr<UGP_BuildingDefinition> MainBaseBuilding;

	bool bNativeCatalogReady = false;
};
