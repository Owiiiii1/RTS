// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GPResourceViewModel.generated.h"

/** Local, read-only presentation projection of player resource attributes. */
UCLASS(BlueprintType)
class GPUIRUNTIME_API UGP_ResourceViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Resources")
	float OrbitalFerronite = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Resources")
	float FerroniteScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Resources")
	float CurrentUnits = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Resources")
	float MaxUnits = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Resources")
	float OpponentFerroniteScore = 0.0f;

	/** Adapter-facing presentation setters. Widgets must not call these as gameplay mutations. */
	void SetOrbitalFerronite(float Value);
	void SetFerroniteScore(float Value);
	void SetCurrentUnits(float Value);
	void SetMaxUnits(float Value);
	void SetOpponentFerroniteScore(float Value);
};
