// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/GPOrbitalDeliverySettings.h"

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
