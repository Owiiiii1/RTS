// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPWallPackageCatalog.generated.h"

class UGP_WallPackageDefinition;
struct FStreamableHandle;

/**
 * Native bootstrap + authored Wall Package definition (GP-S42A).
 * Precedence: authored settings soft ref → native bootstrap.
 * Not a READY building catalog. No BuildingDefinition required.
 */
UCLASS()
class GPRUNTIME_API UGP_WallPackageCatalog : public UObject
{
	GENERATED_BODY()

public:
	/** Creates if allowed. Null during/after engine shutdown. */
	static UGP_WallPackageCatalog* Get();

	/** Live catalog only. Never creates. Never refreshes. Teardown-safe. */
	static UGP_WallPackageCatalog* TryGetExisting();

	/** Idempotent. Engine pre-exit / module shutdown lock recreation. */
	static void ShutdownCatalog();
	static void NotifyEngineShutdown();
	static void BindEngineLifecycle();
	static void UnbindEngineLifecycle();

	void EnsureNativeCatalog();
	void RefreshAuthoredBindings();

	/** Canonical ready definition, or native when authored empty/failed. Null while authored pending. */
	UGP_WallPackageDefinition* GetWallPackage() const;
	UGP_WallPackageDefinition* GetNativeWallPackage() const { return NativePackage; }

	bool IsWallPackageDefinitionPending() const;
	bool IsWallPackageDefinitionReady() const;

	void ResolveDeliveryTiming(float& OutDescentSeconds, float& OutPayloadDeployDelaySeconds) const;

#if !UE_BUILD_SHIPPING
	void DebugAssignLoadedAuthored(UGP_WallPackageDefinition* Definition);
	void DebugForceUnresolvedAuthoredLoad(UGP_WallPackageDefinition* InjectedDefinition, bool bHoldCompletion);
	bool DebugDidRequestAsyncAuthoredLoad() const { return bDebugDidRequestAsyncLoad; }
	void DebugCompletePendingAuthoredLoad();
	void DebugForceAuthoredLoadFailure();
	void DebugClearAuthoredOverrides();
	void DebugBeginContractIsolation();
	void DebugEndContractIsolation();
	bool DebugConsumeLoadFailedLog() { const bool b = bDebugLoadFailedLogged; bDebugLoadFailedLogged = false; return b; }
#endif

private:
	enum class EAuthoredSlotState : uint8
	{
		Empty = 0,
		Pending,
		Ready,
		Failed
	};

	UGP_WallPackageDefinition* CreateNativePackage();
	void RefreshSlot();
	void RequestAsyncLoad(const FSoftObjectPath& SoftPath);
	void HandleLoaded();
	void FinishLoadResolve();
	void CancelLoad();
	UGP_WallPackageDefinition* ResolveLoadedAuthored(const TSoftObjectPtr<UGP_WallPackageDefinition>& Soft) const;
	UGP_WallPackageDefinition* CanonicalOrNative() const;

	UPROPERTY()
	TObjectPtr<UGP_WallPackageDefinition> NativePackage;

	UPROPERTY()
	TObjectPtr<UGP_WallPackageDefinition> AuthoredPackage;

	TSharedPtr<FStreamableHandle> LoadHandle;
	FSoftObjectPath RequestedPath;
	EAuthoredSlotState State = EAuthoredSlotState::Empty;
	bool bNativeCatalogReady = false;

#if !UE_BUILD_SHIPPING
	bool bDebugForceUnresolved = false;
	bool bDebugHoldCompletion = false;
	bool bDebugDidRequestAsyncLoad = false;
	bool bDebugLoadFailedLogged = false;
	TObjectPtr<UGP_WallPackageDefinition> DebugInjectedPackage;
	TSoftObjectPtr<UGP_WallPackageDefinition> DebugSavedSettingsRef;
	bool bDebugSavedSettings = false;
	TSoftObjectPtr<UGP_WallPackageDefinition> ContractSavedSettingsRef;
	bool bContractIsolationActive = false;
#endif
};
