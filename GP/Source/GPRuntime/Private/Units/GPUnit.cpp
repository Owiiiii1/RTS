// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnit.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Tags/GPGameplayTags.h"
#include "UObject/ConstructorHelpers.h"

AGP_Unit::AGP_Unit()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->InitCapsuleSize(42.0f, 88.0f);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CapsuleComponent->SetGenerateOverlapEvents(false);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->SetSimulatePhysics(false);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CapsuleComponent);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetGenerateOverlapEvents(false);
	VisualMesh->SetCanEverAffectNavigation(false);
	VisualMesh->SetMobility(EComponentMobility::Movable);
	// BasicShapes Cylinder is ~100 tall / diameter 100; scale to sit inside capsule (r=42, hh=88).
	VisualMesh->SetRelativeScale3D(FVector(0.80f, 0.80f, 1.60f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(CylinderMesh.Object);
	}

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
