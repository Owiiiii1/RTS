// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPWallPackageCatalog.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Misc/CoreDelegates.h"
#include "Orbital/GPWallPackageDefinition.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Tags/GPGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPWallPackageCatalog, Log, All);

namespace GPWallPackageCatalogPrivate
{
	static TStrongObjectPtr<UGP_WallPackageCatalog> GCatalog;
	static FDelegateHandle EnginePreExitHandle;
	static constexpr TCHAR CatalogObjectName[] = TEXT("GP_WallPackageCatalog");
	static constexpr TCHAR UnresolvedStub[] =
		TEXT("/Game/GrimProtocol/Data/Orbital/DA_GP_WallPackage_UnresolvedSoftRefStub.DA_GP_WallPackage_UnresolvedSoftRefStub");
}

UGP_WallPackageCatalog& UGP_WallPackageCatalog::Get()
{
	if (!GPWallPackageCatalogPrivate::GCatalog.IsValid())
	{
		UGP_WallPackageCatalog* CatalogObj = FindObject<UGP_WallPackageCatalog>(
			GetTransientPackage(),
			GPWallPackageCatalogPrivate::CatalogObjectName);
		if (!IsValid(CatalogObj))
		{
			CatalogObj = NewObject<UGP_WallPackageCatalog>(
				GetTransientPackage(),
				GPWallPackageCatalogPrivate::CatalogObjectName,
				RF_Transient);
		}
		GPWallPackageCatalogPrivate::GCatalog.Reset(CatalogObj);
		CatalogObj->EnsureNativeCatalog();
	}

	UGP_WallPackageCatalog& Catalog = *GPWallPackageCatalogPrivate::GCatalog.Get();
	Catalog.RefreshAuthoredBindings();
	return Catalog;
}

void UGP_WallPackageCatalog::ShutdownCatalog()
{
	if (GPWallPackageCatalogPrivate::GCatalog.IsValid())
	{
		GPWallPackageCatalogPrivate::GCatalog->CancelLoad();
	}
	GPWallPackageCatalogPrivate::GCatalog.Reset();
}

void UGP_WallPackageCatalog::BindEngineLifecycle()
{
	if (GPWallPackageCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		return;
	}

	GPWallPackageCatalogPrivate::EnginePreExitHandle =
		FCoreDelegates::OnEnginePreExit.AddStatic(&UGP_WallPackageCatalog::ShutdownCatalog);
}

void UGP_WallPackageCatalog::UnbindEngineLifecycle()
{
	if (GPWallPackageCatalogPrivate::EnginePreExitHandle.IsValid())
	{
		FCoreDelegates::OnEnginePreExit.Remove(GPWallPackageCatalogPrivate::EnginePreExitHandle);
		GPWallPackageCatalogPrivate::EnginePreExitHandle.Reset();
	}
}

void UGP_WallPackageCatalog::EnsureNativeCatalog()
{
	if (bNativeCatalogReady)
	{
		return;
	}

	NativePackage = CreateNativePackage();
	bNativeCatalogReady = true;
}

UGP_WallPackageDefinition* UGP_WallPackageCatalog::CreateNativePackage()
{
	UGP_WallPackageDefinition* Package = NewObject<UGP_WallPackageDefinition>(
		this,
		FName(TEXT("DA_GP_WallPackage")),
		RF_Transient);
	Package->DisplayName = NSLOCTEXT("GPWallPackageCatalog", "WallPackage", "Wall Package");
	Package->Cost = UGP_WallPackageDefinition::NativeBootstrapCost;
	Package->SegmentCount = UGP_WallPackageDefinition::NativeBootstrapSegmentCount;
	Package->DeliveryDescentSeconds = 2.5f;
	Package->PayloadDeployDelaySeconds = 2.0f;
	const FGPGameplayTags& Tags = FGPGameplayTags::Get();
	if (Tags.Drop_Type_WallPackage.IsValid())
	{
		Package->DropTags.Reset();
		Package->DropTags.AddTag(Tags.Drop_Type_WallPackage);
	}
	return Package;
}

UGP_WallPackageDefinition* UGP_WallPackageCatalog::ResolveLoadedAuthored(
	const TSoftObjectPtr<UGP_WallPackageDefinition>& Soft) const
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
	return Cast<UGP_WallPackageDefinition>(Loaded);
}

UGP_WallPackageDefinition* UGP_WallPackageCatalog::CanonicalOrNative() const
{
	if (State == EAuthoredSlotState::Pending)
	{
		return nullptr;
	}
	if (State == EAuthoredSlotState::Ready && IsValid(AuthoredPackage))
	{
		return AuthoredPackage;
	}
	return NativePackage;
}

void UGP_WallPackageCatalog::RefreshAuthoredBindings()
{
	EnsureNativeCatalog();
	RefreshSlot();
}

void UGP_WallPackageCatalog::RefreshSlot()
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const TSoftObjectPtr<UGP_WallPackageDefinition> Soft =
		Settings != nullptr ? Settings->WallPackageDefinition : TSoftObjectPtr<UGP_WallPackageDefinition>();

	if (Soft.IsNull())
	{
		CancelLoad();
		AuthoredPackage = nullptr;
		RequestedPath.Reset();
		State = EAuthoredSlotState::Empty;
		return;
	}

	const FSoftObjectPath SoftPath = Soft.ToSoftObjectPath();

#if !UE_BUILD_SHIPPING
	if (bDebugForceUnresolved && State != EAuthoredSlotState::Ready)
	{
		if (State == EAuthoredSlotState::Pending && RequestedPath == SoftPath)
		{
			return;
		}
		RequestAsyncLoad(SoftPath);
		return;
	}
#endif

	if (UGP_WallPackageDefinition* Loaded = ResolveLoadedAuthored(Soft))
	{
		CancelLoad();
		AuthoredPackage = Loaded;
		RequestedPath = SoftPath;
		State = EAuthoredSlotState::Ready;
		return;
	}

	if (State == EAuthoredSlotState::Pending && RequestedPath == SoftPath)
	{
		return;
	}

	RequestAsyncLoad(SoftPath);
}

void UGP_WallPackageCatalog::RequestAsyncLoad(const FSoftObjectPath& SoftPath)
{
	if (LoadHandle.IsValid() && RequestedPath == SoftPath)
	{
		return;
	}

	CancelLoad();
	RequestedPath = SoftPath;
	State = EAuthoredSlotState::Pending;
	AuthoredPackage = nullptr;

#if !UE_BUILD_SHIPPING
	bDebugDidRequestAsyncLoad = true;
#endif

	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftPath,
		FStreamableDelegate::CreateUObject(this, &UGP_WallPackageCatalog::HandleLoaded));

	if (!LoadHandle.IsValid())
	{
		UE_LOG(LogGPWallPackageCatalog, Error,
			TEXT("GP WallPackageDefinitionLoadFailed: Path=%s Reason=RequestAsyncLoadNullHandle"),
			*SoftPath.ToString());
#if !UE_BUILD_SHIPPING
		bDebugLoadFailedLogged = true;
		if (bDebugHoldCompletion)
		{
			return;
		}
#endif
		State = EAuthoredSlotState::Failed;
	}
}

void UGP_WallPackageCatalog::HandleLoaded()
{
	if (!IsValid(this))
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	if (bDebugHoldCompletion)
	{
		return;
	}
#endif

	FinishLoadResolve();
}

void UGP_WallPackageCatalog::FinishLoadResolve()
{
#if !UE_BUILD_SHIPPING
	bDebugForceUnresolved = false;
	if (DebugInjectedPackage != nullptr)
	{
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->WallPackageDefinition = DebugInjectedPackage;
		}
		AuthoredPackage = DebugInjectedPackage;
		State = EAuthoredSlotState::Ready;
		LoadHandle.Reset();
		return;
	}
#endif

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const TSoftObjectPtr<UGP_WallPackageDefinition> Soft =
		Settings != nullptr ? Settings->WallPackageDefinition : TSoftObjectPtr<UGP_WallPackageDefinition>();
	UGP_WallPackageDefinition* Loaded = ResolveLoadedAuthored(Soft);
	if (Loaded == nullptr && !Soft.IsNull())
	{
		UE_LOG(LogGPWallPackageCatalog, Error,
			TEXT("GP WallPackageDefinitionLoadFailed: Path=%s Reason=ResolveFailedUsingNativeFallback"),
			*Soft.ToSoftObjectPath().ToString());
#if !UE_BUILD_SHIPPING
		bDebugLoadFailedLogged = true;
#endif
		AuthoredPackage = nullptr;
		State = EAuthoredSlotState::Failed;
		LoadHandle.Reset();
		return;
	}

	AuthoredPackage = Loaded;
	State = IsValid(Loaded) ? EAuthoredSlotState::Ready : EAuthoredSlotState::Empty;
	LoadHandle.Reset();
}

void UGP_WallPackageCatalog::CancelLoad()
{
	if (LoadHandle.IsValid())
	{
		if (LoadHandle->IsLoadingInProgress())
		{
			LoadHandle->CancelHandle();
		}
		LoadHandle.Reset();
	}
}

UGP_WallPackageDefinition* UGP_WallPackageCatalog::GetWallPackage() const
{
	return CanonicalOrNative();
}

bool UGP_WallPackageCatalog::IsWallPackageDefinitionPending() const
{
	return State == EAuthoredSlotState::Pending;
}

bool UGP_WallPackageCatalog::IsWallPackageDefinitionReady() const
{
	return GetWallPackage() != nullptr && !IsWallPackageDefinitionPending();
}

void UGP_WallPackageCatalog::ResolveDeliveryTiming(float& OutDescentSeconds, float& OutPayloadDeployDelaySeconds) const
{
	OutDescentSeconds = 2.5f;
	OutPayloadDeployDelaySeconds = 2.0f;
	if (const UGP_WallPackageDefinition* Package = GetWallPackage())
	{
		OutDescentSeconds = Package->DeliveryDescentSeconds;
		OutPayloadDeployDelaySeconds = Package->PayloadDeployDelaySeconds;
	}
}

#if !UE_BUILD_SHIPPING
void UGP_WallPackageCatalog::DebugAssignLoadedAuthored(UGP_WallPackageDefinition* Definition)
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (!bDebugSavedSettings)
		{
			DebugSavedSettingsRef = Settings->WallPackageDefinition;
			bDebugSavedSettings = true;
		}
		Settings->WallPackageDefinition = Definition;
	}
	bDebugForceUnresolved = false;
	bDebugHoldCompletion = false;
	DebugInjectedPackage = nullptr;
	RefreshSlot();
}

void UGP_WallPackageCatalog::DebugForceUnresolvedAuthoredLoad(
	UGP_WallPackageDefinition* InjectedDefinition,
	bool bHoldCompletion)
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (!bDebugSavedSettings)
		{
			DebugSavedSettingsRef = Settings->WallPackageDefinition;
			bDebugSavedSettings = true;
		}
		Settings->WallPackageDefinition = TSoftObjectPtr<UGP_WallPackageDefinition>(
			FSoftObjectPath(GPWallPackageCatalogPrivate::UnresolvedStub));
	}

	bDebugForceUnresolved = true;
	bDebugHoldCompletion = bHoldCompletion;
	bDebugDidRequestAsyncLoad = false;
	DebugInjectedPackage = InjectedDefinition;
	CancelLoad();
	AuthoredPackage = nullptr;
	State = EAuthoredSlotState::Empty;
	RefreshSlot();
}

void UGP_WallPackageCatalog::DebugCompletePendingAuthoredLoad()
{
	if (State != EAuthoredSlotState::Pending)
	{
		return;
	}
	bDebugHoldCompletion = false;
	FinishLoadResolve();
}

void UGP_WallPackageCatalog::DebugForceAuthoredLoadFailure()
{
	DebugInjectedPackage = nullptr;
	bDebugHoldCompletion = false;
	if (State == EAuthoredSlotState::Pending)
	{
		FinishLoadResolve();
	}
}

void UGP_WallPackageCatalog::DebugClearAuthoredOverrides()
{
	CancelLoad();
	bDebugForceUnresolved = false;
	bDebugHoldCompletion = false;
	bDebugDidRequestAsyncLoad = false;
	DebugInjectedPackage = nullptr;
	AuthoredPackage = nullptr;
	State = EAuthoredSlotState::Empty;
	RequestedPath.Reset();

	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (bDebugSavedSettings)
		{
			Settings->WallPackageDefinition = DebugSavedSettingsRef;
		}
		else
		{
			Settings->WallPackageDefinition.Reset();
		}
	}
	bDebugSavedSettings = false;
	DebugSavedSettingsRef.Reset();
}

void UGP_WallPackageCatalog::DebugBeginContractIsolation()
{
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		if (!bContractIsolationActive)
		{
			ContractSavedSettingsRef = Settings->WallPackageDefinition;
			bContractIsolationActive = true;
		}
		Settings->WallPackageDefinition.Reset();
	}
	CancelLoad();
	AuthoredPackage = nullptr;
	State = EAuthoredSlotState::Empty;
	RequestedPath.Reset();
	bDebugForceUnresolved = false;
	bDebugHoldCompletion = false;
}

void UGP_WallPackageCatalog::DebugEndContractIsolation()
{
	if (!bContractIsolationActive)
	{
		return;
	}
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		Settings->WallPackageDefinition = ContractSavedSettingsRef;
	}
	bContractIsolationActive = false;
	ContractSavedSettingsRef.Reset();
	RefreshAuthoredBindings();
}
#endif
