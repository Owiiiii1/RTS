// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingPlacementGhost.h"

#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"

namespace
{
	constexpr float GPPlacementGridLineHeightCm = 24.0f;
}

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
	GhostMesh->SetHiddenInGame(true);
	GhostMesh->SetVisibility(false);

	GridLineBatch = CreateDefaultSubobject<ULineBatchComponent>(TEXT("GridLineBatch"));
	GridLineBatch->SetupAttachment(SceneRoot);
	GridLineBatch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GridLineBatch->SetCastShadow(false);
	GridLineBatch->SetCanEverAffectNavigation(false);
	GridLineBatch->SetGenerateOverlapEvents(false);
	GridLineBatch->PrimaryComponentTick.bCanEverTick = false;
	GridLineBatch->SetComponentTickEnabled(false);

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
		GhostMesh->SetHiddenInGame(true);
		GhostMesh->SetVisibility(false);
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
	if (GhostMesh != nullptr)
	{
		GhostMesh->SetHiddenInGame(true);
		GhostMesh->SetVisibility(false);
	}
}

void AGP_BuildingPlacementGhost::SetPreviewValid(bool bValid)
{
	(void)bValid;
}

bool AGP_BuildingPlacementGhost::IsGhostFillHidden() const
{
	return GhostMesh == nullptr || GhostMesh->bHiddenInGame;
}

bool AGP_BuildingPlacementGhost::GetPreviewLineWorldSegment(int32 Index, FVector& OutStart, FVector& OutEnd) const
{
	if (!PreviewLineWorldStarts.IsValidIndex(Index) || !PreviewLineWorldEnds.IsValidIndex(Index))
	{
		OutStart = FVector::ZeroVector;
		OutEnd = FVector::ZeroVector;
		return false;
	}
	OutStart = PreviewLineWorldStarts[Index];
	OutEnd = PreviewLineWorldEnds[Index];
	return true;
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

	if (GhostMesh != nullptr)
	{
		GhostMesh->SetHiddenInGame(true);
		GhostMesh->SetVisibility(false);
	}

	FVector Min = FVector::ZeroVector;
	FVector Max = FVector::ZeroVector;
	Grid->GetFootprintWorldAABB(OriginCell, FootprintSize, GroundZ, Min, Max);
	const float CellSize = Grid->GetCellSize();
	const float LineZ = GroundZ + GPPlacementGridLineHeightCm;
	const FColor LineColor = bValid ? FColor(32, 220, 72) : FColor(230, 36, 32);
	constexpr float BorderThickness = 6.0f;
	constexpr float CellThickness = 3.0f;

	auto DrawSeg = [&](const FVector& A, const FVector& B, float Thickness)
	{
		GridLineBatch->DrawLine(A, B, LineColor, SDPG_World, Thickness, 0.0f);
		PreviewLineWorldStarts.Add(A);
		PreviewLineWorldEnds.Add(B);
		++PreviewGridLineCount;
	};

	DrawSeg(FVector(Min.X, Min.Y, LineZ), FVector(Max.X, Min.Y, LineZ), BorderThickness);
	DrawSeg(FVector(Max.X, Min.Y, LineZ), FVector(Max.X, Max.Y, LineZ), BorderThickness);
	DrawSeg(FVector(Max.X, Max.Y, LineZ), FVector(Min.X, Max.Y, LineZ), BorderThickness);
	DrawSeg(FVector(Min.X, Max.Y, LineZ), FVector(Min.X, Min.Y, LineZ), BorderThickness);

	for (int32 X = 1; X < FootprintSize.X; ++X)
	{
		const float WorldX = Min.X + static_cast<float>(X) * CellSize;
		DrawSeg(FVector(WorldX, Min.Y, LineZ), FVector(WorldX, Max.Y, LineZ), CellThickness);
	}
	for (int32 Y = 1; Y < FootprintSize.Y; ++Y)
	{
		const float WorldY = Min.Y + static_cast<float>(Y) * CellSize;
		DrawSeg(FVector(Min.X, WorldY, LineZ), FVector(Max.X, WorldY, LineZ), CellThickness);
	}

	PreviewOuterExtentXY = FVector2D(Max.X - Min.X, Max.Y - Min.Y);
	PreviewCellCount = FootprintSize.X * FootprintSize.Y;
	PreviewStatusLabel = GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(bValid, RejectReason);
	bGridPreviewActive = true;

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
	PreviewLineWorldStarts.Reset();
	PreviewLineWorldEnds.Reset();
}
