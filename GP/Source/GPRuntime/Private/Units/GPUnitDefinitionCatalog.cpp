// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitDefinitionCatalog.h"

#include "Misc/CoreDelegates.h"
#include "Units/GPUnitDefinition.h"
#include "UObject/StrongObjectPtr.h"

namespace GPUnitDefinitionCatalogPrivate
{
	static TStrongObjectPtr<UGP_UnitDefinitionCatalog> GCatalog;
	static FDelegateHandle EnginePreExitHandle;
	static constexpr TCHAR CatalogObjectName[] = TEXT("GP_UnitDefinitionCatalog");
}

UGP_UnitDefinitionCatalog& UGP_UnitDefinitionCatalog::Get()
{
	if (!GPUnitDefinitionCatalogPrivate::GCatalog.IsValid())
	{
		UGP_UnitDefinitionCatalog* CatalogObj = FindObject<UGP_UnitDefinitionCatalog>(
			GetTransientPackage(),
			GPUnitDefinitionCatalogPrivate::CatalogObjectName);
		if (!IsValid(CatalogObj))
		{
			CatalogObj = NewObject<UGP_UnitDefinitionCatalog>(
				GetTransientPackage(),
				GPUnitDefinitionCatalogPrivate::CatalogObjectName,
				RF_Transient);
		}
		GPUnitDefinitionCatalogPrivate::GCatalog.Reset(CatalogObj);
		CatalogObj->EnsureNativeCatalog();
	}

	return *GPUnitDefinitionCatalogPrivate::GCatalog.Get();
}

void UGP_UnitDefinitionCatalog::ShutdownCatalog()
{
	GPUnitDefinitionCatalogPrivate::GCatalog.Reset();
}

void UGP_UnitDefinitionCatalog::BindEngineLifecycle()
{
	if (GPUnitDefinitionCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		return;
	}

	GPUnitDefinitionCatalogPrivate::EnginePreExitHandle =
		FCoreDelegates::OnEnginePreExit.AddStatic(&UGP_UnitDefinitionCatalog::ShutdownCatalog);
}

void UGP_UnitDefinitionCatalog::UnbindEngineLifecycle()
{
	if (GPUnitDefinitionCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(GPUnitDefinitionCatalogPrivate::EnginePreExitHandle);
		GPUnitDefinitionCatalogPrivate::EnginePreExitHandle.Reset();
	}
}

void UGP_UnitDefinitionCatalog::EnsureNativeCatalog()
{
	if (bNativeCatalogReady)
	{
		return;
	}

	// Values copied from current C++/CDO ownership — no rebalance.
	WorkerDefinition = CreateNativeDefinition(
		FName(TEXT("DA_GP_Unit_Worker")),
		NSLOCTEXT("GPUnitDefinitionCatalog", "Worker", "Worker"));
	WorkerDefinition->MaxHealth = 100.0f;
	WorkerDefinition->InitialHealth = 100.0f;
	WorkerDefinition->Armor = 0.0f;
	WorkerDefinition->DamageResistance = 0.0f;
	WorkerDefinition->Damage = 25.0f;
	WorkerDefinition->AttackRangeCm = 250.0f;
	WorkerDefinition->AttackCooldownSeconds = 1.0f;
	WorkerDefinition->SightRangeCm = 900.0f;
	WorkerDefinition->AutoAcquireScanIntervalSeconds = 0.35f;
	WorkerDefinition->AttackFacingRotationSpeedDegreesPerSecond = 360.0f;
	WorkerDefinition->MoveSpeedCmPerSecond = 600.0f;
	WorkerDefinition->RetaliationPursuitSeconds = 5.0f;

	SalvageWalkerDefinition = CreateNativeDefinition(
		FName(TEXT("DA_GP_Unit_SalvageWalker")),
		NSLOCTEXT("GPUnitDefinitionCatalog", "SalvageWalker", "Salvage Walker"));
	SalvageWalkerDefinition->MaxHealth = 200.0f;
	SalvageWalkerDefinition->InitialHealth = 200.0f;
	SalvageWalkerDefinition->Armor = 0.0f;
	SalvageWalkerDefinition->DamageResistance = 0.0f;
	SalvageWalkerDefinition->Damage = 20.0f;
	SalvageWalkerDefinition->AttackRangeCm = 600.0f;
	SalvageWalkerDefinition->AttackCooldownSeconds = 1.0f;
	SalvageWalkerDefinition->SightRangeCm = 900.0f;
	SalvageWalkerDefinition->AutoAcquireScanIntervalSeconds = 0.35f;
	SalvageWalkerDefinition->AttackFacingRotationSpeedDegreesPerSecond = 360.0f;
	SalvageWalkerDefinition->MoveSpeedCmPerSecond = 250.0f;
	SalvageWalkerDefinition->RetaliationPursuitSeconds = 5.0f;

	DefensiveTurretDefinition = CreateNativeDefinition(
		FName(TEXT("DA_GP_Unit_DefensiveTurret")),
		NSLOCTEXT("GPUnitDefinitionCatalog", "DefensiveTurret", "Defensive Turret"));
	DefensiveTurretDefinition->MaxHealth = 400.0f;
	DefensiveTurretDefinition->InitialHealth = 400.0f;
	DefensiveTurretDefinition->Armor = 0.0f;
	DefensiveTurretDefinition->DamageResistance = 0.0f;
	DefensiveTurretDefinition->Damage = 20.0f;
	DefensiveTurretDefinition->AttackRangeCm = 600.0f;
	DefensiveTurretDefinition->AttackCooldownSeconds = 1.0f;
	DefensiveTurretDefinition->SightRangeCm = 600.0f;
	DefensiveTurretDefinition->AutoAcquireScanIntervalSeconds = 0.35f;
	DefensiveTurretDefinition->AttackFacingRotationSpeedDegreesPerSecond = 360.0f;
	DefensiveTurretDefinition->MoveSpeedCmPerSecond = 0.0f;
	DefensiveTurretDefinition->RetaliationPursuitSeconds = 5.0f;

	bNativeCatalogReady = true;
}

UGP_UnitDefinition* UGP_UnitDefinitionCatalog::CreateNativeDefinition(FName AssetName, const FText& DisplayName)
{
	UGP_UnitDefinition* Def = NewObject<UGP_UnitDefinition>(this, AssetName, RF_Transient);
	Def->DisplayName = DisplayName;
	return Def;
}

UGP_UnitDefinition* UGP_UnitDefinitionCatalog::FindDefinition(const FPrimaryAssetId& DefinitionId) const
{
	if (!DefinitionId.IsValid())
	{
		return nullptr;
	}

	UGP_UnitDefinition* Candidates[] = { WorkerDefinition, SalvageWalkerDefinition, DefensiveTurretDefinition };
	for (UGP_UnitDefinition* Def : Candidates)
	{
		if (IsValid(Def) && Def->GetPrimaryAssetId() == DefinitionId)
		{
			return Def;
		}
	}
	return nullptr;
}
