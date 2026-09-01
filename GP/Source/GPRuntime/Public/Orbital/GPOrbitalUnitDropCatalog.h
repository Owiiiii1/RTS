// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPOrbitalUnitDropCatalog.generated.h"

class AGP_SalvageWalker;
class AGP_UnitBase;
class AGP_Worker;
class UGP_OrbitalUnitDropDefinition;
class UGP_UnitDefinition;
struct FGP_UnitDropManifest;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE(FOnGPOrbitalUnitDropCatalogChanged);

/**
 * Native bootstrap + authored unit acquisition definitions (GP-S39E).
 * Precedence: authored settings soft ref (Ready only after nested UnitDefinition + PayloadClass
 * resolve) → native bootstrap (AGP_Worker / AGP_SalvageWalker).
 * Native catalog exists for contracts / empty setup. It must not permanently shadow authored DAs.
 * Native Worker/Walker Cost, TransportSlotCost, PayloadClass, and delivery timing
 * live on the bootstrap products constructed here.
 */
UCLASS()
class GPRUNTIME_API UGP_OrbitalUnitDropCatalog : public UObject
{
	GENERATED_BODY()

public:
	static UGP_OrbitalUnitDropCatalog& Get();
	static UGP_OrbitalUnitDropCatalog* TryGetExisting();
	static void ShutdownCatalog();
	static void BindEngineLifecycle();
	static void UnbindEngineLifecycle();

	void EnsureNativeCatalog();
	void RefreshAuthoredBindings();

	/** Native bootstrap numerics for unconfigured / failed authored unit-drop products. */
	static constexpr float NativeWorkerOrbitalDropCost = 25.0f;
	static constexpr int32 NativeWorkerTransportSlotCost = 1;
	static constexpr float NativeSalvageWalkerOrbitalDropCost = 50.0f;
	static constexpr int32 NativeSalvageWalkerTransportSlotCost = 2;
	static constexpr float NativeDeliveryDescentSeconds = 2.5f;
	static constexpr float NativePayloadDeployDelaySeconds = 1.25f;

	/** Canonical ready definition, or native bootstrap when authored is empty/failed. Null while authored pending. */
	UGP_OrbitalUnitDropDefinition* GetWorkerDrop() const;
	UGP_OrbitalUnitDropDefinition* GetSalvageWalkerDrop() const;

	UGP_OrbitalUnitDropDefinition* GetNativeWorkerDrop() const { return NativeWorkerDrop; }
	UGP_OrbitalUnitDropDefinition* GetNativeSalvageWalkerDrop() const { return NativeSalvageWalkerDrop; }

	bool IsWorkerDropDefinitionPending() const;
	bool IsSalvageWalkerDropDefinitionPending() const;
	bool AreManifestDefinitionsReady(const FGP_UnitDropManifest& Manifest) const;

	int32 GetWorkerTransportSlotCost() const;
	int32 GetSalvageWalkerTransportSlotCost() const;
	float GetWorkerOrbitalDropCost() const;
	float GetSalvageWalkerOrbitalDropCost() const;

	TSubclassOf<AGP_Worker> ResolveWorkerPayloadClass() const;
	TSubclassOf<AGP_SalvageWalker> ResolveSalvageWalkerPayloadClass() const;

	void ResolveManifestDeliveryTiming(
		const FGP_UnitDropManifest& Manifest,
		float& OutDescentSeconds,
		float& OutPayloadDeployDelaySeconds) const;

	void OverrideDeliveryTiming(float DescentSeconds, float PayloadDeployDelaySeconds);

	/** Fires when canonical Worker/Walker availability may have changed (Ready, Failed→native, Pending omit). */
	FOnGPOrbitalUnitDropCatalogChanged OnCatalogChanged;

#if !UE_BUILD_SHIPPING
	void DebugAssignLoadedAuthoredWorker(UGP_OrbitalUnitDropDefinition* Definition);
	void DebugAssignLoadedAuthoredSalvageWalker(UGP_OrbitalUnitDropDefinition* Definition);
	void DebugForceUnresolvedAuthoredWorkerLoad(UGP_OrbitalUnitDropDefinition* InjectedDefinition, bool bHoldCompletion);
	void DebugForceUnresolvedAuthoredSalvageWalkerLoad(UGP_OrbitalUnitDropDefinition* InjectedDefinition, bool bHoldCompletion);
	bool DebugDidRequestAsyncAuthoredWorkerLoad() const { return bDebugDidRequestAsyncWorkerLoad; }
	void DebugCompletePendingAuthoredWorkerLoad();
	void DebugCompletePendingAuthoredSalvageWalkerLoad();
	void DebugForceAuthoredWorkerLoadFailure();
	void DebugForceAuthoredSalvageWalkerLoadFailure();
	void DebugForceUnresolvedNestedWorkerUnitDefinitionLoad(
		UGP_OrbitalUnitDropDefinition* InjectedDrop,
		UGP_UnitDefinition* InjectedUnitDefinition,
		bool bHoldCompletion);
	void DebugForceUnresolvedNestedWorkerPayloadClassLoad(
		UGP_OrbitalUnitDropDefinition* InjectedDrop,
		TSubclassOf<AGP_UnitBase> InjectedPayloadClass,
		bool bHoldCompletion);
	void DebugCompletePendingNestedWorkerUnitDefinitionLoad();
	void DebugCompletePendingNestedWorkerPayloadClassLoad();
	void DebugForceNestedWorkerUnitDefinitionLoadFailure();
	void DebugForceNestedWorkerPayloadClassLoadFailure();
	bool DebugDidRequestAsyncNestedUnitDefinitionLoad() const { return bDebugDidRequestAsyncNestedUnitDefLoad; }
	bool DebugDidRequestAsyncNestedPayloadClassLoad() const { return bDebugDidRequestAsyncNestedPayloadLoad; }
	bool DebugConsumeNestedUnitDefinitionLoadFailedLog();
	bool DebugConsumeNestedPayloadClassLoadFailedLog();
	bool DebugConsumeNullUnitDefinitionLog();
	bool DebugConsumeNullPayloadClassLog();
	void DebugClearAuthoredUnitDropOverrides();
	void DebugBeginContractIsolation();
	void DebugEndContractIsolation();
	bool DebugConsumeWorkerLoadFailedLog() { const bool b = bDebugWorkerLoadFailedLogged; bDebugWorkerLoadFailedLogged = false; return b; }
#endif

private:
	enum class EUnitAuthoredSlot : uint8
	{
		Worker = 0,
		SalvageWalker,
		COUNT
	};

	enum class EAuthoredSlotState : uint8
	{
		Empty = 0,
		Pending,
		Ready,
		Failed
	};

	UGP_OrbitalUnitDropDefinition* CreateNativeDrop(FName AssetName, const FText& DisplayName);
	void RefreshAuthoredSlot(EUnitAuthoredSlot Slot);
	void RequestAuthoredAsyncLoad(EUnitAuthoredSlot Slot, const FSoftObjectPath& SoftPath);
	void RequestAuthoredNestedUnitDefinitionLoad(EUnitAuthoredSlot Slot, const FSoftObjectPath& NestedPath);
	void RequestAuthoredNestedPayloadClassLoad(EUnitAuthoredSlot Slot, const FSoftObjectPath& NestedPath);
	void HandleAuthoredLoaded(EUnitAuthoredSlot Slot);
	void HandleAuthoredNestedUnitDefinitionLoaded(EUnitAuthoredSlot Slot);
	void HandleAuthoredNestedPayloadClassLoaded(EUnitAuthoredSlot Slot);
	void FinishAuthoredLoadResolve(EUnitAuthoredSlot Slot);
	void FinishAuthoredNestedUnitDefinitionLoadResolve(EUnitAuthoredSlot Slot);
	void FinishAuthoredNestedPayloadClassLoadResolve(EUnitAuthoredSlot Slot);
	void ApplyLoadedAuthoredDrop(EUnitAuthoredSlot Slot, UGP_OrbitalUnitDropDefinition* Loaded);
	void MarkAuthoredSlotFailed(EUnitAuthoredSlot Slot);
	void CancelAuthoredTopLevelLoad(EUnitAuthoredSlot Slot);
	void CancelAuthoredNestedUnitDefinitionLoad(EUnitAuthoredSlot Slot);
	void CancelAuthoredNestedPayloadClassLoad(EUnitAuthoredSlot Slot);
	void CancelAuthoredNestedLoads(EUnitAuthoredSlot Slot);
	void CancelAuthoredLoad(EUnitAuthoredSlot Slot);
	void CancelAllAuthoredLoads();
	bool IsCatalogCallbackSafe() const;
	TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> GetAuthoredSoftRef(EUnitAuthoredSlot Slot) const;
	UGP_OrbitalUnitDropDefinition* ResolveLoadedAuthored(const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>& Soft) const;
	UGP_OrbitalUnitDropDefinition* CanonicalForSlot(EUnitAuthoredSlot Slot) const;
	const UGP_OrbitalUnitDropDefinition* ResolveNumericProduct(EUnitAuthoredSlot Slot) const;
	bool HasResolvedAuthoredDependencies(const UGP_OrbitalUnitDropDefinition* Drop, EUnitAuthoredSlot Slot) const;
	bool IsPayloadClassValidForSlot(EUnitAuthoredSlot Slot, const UClass* PayloadClass) const;
	TSubclassOf<AGP_UnitBase> ResolveFallbackPayloadClass(EUnitAuthoredSlot Slot) const;
	void BroadcastCatalogChangedIfNeeded();

	void HandleWorkerLoaded() { HandleAuthoredLoaded(EUnitAuthoredSlot::Worker); }
	void HandleWalkerLoaded() { HandleAuthoredLoaded(EUnitAuthoredSlot::SalvageWalker); }
	void HandleWorkerUnitDefinitionLoaded() { HandleAuthoredNestedUnitDefinitionLoaded(EUnitAuthoredSlot::Worker); }
	void HandleWalkerUnitDefinitionLoaded() { HandleAuthoredNestedUnitDefinitionLoaded(EUnitAuthoredSlot::SalvageWalker); }
	void HandleWorkerPayloadClassLoaded() { HandleAuthoredNestedPayloadClassLoaded(EUnitAuthoredSlot::Worker); }
	void HandleWalkerPayloadClassLoaded() { HandleAuthoredNestedPayloadClassLoaded(EUnitAuthoredSlot::SalvageWalker); }

	UPROPERTY()
	TObjectPtr<UGP_OrbitalUnitDropDefinition> NativeWorkerDrop;

	UPROPERTY()
	TObjectPtr<UGP_OrbitalUnitDropDefinition> NativeSalvageWalkerDrop;

	UPROPERTY()
	TArray<TObjectPtr<UGP_OrbitalUnitDropDefinition>> NativeSlotDrops;

	UPROPERTY()
	TArray<TObjectPtr<UGP_OrbitalUnitDropDefinition>> AuthoredSlotDrops;

	TArray<TSharedPtr<FStreamableHandle>> AuthoredLoadHandles;
	TArray<TSharedPtr<FStreamableHandle>> AuthoredUnitDefLoadHandles;
	TArray<TSharedPtr<FStreamableHandle>> AuthoredPayloadLoadHandles;
	TArray<FSoftObjectPath> AuthoredRequestedPaths;
	TArray<FSoftObjectPath> AuthoredUnitDefRequestedPaths;
	TArray<FSoftObjectPath> AuthoredPayloadRequestedPaths;
	TArray<EAuthoredSlotState> AuthoredStates;
	bool bNativeCatalogReady = false;
	UGP_OrbitalUnitDropDefinition* LastNotifiedWorker = nullptr;
	UGP_OrbitalUnitDropDefinition* LastNotifiedWalker = nullptr;
	bool bLastNotifiedWorkerPending = false;
	bool bLastNotifiedWalkerPending = false;
	bool bCatalogChangedSnapshotValid = false;

#if !UE_BUILD_SHIPPING
	void EnsureDebugSlotArrays();
	void ResetDebugSlotFlags();
	void SaveAuthoredSettingsIfNeeded();
	void AssignAuthoredSettingsDrop(EUnitAuthoredSlot Slot, UGP_OrbitalUnitDropDefinition* Definition);
	void DebugAssignLoadedAuthoredDrop(EUnitAuthoredSlot Slot, UGP_OrbitalUnitDropDefinition* Definition);
	void DebugForceUnresolvedAuthoredLoad(
		EUnitAuthoredSlot Slot,
		UGP_OrbitalUnitDropDefinition* InjectedDefinition,
		bool bHoldCompletion);
	void DebugForceUnresolvedNestedUnitDefinitionLoad(
		EUnitAuthoredSlot Slot,
		UGP_OrbitalUnitDropDefinition* InjectedDrop,
		UGP_UnitDefinition* InjectedUnitDefinition,
		bool bHoldCompletion);
	void DebugForceUnresolvedNestedPayloadClassLoad(
		EUnitAuthoredSlot Slot,
		UGP_OrbitalUnitDropDefinition* InjectedDrop,
		TSubclassOf<AGP_UnitBase> InjectedPayloadClass,
		bool bHoldCompletion);
	void DebugCompletePendingAuthoredLoad(EUnitAuthoredSlot Slot);
	void DebugCompletePendingNestedUnitDefinitionLoad(EUnitAuthoredSlot Slot);
	void DebugCompletePendingNestedPayloadClassLoad(EUnitAuthoredSlot Slot);
	void DebugForceNestedUnitDefinitionLoadFailure(EUnitAuthoredSlot Slot);
	void DebugForceNestedPayloadClassLoadFailure(EUnitAuthoredSlot Slot);

	TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> DebugSavedWorkerSettingsRef;
	TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> DebugSavedWalkerSettingsRef;
	bool bDebugSavedSettings = false;
	TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> ContractSavedWorkerSettingsRef;
	TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> ContractSavedWalkerSettingsRef;
	bool bContractIsolationActive = false;
	TArray<uint8> DebugForceUnresolvedDrop;
	TArray<uint8> DebugHoldDropCompletion;
	TArray<uint8> DebugForceUnresolvedUnitDef;
	TArray<uint8> DebugHoldUnitDefCompletion;
	TArray<uint8> DebugForceUnresolvedPayload;
	TArray<uint8> DebugHoldPayloadCompletion;
	TArray<TObjectPtr<UGP_OrbitalUnitDropDefinition>> DebugInjectedDrops;
	TArray<TObjectPtr<UGP_UnitDefinition>> DebugInjectedUnitDefs;
	TArray<TSubclassOf<AGP_UnitBase>> DebugInjectedPayloadClasses;
	bool bDebugDidRequestAsyncWorkerLoad = false;
	bool bDebugDidRequestAsyncNestedUnitDefLoad = false;
	bool bDebugDidRequestAsyncNestedPayloadLoad = false;
	bool bDebugWorkerLoadFailedLogged = false;
	bool bDebugNestedUnitDefLoadFailedLogged = false;
	bool bDebugNestedPayloadLoadFailedLogged = false;
	bool bDebugNullUnitDefinitionLogged = false;
	bool bDebugNullPayloadClassLogged = false;
#endif
};
