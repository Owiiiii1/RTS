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

/**
 * Native bootstrap unit acquisition definitions (GP-S39E).
 * Contracts do not depend on binary .uasset commits.
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

	UGP_OrbitalUnitDropDefinition* GetWorkerDrop() const { return WorkerDrop; }
	UGP_OrbitalUnitDropDefinition* GetSalvageWalkerDrop() const { return SalvageWalkerDrop; }

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

private:
	UGP_OrbitalUnitDropDefinition* CreateNativeDrop(FName AssetName, const FText& DisplayName);

	UPROPERTY()
	TObjectPtr<UGP_OrbitalUnitDropDefinition> WorkerDrop;

	UPROPERTY()
	TObjectPtr<UGP_OrbitalUnitDropDefinition> SalvageWalkerDrop;

	bool bNativeCatalogReady = false;
};
