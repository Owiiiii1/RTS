// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/GPHUDRootWidget.h"
#include "GPHUDRootWidgetContractStub.generated.h"

/** Concrete UGP_HUDRootWidget used only by the HUD bootstrap contract. Not for authoring. */
UCLASS(NotBlueprintable, HideDropdown)
class UGP_HUDRootWidgetContractStub : public UGP_HUDRootWidget
{
	GENERATED_BODY()
};
