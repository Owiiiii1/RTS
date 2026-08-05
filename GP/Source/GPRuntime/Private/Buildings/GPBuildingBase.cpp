// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPBuildingBase.h"

#include "Tags/GPGameplayTags.h"

AGP_BuildingBase::AGP_BuildingBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	CapabilityTags.Reset();
	if (GPTags.Capability_Selectable.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Capability_Selectable);
	}
	if (GPTags.Capability_Inspectable.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Capability_Inspectable);
	}
	if (GPTags.Selection_Type_Building.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Selection_Type_Building);
	}
	if (GPTags.Unit_Type_Building.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Unit_Type_Building);
	}
}
