// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPOrbitalUnitDropCatalog.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/CoreDelegates.h"
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
	static constexpr TCHAR CatalogObjectName[] = TEXT("GP_OrbitalUnitDropCatalog");
	static constexpr TCHAR UnresolvedWorkerStub[] =
		TEXT("/Game/GrimProtocol/Data/Orbital/DA_GP_OrbitalUnitDrop_UnresolvedSoftRefStub.DA_GP_OrbitalUnitDrop_UnresolvedSoftRefStub");
}

UGP_OrbitalUnitDropCatalog& UGP_OrbitalUnitDropCatalog::Get()
{
	if (!GPOrbitalUnitDropCatalogPrivate::GCatalog.IsValid())
	{
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
	}

	UGP_OrbitalUnitDropCatalog& Catalog = *GPOrbitalUnitDropCatalogPrivate::GCatalog.Get();
	Catalog.RefreshAuthoredBindings();
	return Catalog;
}

void UGP_OrbitalUnitDropCatalog::ShutdownCatalog()
{
	if (GPOrbitalUnitDropCatalogPrivate::GCatalog.IsValid())
	{
		GPOrbitalUnitDropCatalogPrivate::GCatalog->CancelWorkerLoad();
		GPOrbitalUnitDropCatalogPrivate::GCatalog->CancelWalkerLoad();
	}
	GPOrbitalUnitDropCatalogPrivate::GCatalog.Reset();
}

void UGP_OrbitalUnitDropCatalog::BindEngineLifecycle()
{
	if (GPOrbitalUnitDropCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		return;
	}

	GPOrbitalUnitDropCatalogPrivate::EnginePreExitHandle =
		FCoreDelegates::OnEnginePreExit.AddStatic(&UGP_OrbitalUnitDropCatalog::ShutdownCatalog);
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
	NativeWorkerDrop->Cost = 25.0f;
	NativeWorkerDrop->TransportSlotCost = 1;
	NativeWorkerDrop->DeliveryDescentSeconds = 2.5f;
	NativeWorkerDrop->PayloadDeployDelaySeconds = 1.25f;

	NativeSalvageWalkerDrop = CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalUnitDrop_SalvageWalker")),
		NSLOCTEXT("GPOrbitalUnitDropCatalog", "SalvageWalker", "Salvage Walker"));
	NativeSalvageWalkerDrop->UnitDefinition = Units.GetSalvageWalkerDefinition();
	NativeSalvageWalkerDrop->Cost = 50.0f;
	NativeSalvageWalkerDrop->TransportSlotCost = 2;
	NativeSalvageWalkerDrop->DeliveryDescentSeconds = 2.5f;
	NativeSalvageWalkerDrop->PayloadDeployDelaySeconds = 1.25f;

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

UGP_OrbitalUnitDropDefinition* UGP_OrbitalUnitDropCatalog::CanonicalOrNative(
	EAuthoredSlotState State,
	UGP_OrbitalUnitDropDefinition* Authored,
	UGP_OrbitalUnitDropDefinition* Native) const
{
	if (State == EAuthoredSlotState::Pending)
	{
		return nullptr;
	}
	if (State == EAuthoredSlotState::Ready && IsValid(Authored))
	{
		return Authored;
	}
	return Native;
}

void UGP_OrbitalUnitDropCatalog::RefreshAuthoredBindings()
{
	EnsureNativeCatalog();
	RefreshWorkerSlot();
	RefreshWalkerSlot();
}

void UGP_OrbitalUnitDropCatalog::RefreshWorkerSlot()
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> Soft =
		Settings != nullptr ? Settings->WorkerDropDefinition : TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>();

	if (Soft.IsNull())
	{
		CancelWorkerLoad();
		AuthoredWorkerDrop = nullptr;
		WorkerRequestedPath.Reset();
		WorkerState = EAuthoredSlotState::Empty;
		return;
	}

	const FSoftObjectPath SoftPath = Soft.ToSoftObjectPath();

#if !UE_BUILD_SHIPPING
	if (bDebugForceUnresolvedWorker && WorkerState != EAuthoredSlotState::Ready)
	{
		if (WorkerState == EAuthoredSlotState::Pending && WorkerRequestedPath == SoftPath)
		{
			return;
		}
		RequestWorkerAsyncLoad(SoftPath);
		return;
	}
#endif

	if (UGP_OrbitalUnitDropDefinition* Loaded = ResolveLoadedAuthored(Soft))
	{
		CancelWorkerLoad();
		AuthoredWorkerDrop = Loaded;
		WorkerRequestedPath = SoftPath;
		WorkerState = EAuthoredSlotState::Ready;
		return;
	}

	if (WorkerState == EAuthoredSlotState::Pending && WorkerRequestedPath == SoftPath)
	{
		return;
	}

	RequestWorkerAsyncLoad(SoftPath);
}

void UGP_OrbitalUnitDropCatalog::RefreshWalkerSlot()
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> Soft =
		Settings != nullptr
			? Settings->SalvageWalkerDropDefinition
			: TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>();

	if (Soft.IsNull())
	{
		CancelWalkerLoad();
		AuthoredSalvageWalkerDrop = nullptr;
		WalkerRequestedPath.Reset();
		WalkerState = EAuthoredSlotState::Empty;
		return;
	}

	const FSoftObjectPath SoftPath = Soft.ToSoftObjectPath();
	if (UGP_OrbitalUnitDropDefinition* Loaded = ResolveLoadedAuthored(Soft))
	{
		CancelWalkerLoad();
		AuthoredSalvageWalkerDrop = Loaded;
		WalkerRequestedPath = SoftPath;
		WalkerState = EAuthoredSlotState::Ready;
		return;
	}

	if (WalkerState == EAuthoredSlotState::Pending && WalkerRequestedPath == SoftPath)
	{
		return;
	}

	RequestWalkerAsyncLoad(SoftPath);
}

void UGP_OrbitalUnitDropCatalog::RequestWorkerAsyncLoad(const FSoftObjectPath& SoftPath)
{
	if (WorkerLoadHandle.IsValid() && WorkerRequestedPath == SoftPath)
	{
		return;
	}

	CancelWorkerLoad();
	WorkerRequestedPath = SoftPath;
	WorkerState = EAuthoredSlotState::Pending;
	AuthoredWorkerDrop = nullptr;

#if !UE_BUILD_SHIPPING
	bDebugDidRequestAsyncWorkerLoad = true;
#endif

	WorkerLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftPath,
		FStreamableDelegate::CreateUObject(this, &UGP_OrbitalUnitDropCatalog::HandleWorkerLoaded));

	if (!WorkerLoadHandle.IsValid())
	{
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP OrbitalUnitDropDefinitionLoadFailed: Slot=Worker Path=%s Reason=RequestAsyncLoadNullHandle"),
			*SoftPath.ToString());
#if !UE_BUILD_SHIPPING
		bDebugWorkerLoadFailedLogged = true;
		if (bDebugHoldWorkerCompletion)
		{
			return;
		}
#endif
		WorkerState = EAuthoredSlotState::Failed;
	}
}

void UGP_OrbitalUnitDropCatalog::RequestWalkerAsyncLoad(const FSoftObjectPath& SoftPath)
{
	if (WalkerLoadHandle.IsValid() && WalkerRequestedPath == SoftPath)
	{
		return;
	}

	CancelWalkerLoad();
	WalkerRequestedPath = SoftPath;
	WalkerState = EAuthoredSlotState::Pending;
	AuthoredSalvageWalkerDrop = nullptr;

	WalkerLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftPath,
		FStreamableDelegate::CreateUObject(this, &UGP_OrbitalUnitDropCatalog::HandleWalkerLoaded));

	if (!WalkerLoadHandle.IsValid())
	{
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP OrbitalUnitDropDefinitionLoadFailed: Slot=SalvageWalker Path=%s Reason=RequestAsyncLoadNullHandle"),
			*SoftPath.ToString());
		WalkerState = EAuthoredSlotState::Failed;
	}
}

void UGP_OrbitalUnitDropCatalog::HandleWorkerLoaded()
{
	if (!IsValid(this))
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	if (bDebugHoldWorkerCompletion)
	{
		return;
	}
#endif

	FinishWorkerLoadResolve();
}

void UGP_OrbitalUnitDropCatalog::HandleWalkerLoaded()
{
	if (!IsValid(this))
	{
		return;
	}

	FinishWalkerLoadResolve();
}

void UGP_OrbitalUnitDropCatalog::FinishWorkerLoadResolve()
{
#if !UE_BUILD_SHIPPING
	bDebugForceUnresolvedWorker = false;
	if (DebugInjectedWorkerDrop != nullptr)
	{
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->WorkerDropDefinition = DebugInjectedWorkerDrop;
		}
		AuthoredWorkerDrop = DebugInjectedWorkerDrop;
		WorkerState = EAuthoredSlotState::Ready;
		WorkerLoadHandle.Reset();
		return;
	}
#endif

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> Soft =
		Settings != nullptr ? Settings->WorkerDropDefinition : TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>();
	UGP_OrbitalUnitDropDefinition* Loaded = ResolveLoadedAuthored(Soft);
	if (Loaded == nullptr && !Soft.IsNull())
	{
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP OrbitalUnitDropDefinitionLoadFailed: Slot=Worker Path=%s Reason=ResolveFailedUsingNativeFallback"),
			*Soft.ToSoftObjectPath().ToString());
#if !UE_BUILD_SHIPPING
		bDebugWorkerLoadFailedLogged = true;
#endif
		AuthoredWorkerDrop = nullptr;
		WorkerState = EAuthoredSlotState::Failed;
		WorkerLoadHandle.Reset();
		return;
	}

	AuthoredWorkerDrop = Loaded;
	WorkerState = IsValid(Loaded) ? EAuthoredSlotState::Ready : EAuthoredSlotState::Empty;
	WorkerLoadHandle.Reset();
}

void UGP_OrbitalUnitDropCatalog::FinishWalkerLoadResolve()
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const TSoftObjectPtr<UGP_OrbitalUnitDropDefinition> Soft =
		Settings != nullptr
			? Settings->SalvageWalkerDropDefinition
			: TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>();
	UGP_OrbitalUnitDropDefinition* Loaded = ResolveLoadedAuthored(Soft);
	if (Loaded == nullptr && !Soft.IsNull())
	{
		UE_LOG(LogGPOrbitalUnitDropCatalog, Error,
			TEXT("GP OrbitalUnitDropDefinitionLoadFailed: Slot=SalvageWalker Path=%s Reason=ResolveFailedUsingNativeFallback"),
			*Soft.ToSoftObjectPath().ToString());
		AuthoredSalvageWalkerDrop = nullptr;
		WalkerState = EAuthoredSlotState::Failed;
		WalkerLoadHandle.Reset();
		return;
	}

	AuthoredSalvageWalkerDrop = Loaded;
	WalkerState = IsValid(Loaded) ? EAuthoredSlotState::Ready : EAuthoredSlotState::Empty;
	WalkerLoadHandle.Reset();
}

void UGP_OrbitalUnitDropCatalog::CancelWorkerLoad()
{
	if (WorkerLoadHandle.IsValid())
	{
		if (WorkerLoadHandle->IsLoadingInProgress())
		{
			WorkerLoadHandle->CancelHandle();
		}
		WorkerLoadHandle.Reset();
	}
}

void UGP_OrbitalUnitDropCatalog::CancelWalkerLoad()
{
	if (WalkerLoadHandle.IsValid())
	{
		if (WalkerLoadHandle->IsLoadingInProgress())
		{
			WalkerLoadHandle->CancelHandle();
		}
		WalkerLoadHandle.Reset();
	}
}

UGP_OrbitalUnitDropDefinition* UGP_OrbitalUnitDropCatalog::GetWorkerDrop() const
{
	return CanonicalOrNative(WorkerState, AuthoredWorkerDrop, NativeWorkerDrop);
}

UGP_OrbitalUnitDropDefinition* UGP_OrbitalUnitDropCatalog::GetSalvageWalkerDrop() const
{
	return CanonicalOrNative(WalkerState, AuthoredSalvageWalkerDrop, NativeSalvageWalkerDrop);
}

bool UGP_OrbitalUnitDropCatalog::IsWorkerDropDefinitionPending() const
{
	return WorkerState == EAuthoredSlotState::Pending;
}

bool UGP_OrbitalUnitDropCatalog::IsSalvageWalkerDropDefinitionPending() const
{
	return WalkerState == EAuthoredSlotState::Pending;
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
	if (const UGP_OrbitalUnitDropDefinition* Drop = GetWorkerDrop())
	{
		if (Drop->TransportSlotCost > 0)
		{
			return Drop->TransportSlotCost;
		}
	}
	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return FMath::Max(1, Settings->WorkerTransportSlotCost);
	}
	return 1;
}

int32 UGP_OrbitalUnitDropCatalog::GetSalvageWalkerTransportSlotCost() const
{
	if (const UGP_OrbitalUnitDropDefinition* Drop = GetSalvageWalkerDrop())
	{
		if (Drop->TransportSlotCost > 0)
		{
			return Drop->TransportSlotCost;
		}
	}
	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return FMath::Max(1, Settings->SalvageWalkerTransportSlotCost);
	}
	return 2;
}

float UGP_OrbitalUnitDropCatalog::GetWorkerOrbitalDropCost() const
{
	if (const UGP_OrbitalUnitDropDefinition* Drop = GetWorkerDrop())
	{
		return FMath::Max(0.0f, Drop->Cost);
	}
	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return FMath::Max(0.0f, Settings->WorkerOrbitalDropCost);
	}
	return 25.0f;
}

float UGP_OrbitalUnitDropCatalog::GetSalvageWalkerOrbitalDropCost() const
{
	if (const UGP_OrbitalUnitDropDefinition* Drop = GetSalvageWalkerDrop())
	{
		return FMath::Max(0.0f, Drop->Cost);
	}
	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return FMath::Max(0.0f, Settings->SalvageWalkerOrbitalDropCost);
	}
	return 50.0f;
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

	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return Settings->ResolveWorkerPayloadClass();
	}
	return TSubclassOf<AGP_Worker>(AGP_Worker::StaticClass());
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

	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return Settings->ResolveSalvageWalkerPayloadClass();
	}
	return TSubclassOf<AGP_SalvageWalker>(AGP_SalvageWalker::StaticClass());
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
	Apply(AuthoredWorkerDrop);
	Apply(AuthoredSalvageWalkerDrop);
}

#if !UE_BUILD_SHIPPING
void UGP_OrbitalUnitDropCatalog::DebugAssignLoadedAuthoredWorker(UGP_OrbitalUnitDropDefinition* Definition)
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (!bDebugSavedSettings)
		{
			DebugSavedWorkerSettingsRef = Settings->WorkerDropDefinition;
			DebugSavedWalkerSettingsRef = Settings->SalvageWalkerDropDefinition;
			bDebugSavedSettings = true;
		}
		Settings->WorkerDropDefinition = Definition;
	}
	bDebugForceUnresolvedWorker = false;
	bDebugHoldWorkerCompletion = false;
	DebugInjectedWorkerDrop = nullptr;
	RefreshWorkerSlot();
}

void UGP_OrbitalUnitDropCatalog::DebugForceUnresolvedAuthoredWorkerLoad(
	UGP_OrbitalUnitDropDefinition* InjectedDefinition,
	bool bHoldCompletion)
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (!bDebugSavedSettings)
		{
			DebugSavedWorkerSettingsRef = Settings->WorkerDropDefinition;
			DebugSavedWalkerSettingsRef = Settings->SalvageWalkerDropDefinition;
			bDebugSavedSettings = true;
		}
		Settings->WorkerDropDefinition = TSoftObjectPtr<UGP_OrbitalUnitDropDefinition>(
			FSoftObjectPath(GPOrbitalUnitDropCatalogPrivate::UnresolvedWorkerStub));
	}

	bDebugForceUnresolvedWorker = true;
	bDebugHoldWorkerCompletion = bHoldCompletion;
	bDebugDidRequestAsyncWorkerLoad = false;
	DebugInjectedWorkerDrop = InjectedDefinition;
	CancelWorkerLoad();
	AuthoredWorkerDrop = nullptr;
	WorkerState = EAuthoredSlotState::Empty;
	RefreshWorkerSlot();
}

void UGP_OrbitalUnitDropCatalog::DebugCompletePendingAuthoredWorkerLoad()
{
	if (WorkerState != EAuthoredSlotState::Pending)
	{
		return;
	}
	bDebugHoldWorkerCompletion = false;
	FinishWorkerLoadResolve();
}

void UGP_OrbitalUnitDropCatalog::DebugForceAuthoredWorkerLoadFailure()
{
	DebugInjectedWorkerDrop = nullptr;
	bDebugHoldWorkerCompletion = false;
	if (WorkerState == EAuthoredSlotState::Pending)
	{
		FinishWorkerLoadResolve();
	}
}

void UGP_OrbitalUnitDropCatalog::DebugClearAuthoredUnitDropOverrides()
{
	CancelWorkerLoad();
	CancelWalkerLoad();
	bDebugForceUnresolvedWorker = false;
	bDebugHoldWorkerCompletion = false;
	bDebugDidRequestAsyncWorkerLoad = false;
	DebugInjectedWorkerDrop = nullptr;
	AuthoredWorkerDrop = nullptr;
	AuthoredSalvageWalkerDrop = nullptr;
	WorkerState = EAuthoredSlotState::Empty;
	WalkerState = EAuthoredSlotState::Empty;
	WorkerRequestedPath.Reset();
	WalkerRequestedPath.Reset();

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
	CancelWorkerLoad();
	CancelWalkerLoad();
	AuthoredWorkerDrop = nullptr;
	AuthoredSalvageWalkerDrop = nullptr;
	WorkerState = EAuthoredSlotState::Empty;
	WalkerState = EAuthoredSlotState::Empty;
	WorkerRequestedPath.Reset();
	WalkerRequestedPath.Reset();
	bDebugForceUnresolvedWorker = false;
	bDebugHoldWorkerCompletion = false;
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
