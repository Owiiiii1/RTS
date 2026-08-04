// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrototypeArena/GPPrototypeArenaGenerateCommandlet.h"

#include "FileHelpers.h"
#include "Misc/Parse.h"
#include "PrototypeArena/GPPrototypeArenaGenerator.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPPrototypeArenaCommandlet, Log, All);

UGPPrototypeArenaGenerateCommandlet::UGPPrototypeArenaGenerateCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UGPPrototypeArenaGenerateCommandlet::Main(const FString& Params)
{
	UE_LOG(LogGPPrototypeArenaCommandlet, Log, TEXT("GPPrototypeArenaGenerateCommandlet starting Params=%s"), *Params);

	if (FParse::Param(*Params, TEXT("InspectOnly")))
	{
		if (FGPPrototypeArenaGenerator::DoesMapPackageExist())
		{
			UEditorLoadingAndSavingUtils::LoadMap(FGPPrototypeArenaGenerator::MapPackagePath);
		}
		const FGPPrototypeArenaInspectResult InspectOnlyResult = FGPPrototypeArenaGenerator::Inspect();
		FGPPrototypeArenaGenerator::LogInspectResult(InspectOnlyResult);
		return InspectOnlyResult.bReadyForPopulation ? 0 : 3;
	}

	const FGPPrototypeArenaGenerateResult GenerateResult = FGPPrototypeArenaGenerator::Generate();
	if (!GenerateResult.bSuccess)
	{
		UE_LOG(LogGPPrototypeArenaCommandlet, Error,
			TEXT("Generate failed Stage=%s ExistingMapAbort=%s Msg=%s"),
			*GenerateResult.FailureStage,
			GenerateResult.bExistingMapAbort ? TEXT("true") : TEXT("false"),
			*GenerateResult.Message);
		return GenerateResult.bExistingMapAbort ? 2 : 1;
	}

	UE_LOG(LogGPPrototypeArenaCommandlet, Log, TEXT("Generate OK: %s"), *GenerateResult.Message);

	const FGPPrototypeArenaInspectResult InspectResult = FGPPrototypeArenaGenerator::Inspect();
	FGPPrototypeArenaGenerator::LogInspectResult(InspectResult);

	if (!InspectResult.bReadyForPopulation)
	{
		UE_LOG(LogGPPrototypeArenaCommandlet, Warning,
			TEXT("Inspect ReadyForPopulation=false (map may still be usable; check counts/nav)"));
	}

	return 0;
}
