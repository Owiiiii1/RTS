// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingPlacementGhost.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AGP_BuildingPlacementGhost::AGP_BuildingPlacementGhost()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	GhostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostMesh"));
	SetRootComponent(GhostMesh);
	GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostMesh->SetCastShadow(false);
	GhostMesh->SetCanEverAffectNavigation(false);
	GhostMesh->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		GhostMesh->SetStaticMesh(CubeMesh.Object);
		GhostMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 2.4f));
	}

	GhostMesh->SetTranslucentSortPriority(10);
	if (UMaterialInterface* BaseMat = GhostMesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(BaseMat, this))
		{
			Dyn->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.85f, 0.35f, 0.45f));
			GhostMesh->SetMaterial(0, Dyn);
		}
	}
}

void AGP_BuildingPlacementGhost::SetGhostVisible(bool bVisible)
{
	if (GhostMesh == nullptr)
	{
		return;
	}
	GhostMesh->SetHiddenInGame(!bVisible);
	GhostMesh->SetVisibility(bVisible);
}

void AGP_BuildingPlacementGhost::UpdateGhostTransform(const FTransform& WorldTransform)
{
	SetActorTransform(WorldTransform);
}
