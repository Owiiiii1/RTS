// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPPrimitiveVisualProfile.h"

FPrimaryAssetId UGP_PrimitiveVisualProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("GPPrimitiveVisualProfile"), GetFName());
}

bool UGP_PrimitiveVisualProfile::ValidateProfile(TArray<FString>& OutErrors) const
{
	FGP_PrimitiveVisualDefinition Definition;
	Definition.Parts = Parts;
	const FGP_PrimitiveVisualValidationResult Result =
		GPPrimitiveVisualDefaults::ValidateAndSanitizeDefinition(Definition);
	OutErrors = Result.Errors;
	OutErrors.Append(Result.Warnings);
	return Result.bValid;
}

bool UGP_PrimitiveVisualProfile::GetValidatedDefinition(
	FGP_PrimitiveVisualDefinition& OutDefinition,
	TArray<FString>& OutErrors) const
{
	FGP_PrimitiveVisualDefinition Definition;
	Definition.Parts = Parts;
	const FGP_PrimitiveVisualValidationResult Result =
		GPPrimitiveVisualDefaults::ValidateAndSanitizeDefinition(Definition);
	OutErrors = Result.Errors;
	OutErrors.Append(Result.Warnings);
	if (!Result.bValid)
	{
		OutDefinition = FGP_PrimitiveVisualDefinition();
		return false;
	}

	OutDefinition = Result.SanitizedDefinition;
	return true;
}

FGP_PrimitiveVisualDefinition UGP_PrimitiveVisualProfile::SanitizeDefinition() const
{
	FGP_PrimitiveVisualDefinition Definition;
	Definition.Parts = Parts;
	const FGP_PrimitiveVisualValidationResult Result =
		GPPrimitiveVisualDefaults::ValidateAndSanitizeDefinition(Definition);
	return Result.bValid ? Result.SanitizedDefinition : FGP_PrimitiveVisualDefinition();
}
