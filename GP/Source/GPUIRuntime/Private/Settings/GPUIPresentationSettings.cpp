// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/GPUIPresentationSettings.h"

UGP_UIPresentationSettings::UGP_UIPresentationSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("GP UI Presentation");
}

FName UGP_UIPresentationSettings::GetCategoryName() const
{
	return FName(TEXT("Game"));
}

const UGP_UIPresentationSettings* UGP_UIPresentationSettings::Get()
{
	return GetDefault<UGP_UIPresentationSettings>();
}
