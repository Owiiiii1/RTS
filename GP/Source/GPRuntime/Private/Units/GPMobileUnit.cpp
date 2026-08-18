// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPMobileUnit.h"

#include "Components/PrimitiveComponent.h"
#include "Units/GPMovementComponent.h"

AGP_MobileUnit::AGP_MobileUnit()
{
	MovementComponent = CreateDefaultSubobject<UGP_MovementComponent>(TEXT("MovementComponent"));
	// Pawn default is false, but components can still carve independently.
	bCanAffectNavigationGeneration = false;
}

void AGP_MobileUnit::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMobileNavigationGenerationPolicy();
}

void AGP_MobileUnit::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ApplyMobileNavigationGenerationPolicy();
}

void AGP_MobileUnit::BeginPlay()
{
	Super::BeginPlay();
	ApplyMobileNavigationGenerationPolicy();
}

void AGP_MobileUnit::UpdateNavigationRelevance()
{
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive != nullptr)
		{
			Primitive->SetCanEverAffectNavigation(false);
		}
	}
}

void AGP_MobileUnit::ApplyMobileNavigationGenerationPolicy()
{
	SetCanAffectNavigationGeneration(false, /*bForceUpdate=*/true);
}

bool AGP_MobileUnit::HasAnyPrimitiveThatCanAffectNavigation() const
{
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	GetComponents(Primitives);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive != nullptr && Primitive->CanEverAffectNavigation())
		{
			return true;
		}
	}
	return false;
}

UGP_MovementComponent* AGP_MobileUnit::GetUnitMovementComponent() const
{
	return MovementComponent;
}
