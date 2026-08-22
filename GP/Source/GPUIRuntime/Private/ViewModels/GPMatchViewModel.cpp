// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPMatchViewModel.h"

void UGP_MatchViewModel::SetMatchTimeRemaining(float Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(MatchTimeRemaining, Value);
}

void UGP_MatchViewModel::SetMatchStateTag(FGameplayTag Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(MatchStateTag, Value);
}

void UGP_MatchViewModel::SetFerroniteThreatValue(float Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(FerroniteThreatValue, Value);
}

void UGP_MatchViewModel::SetFerroniteThreatNormalized(float Value)
{
	const float Clamped = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(FerroniteThreatNormalized, Clamped);
}

void UGP_MatchViewModel::SetWinnerTeamId(int32 Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(WinnerTeamId, Value);
}

void UGP_MatchViewModel::SetWinReasonTag(FGameplayTag Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(WinReasonTag, Value);
}

void UGP_MatchViewModel::SetMatchDuration(float Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(MatchDuration, Value);
}

void UGP_MatchViewModel::SetMatchFinished(bool bValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bMatchFinished, bValue);
}
