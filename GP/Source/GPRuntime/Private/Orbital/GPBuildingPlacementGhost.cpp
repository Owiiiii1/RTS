// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingPlacementGhost.h"

#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AGP_BuildingPlacementGhost::AGP_BuildingPlacementGhost()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GhostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GhostMesh"));
	GhostMesh->SetupAttachment(SceneRoot);
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

	GridLineBatch = CreateDefaultSubobject<ULineBatchComponent>(TEXT("GridLineBatch"));
	GridLineBatch->SetupAttachment(SceneRoot);
	GridLineBatch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GridLineBatch->SetCastShadow(false);
	GridLineBatch->SetCanEverAffectNavigation(false);
	GridLineBatch->SetGenerateOverlapEvents(false);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatusText->SetCanEverAffectNavigation(false);
	StatusText->SetGenerateOverlapEvents(false);
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(80.0f);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	StatusText->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	StatusText->SetText(FText::GetEmpty());
	StatusText->SetHiddenInGame(true);
}

void AGP_BuildingPlacementGhost::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearGridPreview();
	Super::EndPlay(EndPlayReason);
}

void AGP_BuildingPlacementGhost::SetGhostVisible(bool bVisible)
{
	if (GhostMesh != nullptr)
	{
		GhostMesh->SetHiddenInGame(!bVisible);
		GhostMesh->SetVisibility(bVisible);
	}
	if (!bVisible)
	{
		ClearGridPreview();
	}
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

void AGP_BuildingPlacementGhost::UpdateGridPreview(
	const UGP_BuildGridSubsystem* Grid,
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	float GroundZ,
	bool bValid,
	EGP_BuildingDropRejectReason RejectReason)
{
	ClearGridPreview();
	if (Grid == nullptr || !Grid->IsValidFootprintSize(FootprintSize) || GridLineBatch == nullptr)
	{
		return;
	}

	FVector Min = FVector::ZeroVector;
	FVector Max = FVector::ZeroVector;
	Grid->GetFootprintWorldAABB(OriginCell, FootprintSize, GroundZ, Min, Max);
	const float CellSize = Grid->GetCellSize();
	const FVector ActorLoc = GetActorLocation();
	const float LocalZ = 24.0f;
	const FColor LineColor = bValid ? FColor(32, 220, 72) : FColor(230, 36, 32);
	constexpr float BorderThickness = 6.0f;
	constexpr float CellThickness = 3.0f;
	constexpr float LineLife = 3600.0f;

	auto ToLocal = [&](float WorldX, float WorldY)
	{
		return FVector(WorldX - ActorLoc.X, WorldY - ActorLoc.Y, LocalZ);
	};
	auto DrawSeg = [&](const FVector& A, const FVector& B, float Thickness)
	{
		GridLineBatch->DrawLine(A, B, LineColor, SDPG_World, Thickness, LineLife);
		++PreviewGridLineCount;
	};

	DrawSeg(ToLocal(Min.X, Min.Y), ToLocal(Max.X, Min.Y), BorderThickness);
	DrawSeg(ToLocal(Max.X, Min.Y), ToLocal(Max.X, Max.Y), BorderThickness);
	DrawSeg(ToLocal(Max.X, Max.Y), ToLocal(Min.X, Max.Y), BorderThickness);
	DrawSeg(ToLocal(Min.X, Max.Y), ToLocal(Min.X, Min.Y), BorderThickness);

	for (int32 X = 1; X < FootprintSize.X; ++X)
	{
		const float WorldX = Min.X + static_cast<float>(X) * CellSize;
		DrawSeg(ToLocal(WorldX, Min.Y), ToLocal(WorldX, Max.Y), CellThickness);
	}
	for (int32 Y = 1; Y < FootprintSize.Y; ++Y)
	{
		const float WorldY = Min.Y + static_cast<float>(Y) * CellSize;
		DrawSeg(ToLocal(Min.X, WorldY), ToLocal(Max.X, WorldY), CellThickness);
	}

	PreviewOuterExtentXY = FVector2D(Max.X - Min.X, Max.Y - Min.Y);
	PreviewCellCount = FootprintSize.X * FootprintSize.Y;
	PreviewStatusLabel = GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(bValid, RejectReason);
	bGridPreviewActive = true;

	SetPreviewValid(bValid);

	if (StatusText != nullptr)
	{
		StatusText->SetText(FText::FromString(PreviewStatusLabel));
		StatusText->SetTextRenderColor(LineColor);
		StatusText->SetHiddenInGame(false);
		StatusText->SetVisibility(true);
	}
}

void AGP_BuildingPlacementGhost::ClearGridPreview()
{
	if (GridLineBatch != nullptr)
	{
		GridLineBatch->Flush();
	}
	if (StatusText != nullptr)
	{
		StatusText->SetText(FText::GetEmpty());
		StatusText->SetHiddenInGame(true);
		StatusText->SetVisibility(false);
	}
	PreviewOuterExtentXY = FVector2D::ZeroVector;
	PreviewCellCount = 0;
	PreviewGridLineCount = 0;
	PreviewStatusLabel.Reset();
	bGridPreviewActive = false;
}
