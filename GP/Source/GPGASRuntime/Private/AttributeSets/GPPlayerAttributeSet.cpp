// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Net/UnrealNetwork.h"

UGP_PlayerAttributeSet::UGP_PlayerAttributeSet()
{
}

void UGP_PlayerAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetCurrentUnitsAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void UGP_PlayerAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UGP_PlayerAttributeSet, OrbitalFerronite, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_PlayerAttributeSet, FerroniteScore, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_PlayerAttributeSet, MaxUnits, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_PlayerAttributeSet, CurrentUnits, COND_OwnerOnly, REPNOTIFY_Always);
}

void UGP_PlayerAttributeSet::OnRep_OrbitalFerronite(const FGameplayAttributeData& OldOrbitalFerronite)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_PlayerAttributeSet, OrbitalFerronite, OldOrbitalFerronite);
}

void UGP_PlayerAttributeSet::OnRep_FerroniteScore(const FGameplayAttributeData& OldFerroniteScore)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_PlayerAttributeSet, FerroniteScore, OldFerroniteScore);
}

void UGP_PlayerAttributeSet::OnRep_MaxUnits(const FGameplayAttributeData& OldMaxUnits)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_PlayerAttributeSet, MaxUnits, OldMaxUnits);
}

void UGP_PlayerAttributeSet::OnRep_CurrentUnits(const FGameplayAttributeData& OldCurrentUnits)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_PlayerAttributeSet, CurrentUnits, OldCurrentUnits);
}
