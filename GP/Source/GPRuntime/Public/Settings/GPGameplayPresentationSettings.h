// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GPGameplayPresentationSettings.generated.h"

USTRUCT(BlueprintType)
struct FGP_TeamPresentationStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "GP|Team")
	int32 TeamId = 1;

	UPROPERTY(EditAnywhere, Config, Category = "GP|Team")
	FLinearColor TeamColor = FLinearColor::White;
};

/**
 * Project Settings → Game → GP Gameplay Presentation (GP-S29R).
 * Central TeamId → color mapping and shared presentation tunables.
 * Config=Game → GP/Config/DefaultGame.ini
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GP Gameplay Presentation"))
class GPRUNTIME_API UGP_GameplayPresentationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGP_GameplayPresentationSettings();

	virtual FName GetCategoryName() const override;

	static const UGP_GameplayPresentationSettings* Get();

	/** Resolves configured team color; unknown/unassigned → NeutralTeamColor. */
	UFUNCTION(BlueprintPure, Category = "GP|Presentation")
	FLinearColor GetTeamColor(int32 TeamId) const;

	UPROPERTY(Config, EditAnywhere, Category = "Team Colors")
	TArray<FGP_TeamPresentationStyle> TeamStyles;

	/** Fallback for TeamId < 1 or unlisted teams. */
	UPROPERTY(Config, EditAnywhere, Category = "Team Colors")
	FLinearColor NeutralTeamColor = FLinearColor::White;

	/** Material vector parameter preferred when applying team tint. */
	UPROPERTY(Config, EditAnywhere, Category = "Team Colors")
	FName TeamColorParameterName = TEXT("TeamColor");

	UPROPERTY(Config, EditAnywhere, Category = "Health Bar", meta = (ClampMin = "32.0", ClampMax = "512.0"))
	float HealthBarDrawSizeX = 120.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Health Bar", meta = (ClampMin = "4.0", ClampMax = "64.0"))
	float HealthBarDrawSizeY = 14.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Health Bar")
	FVector HealthBarWorldOffset = FVector(0.0f, 0.0f, 140.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Health Bar")
	FLinearColor HealthBarFillColor = FLinearColor(0.15f, 0.85f, 0.25f, 1.0f);

	UPROPERTY(Config, EditAnywhere, Category = "Health Bar")
	FLinearColor HealthBarFrameColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.85f);

	UPROPERTY(Config, EditAnywhere, Category = "Health Bar")
	FLinearColor HealthBarBackgroundColor = FLinearColor(0.12f, 0.12f, 0.12f, 0.75f);
};
