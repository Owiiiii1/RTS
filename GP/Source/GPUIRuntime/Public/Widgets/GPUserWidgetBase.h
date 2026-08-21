// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "GPUserWidgetBase.generated.h"

/** Minimal project-owned CommonUI base for non-activatable production widgets. */
UCLASS(Abstract, Blueprintable)
class GPUIRUNTIME_API UGP_UserWidgetBase : public UCommonUserWidget
{
	GENERATED_BODY()
};
