// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPOrbitalDropDefinition.h"

#include "Buildings/GPBuildingDefinition.h"

namespace GPOrbitalDropDefinitionPrivate
{
	static constexpr const TCHAR* PrimaryType = TEXT("GPOrbitalDropDefinition");
}

UGP_OrbitalDropDefinition::UGP_OrbitalDropDefinition()
{
	Cost = 0.0f;
	DeliveryDescentSeconds = 2.5f;
	PayloadDeployDelaySeconds = 2.0f;
}

const TCHAR* UGP_OrbitalDropDefinition::PrimaryAssetTypeName()
{
	return GPOrbitalDropDefinitionPrivate::PrimaryType;
}

FPrimaryAssetId UGP_OrbitalDropDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(PrimaryAssetTypeName()), GetFName());
}

UGP_BuildingDefinition* UGP_OrbitalDropDefinition::ResolveLoadedBuildingDefinition() const
{
	if (BuildingDefinition.IsNull())
	{
		return nullptr;
	}

	if (UGP_BuildingDefinition* Loaded = BuildingDefinition.Get())
	{
		return Loaded;
	}

	return Cast<UGP_BuildingDefinition>(BuildingDefinition.ToSoftObjectPath().ResolveObject());
}

FText UGP_OrbitalDropDefinition::GetAcquisitionDisplayName() const
{
	if (const UGP_BuildingDefinition* Building = ResolveLoadedBuildingDefinition())
	{
		if (!Building->DisplayName.IsEmpty())
		{
			return Building->DisplayName;
		}
	}

	return FText::FromName(GetPrimaryAssetId().PrimaryAssetName);
}
