// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPResourceDefinition.h"

#include "GameplayTagsManager.h"
#include "Misc/DataValidation.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPResourceDefinition, Log, All);

#if !UE_BUILD_SHIPPING
#include "Engine/AssetManager.h"
#include "HAL/IConsoleManager.h"
#endif

namespace GPResourceDefinitionPrivate
{
	static constexpr const TCHAR* PrimaryType = TEXT("GPResourceDefinition");
	static constexpr const TCHAR* FerronitePath =
		TEXT("/Game/GrimProtocol/DataAssets/Resources/DA_GP_Resource_Ferronite.DA_GP_Resource_Ferronite");
	static constexpr const TCHAR* FerroniteTagName = TEXT("GP.Resource.Type.Ferronite");
}

UGP_ResourceDefinition::UGP_ResourceDefinition()
{
	ResourceType = EGP_ResourceType::Ore;
	DisplayName = NSLOCTEXT("GPResourceDefinition", "FerroniteDisplayName", "Ferronite");
	Description = NSLOCTEXT(
		"GPResourceDefinition",
		"FerroniteDescription",
		"Primary raw resource. Internal enum name is Ore; canonical identity is Ferronite.");
	ResourceGameplayTag = FGameplayTag::RequestGameplayTag(
		FName(GPResourceDefinitionPrivate::FerroniteTagName),
		false);
	AmountPerMiningCycle = 10.0f;
	MiningCycleDurationSeconds = 1.0f;
	InteractionRangeCm = 200.0f;
	MineRatePerWorker = 10.0f;
	ScoreConversionRate = 1.0f;
	ThreatPerStoredUnit = 0.5f;
	Tint = FLinearColor(0.15f, 0.75f, 0.85f, 1.0f);
}

const TCHAR* UGP_ResourceDefinition::PrimaryAssetTypeName()
{
	return GPResourceDefinitionPrivate::PrimaryType;
}

const TCHAR* UGP_ResourceDefinition::DefaultFerroniteAssetPath()
{
	return GPResourceDefinitionPrivate::FerronitePath;
}

FPrimaryAssetId UGP_ResourceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(PrimaryAssetTypeName()), GetFName());
}

float UGP_ResourceDefinition::GetEffectiveMineRatePerWorker() const
{
	if (MiningCycleDurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	return AmountPerMiningCycle / MiningCycleDurationSeconds;
}

bool UGP_ResourceDefinition::ValidateDefinition(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	if (ResourceType == EGP_ResourceType::None)
	{
		OutErrors.Add(NSLOCTEXT("GPResourceDefinition", "ErrTypeNone", "ResourceType must not be None."));
	}

	if (DisplayName.IsEmpty())
	{
		OutErrors.Add(NSLOCTEXT("GPResourceDefinition", "ErrDisplayName", "DisplayName must not be empty."));
	}

	if (!ResourceGameplayTag.IsValid())
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceDefinition",
			"ErrTagInvalid",
			"ResourceGameplayTag must be a valid gameplay tag (expected GP.Resource.Type.Ferronite)."));
	}

	if (!FMath::IsFinite(AmountPerMiningCycle) || AmountPerMiningCycle <= 0.0f)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceDefinition",
			"ErrAmount",
			"AmountPerMiningCycle must be finite and > 0."));
	}

	if (!FMath::IsFinite(MiningCycleDurationSeconds) || MiningCycleDurationSeconds <= 0.0f)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceDefinition",
			"ErrCycle",
			"MiningCycleDurationSeconds must be finite and > 0."));
	}

	if (!FMath::IsFinite(InteractionRangeCm) || InteractionRangeCm <= 0.0f)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceDefinition",
			"ErrRange",
			"InteractionRangeCm must be finite and > 0."));
	}

	if (!FMath::IsFinite(MineRatePerWorker) || MineRatePerWorker <= 0.0f)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceDefinition",
			"ErrMineRate",
			"MineRatePerWorker must be finite and > 0."));
	}

	if (!FMath::IsFinite(ScoreConversionRate) || ScoreConversionRate < 0.0f)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceDefinition",
			"ErrScoreRate",
			"ScoreConversionRate must be finite and >= 0."));
	}

	if (!FMath::IsFinite(ThreatPerStoredUnit) || ThreatPerStoredUnit < 0.0f)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceDefinition",
			"ErrThreat",
			"ThreatPerStoredUnit must be finite and >= 0."));
	}

	const float EffectiveRate = GetEffectiveMineRatePerWorker();
	if (EffectiveRate > 0.0f && MineRatePerWorker > 0.0f)
	{
		const float RelErr = FMath::Abs(EffectiveRate - MineRatePerWorker)
			/ FMath::Max(MineRatePerWorker, KINDA_SMALL_NUMBER);
		if (RelErr > 0.05f)
		{
			OutWarnings.Add(FText::Format(
				NSLOCTEXT(
					"GPResourceDefinition",
					"WarnRateMismatch",
					"MineRatePerWorker ({0}) differs from Amount/Duration ({1}) by more than 5%. Align content fields."),
				FText::AsNumber(MineRatePerWorker),
				FText::AsNumber(EffectiveRate)));
		}
	}

	if (ResourceType == EGP_ResourceType::Ore
		&& ResourceGameplayTag.IsValid()
		&& ResourceGameplayTag.GetTagName() != FName(GPResourceDefinitionPrivate::FerroniteTagName))
	{
		OutWarnings.Add(NSLOCTEXT(
			"GPResourceDefinition",
			"WarnOreTag",
			"ResourceType=Ore typically pairs with GP.Resource.Type.Ferronite for the Ferronite prototype."));
	}

	return OutErrors.Num() == 0;
}

#if WITH_EDITOR
EDataValidationResult UGP_ResourceDefinition::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const bool bOk = ValidateDefinition(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}

	if (!bOk)
	{
		return EDataValidationResult::Invalid;
	}
	return Warnings.Num() > 0 ? EDataValidationResult::Valid : EDataValidationResult::Valid;
}
#endif

#if !UE_BUILD_SHIPPING
namespace GPResourceDefinitionDebug
{
	static void Inspect(const TArray<FString>& Args)
	{
		FString Path = UGP_ResourceDefinition::DefaultFerroniteAssetPath();
		if (Args.Num() > 0 && !Args[0].IsEmpty())
		{
			Path = Args[0];
		}

		FSoftObjectPath SoftPath(Path);
		UObject* Loaded = SoftPath.TryLoad();
		UGP_ResourceDefinition* Definition = Cast<UGP_ResourceDefinition>(Loaded);
		if (Definition == nullptr)
		{
			UE_LOG(LogGPResourceDefinition, Warning,
				TEXT("GP ResourceDefinition.Inspect: failed to load path=%s"), *Path);
			return;
		}

		TArray<FText> Errors;
		TArray<FText> Warnings;
		const bool bValid = Definition->ValidateDefinition(Errors, Warnings);

		FString LoadPath = TEXT("DirectSoftObjectPath");
		if (UAssetManager::IsInitialized())
		{
			const FPrimaryAssetId Id = Definition->GetPrimaryAssetId();
			if (UAssetManager::Get().GetPrimaryAssetPath(Id).IsValid())
			{
				LoadPath = TEXT("AssetManagerPrimaryAssetPath");
			}
		}

		UE_LOG(LogGPResourceDefinition, Log,
			TEXT("GP ResourceDefinition.Inspect: Path=%s PrimaryAssetId=%s ResourceType=%s DisplayName=%s GameplayTag=%s AmountPerCycle=%.3f CycleDuration=%.3f InteractionRangeCm=%.1f MineRatePerWorker=%.3f EffectiveRate=%.3f ScoreConversionRate=%.3f ThreatPerStoredUnit=%.3f Valid=%s Errors=%d Warnings=%d Resolution=%s"),
			*Definition->GetPathName(),
			*Definition->GetPrimaryAssetId().ToString(),
			GPResourceTypePrivate::ToString(Definition->ResourceType),
			*Definition->DisplayName.ToString(),
			*Definition->ResourceGameplayTag.ToString(),
			Definition->AmountPerMiningCycle,
			Definition->MiningCycleDurationSeconds,
			Definition->InteractionRangeCm,
			Definition->MineRatePerWorker,
			Definition->GetEffectiveMineRatePerWorker(),
			Definition->ScoreConversionRate,
			Definition->ThreatPerStoredUnit,
			bValid ? TEXT("true") : TEXT("false"),
			Errors.Num(),
			Warnings.Num(),
			*LoadPath);
	}

	static FAutoConsoleCommand GResourceDefinitionInspectCommand(
		TEXT("gp.ResourceDefinition.Inspect"),
		TEXT("Inspect a UGP_ResourceDefinition. Usage: gp.ResourceDefinition.Inspect [SoftObjectPath]. Default: Ferronite prototype."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Inspect));
}
#endif
