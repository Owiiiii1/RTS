// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/Texture2D.h"
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

	/**
	 * Static authored minimap background. Same world XY bounds as UGP_MinimapPresenter / FoW grid.
	 * Authored image: world +Y (NormalizedY = 1) at the top of the texture. Async load only.
	 * Empty / loading → solid dark fallback. Not SceneCapture / render-target terrain.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Minimap")
	TSoftObjectPtr<UTexture2D> MinimapBackgroundTexture;

	/** Bounded FoW overlay downsample. Presentation-only; not a new gameplay grid. */
	UPROPERTY(Config, EditAnywhere, Category = "Minimap",
		meta = (ClampMin = "32", ClampMax = "256"))
	int32 MinimapFoWPresentationResolution = 128;
};
