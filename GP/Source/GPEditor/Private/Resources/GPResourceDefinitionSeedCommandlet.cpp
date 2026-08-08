// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPResourceDefinitionSeedCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Resources/GPResourceDefinition.h"
#include "Tags/GPGameplayTags.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPResourceDefinitionSeed, Log, All);

namespace GPResourceDefinitionSeedPrivate
{
	static constexpr const TCHAR* PackagePath = TEXT("/Game/GrimProtocol/DataAssets/Resources/DA_GP_Resource_Ferronite");
	static constexpr const TCHAR* AssetPath =
		TEXT("/Game/GrimProtocol/DataAssets/Resources/DA_GP_Resource_Ferronite.DA_GP_Resource_Ferronite");
	static constexpr const TCHAR* AssetName = TEXT("DA_GP_Resource_Ferronite");

	static void ApplyFerronitePrototypeDefaults(UGP_ResourceDefinition* Definition)
	{
		check(Definition != nullptr);

		Definition->ResourceType = EGP_ResourceType::Ore;
		Definition->DisplayName = FText::FromString(TEXT("Ferronite"));
		Definition->Description = FText::FromString(
			TEXT("Primary raw resource. Internal EGP_ResourceType name is Ore; canonical identity is Ferronite. Prototype defaults — not final balance."));
		Definition->ResourceGameplayTag = FGPGameplayTags::Get().Resource_Type_Ferronite;
		if (!Definition->ResourceGameplayTag.IsValid())
		{
			Definition->ResourceGameplayTag = FGameplayTag::RequestGameplayTag(FName(TEXT("GP.Resource.Type.Ferronite")), false);
		}
		Definition->AmountPerMiningCycle = 10.0f;
		Definition->MiningCycleDurationSeconds = 1.0f;
		Definition->InteractionRangeCm = 200.0f;
		Definition->ScoreConversionRate = 1.0f;
		Definition->OrbitalConversionRate = 1.0f;
		Definition->ThreatPerStoredUnit = 0.5f;
		Definition->Tint = FLinearColor(0.15f, 0.75f, 0.85f, 1.0f);
		Definition->Icon.Reset();
	}

	static bool SaveDefinition(UGP_ResourceDefinition* Definition)
	{
		UPackage* Package = Definition->GetOutermost();
		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Definition);

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GError;
		SaveArgs.SaveFlags = SAVE_None;

		const bool bSaved = UPackage::SavePackage(Package, Definition, *PackageFilename, SaveArgs);
		UE_LOG(LogGPResourceDefinitionSeed, Log, TEXT("SavePackage %s Result=%s"),
			*Definition->GetPathName(),
			bSaved ? TEXT("OK") : TEXT("FAIL"));
		return bSaved;
	}

	static bool VerifyFerronite()
	{
		UGP_ResourceDefinition* Definition = LoadObject<UGP_ResourceDefinition>(nullptr, AssetPath);
		if (Definition == nullptr)
		{
			UE_LOG(LogGPResourceDefinitionSeed, Error, TEXT("Verify: missing asset %s"), AssetPath);
			return false;
		}

		TArray<FText> Errors;
		TArray<FText> Warnings;
		const bool bValid = Definition->ValidateDefinition(Errors, Warnings);
		for (const FText& Error : Errors)
		{
			UE_LOG(LogGPResourceDefinitionSeed, Error, TEXT("Verify Error: %s"), *Error.ToString());
		}
		for (const FText& Warning : Warnings)
		{
			UE_LOG(LogGPResourceDefinitionSeed, Warning, TEXT("Verify Warning: %s"), *Warning.ToString());
		}

		bool bAssetManagerSees = false;
		if (UAssetManager::IsInitialized())
		{
			const FSoftObjectPath RegisteredPath =
				UAssetManager::Get().GetPrimaryAssetPath(Definition->GetPrimaryAssetId());
			bAssetManagerSees = RegisteredPath.IsValid();
		}

		UE_LOG(LogGPResourceDefinitionSeed, Log,
			TEXT("Verify Ferronite Path=%s PrimaryAssetId=%s ResourceType=%s Tag=%s AmountPerMiningCycle=%.3f MiningCycleDurationSeconds=%.3f EffectiveMineRatePerWorker=%.3f InteractionRangeCm=%.1f Valid=%s AssetManagerSees=%s"),
			*Definition->GetPathName(),
			*Definition->GetPrimaryAssetId().ToString(),
			GPResourceTypePrivate::ToString(Definition->ResourceType),
			*Definition->ResourceGameplayTag.ToString(),
			Definition->AmountPerMiningCycle,
			Definition->MiningCycleDurationSeconds,
			Definition->GetEffectiveMineRatePerWorker(),
			Definition->InteractionRangeCm,
			bValid ? TEXT("true") : TEXT("false"),
			bAssetManagerSees ? TEXT("true") : TEXT("false"));

		return bValid
			&& Definition->ResourceType == EGP_ResourceType::Ore
			&& Definition->ResourceGameplayTag.IsValid();
	}

	static bool SeedFerronite()
	{
		UGP_ResourceDefinition* Existing = LoadObject<UGP_ResourceDefinition>(nullptr, AssetPath);
		if (Existing != nullptr)
		{
			UE_LOG(LogGPResourceDefinitionSeed, Log, TEXT("Updating existing %s"), AssetPath);
			ApplyFerronitePrototypeDefaults(Existing);
			Existing->MarkPackageDirty();
			return SaveDefinition(Existing) && VerifyFerronite();
		}

		UPackage* Package = CreatePackage(PackagePath);
		Package->FullyLoad();

		UGP_ResourceDefinition* Definition = NewObject<UGP_ResourceDefinition>(
			Package,
			FName(AssetName),
			RF_Public | RF_Standalone | RF_Transactional);
		if (Definition == nullptr)
		{
			UE_LOG(LogGPResourceDefinitionSeed, Error, TEXT("NewObject failed for %s"), AssetName);
			return false;
		}

		ApplyFerronitePrototypeDefaults(Definition);
		return SaveDefinition(Definition) && VerifyFerronite();
	}
}

UGPResourceDefinitionSeedCommandlet::UGPResourceDefinitionSeedCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UGPResourceDefinitionSeedCommandlet::Main(const FString& Params)
{
	UE_LOG(LogGPResourceDefinitionSeed, Log, TEXT("GPResourceDefinitionSeedCommandlet Params=%s"), *Params);

	if (FParse::Param(*Params, TEXT("VerifyOnly")))
	{
		return GPResourceDefinitionSeedPrivate::VerifyFerronite() ? 0 : 2;
	}

	return GPResourceDefinitionSeedPrivate::SeedFerronite() ? 0 : 1;
}
