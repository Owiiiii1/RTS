// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPOrbitalUnitDropCatalog.h"

#include "Misc/CoreDelegates.h"
#include "Orbital/GPOrbitalUnitDropDefinition.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPUnitDefinitionCatalog.h"
#include "Units/GPWorker.h"
#include "UObject/StrongObjectPtr.h"

namespace GPOrbitalUnitDropCatalogPrivate
{
	static TStrongObjectPtr<UGP_OrbitalUnitDropCatalog> GCatalog;
	static FDelegateHandle EnginePreExitHandle;
	static constexpr TCHAR CatalogObjectName[] = TEXT("GP_OrbitalUnitDropCatalog");
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

	return *GPOrbitalUnitDropCatalogPrivate::GCatalog.Get();
}

void UGP_OrbitalUnitDropCatalog::ShutdownCatalog()
{
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

	WorkerDrop = CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalUnitDrop_Worker")),
		NSLOCTEXT("GPOrbitalUnitDropCatalog", "Worker", "Worker"));
	WorkerDrop->UnitDefinition = Units.GetWorkerDefinition();
	WorkerDrop->Cost = 25.0f;
	WorkerDrop->TransportSlotCost = 1;
	WorkerDrop->DeliveryDescentSeconds = 2.5f;
	WorkerDrop->PayloadDeployDelaySeconds = 1.25f;

	SalvageWalkerDrop = CreateNativeDrop(
		FName(TEXT("DA_GP_OrbitalUnitDrop_SalvageWalker")),
		NSLOCTEXT("GPOrbitalUnitDropCatalog", "SalvageWalker", "Salvage Walker"));
	SalvageWalkerDrop->UnitDefinition = Units.GetSalvageWalkerDefinition();
	SalvageWalkerDrop->Cost = 50.0f;
	SalvageWalkerDrop->TransportSlotCost = 2;
	SalvageWalkerDrop->DeliveryDescentSeconds = 2.5f;
	SalvageWalkerDrop->PayloadDeployDelaySeconds = 1.25f;

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

int32 UGP_OrbitalUnitDropCatalog::GetWorkerTransportSlotCost() const
{
	if (IsValid(WorkerDrop) && WorkerDrop->TransportSlotCost > 0)
	{
		return WorkerDrop->TransportSlotCost;
	}
	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return FMath::Max(1, Settings->WorkerTransportSlotCost);
	}
	return 1;
}

int32 UGP_OrbitalUnitDropCatalog::GetSalvageWalkerTransportSlotCost() const
{
	if (IsValid(SalvageWalkerDrop) && SalvageWalkerDrop->TransportSlotCost > 0)
	{
		return SalvageWalkerDrop->TransportSlotCost;
	}
	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return FMath::Max(1, Settings->SalvageWalkerTransportSlotCost);
	}
	return 2;
}

float UGP_OrbitalUnitDropCatalog::GetWorkerOrbitalDropCost() const
{
	if (IsValid(WorkerDrop))
	{
		return FMath::Max(0.0f, WorkerDrop->Cost);
	}
	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return FMath::Max(0.0f, Settings->WorkerOrbitalDropCost);
	}
	return 25.0f;
}

float UGP_OrbitalUnitDropCatalog::GetSalvageWalkerOrbitalDropCost() const
{
	if (IsValid(SalvageWalkerDrop))
	{
		return FMath::Max(0.0f, SalvageWalkerDrop->Cost);
	}
	if (const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get())
	{
		return FMath::Max(0.0f, Settings->SalvageWalkerOrbitalDropCost);
	}
	return 50.0f;
}

TSubclassOf<AGP_Worker> UGP_OrbitalUnitDropCatalog::ResolveWorkerPayloadClass() const
{
	if (IsValid(WorkerDrop))
	{
		if (TSubclassOf<AGP_UnitBase> Loaded = WorkerDrop->ResolveLoadedPayloadClass())
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
	if (IsValid(SalvageWalkerDrop))
	{
		if (TSubclassOf<AGP_UnitBase> Loaded = SalvageWalkerDrop->ResolveLoadedPayloadClass())
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
	if (Manifest.WorkerCount > 0 && IsValid(WorkerDrop))
	{
		OutDescentSeconds = WorkerDrop->DeliveryDescentSeconds;
		OutPayloadDeployDelaySeconds = WorkerDrop->PayloadDeployDelaySeconds;
		bUsedDefinition = true;
	}
	if (Manifest.SalvageWalkerCount > 0 && IsValid(SalvageWalkerDrop))
	{
		if (bUsedDefinition)
		{
			OutDescentSeconds = FMath::Max(OutDescentSeconds, SalvageWalkerDrop->DeliveryDescentSeconds);
			OutPayloadDeployDelaySeconds = FMath::Max(
				OutPayloadDeployDelaySeconds,
				SalvageWalkerDrop->PayloadDeployDelaySeconds);
		}
		else
		{
			OutDescentSeconds = SalvageWalkerDrop->DeliveryDescentSeconds;
			OutPayloadDeployDelaySeconds = SalvageWalkerDrop->PayloadDeployDelaySeconds;
		}
	}
}

void UGP_OrbitalUnitDropCatalog::OverrideDeliveryTiming(float DescentSeconds, float PayloadDeployDelaySeconds)
{
	if (IsValid(WorkerDrop))
	{
		WorkerDrop->DeliveryDescentSeconds = DescentSeconds;
		WorkerDrop->PayloadDeployDelaySeconds = PayloadDeployDelaySeconds;
	}
	if (IsValid(SalvageWalkerDrop))
	{
		SalvageWalkerDrop->DeliveryDescentSeconds = DescentSeconds;
		SalvageWalkerDrop->PayloadDeployDelaySeconds = PayloadDeployDelaySeconds;
	}
}
