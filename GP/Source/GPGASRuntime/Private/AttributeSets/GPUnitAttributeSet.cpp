// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttributeSets/GPUnitAttributeSet.h"
#include "Net/UnrealNetwork.h"

UGP_UnitAttributeSet::UGP_UnitAttributeSet()
{
}

void UGP_UnitAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// TDD/13: UGP_UnitAttributeSet.* = Mixed (per ASC mode) / standard GAS replication → COND_None + REPNOTIFY_Always.
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, DamageResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, AttackCooldown, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, Damage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, AttackRange, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, AttackSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, MoveSpeed, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UGP_UnitAttributeSet, CarriedFerronite, COND_None, REPNOTIFY_Always);
}

void UGP_UnitAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 0.0f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetCarriedFerroniteAttribute())
	{
		// Upper clamp deferred until WorkerCarryCapacity / cargo slice (no MaxCargo attribute in MVP AttributeSet).
		NewValue = FMath::Max(NewValue, 0.0f);
	}
}

void UGP_UnitAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, Health, OldHealth);
}

void UGP_UnitAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, MaxHealth, OldMaxHealth);
}

void UGP_UnitAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldArmor)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, Armor, OldArmor);
}

void UGP_UnitAttributeSet::OnRep_DamageResistance(const FGameplayAttributeData& OldDamageResistance)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, DamageResistance, OldDamageResistance);
}

void UGP_UnitAttributeSet::OnRep_AttackCooldown(const FGameplayAttributeData& OldAttackCooldown)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, AttackCooldown, OldAttackCooldown);
}

void UGP_UnitAttributeSet::OnRep_Damage(const FGameplayAttributeData& OldDamage)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, Damage, OldDamage);
}

void UGP_UnitAttributeSet::OnRep_AttackRange(const FGameplayAttributeData& OldAttackRange)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, AttackRange, OldAttackRange);
}

void UGP_UnitAttributeSet::OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, AttackSpeed, OldAttackSpeed);
}

void UGP_UnitAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, MoveSpeed, OldMoveSpeed);
}

void UGP_UnitAttributeSet::OnRep_CarriedFerronite(const FGameplayAttributeData& OldCarriedFerronite)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UGP_UnitAttributeSet, CarriedFerronite, OldCarriedFerronite);
}
