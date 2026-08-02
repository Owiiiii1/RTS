// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/GPCameraConfigDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR

EDataValidationResult UGP_CameraConfigDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bHasValidationError = false;

	auto AddErrorAndMark = [&Context, &bHasValidationError](const TCHAR* Message)
	{
		Context.AddError(FText::FromString(Message));
		bHasValidationError = true;
	};

	if (PanSpeed <= 0.0f)
	{
		AddErrorAndMark(TEXT("PanSpeed must be greater than 0."));
	}
	if (ZoomPanScale < 0.0f)
	{
		AddErrorAndMark(TEXT("ZoomPanScale must be greater than or equal to 0."));
	}
	if (EdgeScrollSpeed < 0.0f)
	{
		AddErrorAndMark(TEXT("EdgeScrollSpeed must be greater than or equal to 0."));
	}
	if (EdgeThresholdPx < 0)
	{
		AddErrorAndMark(TEXT("EdgeThresholdPx must be greater than or equal to 0."));
	}
	if (EdgeFalloffPx < 0)
	{
		AddErrorAndMark(TEXT("EdgeFalloffPx must be greater than or equal to 0."));
	}

	if (MinArmLength <= 0.0f)
	{
		AddErrorAndMark(TEXT("MinArmLength must be greater than 0."));
	}
	if (MaxArmLength <= MinArmLength)
	{
		AddErrorAndMark(TEXT("MaxArmLength must be greater than MinArmLength."));
	}
	if (DefaultArmLength <= MinArmLength)
	{
		AddErrorAndMark(TEXT("DefaultArmLength must be greater than MinArmLength (strict Min < Default < Max)."));
	}
	if (DefaultArmLength >= MaxArmLength)
	{
		AddErrorAndMark(TEXT("DefaultArmLength must be less than MaxArmLength (strict Min < Default < Max)."));
	}

	if (ZoomStep <= 0.0f)
	{
		AddErrorAndMark(TEXT("ZoomStep must be greater than 0."));
	}
	if (ZoomInterpSpeed <= 0.0f)
	{
		AddErrorAndMark(TEXT("ZoomInterpSpeed must be greater than 0."));
	}

	if (PitchAtMaxZoom > 0.0f)
	{
		AddErrorAndMark(TEXT("PitchAtMaxZoom must be less than or equal to 0."));
	}
	if (PitchAtMinZoom > 0.0f)
	{
		AddErrorAndMark(TEXT("PitchAtMinZoom must be less than or equal to 0."));
	}
	if (PitchAtMaxZoom > PitchAtMinZoom)
	{
		AddErrorAndMark(TEXT("PitchAtMaxZoom must be less than or equal to PitchAtMinZoom."));
	}

	if (RotateSpeed <= 0.0f)
	{
		AddErrorAndMark(TEXT("RotateSpeed must be greater than 0."));
	}

	if (MoveAccelTime < 0.0f)
	{
		AddErrorAndMark(TEXT("MoveAccelTime must be greater than or equal to 0."));
	}
	if (MoveDecelTime < 0.0f)
	{
		AddErrorAndMark(TEXT("MoveDecelTime must be greater than or equal to 0."));
	}

	if (!FallbackBounds.IsValid)
	{
		AddErrorAndMark(TEXT("FallbackBounds must be a valid FBox."));
	}
	if (FallbackBounds.Min.X >= FallbackBounds.Max.X)
	{
		AddErrorAndMark(TEXT("FallbackBounds.Min.X must be less than FallbackBounds.Max.X."));
	}
	if (FallbackBounds.Min.Y >= FallbackBounds.Max.Y)
	{
		AddErrorAndMark(TEXT("FallbackBounds.Min.Y must be less than FallbackBounds.Max.Y."));
	}
	if (FallbackBounds.Min.Z >= FallbackBounds.Max.Z)
	{
		AddErrorAndMark(TEXT("FallbackBounds.Min.Z must be less than FallbackBounds.Max.Z."));
	}

	if (bEdgeScrollEnabled && FMath::IsNearlyZero(EdgeScrollSpeed))
	{
		Context.AddWarning(FText::FromString(
			TEXT("bEdgeScrollEnabled is true but EdgeScrollSpeed is 0; edge scroll will have no effect.")));
	}
	if (bPitchInterpEnabled && FMath::IsNearlyEqual(PitchAtMaxZoom, PitchAtMinZoom))
	{
		Context.AddWarning(FText::FromString(
			TEXT("bPitchInterpEnabled is true but PitchAtMaxZoom equals PitchAtMinZoom; pitch will not change with zoom.")));
	}

	if (bHasValidationError || Result == EDataValidationResult::Invalid)
	{
		return EDataValidationResult::Invalid;
	}

	return EDataValidationResult::Valid;
}

#endif // WITH_EDITOR
