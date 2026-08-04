// Copyright Epic Games, Inc. All Rights Reserved.

#include "GPEditorModule.h"

#include "PrototypeArena/GPPrototypeArenaGenerator.h"
#include "ToolMenus.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPEditor, Log, All);

#define LOCTEXT_NAMESPACE "GPEditor"

void FGPEditorModule::StartupModule()
{
	RegisterConsoleCommands();

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGPEditorModule::RegisterMenus));
}

void FGPEditorModule::ShutdownModule()
{
	UnregisterConsoleCommands();
	UnregisterMenus();
}

void FGPEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	if (Menu == nullptr)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection("GPGrimProtocol");
	Section.Label = LOCTEXT("GPToolsSection", "Grim Protocol");

	Section.AddMenuEntry(
		"GPGeneratePrototypeArena",
		LOCTEXT("GPGeneratePrototypeArenaLabel", "Generate Prototype Arena"),
		LOCTEXT("GPGeneratePrototypeArenaTooltip", "One-shot create /Game/GrimProtocol/Maps/L_PrototypeArena (abort if exists)."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			const FGPPrototypeArenaGenerateResult Result = FGPPrototypeArenaGenerator::Generate();
			if (!Result.bSuccess)
			{
				UE_LOG(LogGPEditor, Warning,
					TEXT("GP menu GeneratePrototypeArena failed Stage=%s ExistingMapAbort=%s Msg=%s"),
					*Result.FailureStage,
					Result.bExistingMapAbort ? TEXT("true") : TEXT("false"),
					*Result.Message);
			}
		})));
}

void FGPEditorModule::UnregisterMenus()
{
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::UnregisterOwner(this);
	}
}

void FGPEditorModule::RegisterConsoleCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();

	ConsoleCommands.Add(Console.RegisterConsoleCommand(
		TEXT("gp.Editor.GeneratePrototypeArena"),
		TEXT("One-shot generate L_PrototypeArena infrastructure map (abort if exists). Editor only; refuses PIE/SIE."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const FGPPrototypeArenaGenerateResult Result = FGPPrototypeArenaGenerator::Generate();
			if (Result.bSuccess)
			{
				UE_LOG(LogGPEditor, Log, TEXT("GP Editor.GeneratePrototypeArena SUCCESS: %s"), *Result.Message);
			}
			else
			{
				UE_LOG(LogGPEditor, Warning,
					TEXT("GP Editor.GeneratePrototypeArena FAILED Stage=%s ExistingMapAbort=%s Msg=%s"),
					*Result.FailureStage,
					Result.bExistingMapAbort ? TEXT("true") : TEXT("false"),
					*Result.Message);
			}
		}),
		ECVF_Default));

	ConsoleCommands.Add(Console.RegisterConsoleCommand(
		TEXT("gp.Editor.InspectPrototypeArena"),
		TEXT("Inspect L_PrototypeArena infrastructure counts (read-only)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const FGPPrototypeArenaInspectResult Result = FGPPrototypeArenaGenerator::Inspect();
			FGPPrototypeArenaGenerator::LogInspectResult(Result);
		}),
		ECVF_Default));
}

void FGPEditorModule::UnregisterConsoleCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();
	for (IConsoleObject* Obj : ConsoleCommands)
	{
		if (Obj != nullptr)
		{
			Console.UnregisterConsoleObject(Obj);
		}
	}
	ConsoleCommands.Reset();
}

IMPLEMENT_MODULE(FGPEditorModule, GPEditor)

#undef LOCTEXT_NAMESPACE
