// Copyright Epic Games, Inc. All Rights Reserved.

#include "GPRuntime.h"
#include "Modules/ModuleManager.h"
#include "Orbital/GPBuildingDropCatalog.h"

#define LOCTEXT_NAMESPACE "FGPRuntimeModule"

void FGPRuntimeModule::StartupModule()
{
}

void FGPRuntimeModule::ShutdownModule()
{
	UGP_BuildingDropCatalog::ShutdownCatalog();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGPRuntimeModule, GPRuntime)
