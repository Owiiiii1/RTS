// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/GPOrbitalDeliverySettings.h"

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
