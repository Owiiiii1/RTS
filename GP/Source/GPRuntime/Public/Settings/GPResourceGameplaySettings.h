// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GPResourceGameplaySettings.generated.h"

/**
 * Project Settings → Game → GP Resource Gameplay (GP-S28P2).
 * Config=Game → GP/Config/DefaultGame.ini
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GP Resource Gameplay"))
class GPRUNTIME_API UGP_ResourceGameplaySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGP_ResourceGameplaySettings();

	virtual FName GetCategoryName() const override;

	static const UGP_ResourceGameplaySettings* Get();

	/** Auto-search radius for alternative ResourceNodes (SearchCenter metric). */
	UPROPERTY(Config, EditAnywhere, Category = "Search", meta = (ClampMin = "1.0"))
	float ResourceSearchRadiusCm = 3000.0f;

	/** Reject auto-search candidates whose approach path exceeds this length. */
	UPROPERTY(Config, EditAnywhere, Category = "Search", meta = (ClampMin = "1.0"))
	float MaxResourcePathLengthCm = 6000.0f;

	/** WaitingForResource safety retry interval (event wake remains primary). */
	UPROPERTY(Config, EditAnywhere, Category = "Waiting", meta = (ClampMin = "0.1"))
	float WaitingForResourceRetrySeconds = 3.0f;

	/** Delay before Destroy after depletion transition (0 → next tick). */
	UPROPERTY(Config, EditAnywhere, Category = "Depletion", meta = (ClampMin = "0.0"))
	float DepletionDestroyDelaySeconds = 0.25f;

	/** Inward margin beyond AcceptanceRadius for Mine approach geometry. */
	UPROPERTY(Config, EditAnywhere, Category = "Approach", meta = (ClampMin = "0.0"))
	float ResourceApproachSafetyMarginCm = 25.0f;

	/** Cardinal + diagonal approach sample count around a ResourceNode. */
	UPROPERTY(Config, EditAnywhere, Category = "Approach", meta = (ClampMin = "4", ClampMax = "16"))
	int32 ResourceApproachDirectionCount = 8;
};
