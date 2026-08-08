// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/GPGameplayPresentationSettings.h"

UGP_GameplayPresentationSettings::UGP_GameplayPresentationSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("GP Gameplay Presentation");

	TeamStyles.Reset();
	{
		FGP_TeamPresentationStyle Team1;
		Team1.TeamId = 1;
		Team1.TeamColor = FLinearColor(0.15f, 0.40f, 0.95f, 1.0f); // BLUE
		TeamStyles.Add(Team1);
	}
	{
		FGP_TeamPresentationStyle Team2;
		Team2.TeamId = 2;
		Team2.TeamColor = FLinearColor(0.90f, 0.15f, 0.15f, 1.0f); // RED
		TeamStyles.Add(Team2);
	}
}

FName UGP_GameplayPresentationSettings::GetCategoryName() const
{
	return FName(TEXT("Game"));
}

const UGP_GameplayPresentationSettings* UGP_GameplayPresentationSettings::Get()
{
	return GetDefault<UGP_GameplayPresentationSettings>();
}

FLinearColor UGP_GameplayPresentationSettings::GetTeamColor(int32 TeamId) const
{
	if (TeamId < 1)
	{
		return NeutralTeamColor;
	}

	for (const FGP_TeamPresentationStyle& Style : TeamStyles)
	{
		if (Style.TeamId == TeamId)
		{
			return Style.TeamColor;
		}
	}

	return NeutralTeamColor;
}
