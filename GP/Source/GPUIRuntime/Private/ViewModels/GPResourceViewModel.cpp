// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPResourceViewModel.h"

void UGP_ResourceViewModel::SetOrbitalFerronite(float Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(OrbitalFerronite, Value);
}

void UGP_ResourceViewModel::SetFerroniteScore(float Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(FerroniteScore, Value);
}

void UGP_ResourceViewModel::SetCurrentUnits(float Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentUnits, Value);
}

void UGP_ResourceViewModel::SetMaxUnits(float Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(MaxUnits, Value);
}

void UGP_ResourceViewModel::SetOpponentFerroniteScore(float Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(OpponentFerroniteScore, Value);
}

void UGP_ResourceViewModel::SetPlanetFerronite(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? Value : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(PlanetFerronite, SafeValue);
}
