// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnit.h"

#include "Components/CapsuleComponent.h"
#include "Tags/GPGameplayTags.h"
#include "Visual/GPUnitVisualComponent.h"

AGP_Unit::AGP_Unit()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->InitCapsuleSize(42.0f, 88.0f);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// GP-S33M: soft unit-unit presence + static obstacle sweep targets.
	// Soft unit presence for separation queries; static obstacles via NavMesh (not hard sweep block).
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	CapsuleComponent->SetGenerateOverlapEvents(true);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->SetSimulatePhysics(false);

	UnitVisualComponent = CreateDefaultSubobject<UGP_UnitVisualComponent>(TEXT("UnitVisualComponent"));

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	CapabilityTags.Reset();
	if (GPTags.Capability_Selectable.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Capability_Selectable);
	}
	if (GPTags.Capability_Inspectable.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Capability_Inspectable);
	}
	if (GPTags.Selection_Type_Unit.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Selection_Type_Unit);
	}
}

UGP_UnitVisualComponent* AGP_Unit::GetUnitVisualComponent() const
{
	return UnitVisualComponent;
}

bool AGP_Unit::HasLegacyVisualMesh() const
{
	return false;
}
