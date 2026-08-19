// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPOrbitalUnitDropCatalog.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CoreMisc.h"
#include "Orbital/GPOrbitalUnitDropDefinition.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPUnitDefinitionCatalog.h"
#include "Units/GPWorker.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPOrbitalUnitDropCatalog, Log, All);

namespace GPOrbitalUnitDropCatalogPrivate
{
	static TStrongObjectPtr<UGP_OrbitalUnitDropCatalog> GCatalog;
	static FDelegateHandle EnginePreExitHandle;
	static bool bInShutdownCatalog = false;
	static bool bEngineExitLocked = false;

	static bool IsCreationBlocked()
	{
		return bInShutdownCatalog || bEngineExitLocked || IsEngineExitRequested();
	}

	static constexpr TCHAR CatalogObjectName[] = TEXT("GP_OrbitalUnitDropCatalog");
	static constexpr TCHAR UnresolvedWorkerStub[] =
		TEXT("/Game/GrimProtocol/Data/Orbital/DA_GP_OrbitalUnitDrop_UnresolvedSoftRefStub.DA_GP_OrbitalUnitDrop_UnresolvedSoftRefStub");
	static constexpr TCHAR UnresolvedUnitDefStub[] =
		TEXT("/Game/GrimProtocol/Data/Units/DA_GP_Unit_UnresolvedSoftRefStub.DA_GP_Unit_UnresolvedSoftRefStub");
	static constexpr TCHAR UnresolvedPayloadStub[] =
		TEXT("/Game/GrimProtocol/Blueprints/Units/BP_GP_UnresolvedPayloadClassStub.BP_GP_UnresolvedPayloadClassStub_C");

	static const TCHAR* SlotNameFromIndex(int32 Index)
	{
		switch (Index)
		{
		case 0:
			return TEXT("Worker");
		case 1:
			return TEXT("SalvageWalker");
		default:
			return TEXT("Unknown");
		}
	}
}

UGP_OrbitalUnitDropCatalog* UGP_OrbitalUnitDropCatalog::TryGetExisting()
{
	if (GPOrbitalUnitDropCatalogPrivate::GCatalog.IsValid())
	{
		return GPOrbitalUnitDropCatalogPrivate::GCatalog.Get();
	}
	return nullptr;
}

UGP_OrbitalUnitDropCatalog& UGP_OrbitalUnitDropCatalog::Get()
{
	if (UGP_OrbitalUnitDropCatalog* Existing = TryGetExisting())
	{
		if (!GPOrbitalUnitDropCatalogPrivate::IsCreationBlocked())
		{
			Existing->RefreshAuthoredBindings();
		}
		return *Existing;
	}

	if (GPOrbitalUnitDropCatalogPrivate::IsCreationBlocked())
	{
		return *GetMutableDefault<UGP_OrbitalUnitDropCatalog>();
	}

	UGP_OrbitalUnitDropCatalog* CatalogObj = FindObject<UGP_OrbitalUnitDropCatalog>(
		GetTransientPackage(),
		GPOrbitalUnitDropCatalogPrivate::CatalogObjectName);
	if (!IsValid(CatalogObj))
	{
		CatalogObj = NewObject<UGP_OrbitalUnitDropCatalog>(
			GetTransientPackage(),
			GPOrbitalUnitDropCatalogPrivate::CatalogObjectName,
			RF_Transient);
	}

	GPOrbitalUnitDropCatalogPrivate::GCatalog.Reset(CatalogObj);
	CatalogObj->EnsureNativeCatalog();
	CatalogObj->RefreshAuthoredBindings();
	return *CatalogObj;
}

void UGP_OrbitalUnitDropCatalog::ShutdownCatalog()
{
	if (GPOrbitalUnitDropCatalogPrivate::bInShutdownCatalog)
	{
		return;
	}

	GPOrbitalUnitDropCatalogPrivate::bInShutdownCatalog = true;
	if (GPOrbitalUnitDropCatalogPrivate::GCatalog.IsValid())
	{
		GPOrbitalUnitDropCatalogPrivate::GCatalog->CancelAllAuthoredLoads();
	}
	GPOrbitalUnitDropCatalogPrivate::GCatalog.Reset();
	if (!GPOrbitalUnitDropCatalogPrivate::bEngineExitLocked && !IsEngineExitRequested())
	{
		GPOrbitalUnitDropCatalogPrivate::bInShutdownCatalog = false;
	}
}

void UGP_OrbitalUnitDropCatalog::BindEngineLifecycle()
{
	if (GPOrbitalUnitDropCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		return;
	}

	GPOrbitalUnitDropCatalogPrivate::EnginePreExitHandle =
		FCoreDelegates::OnEnginePreExit.AddLambda([]()
		{
			GPOrbitalUnitDropCatalogPrivate::bEngineExitLocked = true;
			UGP_OrbitalUnitDropCatalog::ShutdownCatalog();
		});
}

void UGP_OrbitalUnitDropCatalog::UnbindEngineLifecycle()
{
	if (GPOrbitalUnitDropCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(GPOrbitalUnitDropCatalogPrivate::EnginePreExitHandle);
		GPOrbitalUnitDropCatalogPrivate::EnginePreExitHandle.Reset();
	}
}

void UGP_OrbitalUnitDropCatalog::EnsureNativeCatalog()
{
	if (bNativeCatalogReady)
	{
		return;
	}

	UGP_UnitDefinitionCatalog& Units = UGP_UnitDefinitionCatalog::Get();

	NativeWorkerDrop = CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalUnitDrop_Worker")),
		NSLOCTEXT("GPOrbitalUnitDropCatalog", "Worker", "Worker"));
	NativeWorkerDrop->UnitDefinition = Units.GetWorkerDefinition();
	NativeWorkerDrop->Cost = NativeWorkerOrbitalDropCost;
	NativeWorkerDrop->TransportSlotCost = NativeWorkerTransportSlotCost;
	NativeWorkerDrop->DeliveryDescentSeconds = 2.5f;
	NativeWorkerDrop->PayloadDeployDelaySeconds = 1.25f;

	NativeSalvageWalkerDrop = CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalUnitDrop_SalvageWalker")),
		NSLOCTEXT("GPOrbitalUnitDropCatalog", "SalvageWalker", "Salvage Walker"));
	NativeSalvageWalkerDrop->UnitDefinition = Units.GetSalvageWalkerDefinition();
	NativeSalvageWalkerDrop->Cost = NativeSalvageWalkerOrbitalDropCost;
	NativeSalvageWalkerDrop->TransportSlotCost = NativeSalvageWalkerTransportSlotCost;
	NativeSalvageWalkerDrop->DeliveryDescentSeconds = 2.5f;
	NativeSalvageWalkerDrop->PayloadDeployDelaySeconds = 1.25f;

	const int32 SlotCount = static_cast<int32>(EUnitAuthoredSlot::COUNT);
	NativeSlotDrops.SetNum(SlotCount);
	AuthoredSlotDrops.SetNum(SlotCount);
	AuthoredLoadHandles.SetNum(SlotCount);
	AuthoredUnitDefLoadHandles.SetNum(SlotCount);
	AuthoredPayloadLoadHandles.SetNum(SlotCount);
	AuthoredRequestedPaths.SetNum(SlotCount);
	AuthoredUnitDefRequestedPaths.SetNum(SlotCount);
	AuthoredPayloadRequestedPaths.SetNum(SlotCount);
	AuthoredStates.Init(EAuthoredSlotState::Empty, SlotCount);
	NativeSlotDrops[static_cast<int32>(EUnitAuthoredSlot::Worker)] = NativeWorkerDrop;
	NativeSlotDrops[static_cast<int32>(EUnitAuthoredSlot::SalvageWalker)] = NativeSalvageWalkerDrop;

	bNativeCatalogReady = true;
}

UGP_OrbitalUnitDropDefinition* UGP_OrbitalUnitDropCatalog::CreateNativeDrop(
	FName AssetName,
	const FText& DisplayName)
{
	UGP_OrbitalUnitDropDefinition* Drop = NewObject<UGP_OrbitalUnitDropDefinition>(this, AssetName, RF_Transient);
	Drop->DisplayName = DisplayName;
	return Drop;
}

TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> UGP_OrbitalUnitDropCatalog::GetAuthoredSoftRef(
	EUnitAuthoredSlot Slot) const
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	if (Settings == nullptr)
	{
		return TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>();
	}

	switch (Slot)
	{
	case EUnitAuthoredSlot::Worker:
		return Settings->WorkerDropDefinition;
	case EUnitAuthoredSlot::SalvageWalker:
		return Settings->SalvageWalkerDropDefinition;
	default:
		return TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>();
	}
}

UGP_OrbitalUnitDropDefinition* UGP_OrbitalUnitDropCatalog::ResolveLoadedAuthored(
	const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>& Soft) const
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
	return Cast<UGP_OrbitalUnitDropDefinition>(Loaded);
}

bool UGP_OrbitalUnitDropCatalog::IsPayloadClassValidForSlot(EUnitAuthoredSlot Slot, const UClass* PayloadClass) const
{
	if (PayloadClass == nullptr || !PayloadClass->IsChildOf(AGP_UnitBase::StaticClass()))
	{
		return false;
	}

	switch (Slot)
	{
	case EUnitAuthoredSlot::Worker:
		return PayloadClass->IsChildOf(AGP_Worker::StaticClass());
	case EUnitAuthoredSlot::SalvageWalker:
		return PayloadClass->IsChildOf(AGP_SalvageWalker::StaticClass());
	default:
		return false;
	}
}

bool UGP_OrbitalUnitDropCatalog::HasResolvedAuthoredDependencies(
	const UGP_OrbitalUnitDropDefinition* Drop,
	EUnitAuthoredSlot Slot) const
{
	if (!IsValid(Drop) || Drop->ResolveLoadedUnitDefinition() == nullptr)
	{
		return false;
	}

	const TSubclassOf<AGP_UnitBase> Payload = Drop->ResolveLoadedPayloadClass();
	return Payload != nullptr && IsPayloadClassValidForSlot(Slot, Payload.Get());
}

UGP_OrbitalUnitDropDefinition* UGP_OrbitalUnitDropCatalog::CanonicalForSlot(EUnitAuthoredSlot Slot) const
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
	if (AuthoredStates[Index] == EAuthoredSlotState::Ready
		&& AuthoredSlotDrops.IsValidIndex(Index)
		&& HasResolvedAuthoredDependencies(AuthoredSlotDrops[Index], Slot))
	{
		return AuthoredSlotDrops[Index];
	}
	return NativeSlotDrops.IsValidIndex(Index) ? NativeSlotDrops[Index] : nullptr;
}

const UGP_OrbitalUnitDropDefinition* UGP_OrbitalUnitDropCatalog::ResolveNumericProduct(EUnitAuthoredSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredStates.IsValidIndex(Index)
		&& AuthoredSlotDrops.IsValidIndex(Index)
		&& IsValid(AuthoredSlotDrops[Index])
		&& (AuthoredStates[Index] == EAuthoredSlotState::Pending
			|| AuthoredStates[Index] == EAuthoredSlotState::Ready))
	{
		return AuthoredSlotDrops[Index];
	}
	return CanonicalForSlot(Slot);
}

void UGP_OrbitalUnitDropCatalog::RefreshAuthoredBindings()
{
	EnsureNativeCatalog();
	RefreshAuthoredSlot(EUnitAuthoredSlot::Worker);
	RefreshAuthoredSlot(EUnitAuthoredSlot::SalvageWalker);
}

void UGP_OrbitalUnitDropCatalog::RefreshAuthoredSlot(EUnitAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredStates.IsValidIndex(Index))
	{
		return;
	}

	const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> Soft = GetAuthoredSoftRef(Slot);
	if (Soft.IsNull())
	{
		CancelAuthoredLoad(Slot);
		AuthoredSlotDrops[Index] = nullptr;
		AuthoredRequestedPaths[Index].Reset();
		AuthoredUnitDefRequestedPaths[Index].Reset();
		AuthoredPayloadRequestedPaths[Index].Reset();
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

	if (UGP_OrbitalUnitDropDefinition* Loaded = ResolveLoadedAuthored(Soft))
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

void UGP_OrbitalUnitDropCatalog::RequestAuthoredAsyncLoad(EUnitAuthoredSlot Slot, const FSoftObjectPath& SoftPath)
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredLoadHandles.IsValidIndex(Index) && AuthoredLoadHandles[Index].IsValid()
		&& AuthoredRequestedPaths[Index] == SoftPath)
	{
		return;
	}

	CancelAuthoredLoad(Slot);
	AuthoredRequestedPaths[Index] = SoftPath;
	AuthoredUnitDefRequestedPaths[Index].Reset();
	AuthoredPayloadRequestedPaths[Index].Reset();
	AuthoredStates[Index] = EAuthoredSlotState::Pending;
	AuthoredSlotDrops[Index] = nullptr;

	FStreamableDelegate Delegate;
	switch (Slot)
	{
	case EUnitAuthoredSlot::Worker:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_OrbitalUnitDropCatalog::HandleWorkerLoaded);
		break;
	case EUnitAuthoredSlot::SalvageWalker:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_OrbitalUnitDropCatalog::HandleWalkerLoaded);
		break;
	default:
		AuthoredStates[Index] = EAuthoredSlotState::Failed;
		return;
	}

#if !UE_BUILD_SHIPPING
	if (Slot == EUnitAuthoredSlot::Worker)
	{
		bDebugDidRequestAsyncWorkerLoad = true;
	}
#endif

	AuthoredLoadHandles[Index] = UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPath, Delegate);
	if (!AuthoredLoadHandles[Index].IsValid())
	{
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP OrbitalUnitDropDefinitionLoadFailed: Slot=%s Path=%s Reason=RequestAsyncLoadNullHandle"),
			GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
			*SoftPath.ToString());
#if !UE_BUILD_SHIPPING
		bDebugWorkerLoadFailedLogged = true;
		if (DebugHoldDropCompletion.IsValidIndex(Index) && DebugHoldDropCompletion[Index] != 0)
		{
			return;
		}
#endif
		MarkAuthoredSlotFailed(Slot);
	}
}

void UGP_OrbitalUnitDropCatalog::RequestAuthoredNestedUnitDefinitionLoad(
	EUnitAuthoredSlot Slot,
	const FSoftObjectPath& NestedPath)
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredUnitDefLoadHandles.IsValidIndex(Index) && AuthoredUnitDefLoadHandles[Index].IsValid()
		&& AuthoredUnitDefRequestedPaths[Index] == NestedPath)
	{
		return;
	}

	CancelAuthoredNestedUnitDefinitionLoad(Slot);
	AuthoredUnitDefRequestedPaths[Index] = NestedPath;
	AuthoredStates[Index] = EAuthoredSlotState::Pending;

	FStreamableDelegate Delegate;
	switch (Slot)
	{
	case EUnitAuthoredSlot::Worker:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_OrbitalUnitDropCatalog::HandleWorkerUnitDefinitionLoaded);
		break;
	case EUnitAuthoredSlot::SalvageWalker:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_OrbitalUnitDropCatalog::HandleWalkerUnitDefinitionLoaded);
		break;
	default:
		MarkAuthoredSlotFailed(Slot);
		return;
	}

#if !UE_BUILD_SHIPPING
	bDebugDidRequestAsyncNestedUnitDefLoad = true;
#endif

	AuthoredUnitDefLoadHandles[Index] = UAssetManager::GetStreamableManager().RequestAsyncLoad(NestedPath, Delegate);
	if (!AuthoredUnitDefLoadHandles[Index].IsValid())
	{
		const FSoftObjectPath DropPath = AuthoredRequestedPaths.IsValidIndex(Index)
			? AuthoredRequestedPaths[Index]
			: FSoftObjectPath();
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP UnitDefinitionLoadFailed: Slot=%s Drop=%s UnitDefinition=%s Reason=RequestAsyncLoadNullHandleUsingNativeFallback"),
			GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
			*DropPath.ToString(),
			*NestedPath.ToString());
#if !UE_BUILD_SHIPPING
		bDebugNestedUnitDefLoadFailedLogged = true;
		if (DebugHoldUnitDefCompletion.IsValidIndex(Index) && DebugHoldUnitDefCompletion[Index] != 0)
		{
			return;
		}
#endif
		MarkAuthoredSlotFailed(Slot);
	}
}

void UGP_OrbitalUnitDropCatalog::RequestAuthoredNestedPayloadClassLoad(
	EUnitAuthoredSlot Slot,
	const FSoftObjectPath& NestedPath)
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredPayloadLoadHandles.IsValidIndex(Index) && AuthoredPayloadLoadHandles[Index].IsValid()
		&& AuthoredPayloadRequestedPaths[Index] == NestedPath)
	{
		return;
	}

	CancelAuthoredNestedPayloadClassLoad(Slot);
	AuthoredPayloadRequestedPaths[Index] = NestedPath;
	AuthoredStates[Index] = EAuthoredSlotState::Pending;

	FStreamableDelegate Delegate;
	switch (Slot)
	{
	case EUnitAuthoredSlot::Worker:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_OrbitalUnitDropCatalog::HandleWorkerPayloadClassLoaded);
		break;
	case EUnitAuthoredSlot::SalvageWalker:
		Delegate = FStreamableDelegate::CreateUObject(this, &UGP_OrbitalUnitDropCatalog::HandleWalkerPayloadClassLoaded);
		break;
	default:
		MarkAuthoredSlotFailed(Slot);
		return;
	}

#if !UE_BUILD_SHIPPING
	bDebugDidRequestAsyncNestedPayloadLoad = true;
#endif

	AuthoredPayloadLoadHandles[Index] = UAssetManager::GetStreamableManager().RequestAsyncLoad(NestedPath, Delegate);
	if (!AuthoredPayloadLoadHandles[Index].IsValid())
	{
		const FSoftObjectPath DropPath = AuthoredRequestedPaths.IsValidIndex(Index)
			? AuthoredRequestedPaths[Index]
			: FSoftObjectPath();
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP PayloadClassLoadFailed: Slot=%s Drop=%s PayloadClass=%s Reason=RequestAsyncLoadNullHandleUsingNativeFallback"),
			GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
			*DropPath.ToString(),
			*NestedPath.ToString());
#if !UE_BUILD_SHIPPING
		bDebugNestedPayloadLoadFailedLogged = true;
		if (DebugHoldPayloadCompletion.IsValidIndex(Index) && DebugHoldPayloadCompletion[Index] != 0)
		{
			return;
		}
#endif
		MarkAuthoredSlotFailed(Slot);
	}
}

bool UGP_OrbitalUnitDropCatalog::IsCatalogCallbackSafe() const
{
	return IsValid(this)
		&& !GPOrbitalUnitDropCatalogPrivate::IsCreationBlocked()
		&& GPOrbitalUnitDropCatalogPrivate::GCatalog.Get() == this;
}

void UGP_OrbitalUnitDropCatalog::HandleAuthoredLoaded(EUnitAuthoredSlot Slot)
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

void UGP_OrbitalUnitDropCatalog::HandleAuthoredNestedUnitDefinitionLoaded(EUnitAuthoredSlot Slot)
{
	if (!IsCatalogCallbackSafe())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	const int32 Index = static_cast<int32>(Slot);
	if (DebugHoldUnitDefCompletion.IsValidIndex(Index) && DebugHoldUnitDefCompletion[Index] != 0)
	{
		return;
	}
#endif

	FinishAuthoredNestedUnitDefinitionLoadResolve(Slot);
}

void UGP_OrbitalUnitDropCatalog::HandleAuthoredNestedPayloadClassLoaded(EUnitAuthoredSlot Slot)
{
	if (!IsCatalogCallbackSafe())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	const int32 Index = static_cast<int32>(Slot);
	if (DebugHoldPayloadCompletion.IsValidIndex(Index) && DebugHoldPayloadCompletion[Index] != 0)
	{
		return;
	}
#endif

	FinishAuthoredNestedPayloadClassLoadResolve(Slot);
}

void UGP_OrbitalUnitDropCatalog::FinishAuthoredLoadResolve(EUnitAuthoredSlot Slot)
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

	const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> Soft = GetAuthoredSoftRef(Slot);
	UGP_OrbitalUnitDropDefinition* Loaded = ResolveLoadedAuthored(Soft);
	if (Loaded == nullptr && !Soft.IsNull())
	{
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP OrbitalUnitDropDefinitionLoadFailed: Slot=%s Path=%s Reason=ResolveFailedUsingNativeFallback"),
			GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
			*Soft.ToSoftObjectPath().ToString());
#if !UE_BUILD_SHIPPING
		bDebugWorkerLoadFailedLogged = true;
#endif
		MarkAuthoredSlotFailed(Slot);
		return;
	}

	ApplyLoadedAuthoredDrop(Slot, Loaded);
}

void UGP_OrbitalUnitDropCatalog::FinishAuthoredNestedUnitDefinitionLoadResolve(EUnitAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredUnitDefLoadHandles.IsValidIndex(Index))
	{
		AuthoredUnitDefLoadHandles[Index].Reset();
	}

#if !UE_BUILD_SHIPPING
	if (DebugForceUnresolvedUnitDef.IsValidIndex(Index))
	{
		DebugForceUnresolvedUnitDef[Index] = 0;
	}
	if (DebugInjectedUnitDefs.IsValidIndex(Index) && IsValid(DebugInjectedUnitDefs[Index]))
	{
		UGP_OrbitalUnitDropDefinition* Drop =
			AuthoredSlotDrops.IsValidIndex(Index) ? AuthoredSlotDrops[Index].Get() : nullptr;
		if (!IsValid(Drop) && DebugInjectedDrops.IsValidIndex(Index))
		{
			Drop = DebugInjectedDrops[Index];
		}
		if (IsValid(Drop))
		{
			Drop->UnitDefinition = DebugInjectedUnitDefs[Index];
			ApplyLoadedAuthoredDrop(Slot, Drop);
			return;
		}
	}
#endif

	UGP_OrbitalUnitDropDefinition* Drop =
		AuthoredSlotDrops.IsValidIndex(Index) ? AuthoredSlotDrops[Index].Get() : nullptr;
	if (!IsValid(Drop))
	{
		Drop = ResolveLoadedAuthored(GetAuthoredSoftRef(Slot));
	}

	if (IsValid(Drop) && Drop->ResolveLoadedUnitDefinition() != nullptr)
	{
		ApplyLoadedAuthoredDrop(Slot, Drop);
		return;
	}

	const FSoftObjectPath DropPath = AuthoredRequestedPaths.IsValidIndex(Index)
		? AuthoredRequestedPaths[Index]
		: FSoftObjectPath();
	const FSoftObjectPath NestedPath = AuthoredUnitDefRequestedPaths.IsValidIndex(Index)
		? AuthoredUnitDefRequestedPaths[Index]
		: (IsValid(Drop) ? Drop->UnitDefinition.ToSoftObjectPath() : FSoftObjectPath());
	UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
		TEXT("GP UnitDefinitionLoadFailed: Slot=%s Drop=%s UnitDefinition=%s Reason=ResolveFailedUsingNativeFallback"),
		GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
		*DropPath.ToString(),
		*NestedPath.ToString());
#if !UE_BUILD_SHIPPING
	bDebugNestedUnitDefLoadFailedLogged = true;
#endif
	MarkAuthoredSlotFailed(Slot);
}

void UGP_OrbitalUnitDropCatalog::FinishAuthoredNestedPayloadClassLoadResolve(EUnitAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (AuthoredPayloadLoadHandles.IsValidIndex(Index))
	{
		AuthoredPayloadLoadHandles[Index].Reset();
	}

#if !UE_BUILD_SHIPPING
	if (DebugForceUnresolvedPayload.IsValidIndex(Index))
	{
		DebugForceUnresolvedPayload[Index] = 0;
	}
	if (DebugInjectedPayloadClasses.IsValidIndex(Index) && DebugInjectedPayloadClasses[Index] != nullptr)
	{
		UGP_OrbitalUnitDropDefinition* Drop =
			AuthoredSlotDrops.IsValidIndex(Index) ? AuthoredSlotDrops[Index].Get() : nullptr;
		if (!IsValid(Drop) && DebugInjectedDrops.IsValidIndex(Index))
		{
			Drop = DebugInjectedDrops[Index];
		}
		if (IsValid(Drop))
		{
			Drop->PayloadClass = DebugInjectedPayloadClasses[Index];
			ApplyLoadedAuthoredDrop(Slot, Drop);
			return;
		}
	}
#endif

	UGP_OrbitalUnitDropDefinition* Drop =
		AuthoredSlotDrops.IsValidIndex(Index) ? AuthoredSlotDrops[Index].Get() : nullptr;
	if (!IsValid(Drop))
	{
		Drop = ResolveLoadedAuthored(GetAuthoredSoftRef(Slot));
	}

	if (IsValid(Drop) && HasResolvedAuthoredDependencies(Drop, Slot))
	{
		ApplyLoadedAuthoredDrop(Slot, Drop);
		return;
	}

	const TSubclassOf<AGP_UnitBase> LoadedPayload = IsValid(Drop) ? Drop->ResolveLoadedPayloadClass() : nullptr;
	const TCHAR* Reason = (LoadedPayload != nullptr && !IsPayloadClassValidForSlot(Slot, LoadedPayload.Get()))
		? TEXT("InvalidSubclassUsingNativeFallback")
		: TEXT("ResolveFailedUsingNativeFallback");
	const FSoftObjectPath DropPath = AuthoredRequestedPaths.IsValidIndex(Index)
		? AuthoredRequestedPaths[Index]
		: FSoftObjectPath();
	const FSoftObjectPath NestedPath = AuthoredPayloadRequestedPaths.IsValidIndex(Index)
		? AuthoredPayloadRequestedPaths[Index]
		: (IsValid(Drop) ? Drop->PayloadClass.ToSoftObjectPath() : FSoftObjectPath());
	UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
		TEXT("GP PayloadClassLoadFailed: Slot=%s Drop=%s PayloadClass=%s Reason=%s"),
		GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
		*DropPath.ToString(),
		*NestedPath.ToString(),
		Reason);
#if !UE_BUILD_SHIPPING
	bDebugNestedPayloadLoadFailedLogged = true;
#endif
	MarkAuthoredSlotFailed(Slot);
}

void UGP_OrbitalUnitDropCatalog::ApplyLoadedAuthoredDrop(
	EUnitAuthoredSlot Slot,
	UGP_OrbitalUnitDropDefinition* Loaded)
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
		CancelAuthoredNestedLoads(Slot);
		AuthoredUnitDefRequestedPaths[Index].Reset();
		AuthoredPayloadRequestedPaths[Index].Reset();
		AuthoredStates[Index] = EAuthoredSlotState::Empty;
		return;
	}

	if (Loaded->UnitDefinition.IsNull())
	{
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP UnitDefinitionLoadFailed: Slot=%s Drop=%s UnitDefinition=None Reason=NullUnitDefinitionUsingNativeFallback"),
			GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
			*Loaded->GetPathName());
#if !UE_BUILD_SHIPPING
		bDebugNullUnitDefinitionLogged = true;
#endif
		MarkAuthoredSlotFailed(Slot);
		return;
	}

#if !UE_BUILD_SHIPPING
	if (DebugForceUnresolvedUnitDef.IsValidIndex(Index) && DebugForceUnresolvedUnitDef[Index] != 0
		&& AuthoredStates[Index] != EAuthoredSlotState::Ready)
	{
		const FSoftObjectPath NestedPath = Loaded->UnitDefinition.ToSoftObjectPath();
		if (AuthoredStates[Index] == EAuthoredSlotState::Pending
			&& AuthoredUnitDefRequestedPaths[Index] == NestedPath)
		{
			return;
		}
		RequestAuthoredNestedUnitDefinitionLoad(Slot, NestedPath);
		return;
	}
#endif

	if (Loaded->ResolveLoadedUnitDefinition() == nullptr)
	{
		const FSoftObjectPath NestedPath = Loaded->UnitDefinition.ToSoftObjectPath();
		if (AuthoredStates[Index] == EAuthoredSlotState::Pending
			&& AuthoredUnitDefRequestedPaths[Index] == NestedPath
			&& ((AuthoredUnitDefLoadHandles.IsValidIndex(Index) && AuthoredUnitDefLoadHandles[Index].IsValid())
#if !UE_BUILD_SHIPPING
				|| (DebugHoldUnitDefCompletion.IsValidIndex(Index) && DebugHoldUnitDefCompletion[Index] != 0)
#endif
				))
		{
			return;
		}
		RequestAuthoredNestedUnitDefinitionLoad(Slot, NestedPath);
		return;
	}

	if (Loaded->PayloadClass.IsNull())
	{
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP PayloadClassLoadFailed: Slot=%s Drop=%s PayloadClass=None Reason=NullPayloadClassUsingNativeFallback"),
			GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
			*Loaded->GetPathName());
#if !UE_BUILD_SHIPPING
		bDebugNullPayloadClassLogged = true;
#endif
		MarkAuthoredSlotFailed(Slot);
		return;
	}

#if !UE_BUILD_SHIPPING
	if (DebugForceUnresolvedPayload.IsValidIndex(Index) && DebugForceUnresolvedPayload[Index] != 0
		&& AuthoredStates[Index] != EAuthoredSlotState::Ready)
	{
		const FSoftObjectPath NestedPath = Loaded->PayloadClass.ToSoftObjectPath();
		if (AuthoredStates[Index] == EAuthoredSlotState::Pending
			&& AuthoredPayloadRequestedPaths[Index] == NestedPath)
		{
			return;
		}
		RequestAuthoredNestedPayloadClassLoad(Slot, NestedPath);
		return;
	}
#endif

	const TSubclassOf<AGP_UnitBase> Payload = Loaded->ResolveLoadedPayloadClass();
	if (Payload != nullptr && IsPayloadClassValidForSlot(Slot, Payload.Get()))
	{
		CancelAuthoredNestedLoads(Slot);
		AuthoredUnitDefRequestedPaths[Index].Reset();
		AuthoredPayloadRequestedPaths[Index].Reset();
		AuthoredStates[Index] = EAuthoredSlotState::Ready;
		return;
	}

	if (Payload != nullptr)
	{
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP PayloadClassLoadFailed: Slot=%s Drop=%s PayloadClass=%s Reason=InvalidSubclassUsingNativeFallback"),
			GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
			*Loaded->GetPathName(),
			*Loaded->PayloadClass.ToSoftObjectPath().ToString());
#if !UE_BUILD_SHIPPING
		bDebugNestedPayloadLoadFailedLogged = true;
#endif
		MarkAuthoredSlotFailed(Slot);
		return;
	}

	const FSoftObjectPath NestedPath = Loaded->PayloadClass.ToSoftObjectPath();
	if (AuthoredStates[Index] == EAuthoredSlotState::Pending
		&& AuthoredPayloadRequestedPaths[Index] == NestedPath
		&& ((AuthoredPayloadLoadHandles.IsValidIndex(Index) && AuthoredPayloadLoadHandles[Index].IsValid())
#if !UE_BUILD_SHIPPING
			|| (DebugHoldPayloadCompletion.IsValidIndex(Index) && DebugHoldPayloadCompletion[Index] != 0)
#endif
			))
	{
		return;
	}

	RequestAuthoredNestedPayloadClassLoad(Slot, NestedPath);
}

void UGP_OrbitalUnitDropCatalog::MarkAuthoredSlotFailed(EUnitAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	CancelAuthoredLoad(Slot);
	if (AuthoredSlotDrops.IsValidIndex(Index))
	{
		AuthoredSlotDrops[Index] = nullptr;
	}
	if (AuthoredUnitDefRequestedPaths.IsValidIndex(Index))
	{
		AuthoredUnitDefRequestedPaths[Index].Reset();
	}
	if (AuthoredPayloadRequestedPaths.IsValidIndex(Index))
	{
		AuthoredPayloadRequestedPaths[Index].Reset();
	}
	if (AuthoredStates.IsValidIndex(Index))
	{
		AuthoredStates[Index] = EAuthoredSlotState::Failed;
	}
}

void UGP_OrbitalUnitDropCatalog::CancelAuthoredTopLevelLoad(EUnitAuthoredSlot Slot)
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

void UGP_OrbitalUnitDropCatalog::CancelAuthoredNestedUnitDefinitionLoad(EUnitAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredUnitDefLoadHandles.IsValidIndex(Index) || !AuthoredUnitDefLoadHandles[Index].IsValid())
	{
		return;
	}
	if (AuthoredUnitDefLoadHandles[Index]->IsLoadingInProgress())
	{
		AuthoredUnitDefLoadHandles[Index]->CancelHandle();
	}
	AuthoredUnitDefLoadHandles[Index].Reset();
}

void UGP_OrbitalUnitDropCatalog::CancelAuthoredNestedPayloadClassLoad(EUnitAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredPayloadLoadHandles.IsValidIndex(Index) || !AuthoredPayloadLoadHandles[Index].IsValid())
	{
		return;
	}
	if (AuthoredPayloadLoadHandles[Index]->IsLoadingInProgress())
	{
		AuthoredPayloadLoadHandles[Index]->CancelHandle();
	}
	AuthoredPayloadLoadHandles[Index].Reset();
}

void UGP_OrbitalUnitDropCatalog::CancelAuthoredNestedLoads(EUnitAuthoredSlot Slot)
{
	CancelAuthoredNestedUnitDefinitionLoad(Slot);
	CancelAuthoredNestedPayloadClassLoad(Slot);
}

void UGP_OrbitalUnitDropCatalog::CancelAuthoredLoad(EUnitAuthoredSlot Slot)
{
	CancelAuthoredTopLevelLoad(Slot);
	CancelAuthoredNestedLoads(Slot);
}

void UGP_OrbitalUnitDropCatalog::CancelAllAuthoredLoads()
{
	for (int32 i = 0; i < static_cast<int32>(EUnitAuthoredSlot::COUNT); ++i)
	{
		CancelAuthoredLoad(static_cast<EUnitAuthoredSlot>(i));
	}
}

UGP_OrbitalUnitDropDefinition* UGP_OrbitalUnitDropCatalog::GetWorkerDrop() const
{
	return CanonicalForSlot(EUnitAuthoredSlot::Worker);
}

UGP_OrbitalUnitDropDefinition* UGP_OrbitalUnitDropCatalog::GetSalvageWalkerDrop() const
{
	return CanonicalForSlot(EUnitAuthoredSlot::SalvageWalker);
}

bool UGP_OrbitalUnitDropCatalog::IsWorkerDropDefinitionPending() const
{
	const int32 Index = static_cast<int32>(EUnitAuthoredSlot::Worker);
	return AuthoredStates.IsValidIndex(Index) && AuthoredStates[Index] == EAuthoredSlotState::Pending;
}

bool UGP_OrbitalUnitDropCatalog::IsSalvageWalkerDropDefinitionPending() const
{
	const int32 Index = static_cast<int32>(EUnitAuthoredSlot::SalvageWalker);
	return AuthoredStates.IsValidIndex(Index) && AuthoredStates[Index] == EAuthoredSlotState::Pending;
}

bool UGP_OrbitalUnitDropCatalog::AreManifestDefinitionsReady(const FGP_UnitDropManifest& Manifest) const
{
	if (Manifest.WorkerCount > 0 && IsWorkerDropDefinitionPending())
	{
		return false;
	}
	if (Manifest.SalvageWalkerCount > 0 && IsSalvageWalkerDropDefinitionPending())
	{
		return false;
	}
	return true;
}

int32 UGP_OrbitalUnitDropCatalog::GetWorkerTransportSlotCost() const
{
	if (const UGP_OrbitalUnitDropDefinition* Drop = ResolveNumericProduct(EUnitAuthoredSlot::Worker))
	{
		if (Drop->TransportSlotCost > 0)
		{
			return Drop->TransportSlotCost;
		}
	}
	return NativeWorkerTransportSlotCost;
}

int32 UGP_OrbitalUnitDropCatalog::GetSalvageWalkerTransportSlotCost() const
{
	if (const UGP_OrbitalUnitDropDefinition* Drop = ResolveNumericProduct(EUnitAuthoredSlot::SalvageWalker))
	{
		if (Drop->TransportSlotCost > 0)
		{
			return Drop->TransportSlotCost;
		}
	}
	return NativeSalvageWalkerTransportSlotCost;
}

float UGP_OrbitalUnitDropCatalog::GetWorkerOrbitalDropCost() const
{
	if (const UGP_OrbitalUnitDropDefinition* Drop = ResolveNumericProduct(EUnitAuthoredSlot::Worker))
	{
		return FMath::Max(0.0f, Drop->Cost);
	}
	return NativeWorkerOrbitalDropCost;
}

float UGP_OrbitalUnitDropCatalog::GetSalvageWalkerOrbitalDropCost() const
{
	if (const UGP_OrbitalUnitDropDefinition* Drop = ResolveNumericProduct(EUnitAuthoredSlot::SalvageWalker))
	{
		return FMath::Max(0.0f, Drop->Cost);
	}
	return NativeSalvageWalkerOrbitalDropCost;
}

TSubclassOf<AGP_UnitBase> UGP_OrbitalUnitDropCatalog::ResolveFallbackPayloadClass(EUnitAuthoredSlot Slot) const
{
	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		if (Slot == EUnitAuthoredSlot::Worker)
		{
			return TSubclassOf<AGP_UnitBase>(Settings->ResolveWorkerPayloadClass().Get());
		}
		if (Slot == EUnitAuthoredSlot::SalvageWalker)
		{
			return TSubclassOf<AGP_UnitBase>(Settings->ResolveSalvageWalkerPayloadClass().Get());
		}
	}

	if (Slot == EUnitAuthoredSlot::SalvageWalker)
	{
		return TSubclassOf<AGP_UnitBase>(AGP_SalvageWalker::StaticClass());
	}
	return TSubclassOf<AGP_UnitBase>(AGP_Worker::StaticClass());
}

TSubclassOf<AGP_Worker> UGP_OrbitalUnitDropCatalog::ResolveWorkerPayloadClass() const
{
	if (const UGP_OrbitalUnitDropDefinition* Drop = GetWorkerDrop())
	{
		if (TSubclassOf<AGP_UnitBase> Loaded = Drop->ResolveLoadedPayloadClass())
		{
			if (Loaded->IsChildOf(AGP_Worker::StaticClass()))
			{
				return TSubclassOf<AGP_Worker>(Loaded.Get());
			}
		}
	}

	return TSubclassOf<AGP_Worker>(ResolveFallbackPayloadClass(EUnitAuthoredSlot::Worker).Get());
}

TSubclassOf<AGP_SalvageWalker> UGP_OrbitalUnitDropCatalog::ResolveSalvageWalkerPayloadClass() const
{
	if (const UGP_OrbitalUnitDropDefinition* Drop = GetSalvageWalkerDrop())
	{
		if (TSubclassOf<AGP_UnitBase> Loaded = Drop->ResolveLoadedPayloadClass())
		{
			if (Loaded->IsChildOf(AGP_SalvageWalker::StaticClass()))
			{
				return TSubclassOf<AGP_SalvageWalker>(Loaded.Get());
			}
		}
	}

	return TSubclassOf<AGP_SalvageWalker>(ResolveFallbackPayloadClass(EUnitAuthoredSlot::SalvageWalker).Get());
}

void UGP_OrbitalUnitDropCatalog::ResolveManifestDeliveryTiming(
	const FGP_UnitDropManifest& Manifest,
	float& OutDescentSeconds,
	float& OutPayloadDeployDelaySeconds) const
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	OutDescentSeconds = Settings != nullptr ? Settings->UnitDropDescentDurationSeconds : 2.5f;
	OutPayloadDeployDelaySeconds = Settings != nullptr ? Settings->UnitDropPayloadDeployDelaySeconds : 1.25f;

	bool bUsedDefinition = false;
	if (Manifest.WorkerCount > 0)
	{
		if (const UGP_OrbitalUnitDropDefinition* Drop = GetWorkerDrop())
		{
			OutDescentSeconds = Drop->DeliveryDescentSeconds;
			OutPayloadDeployDelaySeconds = Drop->PayloadDeployDelaySeconds;
			bUsedDefinition = true;
		}
	}
	if (Manifest.SalvageWalkerCount > 0)
	{
		if (const UGP_OrbitalUnitDropDefinition* Drop = GetSalvageWalkerDrop())
		{
			if (bUsedDefinition)
			{
				OutDescentSeconds = FMath::Max(OutDescentSeconds, Drop->DeliveryDescentSeconds);
				OutPayloadDeployDelaySeconds = FMath::Max(
					OutPayloadDeployDelaySeconds,
					Drop->PayloadDeployDelaySeconds);
			}
			else
			{
				OutDescentSeconds = Drop->DeliveryDescentSeconds;
				OutPayloadDeployDelaySeconds = Drop->PayloadDeployDelaySeconds;
			}
		}
	}
}

void UGP_OrbitalUnitDropCatalog::OverrideDeliveryTiming(float DescentSeconds, float PayloadDeployDelaySeconds)
{
	auto Apply = [DescentSeconds, PayloadDeployDelaySeconds](UGP_OrbitalUnitDropDefinition* Drop)
	{
		if (IsValid(Drop))
		{
			Drop->DeliveryDescentSeconds = DescentSeconds;
			Drop->PayloadDeployDelaySeconds = PayloadDeployDelaySeconds;
		}
	};

	Apply(NativeWorkerDrop);
	Apply(NativeSalvageWalkerDrop);
	for (TObjectPtr<UGP_OrbitalUnitDropDefinition>& Drop : AuthoredSlotDrops)
	{
		Apply(Drop.Get());
	}
}

#if !UE_BUILD_SHIPPING
void UGP_OrbitalUnitDropCatalog::EnsureDebugSlotArrays()
{
	const int32 SlotCount = static_cast<int32>(EUnitAuthoredSlot::COUNT);
	DebugForceUnresolvedDrop.SetNumZeroed(SlotCount);
	DebugHoldDropCompletion.SetNumZeroed(SlotCount);
	DebugForceUnresolvedUnitDef.SetNumZeroed(SlotCount);
	DebugHoldUnitDefCompletion.SetNumZeroed(SlotCount);
	DebugForceUnresolvedPayload.SetNumZeroed(SlotCount);
	DebugHoldPayloadCompletion.SetNumZeroed(SlotCount);
	DebugInjectedDrops.SetNum(SlotCount);
	DebugInjectedUnitDefs.SetNum(SlotCount);
	DebugInjectedPayloadClasses.SetNum(SlotCount);
}

void UGP_OrbitalUnitDropCatalog::ResetDebugSlotFlags()
{
	EnsureDebugSlotArrays();
	for (int32 i = 0; i < static_cast<int32>(EUnitAuthoredSlot::COUNT); ++i)
	{
		DebugForceUnresolvedDrop[i] = 0;
		DebugHoldDropCompletion[i] = 0;
		DebugForceUnresolvedUnitDef[i] = 0;
		DebugHoldUnitDefCompletion[i] = 0;
		DebugForceUnresolvedPayload[i] = 0;
		DebugHoldPayloadCompletion[i] = 0;
		DebugInjectedDrops[i] = nullptr;
		DebugInjectedUnitDefs[i] = nullptr;
		DebugInjectedPayloadClasses[i] = nullptr;
	}
	bDebugDidRequestAsyncWorkerLoad = false;
	bDebugDidRequestAsyncNestedUnitDefLoad = false;
	bDebugDidRequestAsyncNestedPayloadLoad = false;
}

void UGP_OrbitalUnitDropCatalog::SaveAuthoredSettingsIfNeeded()
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (!bDebugSavedSettings)
		{
			DebugSavedWorkerSettingsRef = Settings->WorkerDropDefinition;
			DebugSavedWalkerSettingsRef = Settings->SalvageWalkerDropDefinition;
			bDebugSavedSettings = true;
		}
	}
}

void UGP_OrbitalUnitDropCatalog::AssignAuthoredSettingsDrop(
	EUnitAuthoredSlot Slot,
	UGP_OrbitalUnitDropDefinition* Definition)
{
	SaveAuthoredSettingsIfNeeded();
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (Slot == EUnitAuthoredSlot::Worker)
		{
			Settings->WorkerDropDefinition = Definition;
		}
		else if (Slot == EUnitAuthoredSlot::SalvageWalker)
		{
			Settings->SalvageWalkerDropDefinition = Definition;
		}
	}
}

void UGP_OrbitalUnitDropCatalog::DebugAssignLoadedAuthoredDrop(
	EUnitAuthoredSlot Slot,
	UGP_OrbitalUnitDropDefinition* Definition)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(Slot);
	AssignAuthoredSettingsDrop(Slot, Definition);
	DebugForceUnresolvedDrop[Index] = 0;
	DebugHoldDropCompletion[Index] = 0;
	DebugForceUnresolvedUnitDef[Index] = 0;
	DebugHoldUnitDefCompletion[Index] = 0;
	DebugForceUnresolvedPayload[Index] = 0;
	DebugHoldPayloadCompletion[Index] = 0;
	DebugInjectedDrops[Index] = nullptr;
	DebugInjectedUnitDefs[Index] = nullptr;
	DebugInjectedPayloadClasses[Index] = nullptr;
	RefreshAuthoredSlot(Slot);
}

void UGP_OrbitalUnitDropCatalog::DebugAssignLoadedAuthoredWorker(UGP_OrbitalUnitDropDefinition* Definition)
{
	DebugAssignLoadedAuthoredDrop(EUnitAuthoredSlot::Worker, Definition);
}

void UGP_OrbitalUnitDropCatalog::DebugAssignLoadedAuthoredSalvageWalker(UGP_OrbitalUnitDropDefinition* Definition)
{
	DebugAssignLoadedAuthoredDrop(EUnitAuthoredSlot::SalvageWalker, Definition);
}

void UGP_OrbitalUnitDropCatalog::DebugForceUnresolvedAuthoredLoad(
	EUnitAuthoredSlot Slot,
	UGP_OrbitalUnitDropDefinition* InjectedDefinition,
	bool bHoldCompletion)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(Slot);
	SaveAuthoredSettingsIfNeeded();
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		const FSoftObjectPath StubPath(GPOrbitalUnitDropCatalogPrivate::UnresolvedWorkerStub);
		const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> Stub{StubPath};
		if (Slot == EUnitAuthoredSlot::Worker)
		{
			Settings->WorkerDropDefinition = Stub;
		}
		else if (Slot == EUnitAuthoredSlot::SalvageWalker)
		{
			Settings->SalvageWalkerDropDefinition = Stub;
		}
	}

	DebugForceUnresolvedDrop[Index] = 1;
	DebugHoldDropCompletion[Index] = bHoldCompletion ? 1 : 0;
	DebugForceUnresolvedUnitDef[Index] = 0;
	DebugHoldUnitDefCompletion[Index] = 0;
	DebugForceUnresolvedPayload[Index] = 0;
	DebugHoldPayloadCompletion[Index] = 0;
	DebugInjectedDrops[Index] = InjectedDefinition;
	DebugInjectedUnitDefs[Index] = nullptr;
	DebugInjectedPayloadClasses[Index] = nullptr;
	if (Slot == EUnitAuthoredSlot::Worker)
	{
		bDebugDidRequestAsyncWorkerLoad = false;
	}
	CancelAuthoredLoad(Slot);
	if (AuthoredSlotDrops.IsValidIndex(Index))
	{
		AuthoredSlotDrops[Index] = nullptr;
	}
	AuthoredStates[Index] = EAuthoredSlotState::Empty;
	RefreshAuthoredSlot(Slot);
}

void UGP_OrbitalUnitDropCatalog::DebugForceUnresolvedAuthoredWorkerLoad(
	UGP_OrbitalUnitDropDefinition* InjectedDefinition,
	bool bHoldCompletion)
{
	DebugForceUnresolvedAuthoredLoad(EUnitAuthoredSlot::Worker, InjectedDefinition, bHoldCompletion);
}

void UGP_OrbitalUnitDropCatalog::DebugCompletePendingAuthoredLoad(EUnitAuthoredSlot Slot)
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

void UGP_OrbitalUnitDropCatalog::DebugCompletePendingAuthoredWorkerLoad()
{
	DebugCompletePendingAuthoredLoad(EUnitAuthoredSlot::Worker);
}

void UGP_OrbitalUnitDropCatalog::DebugForceAuthoredWorkerLoadFailure()
{
	const int32 Index = static_cast<int32>(EUnitAuthoredSlot::Worker);
	EnsureDebugSlotArrays();
	DebugInjectedDrops[Index] = nullptr;
	if (DebugHoldDropCompletion.IsValidIndex(Index))
	{
		DebugHoldDropCompletion[Index] = 0;
	}
	if (AuthoredStates.IsValidIndex(Index) && AuthoredStates[Index] == EAuthoredSlotState::Pending)
	{
		FinishAuthoredLoadResolve(EUnitAuthoredSlot::Worker);
	}
}

void UGP_OrbitalUnitDropCatalog::DebugForceUnresolvedNestedUnitDefinitionLoad(
	EUnitAuthoredSlot Slot,
	UGP_OrbitalUnitDropDefinition* InjectedDrop,
	UGP_UnitDefinition* InjectedUnitDefinition,
	bool bHoldCompletion)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(Slot);
	if (IsValid(InjectedDrop))
	{
		InjectedDrop->UnitDefinition = TSoftObjectPtr<UGP_UnitDefinition>(
			FSoftObjectPath(GPOrbitalUnitDropCatalogPrivate::UnresolvedUnitDefStub));
	}
	AssignAuthoredSettingsDrop(Slot, InjectedDrop);
	DebugForceUnresolvedDrop[Index] = 0;
	DebugHoldDropCompletion[Index] = 0;
	DebugForceUnresolvedUnitDef[Index] = 1;
	DebugHoldUnitDefCompletion[Index] = bHoldCompletion ? 1 : 0;
	DebugForceUnresolvedPayload[Index] = 0;
	DebugHoldPayloadCompletion[Index] = 0;
	DebugInjectedDrops[Index] = InjectedDrop;
	DebugInjectedUnitDefs[Index] = InjectedUnitDefinition;
	DebugInjectedPayloadClasses[Index] = nullptr;
	bDebugDidRequestAsyncNestedUnitDefLoad = false;
	CancelAuthoredNestedLoads(Slot);
	if (AuthoredStates.IsValidIndex(Index))
	{
		AuthoredStates[Index] = EAuthoredSlotState::Empty;
	}
	RefreshAuthoredSlot(Slot);
}

void UGP_OrbitalUnitDropCatalog::DebugForceUnresolvedNestedPayloadClassLoad(
	EUnitAuthoredSlot Slot,
	UGP_OrbitalUnitDropDefinition* InjectedDrop,
	TSubclassOf<AGP_UnitBase> InjectedPayloadClass,
	bool bHoldCompletion)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(Slot);
	if (IsValid(InjectedDrop))
	{
		InjectedDrop->PayloadClass = TSoftClassPtr<AGP_UnitBase>(
			FSoftObjectPath(GPOrbitalUnitDropCatalogPrivate::UnresolvedPayloadStub));
	}
	AssignAuthoredSettingsDrop(Slot, InjectedDrop);
	DebugForceUnresolvedDrop[Index] = 0;
	DebugHoldDropCompletion[Index] = 0;
	DebugForceUnresolvedUnitDef[Index] = 0;
	DebugHoldUnitDefCompletion[Index] = 0;
	DebugForceUnresolvedPayload[Index] = 1;
	DebugHoldPayloadCompletion[Index] = bHoldCompletion ? 1 : 0;
	DebugInjectedDrops[Index] = InjectedDrop;
	DebugInjectedUnitDefs[Index] = nullptr;
	DebugInjectedPayloadClasses[Index] = InjectedPayloadClass;
	bDebugDidRequestAsyncNestedPayloadLoad = false;
	CancelAuthoredNestedPayloadClassLoad(Slot);
	if (AuthoredStates.IsValidIndex(Index))
	{
		AuthoredStates[Index] = EAuthoredSlotState::Empty;
	}
	RefreshAuthoredSlot(Slot);
}

void UGP_OrbitalUnitDropCatalog::DebugForceUnresolvedNestedWorkerUnitDefinitionLoad(
	UGP_OrbitalUnitDropDefinition* InjectedDrop,
	UGP_UnitDefinition* InjectedUnitDefinition,
	bool bHoldCompletion)
{
	DebugForceUnresolvedNestedUnitDefinitionLoad(
		EUnitAuthoredSlot::Worker,
		InjectedDrop,
		InjectedUnitDefinition,
		bHoldCompletion);
}

void UGP_OrbitalUnitDropCatalog::DebugForceUnresolvedNestedWorkerPayloadClassLoad(
	UGP_OrbitalUnitDropDefinition* InjectedDrop,
	TSubclassOf<AGP_UnitBase> InjectedPayloadClass,
	bool bHoldCompletion)
{
	DebugForceUnresolvedNestedPayloadClassLoad(
		EUnitAuthoredSlot::Worker,
		InjectedDrop,
		InjectedPayloadClass,
		bHoldCompletion);
}

void UGP_OrbitalUnitDropCatalog::DebugCompletePendingNestedUnitDefinitionLoad(EUnitAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredStates.IsValidIndex(Index) || AuthoredStates[Index] != EAuthoredSlotState::Pending)
	{
		return;
	}
	if (DebugHoldUnitDefCompletion.IsValidIndex(Index))
	{
		DebugHoldUnitDefCompletion[Index] = 0;
	}
	FinishAuthoredNestedUnitDefinitionLoadResolve(Slot);
}

void UGP_OrbitalUnitDropCatalog::DebugCompletePendingNestedPayloadClassLoad(EUnitAuthoredSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (!AuthoredStates.IsValidIndex(Index) || AuthoredStates[Index] != EAuthoredSlotState::Pending)
	{
		return;
	}
	if (DebugHoldPayloadCompletion.IsValidIndex(Index))
	{
		DebugHoldPayloadCompletion[Index] = 0;
	}
	FinishAuthoredNestedPayloadClassLoadResolve(Slot);
}

void UGP_OrbitalUnitDropCatalog::DebugCompletePendingNestedWorkerUnitDefinitionLoad()
{
	DebugCompletePendingNestedUnitDefinitionLoad(EUnitAuthoredSlot::Worker);
}

void UGP_OrbitalUnitDropCatalog::DebugCompletePendingNestedWorkerPayloadClassLoad()
{
	DebugCompletePendingNestedPayloadClassLoad(EUnitAuthoredSlot::Worker);
}

void UGP_OrbitalUnitDropCatalog::DebugForceNestedUnitDefinitionLoadFailure(EUnitAuthoredSlot Slot)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(Slot);
	if (DebugInjectedUnitDefs.IsValidIndex(Index))
	{
		DebugInjectedUnitDefs[Index] = nullptr;
	}
	if (DebugHoldUnitDefCompletion.IsValidIndex(Index))
	{
		DebugHoldUnitDefCompletion[Index] = 0;
	}
	if (DebugForceUnresolvedUnitDef.IsValidIndex(Index))
	{
		DebugForceUnresolvedUnitDef[Index] = 0;
	}
	if (!AuthoredStates.IsValidIndex(Index) || AuthoredStates[Index] != EAuthoredSlotState::Pending)
	{
		return;
	}

	if (AuthoredSlotDrops.IsValidIndex(Index) && IsValid(AuthoredSlotDrops[Index]))
	{
		AuthoredSlotDrops[Index]->UnitDefinition.Reset();
	}
	if (DebugInjectedDrops.IsValidIndex(Index) && IsValid(DebugInjectedDrops[Index]))
	{
		DebugInjectedDrops[Index]->UnitDefinition.Reset();
	}

	const FSoftObjectPath DropPath = AuthoredRequestedPaths.IsValidIndex(Index)
		? AuthoredRequestedPaths[Index]
		: FSoftObjectPath();
	const FSoftObjectPath NestedPath = AuthoredUnitDefRequestedPaths.IsValidIndex(Index)
		? AuthoredUnitDefRequestedPaths[Index]
		: FSoftObjectPath();
	UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
		TEXT("GP UnitDefinitionLoadFailed: Slot=%s Drop=%s UnitDefinition=%s Reason=ResolveFailedUsingNativeFallback"),
		GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
		*DropPath.ToString(),
		*NestedPath.ToString());
	bDebugNestedUnitDefLoadFailedLogged = true;
	MarkAuthoredSlotFailed(Slot);
}

void UGP_OrbitalUnitDropCatalog::DebugForceNestedPayloadClassLoadFailure(EUnitAuthoredSlot Slot)
{
	EnsureDebugSlotArrays();
	const int32 Index = static_cast<int32>(Slot);
	if (DebugInjectedPayloadClasses.IsValidIndex(Index))
	{
		DebugInjectedPayloadClasses[Index] = nullptr;
	}
	if (DebugHoldPayloadCompletion.IsValidIndex(Index))
	{
		DebugHoldPayloadCompletion[Index] = 0;
	}
	if (DebugForceUnresolvedPayload.IsValidIndex(Index))
	{
		DebugForceUnresolvedPayload[Index] = 0;
	}
	if (!AuthoredStates.IsValidIndex(Index) || AuthoredStates[Index] != EAuthoredSlotState::Pending)
	{
		return;
	}

	if (AuthoredSlotDrops.IsValidIndex(Index) && IsValid(AuthoredSlotDrops[Index]))
	{
		AuthoredSlotDrops[Index]->PayloadClass.Reset();
	}
	if (DebugInjectedDrops.IsValidIndex(Index) && IsValid(DebugInjectedDrops[Index]))
	{
		DebugInjectedDrops[Index]->PayloadClass.Reset();
	}

	const FSoftObjectPath DropPath = AuthoredRequestedPaths.IsValidIndex(Index)
		? AuthoredRequestedPaths[Index]
		: FSoftObjectPath();
	const FSoftObjectPath NestedPath = AuthoredPayloadRequestedPaths.IsValidIndex(Index)
		? AuthoredPayloadRequestedPaths[Index]
		: FSoftObjectPath();
	UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
		TEXT("GP PayloadClassLoadFailed: Slot=%s Drop=%s PayloadClass=%s Reason=ResolveFailedUsingNativeFallback"),
		GPOrbitalUnitDropCatalogPrivate::SlotNameFromIndex(Index),
		*DropPath.ToString(),
		*NestedPath.ToString());
	bDebugNestedPayloadLoadFailedLogged = true;
	MarkAuthoredSlotFailed(Slot);
}

void UGP_OrbitalUnitDropCatalog::DebugForceNestedWorkerUnitDefinitionLoadFailure()
{
	DebugForceNestedUnitDefinitionLoadFailure(EUnitAuthoredSlot::Worker);
}

void UGP_OrbitalUnitDropCatalog::DebugForceNestedWorkerPayloadClassLoadFailure()
{
	DebugForceNestedPayloadClassLoadFailure(EUnitAuthoredSlot::Worker);
}

bool UGP_OrbitalUnitDropCatalog::DebugConsumeNestedUnitDefinitionLoadFailedLog()
{
	const bool bLogged = bDebugNestedUnitDefLoadFailedLogged;
	bDebugNestedUnitDefLoadFailedLogged = false;
	return bLogged;
}

bool UGP_OrbitalUnitDropCatalog::DebugConsumeNestedPayloadClassLoadFailedLog()
{
	const bool bLogged = bDebugNestedPayloadLoadFailedLogged;
	bDebugNestedPayloadLoadFailedLogged = false;
	return bLogged;
}

bool UGP_OrbitalUnitDropCatalog::DebugConsumeNullUnitDefinitionLog()
{
	const bool bLogged = bDebugNullUnitDefinitionLogged;
	bDebugNullUnitDefinitionLogged = false;
	return bLogged;
}

bool UGP_OrbitalUnitDropCatalog::DebugConsumeNullPayloadClassLog()
{
	const bool bLogged = bDebugNullPayloadClassLogged;
	bDebugNullPayloadClassLogged = false;
	return bLogged;
}

void UGP_OrbitalUnitDropCatalog::DebugClearAuthoredUnitDropOverrides()
{
	CancelAllAuthoredLoads();
	ResetDebugSlotFlags();
	for (int32 i = 0; i < static_cast<int32>(EUnitAuthoredSlot::COUNT); ++i)
	{
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
		if (AuthoredUnitDefRequestedPaths.IsValidIndex(i))
		{
			AuthoredUnitDefRequestedPaths[i].Reset();
		}
		if (AuthoredPayloadRequestedPaths.IsValidIndex(i))
		{
			AuthoredPayloadRequestedPaths[i].Reset();
		}
	}

	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (bDebugSavedSettings)
		{
			Settings->WorkerDropDefinition = DebugSavedWorkerSettingsRef;
			Settings->SalvageWalkerDropDefinition = DebugSavedWalkerSettingsRef;
		}
		else
		{
			Settings->WorkerDropDefinition.Reset();
			Settings->SalvageWalkerDropDefinition.Reset();
		}
	}
	bDebugSavedSettings = false;
	DebugSavedWorkerSettingsRef.Reset();
	DebugSavedWalkerSettingsRef.Reset();
}

void UGP_OrbitalUnitDropCatalog::DebugBeginContractIsolation()
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (!bContractIsolationActive)
		{
			ContractSavedWorkerSettingsRef = Settings->WorkerDropDefinition;
			ContractSavedWalkerSettingsRef = Settings->SalvageWalkerDropDefinition;
			bContractIsolationActive = true;
		}
		Settings->WorkerDropDefinition.Reset();
		Settings->SalvageWalkerDropDefinition.Reset();
	}
	ResetDebugSlotFlags();
	CancelAllAuthoredLoads();
	for (int32 i = 0; i < static_cast<int32>(EUnitAuthoredSlot::COUNT); ++i)
	{
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
		if (AuthoredUnitDefRequestedPaths.IsValidIndex(i))
		{
			AuthoredUnitDefRequestedPaths[i].Reset();
		}
		if (AuthoredPayloadRequestedPaths.IsValidIndex(i))
		{
			AuthoredPayloadRequestedPaths[i].Reset();
		}
	}
}

void UGP_OrbitalUnitDropCatalog::DebugEndContractIsolation()
{
	if (!bContractIsolationActive)
	{
		return;
	}
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		Settings->WorkerDropDefinition = ContractSavedWorkerSettingsRef;
		Settings->SalvageWalkerDropDefinition = ContractSavedWalkerSettingsRef;
	}
	bContractIsolationActive = false;
	ContractSavedWorkerSettingsRef.Reset();
	ContractSavedWalkerSettingsRef.Reset();
	RefreshAuthoredBindings();
}
#endif
