// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingDropCatalog.h"

#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPLogisticsHub.h"
#include "Engine/AssetManager.h"
#include "Misc/CoreDelegates.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Tags/GPGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

namespace GPBuildingDropCatalogPrivate
{
	static TStrongObjectPtr<UGP_BuildingDropCatalog> GCatalog;
	static FDelegateHandle EnginePreExitHandle;

	static constexpr TCHAR CatalogObjectName[] = TEXT("GP_BuildingDropCatalog");
}

UGP_BuildingDropCatalog& UGP_BuildingDropCatalog::Get()
{
	if (!GPBuildingDropCatalogPrivate::GCatalog.IsValid())
	{
		UGP_BuildingDropCatalog* CatalogObj = FindObject<UGP_BuildingDropCatalog>(
			GetTransientPackage(),
			GPBuildingDropCatalogPrivate::CatalogObjectName);
		if (!IsValid(CatalogObj))
		{
			CatalogObj = NewObject<UGP_BuildingDropCatalog>(
				GetTransientPackage(),
				GPBuildingDropCatalogPrivate::CatalogObjectName,
				RF_Transient);
		}
		GPBuildingDropCatalogPrivate::GCatalog.Reset(CatalogObj);
		CatalogObj->EnsureNativeCatalog();
	}

	UGP_BuildingDropCatalog& Catalog = *GPBuildingDropCatalogPrivate::GCatalog.Get();
	Catalog.SyncLegacyLogisticsHubCompatibility();
	return Catalog;
}

void UGP_BuildingDropCatalog::ShutdownCatalog()
{
	GPBuildingDropCatalogPrivate::GCatalog.Reset();
}

void UGP_BuildingDropCatalog::BindEngineLifecycle()
{
	if (GPBuildingDropCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		return;
	}

	GPBuildingDropCatalogPrivate::EnginePreExitHandle =
		FCoreDelegates::OnEnginePreExit.AddStatic(&UGP_BuildingDropCatalog::ShutdownCatalog);
}

void UGP_BuildingDropCatalog::UnbindEngineLifecycle()
{
	if (GPBuildingDropCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(GPBuildingDropCatalogPrivate::EnginePreExitHandle);
		GPBuildingDropCatalogPrivate::EnginePreExitHandle.Reset();
	}
}

void UGP_BuildingDropCatalog::EnsureNativeCatalog()
{
	if (bNativeCatalogReady)
	{
		return;
	}

	const FGPGameplayTags& Tags = FGPGameplayTags::Get();

	UGP_BuildingDefinition* HubBuilding = CreateNativeBuilding(
		FName(TEXT("DA_GP_Building_LogisticsHub")),
		NSLOCTEXT("GPBuildingDropCatalog", "Hub", "Logistics Hub"),
		Tags.Building_Type_LogisticsHub,
		FIntPoint(4, 4),
		500.0f);
	UGP_BuildingDefinition* TurretBuilding = CreateNativeBuilding(
		FName(TEXT("DA_GP_Building_DefensiveTurret")),
		NSLOCTEXT("GPBuildingDropCatalog", "Turret", "Defensive Turret"),
		Tags.Building_Type_DefensiveTurret,
		FIntPoint(4, 4),
		400.0f);
	UGP_BuildingDefinition* WallBuilding = CreateNativeBuilding(
		FName(TEXT("DA_GP_Building_Wall")),
		NSLOCTEXT("GPBuildingDropCatalog", "Wall", "Wall"),
		Tags.Building_Type_Wall,
		FIntPoint(2, 2),
		300.0f);
	UGP_BuildingDefinition* WallTurretBuilding = CreateNativeBuilding(
		FName(TEXT("DA_GP_Building_WallTurret")),
		NSLOCTEXT("GPBuildingDropCatalog", "WallTurret", "Wall Turret"),
		Tags.Building_Type_WallTurret,
		FIntPoint(2, 2),
		350.0f);

	LegacyLogisticsHubDrop = CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub")),
		HubBuilding,
		Tags.Drop_Type_Building,
		100.0f);
	CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalDrop_DefensiveTurret")),
		TurretBuilding,
		Tags.Drop_Type_Building,
		150.0f);
	CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalDrop_Wall")),
		WallBuilding,
		Tags.Drop_Type_Wall,
		25.0f);
	CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalDrop_WallTurret")),
		WallTurretBuilding,
		Tags.Drop_Type_Building,
		75.0f);

	bNativeCatalogReady = true;
	SyncLegacyLogisticsHubCompatibility();
}

UGP_BuildingDefinition* UGP_BuildingDropCatalog::CreateNativeBuilding(
	FName AssetName,
	const FText& DisplayName,
	const FGameplayTag& BuildingTypeTag,
	FIntPoint FootprintCells,
	float MaxHealth)
{
	UGP_BuildingDefinition* Def = NewObject<UGP_BuildingDefinition>(this, AssetName, RF_Transient);
	Def->DisplayName = DisplayName;
	Def->BuildingTags.Reset();
	const FGPGameplayTags& Tags = FGPGameplayTags::Get();
	if (Tags.Unit_Type_Building.IsValid())
	{
		Def->BuildingTags.AddTag(Tags.Unit_Type_Building);
	}
	if (BuildingTypeTag.IsValid())
	{
		Def->BuildingTags.AddTag(BuildingTypeTag);
	}
	Def->FootprintCells = FootprintCells;
	Def->MaxHealth = MaxHealth;
	NativeBuildings.Add(Def);
	return Def;
}

UGP_OrbitalDropDefinition* UGP_BuildingDropCatalog::CreateNativeDrop(
	FName AssetName,
	UGP_BuildingDefinition* BuildingDefinition,
	const FGameplayTag& DropTypeTag,
	float Cost)
{
	UGP_OrbitalDropDefinition* Drop = NewObject<UGP_OrbitalDropDefinition>(this, AssetName, RF_Transient);
	Drop->Cost = FMath::Max(0.0f, Cost);
	Drop->BuildingDefinition = BuildingDefinition;
	Drop->DropTags.Reset();
	if (DropTypeTag.IsValid())
	{
		Drop->DropTags.AddTag(DropTypeTag);
	}
	NativeDrops.Add(Drop);
	return Drop;
}

FPrimaryAssetId UGP_BuildingDropCatalog::GetLegacyLogisticsHubDropId() const
{
	return IsValid(LegacyLogisticsHubDrop) ? LegacyLogisticsHubDrop->GetPrimaryAssetId() : FPrimaryAssetId();
}

void UGP_BuildingDropCatalog::SyncLegacyLogisticsHubCompatibility()
{
	if (!IsValid(LegacyLogisticsHubDrop))
	{
		return;
	}

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	if (Settings == nullptr)
	{
		return;
	}

	LegacyLogisticsHubDrop->Cost = FMath::Max(0.0f, Settings->BuildingOrbitalPurchaseCost);
}

void UGP_BuildingDropCatalog::RegisterDropDefinition(UGP_OrbitalDropDefinition* DropDefinition)
{
	if (!IsValid(DropDefinition))
	{
		return;
	}

	RegisteredDrops.AddUnique(DropDefinition);
}

void UGP_BuildingDropCatalog::RegisterBuildingDefinition(UGP_BuildingDefinition* BuildingDefinition)
{
	if (!IsValid(BuildingDefinition))
	{
		return;
	}

	RegisteredBuildings.AddUnique(BuildingDefinition);
}

UGP_OrbitalDropDefinition* UGP_BuildingDropCatalog::FindDropDefinition(const FPrimaryAssetId& DropDefinitionId) const
{
	if (!DropDefinitionId.IsValid())
	{
		return nullptr;
	}

	auto Matches = [&DropDefinitionId](const UGP_OrbitalDropDefinition* Drop) -> bool
	{
		return IsValid(Drop) && Drop->GetPrimaryAssetId() == DropDefinitionId;
	};

	for (UGP_OrbitalDropDefinition* Drop : RegisteredDrops)
	{
		if (Matches(Drop))
		{
			return Drop;
		}
	}
	for (UGP_OrbitalDropDefinition* Drop : NativeDrops)
	{
		if (Matches(Drop))
		{
			return Drop;
		}
	}

	if (UAssetManager::IsInitialized())
	{
		return Cast<UGP_OrbitalDropDefinition>(UAssetManager::Get().GetPrimaryAssetObject(DropDefinitionId));
	}

	return nullptr;
}

UGP_BuildingDefinition* UGP_BuildingDropCatalog::FindBuildingDefinition(const FPrimaryAssetId& BuildingDefinitionId) const
{
	if (!BuildingDefinitionId.IsValid())
	{
		return nullptr;
	}

	auto Matches = [&BuildingDefinitionId](const UGP_BuildingDefinition* Def) -> bool
	{
		return IsValid(Def) && Def->GetPrimaryAssetId() == BuildingDefinitionId;
	};

	for (UGP_BuildingDefinition* Def : RegisteredBuildings)
	{
		if (Matches(Def))
		{
			return Def;
		}
	}
	for (UGP_BuildingDefinition* Def : NativeBuildings)
	{
		if (Matches(Def))
		{
			return Def;
		}
	}

	if (UAssetManager::IsInitialized())
	{
		return Cast<UGP_BuildingDefinition>(UAssetManager::Get().GetPrimaryAssetObject(BuildingDefinitionId));
	}

	return nullptr;
}

void UGP_BuildingDropCatalog::GetOperatorVisibleDrops(TArray<UGP_OrbitalDropDefinition*>& OutDrops) const
{
	OutDrops.Reset();
	for (UGP_OrbitalDropDefinition* Drop : NativeDrops)
	{
		if (IsValid(Drop))
		{
			OutDrops.Add(Drop);
		}
	}
}

TSubclassOf<AGP_BuildingBase> UGP_BuildingDropCatalog::ResolvePayloadClass(
	const UGP_OrbitalDropDefinition* DropDefinition) const
{
	if (!IsValid(DropDefinition))
	{
		return nullptr;
	}

	if (const UGP_BuildingDefinition* Building = DropDefinition->ResolveLoadedBuildingDefinition())
	{
		if (TSubclassOf<AGP_BuildingBase> Loaded = Building->ResolveLoadedSpawnedClass())
		{
			return Loaded;
		}
	}

	if (DropDefinition == LegacyLogisticsHubDrop)
	{
		const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
		if (Settings != nullptr)
		{
			return Settings->ResolveBuildingPayloadClass();
		}
		return TSubclassOf<AGP_BuildingBase>(AGP_LogisticsHub::StaticClass());
	}

	return nullptr;
}

float UGP_BuildingDropCatalog::GetPurchaseCost(const UGP_OrbitalDropDefinition* DropDefinition) const
{
	if (!IsValid(DropDefinition))
	{
		return 0.0f;
	}

	if (DropDefinition == LegacyLogisticsHubDrop)
	{
		const_cast<UGP_BuildingDropCatalog*>(this)->SyncLegacyLogisticsHubCompatibility();
	}

	return FMath::Max(0.0f, DropDefinition->Cost);
}
