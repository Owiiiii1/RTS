// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "GPBuildingDropCatalog.generated.h"

class AGP_BuildingBase;
class UGP_BuildingDefinition;
class UGP_OrbitalDropDefinition;
struct FStreamableHandle;

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
	/** Live catalog only. Never creates, refreshes, or syncs. Teardown-safe. */
	static UGP_BuildingDropCatalog* TryGetExisting();
	/** Idempotent. Engine pre-exit locks recreation. */
	static void ShutdownCatalog();
	static void NotifyEngineShutdown();
	static void BindEngineLifecycle();
	static void UnbindEngineLifecycle();

	void EnsureNativeCatalog();
	void RefreshAuthoredBindings();

	UGP_OrbitalDropDefinition* FindDropDefinition(const FPrimaryAssetId& DropDefinitionId) const;
	UGP_BuildingDefinition* FindBuildingDefinition(const FPrimaryAssetId& BuildingDefinitionId) const;

	UGP_OrbitalDropDefinition* GetLegacyLogisticsHubDrop() const;
	UGP_BuildingDefinition* GetMainBaseBuilding() const { return MainBaseBuilding; }
	FPrimaryAssetId GetLegacyLogisticsHubDropId() const;

	bool IsDropDefinitionPending(const UGP_OrbitalDropDefinition* DropDefinition) const;
	bool IsDropDefinitionIdPending(const FPrimaryAssetId& DropDefinitionId) const;

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

#if !UE_BUILD_SHIPPING
	void DebugAssignLoadedAuthoredLogisticsHub(UGP_OrbitalDropDefinition* Definition);
	void DebugAssignLoadedAuthoredDefensiveTurret(UGP_OrbitalDropDefinition* Definition);
	void DebugForceUnresolvedAuthoredLogisticsHubLoad(UGP_OrbitalDropDefinition* InjectedDefinition, bool bHoldCompletion);
	void DebugCompletePendingAuthoredLogisticsHubLoad();
	void DebugForceUnresolvedNestedLogisticsHubBuildingLoad(
		UGP_OrbitalDropDefinition* InjectedDrop,
		UGP_BuildingDefinition* InjectedBuilding,
		bool bHoldCompletion);
	void DebugForceUnresolvedNestedDefensiveTurretBuildingLoad(
		UGP_OrbitalDropDefinition* InjectedDrop,
		UGP_BuildingDefinition* InjectedBuilding,
		bool bHoldCompletion);
	void DebugCompletePendingNestedBuildingLoad();
	void DebugForceNestedBuildingLoadFailure();
	bool DebugDidRequestAsyncAuthoredDropLoad() const { return bDebugDidRequestAsyncDropLoad; }
	bool DebugDidRequestAsyncNestedBuildingLoad() const { return bDebugDidRequestAsyncNestedLoad; }
	bool DebugConsumeNestedBuildingLoadFailedLog();
	bool DebugConsumeNullBuildingDefinitionLog();
	bool DebugIsCallbackSafe() const { return IsCatalogCallbackSafe(); }
	UGP_OrbitalDropDefinition* DebugGetCanonicalDefensiveTurretDrop() const;
	void DebugClearAuthoredBuildingDropOverrides();
	void DebugBeginContractIsolation();
	void DebugEndContractIsolation();
	bool IsContractIsolationActive() const { return bContractIsolationActive; }
#endif

private:
	enum class EBuildingAuthoredSlot : uint8
	{
		LogisticsHub = 0,
		DefensiveTurret,
		Wall,
		WallTurret,
		COUNT
	};

	enum class EAuthoredSlotState : uint8
	{
		Empty = 0,
		Pending,
		Ready,
		Failed
	};
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

	UPROPERTY()
	TArray<TObjectPtr<UGP_OrbitalDropDefinition>> NativeSlotDrops;

	UPROPERTY()
	TArray<TObjectPtr<UGP_OrbitalDropDefinition>> AuthoredSlotDrops;

	TArray<TSharedPtr<FStreamableHandle>> AuthoredLoadHandles;
	TArray<TSharedPtr<FStreamableHandle>> AuthoredNestedLoadHandles;
	TArray<FSoftObjectPath> AuthoredRequestedPaths;
	TArray<FSoftObjectPath> AuthoredNestedRequestedPaths;
	TArray<EAuthoredSlotState> AuthoredStates;

	void RefreshAuthoredSlot(EBuildingAuthoredSlot Slot);
	void RequestAuthoredAsyncLoad(EBuildingAuthoredSlot Slot, const FSoftObjectPath& SoftPath);
	void RequestAuthoredNestedAsyncLoad(EBuildingAuthoredSlot Slot, const FSoftObjectPath& NestedPath);
	void HandleAuthoredLoaded(EBuildingAuthoredSlot Slot);
	void HandleAuthoredNestedLoaded(EBuildingAuthoredSlot Slot);
	void FinishAuthoredLoadResolve(EBuildingAuthoredSlot Slot);
	void FinishAuthoredNestedLoadResolve(EBuildingAuthoredSlot Slot);
	void ApplyLoadedAuthoredDrop(EBuildingAuthoredSlot Slot, UGP_OrbitalDropDefinition* Loaded);
	void MarkAuthoredSlotFailed(EBuildingAuthoredSlot Slot);
	void CancelAuthoredTopLevelLoad(EBuildingAuthoredSlot Slot);
	void CancelAuthoredNestedLoad(EBuildingAuthoredSlot Slot);
	void CancelAuthoredLoad(EBuildingAuthoredSlot Slot);
	void CancelAllAuthoredLoads();
	bool IsCatalogCallbackSafe() const;
	TSoftObjectPtr<UGP_OrbitalDropDefinition> GetAuthoredSoftRef(EBuildingAuthoredSlot Slot) const;
	UGP_OrbitalDropDefinition* ResolveLoadedAuthored(const TSoftObjectPtr<UGP_OrbitalDropDefinition>& Soft) const;
	UGP_OrbitalDropDefinition* CanonicalForSlot(EBuildingAuthoredSlot Slot) const;
	EBuildingAuthoredSlot FindSlotForDrop(const UGP_OrbitalDropDefinition* DropDefinition) const;
	EBuildingAuthoredSlot FindSlotForId(const FPrimaryAssetId& DropDefinitionId) const;
	UGP_OrbitalDropDefinition* ResolveCanonicalDrop(const UGP_OrbitalDropDefinition* DropDefinition) const;

	void HandleLogisticsHubLoaded() { HandleAuthoredLoaded(EBuildingAuthoredSlot::LogisticsHub); }
	void HandleDefensiveTurretLoaded() { HandleAuthoredLoaded(EBuildingAuthoredSlot::DefensiveTurret); }
	void HandleWallLoaded() { HandleAuthoredLoaded(EBuildingAuthoredSlot::Wall); }
	void HandleWallTurretLoaded() { HandleAuthoredLoaded(EBuildingAuthoredSlot::WallTurret); }
	void HandleLogisticsHubNestedLoaded() { HandleAuthoredNestedLoaded(EBuildingAuthoredSlot::LogisticsHub); }
	void HandleDefensiveTurretNestedLoaded() { HandleAuthoredNestedLoaded(EBuildingAuthoredSlot::DefensiveTurret); }
	void HandleWallNestedLoaded() { HandleAuthoredNestedLoaded(EBuildingAuthoredSlot::Wall); }
	void HandleWallTurretNestedLoaded() { HandleAuthoredNestedLoaded(EBuildingAuthoredSlot::WallTurret); }

	bool bNativeCatalogReady = false;

#if !UE_BUILD_SHIPPING
	void EnsureDebugSlotArrays();
	void ResetDebugSlotFlags();
	void SaveAuthoredSettingsIfNeeded();
	void AssignAuthoredSettingsDrop(EBuildingAuthoredSlot Slot, UGP_OrbitalDropDefinition* Definition);
	void DebugForceUnresolvedAuthoredLoad(
		EBuildingAuthoredSlot Slot,
		UGP_OrbitalDropDefinition* InjectedDefinition,
		bool bHoldCompletion);
	void DebugForceUnresolvedNestedBuildingLoad(
		EBuildingAuthoredSlot Slot,
		UGP_OrbitalDropDefinition* InjectedDrop,
		UGP_BuildingDefinition* InjectedBuilding,
		bool bHoldCompletion);
	void DebugCompletePendingAuthoredLoad(EBuildingAuthoredSlot Slot);

	TArray<TSoftObjectPtr<UGP_OrbitalDropDefinition>> DebugSavedBuildingRefs;
	bool bDebugSavedBuildingSettings = false;
	TArray<TSoftObjectPtr<UGP_OrbitalDropDefinition>> ContractSavedBuildingRefs;
	bool bContractIsolationActive = false;
	TArray<uint8> DebugForceUnresolvedDrop;
	TArray<uint8> DebugHoldDropCompletion;
	TArray<uint8> DebugForceUnresolvedNested;
	TArray<uint8> DebugHoldNestedCompletion;
	TArray<TObjectPtr<UGP_OrbitalDropDefinition>> DebugInjectedDrops;
	TArray<TObjectPtr<UGP_BuildingDefinition>> DebugInjectedBuildings;
	bool bDebugDidRequestAsyncDropLoad = false;
	bool bDebugDidRequestAsyncNestedLoad = false;
	bool bDebugNestedLoadFailedLogged = false;
	bool bDebugNullBuildingLogged = false;
#endif
};
