// Copyright Epic Games, Inc. All Rights Reserved.

#include "GPRuntime.h"
#include "Modules/ModuleManager.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPOrbitalUnitDropCatalog.h"
#include "Orbital/GPWallPackageCatalog.h"
#include "Units/GPUnitDefinitionCatalog.h"

#define LOCTEXT_NAMESPACE "FGPRuntimeModule"

void FGPRuntimeModule::StartupModule()
{
	UGP_UnitDefinitionCatalog::BindEngineLifecycle();
	UGP_BuildingDropCatalog::BindEngineLifecycle();
	UGP_OrbitalUnitDropCatalog::BindEngineLifecycle();
	UGP_WallPackageCatalog::BindEngineLifecycle();
}

void FGPRuntimeModule::ShutdownModule()
{
	UGP_WallPackageCatalog::UnbindEngineLifecycle();
	UGP_WallPackageCatalog::ShutdownCatalog();
	UGP_OrbitalUnitDropCatalog::UnbindEngineLifecycle();
	UGP_OrbitalUnitDropCatalog::ShutdownCatalog();
	UGP_BuildingDropCatalog::UnbindEngineLifecycle();
	UGP_BuildingDropCatalog::ShutdownCatalog();
	UGP_UnitDefinitionCatalog::UnbindEngineLifecycle();
	UGP_UnitDefinitionCatalog::ShutdownCatalog();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGPRuntimeModule, GPRuntime)
