// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitDefinition.h"

namespace GPUnitDefinitionPrivate
{
	static constexpr const TCHAR* PrimaryType = TEXT("GPUnitDefinition");
}

UGP_UnitDefinition::UGP_UnitDefinition()
{
	RetaliationPursuitSeconds = 5.0f;
}

const TCHAR* UGP_UnitDefinition::PrimaryAssetTypeName()
{
	return GPUnitDefinitionPrivate::PrimaryType;
}

FPrimaryAssetId UGP_UnitDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(PrimaryAssetTypeName()), GetFName());
}
