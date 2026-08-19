// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/GPOrbitalDeliverySettings.h"

#include "Buildings/GPDefensiveTurret.h"
#include "Buildings/GPLogisticsHub.h"
#include "Orbital/GPDropPod.h"

UGP_OrbitalDeliverySettings::UGP_OrbitalDeliverySettings()
{
	CategoryName = FName(TEXT("Game"));
	SectionName = FName(TEXT("GP Orbital Delivery"));
}

FName UGP_OrbitalDeliverySettings::GetCategoryName() const
{
	return FName(TEXT("Game"));
}

const UGP_OrbitalDeliverySettings* UGP_OrbitalDeliverySettings::Get()
{
	return GetDefault<UGP_OrbitalDeliverySettings>();
}

namespace GPOrbitalDeliverySettingsPrivate
{
	template <typename TBase>
	static UClass* TryLoadSoftSubclass(const TSoftClassPtr<TBase>& Soft, bool& bOutHadSoft, bool& bOutInvalid)
	{
		bOutHadSoft = !Soft.IsNull();
		bOutInvalid = false;
		if (!bOutHadSoft)
		{
			return nullptr;
		}

		UClass* Loaded = Soft.LoadSynchronous();
		if (Loaded == nullptr || !Loaded->IsChildOf(TBase::StaticClass()))
		{
			bOutInvalid = true;
			UE_LOG(LogTemp, Warning,
				TEXT("GP OrbitalDeliverySettings: soft class '%s' missing or not a subclass of %s — using native fallback."),
				*Soft.ToString(),
				*TBase::StaticClass()->GetName());
			return nullptr;
		}
		return Loaded;
	}
}

TSubclassOf<AGP_DropPod> UGP_OrbitalDeliverySettings::ResolveUnitDropPodClass(bool* bOutUsedAuthored) const
{
	bool bHadSoft = false;
	bool bInvalid = false;
	UClass* Loaded = GPOrbitalDeliverySettingsPrivate::TryLoadSoftSubclass(UnitDropPodClass, bHadSoft, bInvalid);
	if (bOutUsedAuthored != nullptr)
	{
		*bOutUsedAuthored = Loaded != nullptr;
	}
	return Loaded != nullptr ? Loaded : AGP_DropPod::StaticClass();
}

bool UGP_OrbitalDeliverySettings::IsUnitDropPodClassConfigInvalid() const
{
	bool bHadSoft = false;
	bool bInvalid = false;
	GPOrbitalDeliverySettingsPrivate::TryLoadSoftSubclass(UnitDropPodClass, bHadSoft, bInvalid);
	return bHadSoft && bInvalid;
}

TSubclassOf<AGP_BuildingBase> UGP_OrbitalDeliverySettings::ResolveBuildingPayloadClass(bool* bOutUsedAuthored) const
{
	bool bHadSoft = false;
	bool bInvalid = false;
	UClass* Loaded = GPOrbitalDeliverySettingsPrivate::TryLoadSoftSubclass(BuildingPayloadClass, bHadSoft, bInvalid);
	if (bOutUsedAuthored != nullptr)
	{
		*bOutUsedAuthored = Loaded != nullptr;
	}
	return Loaded != nullptr ? Loaded : AGP_LogisticsHub::StaticClass();
}

bool UGP_OrbitalDeliverySettings::IsBuildingPayloadClassConfigInvalid() const
{
	bool bHadSoft = false;
	bool bInvalid = false;
	GPOrbitalDeliverySettingsPrivate::TryLoadSoftSubclass(BuildingPayloadClass, bHadSoft, bInvalid);
	return bHadSoft && bInvalid;
}

TSubclassOf<AGP_BuildingBase> UGP_OrbitalDeliverySettings::ResolveDefensiveTurretPayloadClass(bool* bOutUsedAuthored) const
{
	bool bHadSoft = false;
	bool bInvalid = false;
	UClass* Loaded = GPOrbitalDeliverySettingsPrivate::TryLoadSoftSubclass(DefensiveTurretPayloadClass, bHadSoft, bInvalid);
	if (Loaded != nullptr && !Loaded->IsChildOf(AGP_DefensiveTurret::StaticClass()))
	{
		Loaded = nullptr;
		bInvalid = true;
	}
	if (bOutUsedAuthored != nullptr)
	{
		*bOutUsedAuthored = Loaded != nullptr;
	}
	return Loaded != nullptr ? Loaded : AGP_DefensiveTurret::StaticClass();
}

bool UGP_OrbitalDeliverySettings::IsDefensiveTurretPayloadClassConfigInvalid() const
{
	bool bHadSoft = false;
	bool bInvalid = false;
	UClass* Loaded = GPOrbitalDeliverySettingsPrivate::TryLoadSoftSubclass(DefensiveTurretPayloadClass, bHadSoft, bInvalid);
	if (Loaded != nullptr && !Loaded->IsChildOf(AGP_DefensiveTurret::StaticClass()))
	{
		return true;
	}
	return bHadSoft && bInvalid;
}
