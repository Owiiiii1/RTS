// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingGroundPlacement.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"

float GPBuildingGroundPlacement::GetGroundSpawnOffsetZForBuildingClass(UClass* BuildingClass)
{
	if (BuildingClass == nullptr || !BuildingClass->IsChildOf(AActor::StaticClass()))
	{
		return 0.0f;
	}

	const AActor* CDO = BuildingClass->GetDefaultObject<AActor>();
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

	return HalfHeight;
}
