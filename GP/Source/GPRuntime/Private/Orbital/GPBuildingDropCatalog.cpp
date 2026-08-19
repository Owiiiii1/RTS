// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingDropCatalog.h"

#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPDefensiveTurret.h"
#include "Buildings/GPLogisticsHub.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CoreMisc.h"
#include "Orbital/GPOrbitalDropDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPBuildingDropCatalog, Log, All);
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPUnitDefinitionCatalog.h"
#include "UObject/StrongObjectPtr.h"

namespace GPBuildingDropCatalogPrivate
{
	static TStrongObjectPtr<UGP_BuildingDropCatalog> GCatalog;
	static FDelegateHandle EnginePreExitHandle;
	static bool bInShutdownCatalog = false;
	static bool bEngineExitLocked = false;

	static bool IsCreationBlocked()
	{
		return bInShutdownCatalog || bEngineExitLocked || IsEngineExitRequested();
	}

	static constexpr TCHAR CatalogObjectName[] = TEXT("GP_BuildingDropCatalog");
	static constexpr TCHAR UnresolvedDropStub[] =
		TEXT("/Game/GrimProtocol/Data/Orbital/DA_GP_OrbitalDrop_UnresolvedSoftRefStub.DA_GP_OrbitalDrop_UnresolvedSoftRefStub");
	static constexpr TCHAR UnresolvedBuildingStub[] =
		TEXT("/Game/GrimProtocol/Data/Buildings/DA_GP_Building_UnresolvedSoftRefStub.DA_GP_Building_UnresolvedSoftRefStub");

	static const TCHAR* SlotNameFromIndex(int32 Index)
	{
		switch (Index)
		{
		case 0:
			return TEXT("LogisticsHub");
		case 1:
			return TEXT("DefensiveTurret");
		case 2:
			return TEXT("Wall");
		case 3:
			return TEXT("WallTurret");
		default:
			return TEXT("Unknown");
		}
	}
}

UGP_BuildingDropCatalog* UGP_BuildingDropCatalog::TryGetExisting()
{
	if (GPBuildingDropCatalogPrivate::GCatalog.IsValid())
	{
		return GPBuildingDropCatalogPrivate::GCatalog.Get();
	}
	return nullptr;
}

UGP_BuildingDropCatalog& UGP_BuildingDropCatalog::Get()
{
	if (UGP_BuildingDropCatalog* Existing = TryGetExisting())
	{
		if (!GPBuildingDropCatalogPrivate::IsCreationBlocked())
		{
			Existing->RefreshAuthoredBindings();
			Existing->SyncLegacyLogisticsHubCompatibility();
		}
		return *Existing;
	}

	if (GPBuildingDropCatalogPrivate::IsCreationBlocked())
	{
		return *GetMutableDefault<UGP_BuildingDropCatalog>();
	}

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
	CatalogObj->RefreshAuthoredBindings();
	CatalogObj->SyncLegacyLogisticsHubCompatibility();
	return *CatalogObj;
}

void UGP_BuildingDropCatalog::ShutdownCatalog()
{
	if (GPBuildingDropCatalogPrivate::bInShutdownCatalog)
	{
		return;
	}

	GPBuildingDropCatalogPrivate::bInShutdownCatalog = true;
	if (GPBuildingDropCatalogPrivate::GCatalog.IsValid())
	{
		GPBuildingDropCatalogPrivate::GCatalog->CancelAllAuthoredLoads();
	}
	GPBuildingDropCatalogPrivate::GCatalog.Reset();
	if (!GPBuildingDropCatalogPrivate::bEngineExitLocked && !IsEngineExitRequested())
	{
		GPBuildingDropCatalogPrivate::bInShutdownCatalog = false;
	}
}

void UGP_BuildingDropCatalog::NotifyEngineShutdown()
{
	GPBuildingDropCatalogPrivate::bEngineExitLocked = true;
	ShutdownCatalog();
}

void UGP_BuildingDropCatalog::BindEngineLifecycle()
{
	if (GPBuildingDropCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		return;
	}

	GPBuildingDropCatalogPrivate::EnginePreExitHandle =
		FCoreDelegates::OnEnginePreExit.AddStatic(&UGP_BuildingDropCatalog::NotifyEngineShutdown);
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
	AuthoredNestedLoadHandles.SetNum(SlotCount);
	AuthoredRequestedPaths.SetNum(SlotCount);
	AuthoredNestedRequestedPaths.SetNum(SlotCount);
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
	Drop->DeliveryDescentSeconds = NativeDeliveryDescentSeconds;
	Drop->PayloadDeployDelaySeconds = NativePayloadDeployDelaySeconds;
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
		&& IsValid(AuthoredSlotDrops[Index])
		&& AuthoredSlotDrops[Index]->ResolveLoadedBuildingDefinition() != nullptr)
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
		AuthoredNestedRequestedPaths[Index].Reset();
		AuthoredStates[Index] = EAuthoredSlotState::Empty;
		return;
	}

	const FSoftObjectPath SoftPath = Soft.ToSoftObjectPath();

#if !UE_BUILD_SHIPPING
	if (DebugForceUnresolvedDrop.IsValidIndex(Index) && DebugForceUnresolvedDrop[Index] != 0
		&& AuthoredStates[Index] != EAuthoredSlotState::Ready)
	{
		if (AuthoredStates[Index] == EAuthoredSlotState::Pending && AuthoredRequestedPaths[Index] == SoftPath)
		{
			return;
		}
		RequestAuthoredAsyncLoad(Slot, SoftPath);
		return;
	}
#endif

	if (UGP_OrbitalDropDefinition* Loaded = ResolveLoadedAuthored(Soft))
	{
		ApplyLoadedAuthoredDrop(Slot, Loaded);
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
	if (AuthoredNestedRequestedPaths.IsValidIndex(Index))
	{
		AuthoredNestedRequestedPaths[Index].Reset();
	}
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

#if !UE_BUILD_SHIPPING
	bDebugDidRequestAsyncDropLoad = true;
#endif

	AuthoredLoadHandles[Index] = UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPath, Delegate);
	if (!AuthoredLoadHandles[Index].IsValid())
	{
		UE_LOG(LogGPBuildingDropCatalog, Error,
			TEXT("GP OrbitalDropDefinitionLoadFailed: Slot=%s Path=%s Reason=RequestAsyncLoadNullHandle"),
			GPBuildingDropCatalogPrivate::SlotNameFromIndex(Index),
			*SoftPath.ToString());
#if !UE_BUILD_SHIPPING
		if (DebugHoldDropCompletion.IsValidIndex(Index) && DebugHoldDropCompletion[Index] != 0)
		{
			return;
		}
#endif
		MarkAuthoredSlotFailed(Slot);
	}
}

void UGP_BuildingDropCatalog::RequestAuthoredNestedAsyncLoad(
	EBuildingAuthoredSlot Slot,
	const FSoftObjectPath& NestedPath)
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredNestedLoadHandles.IsValidIndex(Index) && AuthoredNestedLoadHandles[Index].IsValid()
		&& AuthoredNestedRequestedPaths[Index] == NestedPath)
	{
		return;
	}

	CancelAuthoredNestedLoad(Slot);
	AuthoredNestedRequestedPaths[Index] = NestedPath;
	AuthoredStates[Index] = EAuthoredSlotState::Pending;

	FStreamableDelegate Delegate;
	switch (Slot)
	{
	case EBuildingAuthoredSlot::LogisticsHub:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_BuildingDropCatalog::HandleLogisticsHubNestedLoaded);
		break;
	case EBuildingAuthoredSlot::DefensiveTurret:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_BuildingDropCatalog::HandleDefensiveTurretNestedLoaded);
		break;
	case EBuildingAuthoredSlot::Wall:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_BuildingDropCatalog::HandleWallNestedLoaded);
		break;
	case EBuildingAuthoredSlot::WallTurret:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_BuildingDropCatalog::HandleWallTurretNestedLoaded);
		break;
	default:
		MarkAuthoredSlotFailed(Slot);
		return;
	}

#if !UE_BUILD_SHIPPING
	bDebugDidRequestAsyncNestedLoad = true;
#endif

	AuthoredNestedLoadHandles[Index] = UAssetManager::GetStreamableManager().RequestAsyncLoad(NestedPath, Delegate);
	if (!AuthoredNestedLoadHandles[Index].IsValid())
	{
		const FSoftObjectPath DropPath = AuthoredRequestedPaths.IsValidIndex(Index)
			? AuthoredRequestedPaths[Index]
			: FSoftObjectPath();
		UE_LOG(LogGPBuildingDropCatalog, Error,
			TEXT("GP BuildingDefinitionLoadFailed: Slot=%s Drop=%s BuildingDefinition=%s Reason=RequestAsyncLoadNullHandleUsingNativeFallback"),
			GPBuildingDropCatalogPrivate::SlotNameFromIndex(Index),
			*DropPath.ToString(),
			*NestedPath.ToString());
#if !UE_BUILD_SHIPPING
		bDebugNestedLoadFailedLogged = true;
		if (DebugHoldNestedCompletion.IsValidIndex(Index) && DebugHoldNestedCompletion[Index] != 0)
		{
			return;
		}
#endif
		MarkAuthoredSlotFailed(Slot);
	}
}

bool UGP_BuildingDropCatalog::IsCatalogCallbackSafe() const
{
	return IsValid(this)
		&& !GPBuildingDropCatalogPrivate::IsCreationBlocked()
		&& GPBuildingDropCatalogPrivate::GCatalog.Get() == this;
}

void UGP_BuildingDropCatalog::HandleAuthoredLoaded(EBuildingAuthoredSlot Slot)
{
	if (!IsCatalogCallbackSafe())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	const int32 Index = static_cast<int32>(Slot);
	if (DebugHoldDropCompletion.IsValidIndex(Index) && DebugHoldDropCompletion[Index] != 0)
	{
		return;
	}
#endif

	FinishAuthoredLoadResolve(Slot);
}

void UGP_BuildingDropCatalog::HandleAuthoredNestedLoaded(EBuildingAuthoredSlot Slot)
{
	if (!IsCatalogCallbackSafe())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	const int32 Index = static_cast<int32>(Slot);
	if (DebugHoldNestedCompletion.IsValidIndex(Index) && DebugHoldNestedCompletion[Index] != 0)
	{
		return;
	}
#endif

	FinishAuthoredNestedLoadResolve(Slot);
}

void UGP_BuildingDropCatalog::FinishAuthoredLoadResolve(EBuildingAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredLoadHandles.IsValidIndex(Index))
	{
		AuthoredLoadHandles[Index].Reset();
	}

#if !UE_BUILD_SHIPPING
	if (DebugForceUnresolvedDrop.IsValidIndex(Index))
	{
		DebugForceUnresolvedDrop[Index] = 0;
	}
	if (DebugInjectedDrops.IsValidIndex(Index) && IsValid(DebugInjectedDrops[Index]))
	{
		AssignAuthoredSettingsDrop(Slot, DebugInjectedDrops[Index]);
		ApplyLoadedAuthoredDrop(Slot, DebugInjectedDrops[Index]);
		return;
	}
#endif

	const TSoftObjectPtr<UGP_OrbitalDropDefinition> Soft = GetAuthoredSoftRef(Slot);
	UGP_OrbitalDropDefinition* Loaded = ResolveLoadedAuthored(Soft);
	if (Loaded == nullptr && !Soft.IsNull())
	{
		UE_LOG(LogGPBuildingDropCatalog, Error,
			TEXT("GP OrbitalDropDefinitionLoadFailed: Slot=%s Path=%s Reason=ResolveFailedUsingNativeFallback"),
			GPBuildingDropCatalogPrivate::SlotNameFromIndex(Index),
			*Soft.ToSoftObjectPath().ToString());
		MarkAuthoredSlotFailed(Slot);
		return;
	}

	ApplyLoadedAuthoredDrop(Slot, Loaded);
}

void UGP_BuildingDropCatalog::FinishAuthoredNestedLoadResolve(EBuildingAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredNestedLoadHandles.IsValidIndex(Index))
	{
		AuthoredNestedLoadHandles[Index].Reset();
	}

#if !UE_BUILD_SHIPPING
	if (DebugForceUnresolvedNested.IsValidIndex(Index))
	{
		DebugForceUnresolvedNested[Index] = 0;
	}
	if (DebugInjectedBuildings.IsValidIndex(Index) && IsValid(DebugInjectedBuildings[Index]))
	{
		UGP_OrbitalDropDefinition* Drop = AuthoredSlotDrops.IsValidIndex(Index) ? AuthoredSlotDrops[Index].Get() : nullptr;
		if (!IsValid(Drop) && DebugInjectedDrops.IsValidIndex(Index))
		{
			Drop = DebugInjectedDrops[Index];
		}
		if (IsValid(Drop))
		{
			Drop->BuildingDefinition = DebugInjectedBuildings[Index];
			ApplyLoadedAuthoredDrop(Slot, Drop);
			return;
		}
	}
#endif

	UGP_OrbitalDropDefinition* Drop = AuthoredSlotDrops.IsValidIndex(Index) ? AuthoredSlotDrops[Index].Get() : nullptr;
	if (!IsValid(Drop))
	{
		Drop = ResolveLoadedAuthored(GetAuthoredSoftRef(Slot));
	}

	if (IsValid(Drop) && Drop->ResolveLoadedBuildingDefinition() != nullptr)
	{
		ApplyLoadedAuthoredDrop(Slot, Drop);
		return;
	}

	const FSoftObjectPath DropPath = AuthoredRequestedPaths.IsValidIndex(Index)
		? AuthoredRequestedPaths[Index]
		: FSoftObjectPath();
	const FSoftObjectPath NestedPath = AuthoredNestedRequestedPaths.IsValidIndex(Index)
		? AuthoredNestedRequestedPaths[Index]
		: (IsValid(Drop) ? Drop->BuildingDefinition.ToSoftObjectPath() : FSoftObjectPath());
	UE_LOG(LogGPBuildingDropCatalog, Error,
		TEXT("GP BuildingDefinitionLoadFailed: Slot=%s Drop=%s BuildingDefinition=%s Reason=ResolveFailedUsingNativeFallback"),
		GPBuildingDropCatalogPrivate::SlotNameFromIndex(Index),
		*DropPath.ToString(),
		*NestedPath.ToString());
#if !UE_BUILD_SHIPPING
	bDebugNestedLoadFailedLogged = true;
#endif
	MarkAuthoredSlotFailed(Slot);
}

void UGP_BuildingDropCatalog::ApplyLoadedAuthoredDrop(EBuildingAuthoredSlot Slot, UGP_OrbitalDropDefinition* Loaded)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredStates.IsValidIndex(Index))
	{
		return;
	}

	CancelAuthoredTopLevelLoad(Slot);
	AuthoredRequestedPaths[Index] = GetAuthoredSoftRef(Slot).ToSoftObjectPath();
	AuthoredSlotDrops[Index] = Loaded;

	if (!IsValid(Loaded))
	{
		CancelAuthoredNestedLoad(Slot);
		AuthoredNestedRequestedPaths[Index].Reset();
		AuthoredStates[Index] = EAuthoredSlotState::Empty;
		return;
	}

	if (Loaded->BuildingDefinition.IsNull())
	{
		UE_LOG(LogGPBuildingDropCatalog, Error,
			TEXT("GP BuildingDefinitionLoadFailed: Slot=%s Drop=%s BuildingDefinition=None Reason=NullBuildingDefinitionUsingNativeFallback"),
			GPBuildingDropCatalogPrivate::SlotNameFromIndex(Index),
			*Loaded->GetPathName());
#if !UE_BUILD_SHIPPING
		bDebugNullBuildingLogged = true;
#endif
		MarkAuthoredSlotFailed(Slot);
		return;
	}

#if !UE_BUILD_SHIPPING
	if (DebugForceUnresolvedNested.IsValidIndex(Index) && DebugForceUnresolvedNested[Index] != 0
		&& AuthoredStates[Index] != EAuthoredSlotState::Ready)
	{
		const FSoftObjectPath NestedPath = Loaded->BuildingDefinition.ToSoftObjectPath();
		if (AuthoredStates[Index] == EAuthoredSlotState::Pending && AuthoredNestedRequestedPaths[Index] == NestedPath)
		{
			return;
		}
		RequestAuthoredNestedAsyncLoad(Slot, NestedPath);
		return;
	}
#endif

	if (Loaded->ResolveLoadedBuildingDefinition() != nullptr)
	{
		CancelAuthoredNestedLoad(Slot);
		AuthoredNestedRequestedPaths[Index].Reset();
		AuthoredStates[Index] = EAuthoredSlotState::Ready;
		return;
	}

	const FSoftObjectPath NestedPath = Loaded->BuildingDefinition.ToSoftObjectPath();
	if (AuthoredStates[Index] == EAuthoredSlotState::Pending && AuthoredNestedRequestedPaths[Index] == NestedPath
		&& ((AuthoredNestedLoadHandles.IsValidIndex(Index) && AuthoredNestedLoadHandles[Index].IsValid())
#if !UE_BUILD_SHIPPING
			|| (DebugHoldNestedCompletion.IsValidIndex(Index) && DebugHoldNestedCompletion[Index] != 0)
#endif
			))
	{
		return;
	}

	RequestAuthoredNestedAsyncLoad(Slot, NestedPath);
}

void UGP_BuildingDropCatalog::MarkAuthoredSlotFailed(EBuildingAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	CancelAuthoredLoad(Slot);
	if (AuthoredSlotDrops.IsValidIndex(Index))
	{
		AuthoredSlotDrops[Index] = nullptr;
	}
	if (AuthoredNestedRequestedPaths.IsValidIndex(Index))
	{
		AuthoredNestedRequestedPaths[Index].Reset();
	}
	if (AuthoredStates.IsValidIndex(Index))
	{
		AuthoredStates[Index] = EAuthoredSlotState::Failed;
	}
}

void UGP_BuildingDropCatalog::CancelAuthoredTopLevelLoad(EBuildingAuthoredSlot Slot)
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

void UGP_BuildingDropCatalog::CancelAuthoredNestedLoad(EBuildingAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredNestedLoadHandles.IsValidIndex(Index) || !AuthoredNestedLoadHandles[Index].IsValid())
	{
		return;
	}
	if (AuthoredNestedLoadHandles[Index]->IsLoadingInProgress())
	{
		AuthoredNestedLoadHandles[Index]->CancelHandle();
	}
	AuthoredNestedLoadHandles[Index].Reset();
}

void UGP_BuildingDropCatalog::CancelAuthoredLoad(EBuildingAuthoredSlot Slot)
{
	CancelAuthoredTopLevelLoad(Slot);
	CancelAuthoredNestedLoad(Slot);
}

void UGP_BuildingDropCatalog::CancelAllAuthoredLoads()
{
	for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
	{
		CancelAuthoredLoad(static_cast<EBuildingAuthoredSlot>(i));
	}
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
	const UGP_OrbitalDropDefinition* Canonical = ResolveCanonicalDrop(DropDefinition);
	if (IsValid(Canonical))
	{
		OutDescentSeconds = Canonical->DeliveryDescentSeconds;
		OutPayloadDeployDelaySeconds = Canonical->PayloadDeployDelaySeconds;
		return;
	}

	OutDescentSeconds = NativeDeliveryDescentSeconds;
	OutPayloadDeployDelaySeconds = NativePayloadDeployDelaySeconds;
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
void UGP_BuildingDropCatalog::EnsureDebugSlotArrays()
{
	const int32 SlotCount = static_cast<int32>(EBuildingAuthoredSlot::COUNT);
	DebugForceUnresolvedDrop.SetNumZeroed(SlotCount);
	DebugHoldDropCompletion.SetNumZeroed(SlotCount);
	DebugForceUnresolvedNested.SetNumZeroed(SlotCount);
	DebugHoldNestedCompletion.SetNumZeroed(SlotCount);
	DebugInjectedDrops.SetNum(SlotCount);
	DebugInjectedBuildings.SetNum(SlotCount);
}

void UGP_BuildingDropCatalog::ResetDebugSlotFlags()
{
	EnsureDebugSlotArrays();
	for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
	{
		DebugForceUnresolvedDrop[i] = 0;
		DebugHoldDropCompletion[i] = 0;
		DebugForceUnresolvedNested[i] = 0;
		DebugHoldNestedCompletion[i] = 0;
		DebugInjectedDrops[i] = nullptr;
		DebugInjectedBuildings[i] = nullptr;
	}
	bDebugDidRequestAsyncDropLoad = false;
	bDebugDidRequestAsyncNestedLoad = false;
}

void UGP_BuildingDropCatalog::SaveAuthoredSettingsIfNeeded()
{
	if (bDebugSavedBuildingSettings)
	{
		return;
	}
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
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
}

void UGP_BuildingDropCatalog::AssignAuthoredSettingsDrop(EBuildingAuthoredSlot Slot, UGP_OrbitalDropDefinition* Definition)
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		SaveAuthoredSettingsIfNeeded();
		switch (Slot)
		{
		case EBuildingAuthoredSlot::LogisticsHub:
			Settings->LogisticsHubDropDefinition = Definition;
			break;
		case EBuildingAuthoredSlot::DefensiveTurret:
			Settings->DefensiveTurretDropDefinition = Definition;
			break;
		case EBuildingAuthoredSlot::Wall:
			Settings->WallDropDefinition = Definition;
			break;
		case EBuildingAuthoredSlot::WallTurret:
			Settings->WallTurretDropDefinition = Definition;
			break;
		default:
			break;
		}
	}
}

void UGP_BuildingDropCatalog::DebugAssignLoadedAuthoredLogisticsHub(UGP_OrbitalDropDefinition* Definition)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(EBuildingAuthoredSlot::LogisticsHub);
	DebugForceUnresolvedDrop[Index] = 0;
	DebugHoldDropCompletion[Index] = 0;
	DebugForceUnresolvedNested[Index] = 0;
	DebugHoldNestedCompletion[Index] = 0;
	DebugInjectedDrops[Index] = nullptr;
	DebugInjectedBuildings[Index] = nullptr;
	AssignAuthoredSettingsDrop(EBuildingAuthoredSlot::LogisticsHub, Definition);
	RefreshAuthoredSlot(EBuildingAuthoredSlot::LogisticsHub);
}

void UGP_BuildingDropCatalog::DebugAssignLoadedAuthoredDefensiveTurret(UGP_OrbitalDropDefinition* Definition)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(EBuildingAuthoredSlot::DefensiveTurret);
	DebugForceUnresolvedDrop[Index] = 0;
	DebugHoldDropCompletion[Index] = 0;
	DebugForceUnresolvedNested[Index] = 0;
	DebugHoldNestedCompletion[Index] = 0;
	DebugInjectedDrops[Index] = nullptr;
	DebugInjectedBuildings[Index] = nullptr;
	AssignAuthoredSettingsDrop(EBuildingAuthoredSlot::DefensiveTurret, Definition);
	RefreshAuthoredSlot(EBuildingAuthoredSlot::DefensiveTurret);
}

void UGP_BuildingDropCatalog::DebugForceUnresolvedAuthoredLoad(
	EBuildingAuthoredSlot Slot,
	UGP_OrbitalDropDefinition* InjectedDefinition,
	bool bHoldCompletion)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(Slot);
	SaveAuthoredSettingsIfNeeded();
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		const FSoftObjectPath StubPath(GPBuildingDropCatalogPrivate::UnresolvedDropStub);
		const TSoftObjectPtr<UGP_OrbitalDropDefinition> Stub{StubPath};
		switch (Slot)
		{
		case EBuildingAuthoredSlot::LogisticsHub:
			Settings->LogisticsHubDropDefinition = Stub;
			break;
		case EBuildingAuthoredSlot::DefensiveTurret:
			Settings->DefensiveTurretDropDefinition = Stub;
			break;
		case EBuildingAuthoredSlot::Wall:
			Settings->WallDropDefinition = Stub;
			break;
		case EBuildingAuthoredSlot::WallTurret:
			Settings->WallTurretDropDefinition = Stub;
			break;
		default:
			break;
		}
	}

	DebugForceUnresolvedDrop[Index] = 1;
	DebugHoldDropCompletion[Index] = bHoldCompletion ? 1 : 0;
	DebugForceUnresolvedNested[Index] = 0;
	DebugHoldNestedCompletion[Index] = 0;
	DebugInjectedDrops[Index] = InjectedDefinition;
	DebugInjectedBuildings[Index] = nullptr;
	bDebugDidRequestAsyncDropLoad = false;
	CancelAuthoredLoad(Slot);
	if (AuthoredSlotDrops.IsValidIndex(Index))
	{
		AuthoredSlotDrops[Index] = nullptr;
	}
	AuthoredStates[Index] = EAuthoredSlotState::Empty;
	RefreshAuthoredSlot(Slot);
}

void UGP_BuildingDropCatalog::DebugForceUnresolvedAuthoredLogisticsHubLoad(
	UGP_OrbitalDropDefinition* InjectedDefinition,
	bool bHoldCompletion)
{
	DebugForceUnresolvedAuthoredLoad(EBuildingAuthoredSlot::LogisticsHub, InjectedDefinition, bHoldCompletion);
}

void UGP_BuildingDropCatalog::DebugCompletePendingAuthoredLoad(EBuildingAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredStates.IsValidIndex(Index) || AuthoredStates[Index] != EAuthoredSlotState::Pending)
	{
		return;
	}
	if (DebugHoldDropCompletion.IsValidIndex(Index))
	{
		DebugHoldDropCompletion[Index] = 0;
	}
	FinishAuthoredLoadResolve(Slot);
}

void UGP_BuildingDropCatalog::DebugCompletePendingAuthoredLogisticsHubLoad()
{
	DebugCompletePendingAuthoredLoad(EBuildingAuthoredSlot::LogisticsHub);
}

void UGP_BuildingDropCatalog::DebugForceUnresolvedNestedBuildingLoad(
	EBuildingAuthoredSlot Slot,
	UGP_OrbitalDropDefinition* InjectedDrop,
	UGP_BuildingDefinition* InjectedBuilding,
	bool bHoldCompletion)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(Slot);
	if (IsValid(InjectedDrop))
	{
		InjectedDrop->BuildingDefinition = TSoftObjectPtr<UGP_BuildingDefinition>(
			FSoftObjectPath(GPBuildingDropCatalogPrivate::UnresolvedBuildingStub));
	}
	AssignAuthoredSettingsDrop(Slot, InjectedDrop);
	DebugForceUnresolvedDrop[Index] = 0;
	DebugHoldDropCompletion[Index] = 0;
	DebugForceUnresolvedNested[Index] = 1;
	DebugHoldNestedCompletion[Index] = bHoldCompletion ? 1 : 0;
	DebugInjectedDrops[Index] = InjectedDrop;
	DebugInjectedBuildings[Index] = InjectedBuilding;
	bDebugDidRequestAsyncNestedLoad = false;
	CancelAuthoredNestedLoad(Slot);
	if (AuthoredStates.IsValidIndex(Index))
	{
		AuthoredStates[Index] = EAuthoredSlotState::Empty;
	}
	RefreshAuthoredSlot(Slot);
}

void UGP_BuildingDropCatalog::DebugForceUnresolvedNestedLogisticsHubBuildingLoad(
	UGP_OrbitalDropDefinition* InjectedDrop,
	UGP_BuildingDefinition* InjectedBuilding,
	bool bHoldCompletion)
{
	DebugForceUnresolvedNestedBuildingLoad(
		EBuildingAuthoredSlot::LogisticsHub,
		InjectedDrop,
		InjectedBuilding,
		bHoldCompletion);
}

void UGP_BuildingDropCatalog::DebugForceUnresolvedNestedDefensiveTurretBuildingLoad(
	UGP_OrbitalDropDefinition* InjectedDrop,
	UGP_BuildingDefinition* InjectedBuilding,
	bool bHoldCompletion)
{
	DebugForceUnresolvedNestedBuildingLoad(
		EBuildingAuthoredSlot::DefensiveTurret,
		InjectedDrop,
		InjectedBuilding,
		bHoldCompletion);
}

void UGP_BuildingDropCatalog::DebugCompletePendingNestedBuildingLoad()
{
	for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
	{
		if (!AuthoredStates.IsValidIndex(i) || AuthoredStates[i] != EAuthoredSlotState::Pending)
		{
			continue;
		}
		if (DebugHoldNestedCompletion.IsValidIndex(i))
		{
			DebugHoldNestedCompletion[i] = 0;
		}
		FinishAuthoredNestedLoadResolve(static_cast<EBuildingAuthoredSlot>(i));
	}
}

void UGP_BuildingDropCatalog::DebugForceNestedBuildingLoadFailure()
{
	for (int32 i = 0; i < static_cast<int32>(EBuildingAuthoredSlot::COUNT); ++i)
	{
		if (DebugInjectedBuildings.IsValidIndex(i))
		{
			DebugInjectedBuildings[i] = nullptr;
		}
		if (DebugHoldNestedCompletion.IsValidIndex(i))
		{
			DebugHoldNestedCompletion[i] = 0;
		}
		if (DebugForceUnresolvedNested.IsValidIndex(i))
		{
			DebugForceUnresolvedNested[i] = 0;
		}
		if (!AuthoredStates.IsValidIndex(i) || AuthoredStates[i] != EAuthoredSlotState::Pending)
		{
			continue;
		}

		if (AuthoredSlotDrops.IsValidIndex(i) && IsValid(AuthoredSlotDrops[i]))
		{
			AuthoredSlotDrops[i]->BuildingDefinition.Reset();
		}
		if (DebugInjectedDrops.IsValidIndex(i) && IsValid(DebugInjectedDrops[i]))
		{
			DebugInjectedDrops[i]->BuildingDefinition.Reset();
		}

		const FSoftObjectPath DropPath = AuthoredRequestedPaths.IsValidIndex(i)
			? AuthoredRequestedPaths[i]
			: FSoftObjectPath();
		const FSoftObjectPath NestedPath = AuthoredNestedRequestedPaths.IsValidIndex(i)
			? AuthoredNestedRequestedPaths[i]
			: FSoftObjectPath();
		UE_LOG(LogGPBuildingDropCatalog, Error,
			TEXT("GP BuildingDefinitionLoadFailed: Slot=%s Drop=%s BuildingDefinition=%s Reason=ResolveFailedUsingNativeFallback"),
			GPBuildingDropCatalogPrivate::SlotNameFromIndex(i),
			*DropPath.ToString(),
			*NestedPath.ToString());
		bDebugNestedLoadFailedLogged = true;
		MarkAuthoredSlotFailed(static_cast<EBuildingAuthoredSlot>(i));
	}
}

bool UGP_BuildingDropCatalog::DebugConsumeNestedBuildingLoadFailedLog()
{
	const bool bLogged = bDebugNestedLoadFailedLogged;
	bDebugNestedLoadFailedLogged = false;
	return bLogged;
}

bool UGP_BuildingDropCatalog::DebugConsumeNullBuildingDefinitionLog()
{
	const bool bLogged = bDebugNullBuildingLogged;
	bDebugNullBuildingLogged = false;
	return bLogged;
}

UGP_OrbitalDropDefinition* UGP_BuildingDropCatalog::DebugGetCanonicalDefensiveTurretDrop() const
{
	return CanonicalForSlot(EBuildingAuthoredSlot::DefensiveTurret);
}

void UGP_BuildingDropCatalog::DebugClearAuthoredBuildingDropOverrides()
{
	ResetDebugSlotFlags();
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
		if (AuthoredNestedRequestedPaths.IsValidIndex(i))
		{
			AuthoredNestedRequestedPaths[i].Reset();
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
	ResetDebugSlotFlags();
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
		if (AuthoredNestedRequestedPaths.IsValidIndex(i))
		{
			AuthoredNestedRequestedPaths[i].Reset();
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
