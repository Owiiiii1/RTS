// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "GPMatchViewModel.generated.h"

/** Local presentation projection of factual AGP_GameState match data. */
UCLASS(BlueprintType)
class GPUIRUNTIME_API UGP_MatchViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Match")
	float MatchTimeRemaining = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Match")
	FGameplayTag MatchStateTag;

	/** Per-team Planetary Ferronite threat for the owning local player's team. */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Match")
	float FerroniteThreatValue = 0.0f;

	/**
	 * Presentation-only Threat bar fill in [0,1].
	 * Derived from FerroniteThreatValue / (MainBase storage capacity × ThreatPerStoredUnit).
	 * Not a gameplay threshold.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Match")
	float FerroniteThreatNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Match")
	int32 WinnerTeamId = -1;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Match")
	FGameplayTag WinReasonTag;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Match")
	float MatchDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Match")
	bool bMatchFinished = false;

	/** Adapter-facing presentation setters. Widgets remain read-only consumers. */
	void SetMatchTimeRemaining(float Value);
	void SetMatchStateTag(FGameplayTag Value);
	void SetFerroniteThreatValue(float Value);
	void SetFerroniteThreatNormalized(float Value);
	void SetWinnerTeamId(int32 Value);
	void SetWinReasonTag(FGameplayTag Value);
	void SetMatchDuration(float Value);
	void SetMatchFinished(bool bValue);
};
