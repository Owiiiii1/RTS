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
			GhostMaterial = Dyn;
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

void AGP_BuildingPlacementGhost::SetFootprintCells(FIntPoint FootprintCells)
{
	ActiveFootprintCells = FIntPoint(FMath::Max(1, FootprintCells.X), FMath::Max(1, FootprintCells.Y));
	if (GhostMesh == nullptr)
	{
		return;
	}

	// Engine cube is 100 cm. Footprint cell is 200 cm.
	const float ScaleXY_X = static_cast<float>(ActiveFootprintCells.X) * 2.0f;
	const float ScaleXY_Y = static_cast<float>(ActiveFootprintCells.Y) * 2.0f;
	GhostMesh->SetRelativeScale3D(FVector(ScaleXY_X, ScaleXY_Y, 0.2f));
	GhostMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
}

void AGP_BuildingPlacementGhost::SetPreviewValid(bool bValid)
{
	if (GhostMaterial == nullptr)
	{
		return;
	}
	const FLinearColor Color = bValid
		? FLinearColor(0.2f, 0.85f, 0.35f, 0.45f)
		: FLinearColor(0.9f, 0.15f, 0.12f, 0.5f);
	GhostMaterial->SetVectorParameterValue(TEXT("Color"), Color);
}
