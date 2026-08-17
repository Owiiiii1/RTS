// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingPlacementGhost.h"

#include "Buildings/GPBuildingBase.h"
#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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
	HideLegacyCubeFill();

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
	StatusText->SetWorldSize(72.0f);
	StatusText->SetRelativeLocation(FVector(0.0f, 0.0f, 160.0f));
	StatusText->SetText(FText::GetEmpty());
	StatusText->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		FallbackCylinderMesh = CylinderMesh.Object;
	}
}

void AGP_BuildingPlacementGhost::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearGridPreview();
	Super::EndPlay(EndPlayReason);
}

void AGP_BuildingPlacementGhost::HideLegacyCubeFill()
{
	if (GhostMesh == nullptr)
	{
		return;
	}
	GhostMesh->SetHiddenInGame(true);
	GhostMesh->SetVisibility(false);
	GhostMesh->SetStaticMesh(nullptr);
}

void AGP_BuildingPlacementGhost::SetGhostVisible(bool bVisible)
{
	HideLegacyCubeFill();
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
	HideLegacyCubeFill();
}

void AGP_BuildingPlacementGhost::SetFootprintLocalOffset(FVector2D LocalCenterOffsetCm)
{
	FootprintLocalOffsetCm = LocalCenterOffsetCm;
}

void AGP_BuildingPlacementGhost::SetPreviewValid(bool bValid)
{
	(void)bValid;
}

void AGP_BuildingPlacementGhost::SetBuildingGhostClass(TSubclassOf<AGP_BuildingBase> PayloadClass)
{
	if (ActivePayloadClass == PayloadClass && BuildingGhostMeshes.Num() > 0)
	{
		return;
	}
	ActivePayloadClass = PayloadClass;
	RebuildBuildingGhostMeshes();
}

void AGP_BuildingPlacementGhost::RebuildBuildingGhostMeshes()
{
	for (UStaticMeshComponent* Comp : BuildingGhostMeshes)
	{
		if (Comp != nullptr)
		{
			Comp->DestroyComponent();
		}
	}
	BuildingGhostMeshes.Reset();
	bBuildingGhostVisible = false;
	if (ActivePayloadClass == nullptr || SceneRoot == nullptr)
	{
		return;
	}

	const AGP_BuildingBase* CDO = ActivePayloadClass->GetDefaultObject<AGP_BuildingBase>();
	if (CDO == nullptr)
	{
		return;
	}

	TArray<UStaticMeshComponent*> SourceMeshes;
	CDO->GetComponents<UStaticMeshComponent>(SourceMeshes);
	const FTransform CdoActorTM = CDO->GetActorTransform();
	for (UStaticMeshComponent* Source : SourceMeshes)
	{
		if (Source == nullptr || Source->GetStaticMesh() == nullptr)
		{
			continue;
		}
		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(this);
		Copy->SetupAttachment(SceneRoot);
		Copy->SetStaticMesh(Source->GetStaticMesh());
		Copy->SetRelativeTransform(Source->GetComponentTransform().GetRelativeTransform(CdoActorTM));
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->SetCastShadow(false);
		Copy->SetCanEverAffectNavigation(false);
		Copy->SetGenerateOverlapEvents(false);
		Copy->SetHiddenInGame(true);
		Copy->SetVisibility(false);
		Copy->RegisterComponent();
		BuildingGhostMeshes.Add(Copy);
	}

	if (BuildingGhostMeshes.Num() == 0 && FallbackCylinderMesh != nullptr)
	{
		UStaticMeshComponent* Fallback = NewObject<UStaticMeshComponent>(this);
		Fallback->SetupAttachment(SceneRoot);
		Fallback->SetStaticMesh(FallbackCylinderMesh);
		float Radius = 80.0f;
		float HalfHeight = 120.0f;
		if (const UCapsuleComponent* Capsule = CDO->FindComponentByClass<UCapsuleComponent>())
		{
			Radius = Capsule->GetUnscaledCapsuleRadius();
			HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
		}
		Fallback->SetRelativeScale3D(FVector(Radius / 50.0f, Radius / 50.0f, HalfHeight / 50.0f));
		Fallback->SetRelativeLocation(FVector(0.0f, 0.0f, HalfHeight));
		Fallback->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Fallback->SetCastShadow(false);
		Fallback->SetCanEverAffectNavigation(false);
		Fallback->SetGenerateOverlapEvents(false);
		Fallback->SetHiddenInGame(true);
		Fallback->SetVisibility(false);
		Fallback->RegisterComponent();
		BuildingGhostMeshes.Add(Fallback);
	}
}

void AGP_BuildingPlacementGhost::SetBuildingGhostHidden(bool bHideMeshes)
{
	bBuildingGhostVisible = !bHideMeshes && BuildingGhostMeshes.Num() > 0;
	for (UStaticMeshComponent* Comp : BuildingGhostMeshes)
	{
		if (Comp == nullptr)
		{
			continue;
		}
		Comp->SetHiddenInGame(bHideMeshes);
		Comp->SetVisibility(!bHideMeshes);
	}
}

bool AGP_BuildingPlacementGhost::IsGhostFillHidden() const
{
	return GhostMesh == nullptr || GhostMesh->bHiddenInGame;
}

int32 AGP_BuildingPlacementGhost::GetPreviewInvalidCellCount() const
{
	int32 Count = 0;
	for (const EGP_PlacementPreviewCellState State : PreviewCellStates)
	{
		if (State != EGP_PlacementPreviewCellState::Free)
		{
			++Count;
		}
	}
	return Count;
}

EGP_PlacementPreviewCellState AGP_BuildingPlacementGhost::GetPreviewCellState(int32 Index) const
{
	return PreviewCellStates.IsValidIndex(Index)
		? PreviewCellStates[Index]
		: EGP_PlacementPreviewCellState::Free;
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

void AGP_BuildingPlacementGhost::FaceStatusTextToCamera()
{
	if (StatusText == nullptr)
	{
		return;
	}
	UWorld* World = GetWorld();
	APlayerController* PC = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	if (PC == nullptr)
	{
		return;
	}
	FVector CamLoc = FVector::ZeroVector;
	FRotator CamRot = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector ToCamera = CamLoc - StatusText->GetComponentLocation();
	if (!ToCamera.IsNearlyZero())
	{
		StatusText->SetWorldRotation(ToCamera.GetSafeNormal().Rotation());
	}
}

void AGP_BuildingPlacementGhost::UpdateGridPreview(
	const UGP_BuildGridSubsystem* Grid,
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	float GroundZ,
	bool bValid,
	EGP_BuildingDropRejectReason RejectReason,
	const TArray<EGP_PlacementPreviewCellState>* CellStates)
{
	ClearGridPreview();
	if (Grid == nullptr || !Grid->IsValidFootprintSize(FootprintSize) || GridLineBatch == nullptr)
	{
		return;
	}

	HideLegacyCubeFill();
	PreviewGroundZ = GroundZ;

	FVector Min = FVector::ZeroVector;
	FVector Max = FVector::ZeroVector;
	Grid->GetFootprintWorldAABB(OriginCell, FootprintSize, GroundZ, Min, Max);
	PreviewFillWorldMin = Min;
	PreviewFillWorldMax = Max;
	const float CellSize = Grid->GetCellSize();
	const int32 Width = FootprintSize.X;
	const int32 Height = FootprintSize.Y;
	PreviewCellCount = Width * Height;
	if (CellStates != nullptr && CellStates->Num() == PreviewCellCount)
	{
		PreviewCellStates = *CellStates;
	}
	else
	{
		PreviewCellStates.Init(
			bValid ? EGP_PlacementPreviewCellState::Free : EGP_PlacementPreviewCellState::Occupied,
			PreviewCellCount);
		if (!bValid && RejectReason == EGP_BuildingDropRejectReason::OutOfDeployRadius)
		{
			for (EGP_PlacementPreviewCellState& State : PreviewCellStates)
			{
				State = EGP_PlacementPreviewCellState::OutOfRange;
			}
		}
	}

	auto ColorForState = [](EGP_PlacementPreviewCellState State, bool bInner) -> FColor
	{
		if (State != EGP_PlacementPreviewCellState::Free)
		{
			return FColor(230, 36, 32, bInner ? 210 : 150);
		}
		return bInner ? FColor(16, 210, 64, 200) : FColor(70, 220, 110, 85);
	};

	int32 CellIndex = 0;
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X, ++CellIndex)
		{
			const EGP_PlacementPreviewCellState State = PreviewCellStates.IsValidIndex(CellIndex)
				? PreviewCellStates[CellIndex]
				: EGP_PlacementPreviewCellState::Free;
			const int32 BorderDist = FMath::Min3(X, Y, FMath::Min(Width - 1 - X, Height - 1 - Y));
			const bool bInner = BorderDist > 0;
			const FColor FillColor = ColorForState(State, bInner);
			const float Half = CellSize * 0.5f - 6.0f;
			const float BoxHeight = bInner ? 16.0f : 6.0f;
			const FVector CellMinWorld(Min.X + static_cast<float>(X) * CellSize, Min.Y + static_cast<float>(Y) * CellSize, GroundZ);
			const FVector Center(
				CellMinWorld.X + CellSize * 0.5f,
				CellMinWorld.Y + CellSize * 0.5f,
				GroundZ + 2.0f);
			const FBox LocalBox(FVector(-Half, -Half, 0.0f), FVector(Half, Half, BoxHeight));
			GridLineBatch->DrawSolidBox(LocalBox, FTransform(FRotator::ZeroRotator, Center), FillColor, SDPG_World, 0.0f);
		}
	}

	PreviewGridLineCount = 0;
	PreviewOuterExtentXY = FVector2D(Max.X - Min.X, Max.Y - Min.Y);
	PreviewStatusLabel = GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(bValid, RejectReason);
	bGridPreviewActive = true;
	SetBuildingGhostHidden(!bValid);

	const FColor StatusColor = bValid ? FColor(32, 220, 72) : FColor(230, 36, 32);
	if (StatusText != nullptr)
	{
		StatusText->SetText(FText::FromString(PreviewStatusLabel));
		StatusText->SetTextRenderColor(StatusColor);
		StatusText->SetHiddenInGame(false);
		StatusText->SetVisibility(true);
		StatusText->SetRelativeLocation(FVector(FootprintLocalOffsetCm.X, FootprintLocalOffsetCm.Y, 160.0f));
		FaceStatusTextToCamera();
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
	SetBuildingGhostHidden(true);
	PreviewOuterExtentXY = FVector2D::ZeroVector;
	PreviewFillWorldMin = FVector::ZeroVector;
	PreviewFillWorldMax = FVector::ZeroVector;
	PreviewCellCount = 0;
	PreviewGridLineCount = 0;
	PreviewStatusLabel.Reset();
	bGridPreviewActive = false;
	PreviewGroundZ = 0.0f;
	PreviewLineWorldStarts.Reset();
	PreviewLineWorldEnds.Reset();
	PreviewCellStates.Reset();
}
