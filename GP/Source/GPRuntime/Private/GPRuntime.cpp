// Copyright Epic Games, Inc. All Rights Reserved.

#include "GPRuntime.h"
#include "Modules/ModuleManager.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Units/GPUnitDefinitionCatalog.h"

#define LOCTEXT_NAMESPACE "FGPRuntimeModule"

void FGPRuntimeModule::StartupModule()
{
	UGP_UnitDefinitionCatalog::BindEngineLifecycle();
	UGP_BuildingDropCatalog::BindEngineLifecycle();
}

void FGPRuntimeModule::ShutdownModule()
{
	UGP_BuildingDropCatalog::UnbindEngineLifecycle();
	UGP_BuildingDropCatalog::ShutdownCatalog();
	UGP_UnitDefinitionCatalog::UnbindEngineLifecycle();
	UGP_UnitDefinitionCatalog::ShutdownCatalog();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGPRuntimeModule, GPRuntime)
