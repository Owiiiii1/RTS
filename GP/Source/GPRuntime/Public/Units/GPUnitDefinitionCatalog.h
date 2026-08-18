// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPUnitDefinitionCatalog.generated.h"

class UGP_UnitDefinition;

/**
 * Native bootstrap UnitDefinitions (GP-S38D).
 * Contracts and catalog links do not depend on binary .uasset commits.
 */
UCLASS()
class GPRUNTIME_API UGP_UnitDefinitionCatalog : public UObject
{
	GENERATED_BODY()

public:
	static UGP_UnitDefinitionCatalog& Get();
	static void ShutdownCatalog();
	static void BindEngineLifecycle();
	static void UnbindEngineLifecycle();

	void EnsureNativeCatalog();

	UGP_UnitDefinition* GetWorkerDefinition() const { return WorkerDefinition; }
	UGP_UnitDefinition* GetSalvageWalkerDefinition() const { return SalvageWalkerDefinition; }
	UGP_UnitDefinition* GetDefensiveTurretDefinition() const { return DefensiveTurretDefinition; }

	UGP_UnitDefinition* FindDefinition(const FPrimaryAssetId& DefinitionId) const;

private:
	UGP_UnitDefinition* CreateNativeDefinition(FName AssetName, const FText& DisplayName);

	UPROPERTY()
	TObjectPtr<UGP_UnitDefinition> WorkerDefinition;

	UPROPERTY()
	TObjectPtr<UGP_UnitDefinition> SalvageWalkerDefinition;

	UPROPERTY()
	TObjectPtr<UGP_UnitDefinition> DefensiveTurretDefinition;

	bool bNativeCatalogReady = false;
};
