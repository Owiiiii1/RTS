// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPUnitGroundPlacement.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"

float GPUnitGroundPlacement::GetGroundSpawnOffsetZForUnitClass(UClass* UnitClass)
{
	if (UnitClass == nullptr || !UnitClass->IsChildOf(AActor::StaticClass()))
	{
		return 0.0f;
	}

	const AActor* CDO = UnitClass->GetDefaultObject<AActor>();
	if (CDO == nullptr)
	{
		return 0.0f;
	}

	const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(CDO->GetRootComponent());
	if (Capsule == nullptr)
	{
		Capsule = CDO->FindComponentByClass<UCapsuleComponent>();
	}
	if (Capsule == nullptr)
	{
		return 0.0f;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	if (!FMath::IsFinite(HalfHeight) || HalfHeight < 0.0f)
	{
		return 0.0f;
	}

	// Capsule center is actor origin; bottom = Location.Z - HalfHeight when upright.
	return HalfHeight;
}
