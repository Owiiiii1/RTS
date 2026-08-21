// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Widgets/GPHUDRootWidget.h"
#include "GPUIPresentationSettings.generated.h"

/**
 * Project Settings → Game → GP UI Presentation.
 * Config=Game → GP/Config/DefaultGame.ini (operator-authored; this slice does not write Config).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GP UI Presentation"))
class GPUIRUNTIME_API UGP_UIPresentationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGP_UIPresentationSettings();

	virtual FName GetCategoryName() const override;

	static const UGP_UIPresentationSettings* Get();

	/**
	 * Authored production HUD root. Assign WBP_GP_HUD (or another UGP_HUDRootWidget subclass)
	 * in the editor. Unconfigured → runtime bootstrap is a safe no-op and TEMP HUD remains.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Production HUD",
		meta = (AllowAbstract = "false"))
	TSoftClassPtr<UGP_HUDRootWidget> ProductionHUDWidgetClass;
};
