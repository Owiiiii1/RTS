// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPVisualProfileSeedCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Visual/GPPrimitiveVisualProfile.h"
#include "Visual/GPPrimitiveVisualTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPVisualProfileSeed, Log, All);

namespace GPVisualProfileSeedPrivate
{
	static constexpr const TCHAR* InfantryPackageName =
		TEXT("/Game/GrimProtocol/VisualProfiles/DA_Visual_InfantryMelee");
	static constexpr const TCHAR* OrePackageName =
		TEXT("/Game/GrimProtocol/VisualProfiles/DA_Visual_Ore");

	static bool SaveAssetPackage(UPackage* Package, UObject* Asset)
	{
		if (Package == nullptr || Asset == nullptr)
		{
			return false;
		}

		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Asset);

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GError;
		SaveArgs.bForceByteSwapping = false;
		SaveArgs.bWarnOfLongFilename = true;
		SaveArgs.SaveFlags = SAVE_None;

		const bool bSaved = UPackage::SavePackage(Package, Asset, *PackageFilename, SaveArgs);
		UE_LOG(LogGPVisualProfileSeed, Log,
			TEXT("SavePackage Asset=%s Path=%s Result=%s"),
			*Asset->GetName(),
			*PackageFilename,
			bSaved ? TEXT("OK") : TEXT("FAIL"));
		return bSaved;
	}

	static UGP_PrimitiveVisualProfile* CreateOrUpdateProfile(
		const TCHAR* PackageName,
		const FName AssetName,
		const FName ProfileId,
		const FText& DisplayName,
		EGP_PrimitiveVisualProfileCategory Category,
		const TArray<FGP_PrimitiveVisualPart>& Parts)
	{
		UPackage* Package = CreatePackage(PackageName);
		if (Package == nullptr)
		{
			UE_LOG(LogGPVisualProfileSeed, Error, TEXT("CreatePackage failed: %s"), PackageName);
			return nullptr;
		}

		Package->FullyLoad();

		UGP_PrimitiveVisualProfile* Profile = FindObject<UGP_PrimitiveVisualProfile>(Package, *AssetName.ToString());
		if (Profile == nullptr)
		{
			Profile = NewObject<UGP_PrimitiveVisualProfile>(
				Package,
				AssetName,
				RF_Public | RF_Standalone | RF_Transactional);
		}

		if (Profile == nullptr)
		{
			UE_LOG(LogGPVisualProfileSeed, Error, TEXT("NewObject failed: %s"), *AssetName.ToString());
			return nullptr;
		}

		Profile->ProfileId = ProfileId;
		Profile->DisplayName = DisplayName;
		Profile->Category = Category;
		Profile->ProfileVersion = 1;
		Profile->Parts = Parts;

		TArray<FString> Errors;
		if (!Profile->ValidateProfile(Errors))
		{
			for (const FString& Error : Errors)
			{
				UE_LOG(LogGPVisualProfileSeed, Error, TEXT("Validation %s: %s"), *AssetName.ToString(), *Error);
			}
			return nullptr;
		}

		if (!SaveAssetPackage(Package, Profile))
		{
			return nullptr;
		}

		return Profile;
	}
}

UGPVisualProfileSeedCommandlet::UGPVisualProfileSeedCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UGPVisualProfileSeedCommandlet::Main(const FString& Params)
{
	(void)Params;
	UE_LOG(LogGPVisualProfileSeed, Log, TEXT("GPVisualProfileSeedCommandlet starting"));

	const FGP_PrimitiveVisualDefinition InfantryNative =
		GPPrimitiveVisualDefaults::MakeInfantryMeleeDefinition();
	UGP_PrimitiveVisualProfile* Infantry = GPVisualProfileSeedPrivate::CreateOrUpdateProfile(
		GPVisualProfileSeedPrivate::InfantryPackageName,
		TEXT("DA_Visual_InfantryMelee"),
		TEXT("InfantryMelee"),
		FText::FromString(TEXT("Infantry Melee")),
		EGP_PrimitiveVisualProfileCategory::Unit,
		InfantryNative.Parts);

	const FGP_PrimitiveVisualDefinition OreNative =
		GPPrimitiveVisualDefaults::MakeOreNodeDefinition();
	UGP_PrimitiveVisualProfile* Ore = GPVisualProfileSeedPrivate::CreateOrUpdateProfile(
		GPVisualProfileSeedPrivate::OrePackageName,
		TEXT("DA_Visual_Ore"),
		TEXT("Ore"),
		FText::FromString(TEXT("Ore")),
		EGP_PrimitiveVisualProfileCategory::Resource,
		OreNative.Parts);

	if (Infantry == nullptr || Ore == nullptr)
	{
		UE_LOG(LogGPVisualProfileSeed, Error, TEXT("Seed failed Infantry=%s Ore=%s"),
			Infantry != nullptr ? TEXT("OK") : TEXT("FAIL"),
			Ore != nullptr ? TEXT("OK") : TEXT("FAIL"));
		return 1;
	}

	UE_LOG(LogGPVisualProfileSeed, Log,
		TEXT("Seed OK: %s Parts=%d | %s Parts=%d"),
		*Infantry->GetPathName(),
		Infantry->Parts.Num(),
		*Ore->GetPathName(),
		Ore->Parts.Num());
	return 0;
}
