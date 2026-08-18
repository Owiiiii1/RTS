// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingDropCatalog.h"

#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPDefensiveTurret.h"
#include "Buildings/GPLogisticsHub.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/CoreDelegates.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPUnitDefinitionCatalog.h"
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
	Catalog.RefreshAuthoredBindings();
	Catalog.SyncLegacyLogisticsHubCompatibility();
	return Catalog;
}

void UGP_BuildingDropCatalog::ShutdownCatalog()
{
	if (GPBuildingDropCatalogPrivate::GCatalog.IsValid())
	{
		for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
		{
			GPBuildingDropCatalogPrivate::GCatalog->CancelAuthoredLoad(static_cast<EBuildingAuthoredSlot>(i));
		}
	}
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

	MainBaseBuilding = CreateNativeBuilding(
		FName(TEXT("DA_GP_Building_MainBase")),
		NSLOCTEXT("GPBuildingDropCatalog", "MainBase", "Main Base"),
		Tags.Building_Type_MainBase,
		FIntPoint(5, 5),
		100.0f);
	MainBaseBuilding->ContainerCapacity = 100.0f;
	MainBaseBuilding->ContainerCount = 5;
	MainBaseBuilding->UnitCapBonus = 0;
	MainBaseBuilding->UnitDefinition = UGP_UnitDefinitionCatalog::Get().GetMainBaseDefinition();

	UGP_BuildingDefinition* HubBuilding = CreateNativeBuilding(
		FName(TEXT("DA_GP_Building_LogisticsHub")),
		NSLOCTEXT("GPBuildingDropCatalog", "Hub", "Logistics Hub"),
		Tags.Building_Type_LogisticsHub,
		FIntPoint(4, 4),
		500.0f);
	HubBuilding->ContainerCapacity = 0.0f;
	HubBuilding->ContainerCount = 0;
	HubBuilding->UnitCapBonus = 5;
	HubBuilding->UnitDefinition = UGP_UnitDefinitionCatalog::Get().GetLogisticsHubDefinition();
	UGP_BuildingDefinition* TurretBuilding = CreateNativeBuilding(
		FName(TEXT("DA_GP_Building_DefensiveTurret")),
		NSLOCTEXT("GPBuildingDropCatalog", "Turret", "Defensive Turret"),
		Tags.Building_Type_DefensiveTurret,
		FIntPoint(2, 2),
		400.0f);
	TurretBuilding->SpawnedClass = AGP_DefensiveTurret::StaticClass();
	TurretBuilding->UnitDefinition = UGP_UnitDefinitionCatalog::Get().GetDefensiveTurretDefinition();
	TurretBuilding->ContainerCapacity = 0.0f;
	TurretBuilding->ContainerCount = 0;
	TurretBuilding->UnitCapBonus = 0;
	UGP_BuildingDefinition* WallBuilding = CreateNativeBuilding(
		FName(TEXT("DA_GP_Building_Wall")),
		NSLOCTEXT("GPBuildingDropCatalog", "Wall", "Wall"),
		Tags.Building_Type_Wall,
		FIntPoint(2, 2),
		300.0f);
	WallBuilding->ContainerCapacity = 0.0f;
	WallBuilding->ContainerCount = 0;
	WallBuilding->UnitCapBonus = 0;
	UGP_BuildingDefinition* WallTurretBuilding = CreateNativeBuilding(
		FName(TEXT("DA_GP_Building_WallTurret")),
		NSLOCTEXT("GPBuildingDropCatalog", "WallTurret", "Wall Turret"),
		Tags.Building_Type_WallTurret,
		FIntPoint(2, 2),
		350.0f);
	WallTurretBuilding->ContainerCapacity = 0.0f;
	WallTurretBuilding->ContainerCount = 0;
	WallTurretBuilding->UnitCapBonus = 0;

	const int32 SlotCount = static_cast<int32>(EBuildingAuthoredSlot::COUNT);
	NativeSlotDrops.SetNum(SlotCount);
	AuthoredSlotDrops.SetNum(SlotCount);
	AuthoredLoadHandles.SetNum(SlotCount);
	AuthoredRequestedPaths.SetNum(SlotCount);
	AuthoredStates.Init(EAuthoredSlotState::Empty, SlotCount);

	LegacyLogisticsHubDrop = CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalDrop_LogisticsHub")),
		HubBuilding,
		Tags.Drop_Type_Building,
		100.0f);
	NativeSlotDrops[static_cast<int32>(EBuildingAuthoredSlot::LogisticsHub)] = LegacyLogisticsHubDrop;
	NativeSlotDrops[static_cast<int32>(EBuildingAuthoredSlot::DefensiveTurret)] = CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalDrop_DefensiveTurret")),
		TurretBuilding,
		Tags.Drop_Type_Building,
		150.0f);
	NativeSlotDrops[static_cast<int32>(EBuildingAuthoredSlot::Wall)] = CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalDrop_Wall")),
		WallBuilding,
		Tags.Drop_Type_Wall,
		25.0f);
	NativeSlotDrops[static_cast<int32>(EBuildingAuthoredSlot::WallTurret)] = CreateNativeDrop(
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
	Drop->DeliveryDescentSeconds = 2.5f;
	Drop->PayloadDeployDelaySeconds = 2.0f;
	Drop->DropTags.Reset();
	if (DropTypeTag.IsValid())
	{
		Drop->DropTags.AddTag(DropTypeTag);
	}
	NativeDrops.Add(Drop);
	return Drop;
}

UGP_OrbitalDropDefinition* UGP_BuildingDropCatalog::GetLegacyLogisticsHubDrop() const
{
	if (UGP_OrbitalDropDefinition* Canonical = CanonicalForSlot(EBuildingAuthoredSlot::LogisticsHub))
	{
		return Canonical;
	}
	return LegacyLogisticsHubDrop;
}

FPrimaryAssetId UGP_BuildingDropCatalog::GetLegacyLogisticsHubDropId() const
{
	if (const UGP_OrbitalDropDefinition* Drop = GetLegacyLogisticsHubDrop())
	{
		return Drop->GetPrimaryAssetId();
	}
	return FPrimaryAssetId();
}

TSoftObjectPtr<UGP_OrbitalDropDefinition> UGP_BuildingDropCatalog::GetAuthoredSoftRef(EBuildingAuthoredSlot Slot) const
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	if (Settings == nullptr)
	{
		return TSoftObjectPtr<UGP_OrbitalDropDefinition>();
	}

	switch (Slot)
	{
	case EBuildingAuthoredSlot::LogisticsHub:
		return Settings->LogisticsHubDropDefinition;
	case EBuildingAuthoredSlot::DefensiveTurret:
		return Settings->DefensiveTurretDropDefinition;
	case EBuildingAuthoredSlot::Wall:
		return Settings->WallDropDefinition;
	case EBuildingAuthoredSlot::WallTurret:
		return Settings->WallTurretDropDefinition;
	default:
		return TSoftObjectPtr<UGP_OrbitalDropDefinition>();
	}
}

UGP_OrbitalDropDefinition* UGP_BuildingDropCatalog::ResolveLoadedAuthored(
	const TSoftObjectPtr<UGP_OrbitalDropDefinition>& Soft) const
{
	if (Soft.IsNull())
	{
		return nullptr;
	}

	UObject* Loaded = Soft.Get();
	if (Loaded == nullptr)
	{
		Loaded = Soft.ToSoftObjectPath().ResolveObject();
	}
	return Cast<UGP_OrbitalDropDefinition>(Loaded);
}

UGP_OrbitalDropDefinition* UGP_BuildingDropCatalog::CanonicalForSlot(EBuildingAuthoredSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredStates.IsValidIndex(Index))
	{
		return NativeSlotDrops.IsValidIndex(Index) ? NativeSlotDrops[Index] : nullptr;
	}
	if (AuthoredStates[Index] == EAuthoredSlotState::Pending)
	{
		return nullptr;
	}
	if (AuthoredStates[Index] == EAuthoredSlotState::Ready && AuthoredSlotDrops.IsValidIndex(Index)
		&& IsValid(AuthoredSlotDrops[Index]))
	{
		return AuthoredSlotDrops[Index];
	}
	return NativeSlotDrops.IsValidIndex(Index) ? NativeSlotDrops[Index] : nullptr;
}

void UGP_BuildingDropCatalog::RefreshAuthoredBindings()
{
	EnsureNativeCatalog();
	for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
	{
		RefreshAuthoredSlot(static_cast<EBuildingAuthoredSlot>(i));
	}
}

void UGP_BuildingDropCatalog::RefreshAuthoredSlot(EBuildingAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredStates.IsValidIndex(Index))
	{
		return;
	}

	const TSoftObjectPtr<UGP_OrbitalDropDefinition> Soft = GetAuthoredSoftRef(Slot);
	if (Soft.IsNull())
	{
		CancelAuthoredLoad(Slot);
		AuthoredSlotDrops[Index] = nullptr;
		AuthoredRequestedPaths[Index].Reset();
		AuthoredStates[Index] = EAuthoredSlotState::Empty;
		return;
	}

	const FSoftObjectPath SoftPath = Soft.ToSoftObjectPath();
	if (UGP_OrbitalDropDefinition* Loaded = ResolveLoadedAuthored(Soft))
	{
		CancelAuthoredLoad(Slot);
		AuthoredSlotDrops[Index] = Loaded;
		AuthoredRequestedPaths[Index] = SoftPath;
		AuthoredStates[Index] = EAuthoredSlotState::Ready;
		return;
	}

	if (AuthoredStates[Index] == EAuthoredSlotState::Pending && AuthoredRequestedPaths[Index] == SoftPath)
	{
		return;
	}

	RequestAuthoredAsyncLoad(Slot, SoftPath);
}

void UGP_BuildingDropCatalog::RequestAuthoredAsyncLoad(EBuildingAuthoredSlot Slot, const FSoftObjectPath& SoftPath)
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredLoadHandles.IsValidIndex(Index) && AuthoredLoadHandles[Index].IsValid()
		&& AuthoredRequestedPaths[Index] == SoftPath)
	{
		return;
	}

	CancelAuthoredLoad(Slot);
	AuthoredRequestedPaths[Index] = SoftPath;
	AuthoredStates[Index] = EAuthoredSlotState::Pending;
	AuthoredSlotDrops[Index] = nullptr;

	FStreamableDelegate Delegate;
	switch (Slot)
	{
	case EBuildingAuthoredSlot::LogisticsHub:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_BuildingDropCatalog::HandleLogisticsHubLoaded);
		break;
	case EBuildingAuthoredSlot::DefensiveTurret:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_BuildingDropCatalog::HandleDefensiveTurretLoaded);
		break;
	case EBuildingAuthoredSlot::Wall:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_BuildingDropCatalog::HandleWallLoaded);
		break;
	case EBuildingAuthoredSlot::WallTurret:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_BuildingDropCatalog::HandleWallTurretLoaded);
		break;
	default:
		AuthoredStates[Index] = EAuthoredSlotState::Failed;
		return;
	}

	AuthoredLoadHandles[Index] = UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPath, Delegate);
	if (!AuthoredLoadHandles[Index].IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("GP OrbitalDropDefinitionLoadFailed: Slot=%d Path=%s Reason=RequestAsyncLoadNullHandle"),
			Index,
			*SoftPath.ToString());
		AuthoredStates[Index] = EAuthoredSlotState::Failed;
	}
}

void UGP_BuildingDropCatalog::HandleAuthoredLoaded(EBuildingAuthoredSlot Slot)
{
	if (!IsValid(this))
	{
		return;
	}
	FinishAuthoredLoadResolve(Slot);
}

void UGP_BuildingDropCatalog::FinishAuthoredLoadResolve(EBuildingAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	const TSoftObjectPtr<UGP_OrbitalDropDefinition> Soft = GetAuthoredSoftRef(Slot);
	UGP_OrbitalDropDefinition* Loaded = ResolveLoadedAuthored(Soft);
	if (Loaded == nullptr && !Soft.IsNull())
	{
		UE_LOG(LogTemp, Error,
			TEXT("GP OrbitalDropDefinitionLoadFailed: Slot=%d Path=%s Reason=ResolveFailedUsingNativeFallback"),
			Index,
			*Soft.ToSoftObjectPath().ToString());
		AuthoredSlotDrops[Index] = nullptr;
		AuthoredStates[Index] = EAuthoredSlotState::Failed;
		if (AuthoredLoadHandles.IsValidIndex(Index))
		{
			AuthoredLoadHandles[Index].Reset();
		}
		return;
	}

	AuthoredSlotDrops[Index] = Loaded;
	AuthoredStates[Index] = IsValid(Loaded) ? EAuthoredSlotState::Ready : EAuthoredSlotState::Empty;
	if (AuthoredLoadHandles.IsValidIndex(Index))
	{
		AuthoredLoadHandles[Index].Reset();
	}
}

void UGP_BuildingDropCatalog::CancelAuthoredLoad(EBuildingAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredLoadHandles.IsValidIndex(Index) || !AuthoredLoadHandles[Index].IsValid())
	{
		return;
	}
	if (AuthoredLoadHandles[Index]->IsLoadingInProgress())
	{
		AuthoredLoadHandles[Index]->CancelHandle();
	}
	AuthoredLoadHandles[Index].Reset();
}

UGP_BuildingDropCatalog::EBuildingAuthoredSlot UGP_BuildingDropCatalog::FindSlotForDrop(
	const UGP_OrbitalDropDefinition* DropDefinition) const
{
	if (!IsValid(DropDefinition))
	{
		return EBuildingAuthoredSlot::COUNT;
	}
	return FindSlotForId(DropDefinition->GetPrimaryAssetId());
}

UGP_BuildingDropCatalog::EBuildingAuthoredSlot UGP_BuildingDropCatalog::FindSlotForId(
	const FPrimaryAssetId& DropDefinitionId) const
{
	if (!DropDefinitionId.IsValid())
	{
		return EBuildingAuthoredSlot::COUNT;
	}

	for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
	{
		if (NativeSlotDrops.IsValidIndex(i) && IsValid(NativeSlotDrops[i])
			&& NativeSlotDrops[i]->GetPrimaryAssetId() == DropDefinitionId)
		{
			return static_cast<EBuildingAuthoredSlot>(i);
		}
		if (AuthoredSlotDrops.IsValidIndex(i) && IsValid(AuthoredSlotDrops[i])
			&& AuthoredSlotDrops[i]->GetPrimaryAssetId() == DropDefinitionId)
		{
			return static_cast<EBuildingAuthoredSlot>(i);
		}
		if (AuthoredRequestedPaths.IsValidIndex(i) && AuthoredRequestedPaths[i].IsValid()
			&& FName(*AuthoredRequestedPaths[i].GetAssetName()) == DropDefinitionId.PrimaryAssetName)
		{
			return static_cast<EBuildingAuthoredSlot>(i);
		}
	}
	return EBuildingAuthoredSlot::COUNT;
}

UGP_OrbitalDropDefinition* UGP_BuildingDropCatalog::ResolveCanonicalDrop(
	const UGP_OrbitalDropDefinition* DropDefinition) const
{
	const EBuildingAuthoredSlot Slot = FindSlotForDrop(DropDefinition);
	if (Slot != EBuildingAuthoredSlot::COUNT)
	{
		return CanonicalForSlot(Slot);
	}
	return const_cast<UGP_OrbitalDropDefinition*>(DropDefinition);
}

bool UGP_BuildingDropCatalog::IsDropDefinitionPending(const UGP_OrbitalDropDefinition* DropDefinition) const
{
	return IsDropDefinitionIdPending(IsValid(DropDefinition) ? DropDefinition->GetPrimaryAssetId() : FPrimaryAssetId());
}

bool UGP_BuildingDropCatalog::IsDropDefinitionIdPending(const FPrimaryAssetId& DropDefinitionId) const
{
	const EBuildingAuthoredSlot Slot = FindSlotForId(DropDefinitionId);
	const int32 Index = static_cast<int32>(Slot);
	return AuthoredStates.IsValidIndex(Index) && AuthoredStates[Index] == EAuthoredSlotState::Pending;
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

	const EBuildingAuthoredSlot Slot = FindSlotForId(DropDefinitionId);
	if (Slot != EBuildingAuthoredSlot::COUNT)
	{
		const int32 Index = static_cast<int32>(Slot);
		if (AuthoredStates.IsValidIndex(Index) && AuthoredStates[Index] == EAuthoredSlotState::Pending)
		{
			return nullptr;
		}
		if (UGP_OrbitalDropDefinition* Canonical = CanonicalForSlot(Slot))
		{
			return Canonical;
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
	for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
	{
		if (UGP_OrbitalDropDefinition* Canonical = CanonicalForSlot(static_cast<EBuildingAuthoredSlot>(i)))
		{
			OutDrops.Add(Canonical);
		}
		else if (NativeSlotDrops.IsValidIndex(i) && IsValid(NativeSlotDrops[i]))
		{
			// Pending authored: keep native identity visible, purchase still rejects DefinitionNotReady.
			OutDrops.Add(NativeSlotDrops[i]);
		}
	}
}

TSubclassOf<AGP_BuildingBase> UGP_BuildingDropCatalog::ResolvePayloadClass(
	const UGP_OrbitalDropDefinition* DropDefinition) const
{
	DropDefinition = ResolveCanonicalDrop(DropDefinition);
	if (!IsValid(DropDefinition))
	{
		return nullptr;
	}

	const UGP_BuildingDefinition* Building = DropDefinition->ResolveLoadedBuildingDefinition();
	const FGPGameplayTags& Tags = FGPGameplayTags::Get();
	const bool bDefensiveTurretDrop = IsValid(Building)
		&& Tags.Building_Type_DefensiveTurret.IsValid()
		&& Building->BuildingTags.HasTag(Tags.Building_Type_DefensiveTurret);

	if (bDefensiveTurretDrop)
	{
		if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
		{
			bool bUsedAuthored = false;
			const TSubclassOf<AGP_BuildingBase> Authored = Settings->ResolveDefensiveTurretPayloadClass(&bUsedAuthored);
			if (bUsedAuthored)
			{
				return Authored;
			}
		}
	}

	if (IsValid(Building))
	{
		if (TSubclassOf<AGP_BuildingBase> Loaded = Building->ResolveLoadedSpawnedClass())
		{
			return Loaded;
		}
	}

	if (bDefensiveTurretDrop)
	{
		return TSubclassOf<AGP_BuildingBase>(AGP_DefensiveTurret::StaticClass());
	}

	if (DropDefinition == LegacyLogisticsHubDrop
		|| FindSlotForDrop(DropDefinition) == EBuildingAuthoredSlot::LogisticsHub)
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

	if (IsDropDefinitionPending(DropDefinition))
	{
		return 0.0f;
	}

	const UGP_OrbitalDropDefinition* Canonical = ResolveCanonicalDrop(DropDefinition);
	if (!IsValid(Canonical))
	{
		return 0.0f;
	}

	if (Canonical == LegacyLogisticsHubDrop)
	{
		const_cast<UGP_BuildingDropCatalog*>(this)->SyncLegacyLogisticsHubCompatibility();
	}

	return FMath::Max(0.0f, Canonical->Cost);
}

void UGP_BuildingDropCatalog::ResolveDeliveryTiming(
	const UGP_OrbitalDropDefinition* DropDefinition,
	float& OutDescentSeconds,
	float& OutPayloadDeployDelaySeconds) const
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	OutDescentSeconds = Settings != nullptr ? Settings->BuildingDropDescentDurationSeconds : 2.5f;
	OutPayloadDeployDelaySeconds = Settings != nullptr ? Settings->BuildingDropPayloadDeployDelaySeconds : 2.0f;

	const UGP_OrbitalDropDefinition* Canonical = ResolveCanonicalDrop(DropDefinition);
	if (IsValid(Canonical))
	{
		OutDescentSeconds = Canonical->DeliveryDescentSeconds;
		OutPayloadDeployDelaySeconds = Canonical->PayloadDeployDelaySeconds;
	}
}

void UGP_BuildingDropCatalog::OverrideDeliveryTiming(float DescentSeconds, float PayloadDeployDelaySeconds)
{
	auto Apply = [DescentSeconds, PayloadDeployDelaySeconds](UGP_OrbitalDropDefinition* Drop)
	{
		if (IsValid(Drop))
		{
			Drop->DeliveryDescentSeconds = DescentSeconds;
			Drop->PayloadDeployDelaySeconds = PayloadDeployDelaySeconds;
		}
	};

	for (UGP_OrbitalDropDefinition* Drop : NativeDrops)
	{
		Apply(Drop);
	}
	for (UGP_OrbitalDropDefinition* Drop : RegisteredDrops)
	{
		Apply(Drop);
	}
	for (UGP_OrbitalDropDefinition* Drop : AuthoredSlotDrops)
	{
		Apply(Drop);
	}
}

#if !UE_BUILD_SHIPPING
void UGP_BuildingDropCatalog::DebugAssignLoadedAuthoredLogisticsHub(UGP_OrbitalDropDefinition* Definition)
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (!bDebugSavedBuildingSettings)
		{
			DebugSavedBuildingRefs.SetNum(static_cast<int32>(EBuildingAuthoredSlot::COUNT));
			DebugSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::LogisticsHub)] =
				Settings->LogisticsHubDropDefinition;
			DebugSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::DefensiveTurret)] =
				Settings->DefensiveTurretDropDefinition;
			DebugSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::Wall)] = Settings->WallDropDefinition;
			DebugSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::WallTurret)] =
				Settings->WallTurretDropDefinition;
			bDebugSavedBuildingSettings = true;
		}
		Settings->LogisticsHubDropDefinition = Definition;
	}
	RefreshAuthoredSlot(EBuildingAuthoredSlot::LogisticsHub);
}

void UGP_BuildingDropCatalog::DebugClearAuthoredBuildingDropOverrides()
{
	for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
	{
		CancelAuthoredLoad(static_cast<EBuildingAuthoredSlot>(i));
		if (AuthoredSlotDrops.IsValidIndex(i))
		{
			AuthoredSlotDrops[i] = nullptr;
		}
		if (AuthoredStates.IsValidIndex(i))
		{
			AuthoredStates[i] = EAuthoredSlotState::Empty;
		}
		if (AuthoredRequestedPaths.IsValidIndex(i))
		{
			AuthoredRequestedPaths[i].Reset();
		}
	}

	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (bDebugSavedBuildingSettings && DebugSavedBuildingRefs.Num() >= static_cast<int32>(EBuildingAuthoredSlot::COUNT))
		{
			Settings->LogisticsHubDropDefinition =
				DebugSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::LogisticsHub)];
			Settings->DefensiveTurretDropDefinition =
				DebugSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::DefensiveTurret)];
			Settings->WallDropDefinition =
				DebugSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::Wall)];
			Settings->WallTurretDropDefinition =
				DebugSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::WallTurret)];
		}
		else
		{
			Settings->LogisticsHubDropDefinition.Reset();
			Settings->DefensiveTurretDropDefinition.Reset();
			Settings->WallDropDefinition.Reset();
			Settings->WallTurretDropDefinition.Reset();
		}
	}
	bDebugSavedBuildingSettings = false;
	DebugSavedBuildingRefs.Reset();
}

void UGP_BuildingDropCatalog::DebugBeginContractIsolation()
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (!bContractIsolationActive)
		{
			ContractSavedBuildingRefs.SetNum(static_cast<int32>(EBuildingAuthoredSlot::COUNT));
			ContractSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::LogisticsHub)] =
				Settings->LogisticsHubDropDefinition;
			ContractSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::DefensiveTurret)] =
				Settings->DefensiveTurretDropDefinition;
			ContractSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::Wall)] = Settings->WallDropDefinition;
			ContractSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::WallTurret)] =
				Settings->WallTurretDropDefinition;
			bContractIsolationActive = true;
		}
		Settings->LogisticsHubDropDefinition.Reset();
		Settings->DefensiveTurretDropDefinition.Reset();
		Settings->WallDropDefinition.Reset();
		Settings->WallTurretDropDefinition.Reset();
	}
	for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
	{
		CancelAuthoredLoad(static_cast<EBuildingAuthoredSlot>(i));
		if (AuthoredSlotDrops.IsValidIndex(i))
		{
			AuthoredSlotDrops[i] = nullptr;
		}
		if (AuthoredStates.IsValidIndex(i))
		{
			AuthoredStates[i] = EAuthoredSlotState::Empty;
		}
		if (AuthoredRequestedPaths.IsValidIndex(i))
		{
			AuthoredRequestedPaths[i].Reset();
		}
	}
}

void UGP_BuildingDropCatalog::DebugEndContractIsolation()
{
	if (!bContractIsolationActive)
	{
		return;
	}
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (ContractSavedBuildingRefs.Num() >= static_cast<int32>(EBuildingAuthoredSlot::COUNT))
		{
			Settings->LogisticsHubDropDefinition =
				ContractSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::LogisticsHub)];
			Settings->DefensiveTurretDropDefinition =
				ContractSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::DefensiveTurret)];
			Settings->WallDropDefinition =
				ContractSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::Wall)];
			Settings->WallTurretDropDefinition =
				ContractSavedBuildingRefs[static_cast<int32>(EBuildingAuthoredSlot::WallTurret)];
		}
	}
	bContractIsolationActive = false;
	ContractSavedBuildingRefs.Reset();
	RefreshAuthoredBindings();
}
#endif
