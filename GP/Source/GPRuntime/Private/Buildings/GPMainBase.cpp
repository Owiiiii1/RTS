// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPMainBase.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Game/GPGameMode.h"
#include "Game/GPGameState.h"
#include "Net/UnrealNetwork.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPWallSegmentInventoryComponent.h"
#include "Resources/GPStorageComponent.h"
#include "Tags/GPGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

AGP_MainBase::AGP_MainBase()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->InitCapsuleSize(80.0f, 120.0f);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CapsuleComponent->SetGenerateOverlapEvents(false);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->SetSimulatePhysics(false);

	AttachNavigationObstacleToRoot();
	if (NavigationObstacle)
	{
		// Rough MainBase footprint (~capsule 80r / 120hh) — BP may retune freely.
		NavigationObstacle->SetBoxExtent(FVector(160.0f, 160.0f, 130.0f));
	}
	if (PlacementFootprintBounds)
	{
		// Native 5×5 BuildGrid (1000×1000 cm). BP children may retune extent/scale.
		PlacementFootprintBounds->SetBoxExtent(FVector(500.0f, 500.0f, 20.0f));
	}

	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	PresentationRoot->SetupAttachment(CapsuleComponent);
	PresentationRoot->SetCanEverAffectNavigation(false);

	DropOffVisualAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("DropOffVisualAnchor"));
	DropOffVisualAnchor->SetupAttachment(PresentationRoot);
	DropOffVisualAnchor->SetCanEverAffectNavigation(false);

	UnitDropZone = CreateDefaultSubobject<USceneComponent>(TEXT("UnitDropZone"));
	UnitDropZone->SetupAttachment(PresentationRoot);
	UnitDropZone->SetCanEverAffectNavigation(false);
	// Default authored-relative offset (tuning). Owner repositions in BP-derived MainBase.
	UnitDropZone->SetRelativeLocation(FVector(350.0f, 0.0f, 0.0f));

	WallPackageDropZone = CreateDefaultSubobject<USceneComponent>(TEXT("WallPackageDropZone"));
	WallPackageDropZone->SetupAttachment(PresentationRoot);
	WallPackageDropZone->SetCanEverAffectNavigation(false);
	WallPackageDropZone->SetRelativeLocation(FVector(-350.0f, 0.0f, 0.0f));

	StorageComponent = CreateDefaultSubobject<UGP_StorageComponent>(TEXT("StorageComponent"));
	WallSegmentInventoryComponent = CreateDefaultSubobject<UGP_WallSegmentInventoryComponent>(TEXT("WallSegmentInventoryComponent"));
	DropOffRangeCm = 400.0f;

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPTags.Building_Type_MainBase.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Building_Type_MainBase);
	}
}

float AGP_MainBase::ComputeDropOffDistance2D(const FVector& FromLocation) const
{
	return FVector::Dist2D(FromLocation, GetActorLocation());
}

bool AGP_MainBase::IsWithinDropOffRange2D(const FVector& FromLocation) const
{
	return ComputeDropOffDistance2D(FromLocation) <= DropOffRangeCm + KINDA_SMALL_NUMBER;
}

void AGP_MainBase::BeginPlay()
{
	Super::BeginPlay();
	// Register only when TeamId is already playable (deferred spawn sets TeamId before FinishSpawning).
	RefreshMainBaseRegistration();
}

void AGP_MainBase::NotifyBuildingDefinitionReady()
{
	ApplyStorageFromBuildingDefinition();
}

void AGP_MainBase::ApplyStorageFromBuildingDefinition()
{
	if (!IsValid(StorageComponent))
	{
		return;
	}

	float Capacity = 100.0f;
	int32 Count = 5;
	if (const UGP_BuildingDefinition* Def = ResolveLoadedBuildingDefinition())
	{
		Capacity = Def->ContainerCapacity;
		Count = Def->ContainerCount;
	}
	StorageComponent->ConfigureFromDefinition(Capacity, Count);
}

void AGP_MainBase::NotifyAuthorityDeath()
{
	Super::NotifyAuthorityDeath();

	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown)
	{
		return;
	}

	if (IsValid(WallSegmentInventoryComponent))
	{
		WallSegmentInventoryComponent->AuthorityClearForDestruction();
	}

	if (AGP_GameMode* GameMode = World->GetAuthGameMode<AGP_GameMode>())
	{
		GameMode->NotifyMainBaseDied(this);
	}
}

void AGP_MainBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromGameState();
	Super::EndPlay(EndPlayReason);
}

void AGP_MainBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGP_MainBase, DropOffRangeCm);
}

void AGP_MainBase::NotifyTeamIdChanged(int32 OldTeamId, int32 NewTeamId)
{
	Super::NotifyTeamIdChanged(OldTeamId, NewTeamId);
	if (!HasAuthority())
	{
		return;
	}

	// Drop stale registry entry when team changes (including to unassigned).
	if (bRegisteredWithGameState)
	{
		UnregisterFromGameState();
	}

	if (IsValid(WallSegmentInventoryComponent) && WallSegmentInventoryComponent->IsWallPackagePending())
	{
		WallSegmentInventoryComponent->AuthorityCancelPackageDelivery();
	}

	if (NewTeamId >= 1)
	{
		RegisterWithGameState();
	}
}

void AGP_MainBase::RefreshMainBaseRegistration()
{
	if (!HasAuthority())
	{
		return;
	}

	if (GetTeamId() < 1)
	{
		// Production-safe: wait for authority TeamId assignment. No warning spam.
		if (bRegisteredWithGameState)
		{
			UnregisterFromGameState();
		}
		return;
	}

	if (!bRegisteredWithGameState)
	{
		RegisterWithGameState();
	}
}

UGP_StorageComponent* AGP_MainBase::GetStorageComponent() const
{
	return StorageComponent;
}

UCapsuleComponent* AGP_MainBase::GetCapsuleComponent() const
{
	return CapsuleComponent;
}

USceneComponent* AGP_MainBase::GetPresentationRoot() const
{
	return PresentationRoot;
}

USceneComponent* AGP_MainBase::GetDropOffVisualAnchor() const
{
	return DropOffVisualAnchor;
}

USceneComponent* AGP_MainBase::GetUnitDropZone() const
{
	return UnitDropZone;
}

USceneComponent* AGP_MainBase::GetWallPackageDropZone() const
{
	return WallPackageDropZone;
}

UGP_WallSegmentInventoryComponent* AGP_MainBase::GetWallSegmentInventoryComponent() const
{
	return WallSegmentInventoryComponent;
}

float AGP_MainBase::GetPlanetaryStored() const
{
	return IsValid(StorageComponent) ? StorageComponent->GetTotalStored() : 0.0f;
}

float AGP_MainBase::GetPlanetaryCapacity() const
{
	return IsValid(StorageComponent) ? StorageComponent->GetTotalCapacity() : 0.0f;
}

void AGP_MainBase::RegisterWithGameState()
{
	if (!HasAuthority())
	{
		return;
	}

	if (GetTeamId() < 1)
	{
		return;
	}

	// Idempotent refresh when already marked registered.
	if (bRegisteredWithGameState)
	{
		if (UWorld* World = GetWorld())
		{
			if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
			{
				const AGP_GameState::EGP_MainBaseRegisterResult Result = GS->RegisterMainBase(this);
				if (Result == AGP_GameState::EGP_MainBaseRegisterResult::RejectedDuplicate)
				{
					bRegisteredWithGameState = false;
				}
			}
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			const AGP_GameState::EGP_MainBaseRegisterResult Result = GS->RegisterMainBase(this);
			bRegisteredWithGameState =
				Result == AGP_GameState::EGP_MainBaseRegisterResult::Registered
				|| Result == AGP_GameState::EGP_MainBaseRegisterResult::AlreadyRegistered;
		}
	}
}

void AGP_MainBase::UnregisterFromGameState()
{
	if (!bRegisteredWithGameState)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			GS->UnregisterMainBase(this);
		}
	}
	bRegisteredWithGameState = false;
}

bool AGP_MainBase::ValidateMainBaseContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	if (!IsValid(StorageComponent))
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrStorage", "MainBase requires StorageComponent."));
	}
	else if (StorageComponent->GetOwner() != this)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrStorageOwner", "StorageComponent owner must be MainBase."));
	}

	if (!IsValid(PresentationRoot))
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrPresentationRoot", "MainBase requires PresentationRoot."));
	}
	if (!IsValid(DropOffVisualAnchor))
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrDropOffAnchor", "MainBase requires DropOffVisualAnchor."));
	}
	if (IsValid(PresentationRoot) && PresentationRoot->GetAttachParent() != CapsuleComponent)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrPresentationAttach", "PresentationRoot must attach to Capsule."));
	}
	if (IsValid(DropOffVisualAnchor) && DropOffVisualAnchor->GetAttachParent() != PresentationRoot)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrDropOffAttach", "DropOffVisualAnchor must attach to PresentationRoot."));
	}
	if (!IsValid(UnitDropZone))
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrUnitDropZone", "MainBase requires UnitDropZone."));
	}
	if (IsValid(UnitDropZone) && UnitDropZone->GetAttachParent() != PresentationRoot)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrUnitDropZoneAttach", "UnitDropZone must attach to PresentationRoot."));
	}
	if (!IsValid(WallPackageDropZone))
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrWallPackageDropZone", "MainBase requires WallPackageDropZone."));
	}
	if (IsValid(WallPackageDropZone) && WallPackageDropZone->GetAttachParent() != PresentationRoot)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrWallPackageDropZoneAttach", "WallPackageDropZone must attach to PresentationRoot."));
	}
	if (!IsValid(WallSegmentInventoryComponent))
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrWallInventory", "MainBase requires WallSegmentInventoryComponent."));
	}

	if (!FMath::IsFinite(DropOffRangeCm) || DropOffRangeCm <= 0.0f)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrDropOff", "DropOffRangeCm must be finite and > 0."));
	}

	if (PrimaryActorTick.bCanEverTick)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrTick", "MainBase must not enable permanent Tick."));
	}

	if (!GetCapabilityTags().HasTagExact(FGPGameplayTags::Get().Building_Type_MainBase))
	{
		OutWarnings.Add(NSLOCTEXT("GPMainBase", "WarnTag", "MainBase missing GP.Building.Type.MainBase tag."));
	}

	// UGP_BuildingDefinition is deferred (GP-S34/S39). GP-S28 DropOffRange/container
	// placeholders must not emit Blueprint Compile / DataValidation warnings in S28P1.

	if (IsValid(StorageComponent))
	{
		TArray<FText> StorageErrors;
		TArray<FText> StorageWarnings;
		StorageComponent->ValidateStorageContract(StorageErrors, StorageWarnings);
		OutErrors.Append(StorageErrors);
		OutWarnings.Append(StorageWarnings);
	}

	return OutErrors.Num() == 0;
}

#if WITH_EDITOR
EDataValidationResult AGP_MainBase::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const bool bOk = ValidateMainBaseContract(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}
	return bOk ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
