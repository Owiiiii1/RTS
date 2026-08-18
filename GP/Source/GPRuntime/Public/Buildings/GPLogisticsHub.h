// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/GPBuildingBase.h"
#include "GameplayEffectTypes.h"
#include "GPLogisticsHub.generated.h"

class AGP_PlayerState;
class UCapsuleComponent;
class USceneComponent;

/**
 * Native Logistics Hub (GP-S32R identity, GP-S33C +5 MaxUnits).
 * Deployed living Hub applies infinite UGP_GE_UnitCap_Plus5 to the owning PlayerState.
 * Container-cap bonus remains deferred (N TBD).
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_LogisticsHub : public AGP_BuildingBase
{
	GENERATED_BODY()

public:
	AGP_LogisticsHub();

	UFUNCTION(BlueprintPure, Category = "GP|Building")
	UCapsuleComponent* GetCapsuleComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|Building|Presentation")
	USceneComponent* GetPresentationRoot() const;

	/** Authority catch-up for editor-placed / late PlayerState team assign. */
	void TryApplyUnitCapBonus();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyTeamIdChanged(int32 OldTeamId, int32 NewTeamId) override;
	virtual void NotifyAuthorityDeath() override;
	virtual void NotifyBuildingDefinitionReady() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PresentationRoot;

private:
	void RemoveUnitCapBonus();
	AGP_PlayerState* ResolveOwningPlayerStateForHubBonus() const;
	int32 ResolveUnitCapBonusMagnitude() const;

	FActiveGameplayEffectHandle UnitCapBonusHandle;
	bool bUnitCapBonusApplied = false;
	TWeakObjectPtr<AGP_PlayerState> UnitCapBonusOwnerWeak;
};
