// Copyright Epic Games, Inc. All Rights Reserved.

#include "GPGASRuntime.h"
#include "Modules/ModuleManager.h"
#include "Tags/GPGameplayTags.h"

#define LOCTEXT_NAMESPACE "FGPGASRuntimeModule"

void FGPGASRuntimeModule::StartupModule()
{
	// Register native GP.* tags before gameplay actors access FGPGameplayTags::Get().
	FGPGameplayTags::InitializeNativeTags();
}

void FGPGASRuntimeModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGPGASRuntimeModule, GPGASRuntime)
