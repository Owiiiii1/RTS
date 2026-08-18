// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPOrbitalUnitDropCatalog.generated.h"

class AGP_SalvageWalker;
class AGP_UnitBase;
class AGP_Worker;
class UGP_OrbitalUnitDropDefinition;
struct FGP_UnitDropManifest;
struct FStreamableHandle;

/**
 * Native bootstrap + authored unit acquisition definitions (GP-S39E).
 * Precedence: authored settings soft ref → native bootstrap → deprecated settings numerics/class.
 * Native catalog exists for contracts / empty setup. It must not permanently shadow authored DAs.
 */
UCLASS()
class GPRUNTIME_API UGP_OrbitalUnitDropCatalog : public UObject
{
	GENERATED_BODY()

public:
	static UGP_OrbitalUnitDropCatalog& Get();
	static void ShutdownCatalog();
	static void BindEngineLifecycle();
	static void UnbindEngineLifecycle();

	void EnsureNativeCatalog();
	void RefreshAuthoredBindings();

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

#if !UE_BUILD_SHIPPING
	void DebugAssignLoadedAuthoredWorker(UGP_OrbitalUnitDropDefinition* Definition);
	void DebugForceUnresolvedAuthoredWorkerLoad(UGP_OrbitalUnitDropDefinition* InjectedDefinition, bool bHoldCompletion);
	bool DebugDidRequestAsyncAuthoredWorkerLoad() const { return bDebugDidRequestAsyncWorkerLoad; }
	void DebugCompletePendingAuthoredWorkerLoad();
	void DebugForceAuthoredWorkerLoadFailure();
	void DebugClearAuthoredUnitDropOverrides();
	bool DebugConsumeWorkerLoadFailedLog() { const bool b = bDebugWorkerLoadFailedLogged; bDebugWorkerLoadFailedLogged = false; return b; }
#endif

private:
	enum class EAuthoredSlotState : uint8
	{
		Empty = 0,
		Pending,
		Ready,
		Failed
	};

	UGP_OrbitalUnitDropDefinition* CreateNativeDrop(FName AssetName, const FText& DisplayName);
	void RefreshWorkerSlot();
	void RefreshWalkerSlot();
	void RequestWorkerAsyncLoad(const FSoftObjectPath& SoftPath);
	void RequestWalkerAsyncLoad(const FSoftObjectPath& SoftPath);
	void HandleWorkerLoaded();
	void HandleWalkerLoaded();
	void FinishWorkerLoadResolve();
	void FinishWalkerLoadResolve();
	void CancelWorkerLoad();
	void CancelWalkerLoad();
	UGP_OrbitalUnitDropDefinition* ResolveLoadedAuthored(const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>& Soft) const;
	UGP_OrbitalUnitDropDefinition* CanonicalOrNative(
		EAuthoredSlotState State,
		UGP_OrbitalUnitDropDefinition* Authored,
		UGP_OrbitalUnitDropDefinition* Native) const;

	UPROPERTY()
	TObjectPtr<UGP_OrbitalUnitDropDefinition> NativeWorkerDrop;

	UPROPERTY()
	TObjectPtr<UGP_OrbitalUnitDropDefinition> NativeSalvageWalkerDrop;

	UPROPERTY()
	TObjectPtr<UGP_OrbitalUnitDropDefinition> AuthoredWorkerDrop;

	UPROPERTY()
	TObjectPtr<UGP_OrbitalUnitDropDefinition> AuthoredSalvageWalkerDrop;

	TSharedPtr<FStreamableHandle> WorkerLoadHandle;
	TSharedPtr<FStreamableHandle> WalkerLoadHandle;
	FSoftObjectPath WorkerRequestedPath;
	FSoftObjectPath WalkerRequestedPath;
	EAuthoredSlotState WorkerState = EAuthoredSlotState::Empty;
	EAuthoredSlotState WalkerState = EAuthoredSlotState::Empty;
	bool bNativeCatalogReady = false;

#if !UE_BUILD_SHIPPING
	bool bDebugForceUnresolvedWorker = false;
	bool bDebugHoldWorkerCompletion = false;
	bool bDebugDidRequestAsyncWorkerLoad = false;
	bool bDebugWorkerLoadFailedLogged = false;
	TObjectPtr<UGP_OrbitalUnitDropDefinition> DebugInjectedWorkerDrop;
	TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> DebugSavedWorkerSettingsRef;
	TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> DebugSavedWalkerSettingsRef;
	bool bDebugSavedSettings = false;
#endif
};
