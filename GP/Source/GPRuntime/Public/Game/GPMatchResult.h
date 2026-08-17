// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GPMatchResult.generated.h"

/** Per-team score snapshot captured at match finish (GP-S34W). */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_MatchTeamScore
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	int32 TeamId = -1;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	float FerroniteScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	float OrbitalFerronite = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	int32 CurrentUnits = 0;
};

/**
 * Compact replicated match outcome. TArray instead of TMap for UE replication.
 * WinnerTeamId -1 means no winner (invalid match config only — MVP has no Draw).
 */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_MatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	int32 WinnerTeamId = -1;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	FGameplayTag WinnerReason;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	float MatchDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Match")
	TArray<FGP_MatchTeamScore> FinalScores;

	bool HasWinner() const { return WinnerTeamId >= 1 && WinnerReason.IsValid(); }
};
