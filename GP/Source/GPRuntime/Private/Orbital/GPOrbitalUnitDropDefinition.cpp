// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPOrbitalUnitDropDefinition.h"

#include "Units/GPUnitBase.h"
#include "Units/GPUnitDefinition.h"

namespace GPOrbitalUnitDropDefinitionPrivate
{
	static constexpr const TCHAR* PrimaryType = TEXT("GPOrbitalUnitDropDefinition");
}

UGP_OrbitalUnitDropDefinition::UGP_OrbitalUnitDropDefinition()
{
	Cost = 0.0f;
	TransportSlotCost = 1;
	DeliveryDescentSeconds = 2.5f;
	PayloadDeployDelaySeconds = 1.25f;
}

const TCHAR* UGP_OrbitalUnitDropDefinition::PrimaryAssetTypeName()
{
	return GPOrbitalUnitDropDefinitionPrivate::PrimaryType;
}

FPrimaryAssetId UGP_OrbitalUnitDropDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(PrimaryAssetTypeName()), GetFName());
}

const UGP_UnitDefinition* UGP_OrbitalUnitDropDefinition::ResolveLoadedUnitDefinition() const
{
	if (UnitDefinition.IsNull())
	{
		return nullptr;
	}

	UObject* Loaded = UnitDefinition.Get();
	if (Loaded == nullptr)
	{
		Loaded = UnitDefinition.ToSoftObjectPath().ResolveObject();
	}
	return Cast<UGP_UnitDefinition>(Loaded);
}

TSubclassOf<AGP_UnitBase> UGP_OrbitalUnitDropDefinition::ResolveLoadedPayloadClass() const
{
	if (PayloadClass.IsNull())
	{
		return nullptr;
	}

	UClass* Loaded = PayloadClass.Get();
	if (Loaded == nullptr)
	{
		Loaded = Cast<UClass>(PayloadClass.ToSoftObjectPath().ResolveObject());
	}

	if (Loaded == nullptr || !Loaded->IsChildOf(AGP_UnitBase::StaticClass()))
	{
		return nullptr;
	}

	return TSubclassOf<AGP_UnitBase>(Loaded);
}
