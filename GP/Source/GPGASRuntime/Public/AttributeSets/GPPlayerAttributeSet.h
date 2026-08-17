// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AttributeSets/GPAttributeMacros.h"
#include "GPPlayerAttributeSet.generated.h"

/**
 * Player-scoped GAS attributes (spendable orbital currency, score, unit caps).
 * Planetary Ferronite is NOT an attribute — it lives on UGP_StorageComponent / GameState threat.
 */
UCLASS()
class GPGASRUNTIME_API UGP_PlayerAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UGP_PlayerAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** Spendable orbital currency. Replicated OwnerOnly. */
	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Player", ReplicatedUsing = OnRep_OrbitalFerronite)
	FGameplayAttributeData OrbitalFerronite;
	ATTRIBUTE_ACCESSORS(UGP_PlayerAttributeSet, OrbitalFerronite)

	/** Cumulative shipped victory score. Replicated to all (COND_None). */
	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Player", ReplicatedUsing = OnRep_FerroniteScore)
	FGameplayAttributeData FerroniteScore;
	ATTRIBUTE_ACCESSORS(UGP_PlayerAttributeSet, FerroniteScore)

	/** Unit capacity ceiling. Replicated OwnerOnly. */
	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Player", ReplicatedUsing = OnRep_MaxUnits)
	FGameplayAttributeData MaxUnits;
	ATTRIBUTE_ACCESSORS(UGP_PlayerAttributeSet, MaxUnits)

	/** Current living unit count. Replicated OwnerOnly. */
	UPROPERTY(BlueprintReadOnly, Category = "GP|Attributes|Player", ReplicatedUsing = OnRep_CurrentUnits)
	FGameplayAttributeData CurrentUnits;
	ATTRIBUTE_ACCESSORS(UGP_PlayerAttributeSet, CurrentUnits)

protected:
	UFUNCTION()
	void OnRep_OrbitalFerronite(const FGameplayAttributeData& OldOrbitalFerronite);

	UFUNCTION()
	void OnRep_FerroniteScore(const FGameplayAttributeData& OldFerroniteScore);

	UFUNCTION()
	void OnRep_MaxUnits(const FGameplayAttributeData& OldMaxUnits);

	UFUNCTION()
	void OnRep_CurrentUnits(const FGameplayAttributeData& OldCurrentUnits);
};
