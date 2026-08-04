// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGPEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	void UnregisterMenus();
	void RegisterConsoleCommands();
	void UnregisterConsoleCommands();

	TArray<IConsoleObject*> ConsoleCommands;
};
