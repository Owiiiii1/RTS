// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPBuildingDefinition.h"

#include "Buildings/GPBuildingBase.h"

namespace GPBuildingDefinitionPrivate
{
	static constexpr const TCHAR* PrimaryType = TEXT("GPBuildingDefinition");
}

UGP_BuildingDefinition::UGP_BuildingDefinition()
{
	MaxHealth = 500.0f;
	FootprintCells = FIntPoint(1, 1);
}

const TCHAR* UGP_BuildingDefinition::PrimaryAssetTypeName()
{
	return GPBuildingDefinitionPrivate::PrimaryType;
}

FPrimaryAssetId UGP_BuildingDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(PrimaryAssetTypeName()), GetFName());
}

TSubclassOf<AGP_BuildingBase> UGP_BuildingDefinition::ResolveLoadedSpawnedClass() const
{
	if (SpawnedClass.IsNull())
	{
		return nullptr;
	}

	UClass* Loaded = SpawnedClass.Get();
	if (Loaded == nullptr)
	{
		Loaded = Cast<UClass>(SpawnedClass.ToSoftObjectPath().ResolveObject());
	}

	if (Loaded == nullptr || !Loaded->IsChildOf(AGP_BuildingBase::StaticClass()))
	{
		return nullptr;
	}

	return TSubclassOf<AGP_BuildingBase>(Loaded);
}
