// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/GPResourceGameplaySettings.h"

UGP_ResourceGameplaySettings::UGP_ResourceGameplaySettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("GP Resource Gameplay");
}

FName UGP_ResourceGameplaySettings::GetCategoryName() const
{
	return FName(TEXT("Game"));
}

const UGP_ResourceGameplaySettings* UGP_ResourceGameplaySettings::Get()
{
	return GetDefault<UGP_ResourceGameplaySettings>();
}
