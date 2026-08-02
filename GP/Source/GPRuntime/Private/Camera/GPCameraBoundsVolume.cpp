// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/GPCameraBoundsVolume.h"

#include "Components/BoxComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

AGP_CameraBoundsVolume::AGP_CameraBoundsVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = false;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);

	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
	SetRootComponent(BoundsBox);

	BoundsBox->SetBoxExtent(FVector(50000.0f, 50000.0f, 3000.0f));
	BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundsBox->SetGenerateOverlapEvents(false);
	BoundsBox->SetCanEverAffectNavigation(false);
	BoundsBox->SetHiddenInGame(true);
	BoundsBox->SetMobility(EComponentMobility::Static);
}

FBox AGP_CameraBoundsVolume::GetCameraBounds() const
{
	if (BoundsBox == nullptr)
	{
		return FBox(ForceInit);
	}

	// CalcBounds uses the current component transform (scale included) rather than a possibly-stale Bounds cache.
	return BoundsBox->CalcBounds(BoundsBox->GetComponentTransform()).GetBox();
}

#if WITH_EDITOR

EDataValidationResult AGP_CameraBoundsVolume::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bHasValidationError = false;

	auto AddErrorAndMark = [&Context, &bHasValidationError](const FText& Message)
	{
		Context.AddError(Message);
		bHasValidationError = true;
	};

	if (BoundsBox == nullptr)
	{
		AddErrorAndMark(NSLOCTEXT(
			"GPCameraBoundsVolume",
			"MissingBoundsBox",
			"BoundsBox component is missing."));
	}
	else
	{
		const FVector Extent = BoundsBox->GetUnscaledBoxExtent();
		if (Extent.X <= 0.0f)
		{
			AddErrorAndMark(NSLOCTEXT(
				"GPCameraBoundsVolume",
				"InvalidExtentX",
				"BoxExtent.X must be greater than 0."));
		}
		if (Extent.Y <= 0.0f)
		{
			AddErrorAndMark(NSLOCTEXT(
				"GPCameraBoundsVolume",
				"InvalidExtentY",
				"BoxExtent.Y must be greater than 0."));
		}
		if (Extent.Z <= 0.0f)
		{
			AddErrorAndMark(NSLOCTEXT(
				"GPCameraBoundsVolume",
				"InvalidExtentZ",
				"BoxExtent.Z must be greater than 0."));
		}
	}

	const FRotator ActorRotation = GetActorRotation();
	constexpr float RotationToleranceDegrees = 0.1f;
	if (!FMath::IsNearlyZero(ActorRotation.Pitch, RotationToleranceDegrees)
		|| !FMath::IsNearlyZero(ActorRotation.Yaw, RotationToleranceDegrees)
		|| !FMath::IsNearlyZero(ActorRotation.Roll, RotationToleranceDegrees))
	{
		AddErrorAndMark(NSLOCTEXT(
			"GPCameraBoundsVolume",
			"NonZeroRotation",
			"Actor rotation must be nearly zero (axis-aligned bounds only). Tolerance 0.1 degrees."));
	}

	const FVector ActorScale = GetActorScale3D();
	if (FMath::IsNearlyZero(ActorScale.X)
		|| FMath::IsNearlyZero(ActorScale.Y)
		|| FMath::IsNearlyZero(ActorScale.Z))
	{
		AddErrorAndMark(NSLOCTEXT(
			"GPCameraBoundsVolume",
			"NearlyZeroScale",
			"Actor scale X/Y/Z must not be nearly zero."));
	}
	if (ActorScale.X < 0.0f || ActorScale.Y < 0.0f || ActorScale.Z < 0.0f)
	{
		AddErrorAndMark(NSLOCTEXT(
			"GPCameraBoundsVolume",
			"NegativeScale",
			"Actor scale X/Y/Z must not be negative."));
	}

	const FBox WorldBounds = GetCameraBounds();
	if (!WorldBounds.IsValid)
	{
		AddErrorAndMark(NSLOCTEXT(
			"GPCameraBoundsVolume",
			"InvalidWorldBounds",
			"Resulting camera bounds FBox is invalid."));
	}
	if (WorldBounds.Min.X >= WorldBounds.Max.X)
	{
		AddErrorAndMark(NSLOCTEXT(
			"GPCameraBoundsVolume",
			"InvalidBoundsX",
			"Resulting bounds Min.X must be less than Max.X."));
	}
	if (WorldBounds.Min.Y >= WorldBounds.Max.Y)
	{
		AddErrorAndMark(NSLOCTEXT(
			"GPCameraBoundsVolume",
			"InvalidBoundsY",
			"Resulting bounds Min.Y must be less than Max.Y."));
	}
	if (WorldBounds.Min.Z >= WorldBounds.Max.Z)
	{
		AddErrorAndMark(NSLOCTEXT(
			"GPCameraBoundsVolume",
			"InvalidBoundsZ",
			"Resulting bounds Min.Z must be less than Max.Z."));
	}

	if (bHasValidationError)
	{
		return EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
