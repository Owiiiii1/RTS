// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AttributeSets/GPAttributeMacros.h"
#include "GPUnitAttributeSet.generated.h"

/**
 * Unit/building-scoped GAS attributes (survivability, combat stats).
 * Carried Ferronite lives on UGP_CargoComponent (GP-S25), not on this AttributeSet.
 * Defaults stay 0; initialization comes from external Init GEs / DataAssets later.
 */
UCLASS()
class GPGASRUNTIME_API UGP_UnitAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGP_UnitAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Unit", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UGP_UnitAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Unit", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UGP_UnitAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Unit", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UGP_UnitAttributeSet, Armor)

	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Unit", ReplicatedUsing = OnRep_DamageResistance)
	FGameplayAttributeData DamageResistance;
	ATTRIBUTE_ACCESSORS(UGP_UnitAttributeSet, DamageResistance)

	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Unit", ReplicatedUsing = OnRep_AttackCooldown)
	FGameplayAttributeData AttackCooldown;
	ATTRIBUTE_ACCESSORS(UGP_UnitAttributeSet, AttackCooldown)

	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Unit", ReplicatedUsing = OnRep_Damage)
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UGP_UnitAttributeSet, Damage)

	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Unit", ReplicatedUsing = OnRep_AttackRange)
	FGameplayAttributeData AttackRange;
	ATTRIBUTE_ACCESSORS(UGP_UnitAttributeSet, AttackRange)

	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Unit", ReplicatedUsing = OnRep_AttackSpeed)
	FGameplayAttributeData AttackSpeed;
	ATTRIBUTE_ACCESSORS(UGP_UnitAttributeSet, AttackSpeed)

	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Unit", ReplicatedUsing = OnRep_MoveSpeed)
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UGP_UnitAttributeSet, MoveSpeed)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldArmor);

	UFUNCTION()
	void OnRep_DamageResistance(const FGameplayAttributeData& OldDamageResistance);

	UFUNCTION()
	void OnRep_AttackCooldown(const FGameplayAttributeData& OldAttackCooldown);

	UFUNCTION()
	void OnRep_Damage(const FGameplayAttributeData& OldDamage);

	UFUNCTION()
	void OnRep_AttackRange(const FGameplayAttributeData& OldAttackRange);

	UFUNCTION()
	void OnRep_AttackSpeed(const FGameplayAttributeData& OldAttackSpeed);

	UFUNCTION()
	void OnRep_MoveSpeed(const FGameplayAttributeData& OldMoveSpeed);

private:
	/** Captured in PreGameplayEffectExecute; Health is already modified by PostGameplayEffectExecute. */
	float HealthBeforeGameplayEffect = 0.0f;
	bool bHasHealthBeforeGameplayEffect = false;
};
