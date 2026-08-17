// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPDropPod.h"

#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Buildings/GPBuildingBase.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "Orbital/GPBuildingGroundPlacement.h"
#include "Orbital/GPUnitGroundPlacement.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitBase.h"
#include "Units/GPWorker.h"

AGP_DropPod::AGP_DropPod()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bUseNativePlaceholder = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(SceneRoot);
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlaceholderMesh->SetCanEverAffectNavigation(false);
	PlaceholderMesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		PlaceholderMesh->SetStaticMesh(CylinderMesh.Object);
		PlaceholderMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 2.0f));
	}
}

void AGP_DropPod::ApplyNativePlaceholderVisibility()
{
	if (PlaceholderMesh == nullptr)
	{
		return;
	}
	const bool bShow = bUseNativePlaceholder && Phase == EGP_DropPodPhase::Descending;
	PlaceholderMesh->SetHiddenInGame(!bShow);
	PlaceholderMesh->SetVisibility(bShow);
}

void AGP_DropPod::HideNativePlaceholder()
{
	if (PlaceholderMesh == nullptr)
	{
		return;
	}
	PlaceholderMesh->SetHiddenInGame(true);
	PlaceholderMesh->SetVisibility(false);
}

void AGP_DropPod::AuthoritySetPhase(EGP_DropPodPhase NewPhase)
{
	if (!HasAuthority() || Phase == NewPhase)
	{
		return;
	}
	const EGP_DropPodPhase Previous = Phase;
	Phase = NewPhase;
	OnRep_Phase(Previous);
}

void AGP_DropPod::OnRep_Phase(EGP_DropPodPhase PreviousPhase)
{
	(void)PreviousPhase;
	if (Phase == EGP_DropPodPhase::Descending)
	{
		ApplyNativePlaceholderVisibility();
	}
	else if (Phase == EGP_DropPodPhase::Deploying || Phase == EGP_DropPodPhase::PayloadDeployed)
	{
		HideNativePlaceholder();
	}
}

void AGP_DropPod::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGP_DropPod, LandingLocation);
	DOREPLIFETIME(AGP_DropPod, LandingRotation);
	DOREPLIFETIME(AGP_DropPod, StartLocation);
	DOREPLIFETIME(AGP_DropPod, OwnerTeamId);
	DOREPLIFETIME(AGP_DropPod, DescentProgress01);
	DOREPLIFETIME(AGP_DropPod, Phase);
	DOREPLIFETIME(AGP_DropPod, PayloadKind);
}

void AGP_DropPod::BeginPlay()
{
	Super::BeginPlay();
	ApplyNativePlaceholderVisibility();
}

void AGP_DropPod::ClearLifecycleTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeployTimerHandle);
		World->GetTimerManager().ClearTimer(CleanupTimerHandle);
	}
}

void AGP_DropPod::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLifecycleTimers();
	AuthorityReleaseLeftoverUnitReservation();
	AuthorityReleaseBuildingGridReservation();
	Super::EndPlay(EndPlayReason);
}

void AGP_DropPod::AuthorityReleaseLeftoverUnitReservation()
{
	if (!HasAuthority() || RemainingUnitReservation <= 0)
	{
		return;
	}

	if (AGP_PlayerState* PS = RequestingPlayerStateWeak.Get())
	{
		PS->ReleaseOrbitalUnitReservation(RemainingUnitReservation);
	}
	RemainingUnitReservation = 0;
}

void AGP_DropPod::AuthorityInitUnitDrop(
	AGP_PlayerState* RequestingPlayerState,
	int32 TeamId,
	const FVector& LandingWorldLocation,
	const FRotator& LandingWorldRotation,
	const FGP_UnitDropManifest& Manifest,
	float InDescentDurationSeconds,
	float SpawnAltitudeCm,
	float InSpawnSpacingCm,
	float InPayloadDeployDelaySeconds,
	float InCleanupDelaySeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	RequestingPlayerStateWeak = RequestingPlayerState;
	OwnerTeamId = TeamId;
	PayloadKind = EGP_DropPodPayloadKind::Unit;
	PendingDropDefinitionId = FPrimaryAssetId();
	PendingBuildingPayloadClass = nullptr;
	PendingGridOriginCell = FIntPoint::ZeroValue;
	PendingGridFootprintSize = FIntPoint::ZeroValue;
	BuildingGridReservationId.Invalidate();
	bGridReservationPromoted = false;
	PendingManifest = Manifest;
	LandingLocation = LandingWorldLocation;
	LandingRotation = LandingWorldRotation;
	DescentDurationSeconds = FMath::Max(0.05f, InDescentDurationSeconds);
	SpawnSpacingCm = FMath::Max(50.0f, InSpawnSpacingCm);
	PayloadDeployDelaySeconds = FMath::Max(0.0f, InPayloadDeployDelaySeconds);
	CleanupDelaySeconds = FMath::Max(0.0f, InCleanupDelaySeconds);
	StartLocation = LandingLocation + FVector(0.0f, 0.0f, FMath::Max(100.0f, SpawnAltitudeCm));
	DescentElapsed = 0.0f;
	DescentProgress01 = 0.0f;
	bLandingCompleted = false;
	bPayloadSpawned = false;
	RemainingUnitReservation = FMath::Max(0, Manifest.GetTotalUnitCount());
#if !UE_BUILD_SHIPPING
	bDebugSkipPayloadSpawn = false;
#endif
	ClearLifecycleTimers();

	AuthoritySetPhase(EGP_DropPodPhase::Descending);
	ApplyNativePlaceholderVisibility();
	SetActorLocationAndRotation(StartLocation, LandingRotation);
	SetActorTickEnabled(true);
	Multicast_PresentationDescentStarted();
}

void AGP_DropPod::AuthorityInitBuildingDrop(
	AGP_PlayerState* RequestingPlayerState,
	int32 TeamId,
	FPrimaryAssetId DropDefinitionId,
	TSubclassOf<AGP_BuildingBase> PayloadClass,
	const FVector& LandingWorldLocation,
	const FRotator& LandingWorldRotation,
	float InDescentDurationSeconds,
	float SpawnAltitudeCm,
	float InPayloadDeployDelaySeconds,
	float InCleanupDelaySeconds,
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	FGuid GridReservationId)
{
	if (!HasAuthority())
	{
		return;
	}

	RequestingPlayerStateWeak = RequestingPlayerState;
	OwnerTeamId = TeamId;
	PayloadKind = EGP_DropPodPayloadKind::Building;
	PendingDropDefinitionId = DropDefinitionId;
	PendingBuildingPayloadClass = PayloadClass;
	PendingManifest = FGP_UnitDropManifest();
	PendingGridOriginCell = OriginCell;
	PendingGridFootprintSize = FootprintSize;
	BuildingGridReservationId = GridReservationId;
	bGridReservationPromoted = false;
	LandingLocation = LandingWorldLocation;
	LandingRotation = LandingWorldRotation;
	DescentDurationSeconds = FMath::Max(0.05f, InDescentDurationSeconds);
	PayloadDeployDelaySeconds = FMath::Max(0.0f, InPayloadDeployDelaySeconds);
	CleanupDelaySeconds = FMath::Max(0.0f, InCleanupDelaySeconds);
	StartLocation = LandingLocation + FVector(0.0f, 0.0f, FMath::Max(100.0f, SpawnAltitudeCm));
	DescentElapsed = 0.0f;
	DescentProgress01 = 0.0f;
	bLandingCompleted = false;
	bPayloadSpawned = false;
	RemainingUnitReservation = 0;
	ClearLifecycleTimers();

	AuthoritySetPhase(EGP_DropPodPhase::Descending);
	ApplyNativePlaceholderVisibility();
	SetActorLocationAndRotation(StartLocation, LandingRotation);
	SetActorTickEnabled(true);
	Multicast_PresentationDescentStarted();
}

void AGP_DropPod::Multicast_PresentationDescentStarted_Implementation()
{
	ApplyNativePlaceholderVisibility();
	OnDescentStarted();
}

void AGP_DropPod::Multicast_PresentationImpact_Implementation()
{
	HideNativePlaceholder();
	OnImpact();
}

void AGP_DropPod::Multicast_PresentationPayloadDeployed_Implementation()
{
	OnPayloadDeployed();
}

void AGP_DropPod::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Phase != EGP_DropPodPhase::Descending || bLandingCompleted)
	{
		return;
	}

	DescentElapsed += DeltaSeconds;
	const float Duration = FMath::Max(0.05f, DescentDurationSeconds);
	DescentProgress01 = FMath::Clamp(DescentElapsed / Duration, 0.0f, 1.0f);
	const FVector NewLoc = FMath::Lerp(StartLocation, LandingLocation, DescentProgress01);
	SetActorLocation(NewLoc);

	if (HasAuthority() && DescentProgress01 >= 1.0f - KINDA_SMALL_NUMBER)
	{
		AuthorityCompleteLanding();
	}
}

void AGP_DropPod::AuthorityCompleteLanding()
{
	if (!HasAuthority() || bLandingCompleted)
	{
		return;
	}

	bLandingCompleted = true;
	DescentProgress01 = 1.0f;
	SetActorLocationAndRotation(LandingLocation, LandingRotation);
	SetActorTickEnabled(false);

	AuthoritySetPhase(EGP_DropPodPhase::Deploying);
	Multicast_PresentationImpact();

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (PayloadDeployDelaySeconds <= KINDA_SMALL_NUMBER)
	{
		AuthorityBeginPayloadDeploy();
		return;
	}

	World->GetTimerManager().SetTimer(
		DeployTimerHandle,
		FTimerDelegate::CreateUObject(this, &AGP_DropPod::AuthorityBeginPayloadDeploy),
		PayloadDeployDelaySeconds,
		false);
}

void AGP_DropPod::AuthorityBeginPayloadDeploy()
{
	if (!HasAuthority() || bPayloadSpawned)
	{
		return;
	}

	if (PayloadKind == EGP_DropPodPayloadKind::Building)
	{
		AuthoritySpawnBuildingPayload();
	}
	else
	{
		AuthoritySpawnUnitPayload();
	}
	AuthoritySetPhase(EGP_DropPodPhase::PayloadDeployed);
	Multicast_PresentationPayloadDeployed();
	AuthorityScheduleCleanup();
}

void AGP_DropPod::AuthoritySpawnUnitPayload()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || World == nullptr || bPayloadSpawned)
	{
		return;
	}
	bPayloadSpawned = true;

#if !UE_BUILD_SHIPPING
	if (bDebugSkipPayloadSpawn)
	{
		AuthorityReleaseLeftoverUnitReservation();
		UE_LOG(LogTemp, Log,
			TEXT("GP UnitCap DropPod skip payload spawn: Pod=%s ReleasedReservation"),
			*GetName());
		return;
	}
#endif

	const int32 WorkerCount = FMath::Max(0, PendingManifest.WorkerCount);
	const int32 WalkerCount = FMath::Max(0, PendingManifest.SalvageWalkerCount);
	const int32 Total = WorkerCount + WalkerCount;
	if (Total <= 0)
	{
		return;
	}

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const TSubclassOf<AGP_Worker> WorkerClass =
		Settings != nullptr ? Settings->ResolveWorkerPayloadClass() : TSubclassOf<AGP_Worker>(AGP_Worker::StaticClass());
	const TSubclassOf<AGP_SalvageWalker> WalkerClass =
		Settings != nullptr
			? Settings->ResolveSalvageWalkerPayloadClass()
			: TSubclassOf<AGP_SalvageWalker>(AGP_SalvageWalker::StaticClass());

	const FVector Forward = LandingRotation.RotateVector(FVector::ForwardVector).GetSafeNormal2D();
	const FVector Right = LandingRotation.RotateVector(FVector::RightVector).GetSafeNormal2D();
	const FVector BasisF = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward;
	const FVector BasisR = Right.IsNearlyZero() ? FVector::RightVector : Right;

	auto SpawnXY = [&](int32 Index) -> FVector
	{
		if (Total == 1)
		{
			return FVector(LandingLocation.X, LandingLocation.Y, LandingLocation.Z);
		}
		const float Angle = (2.0f * PI * static_cast<float>(Index)) / static_cast<float>(Total);
		const FVector Offset = (BasisF * FMath::Cos(Angle) + BasisR * FMath::Sin(Angle)) * SpawnSpacingCm;
		return FVector(
			LandingLocation.X + Offset.X,
			LandingLocation.Y + Offset.Y,
			LandingLocation.Z);
	};

	int32 SpawnIndex = 0;
	auto SpawnTyped = [&](UClass* Class)
	{
		if (Class == nullptr)
		{
			return;
		}
		const FVector Ground = SpawnXY(SpawnIndex++);
		const float OffsetZ = GPUnitGroundPlacement::GetGroundSpawnOffsetZForUnitClass(Class);
		const FVector Loc(Ground.X, Ground.Y, Ground.Z + OffsetZ);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Params.Owner = RequestingPlayerStateWeak.Get();

		AGP_UnitBase* Unit = World->SpawnActor<AGP_UnitBase>(Class, Loc, LandingRotation, Params);
		if (!IsValid(Unit))
		{
			return;
		}
		Unit->SetTeamId(OwnerTeamId);
		if (Unit->HasBeenCountedTowardPlayerUnitCap())
		{
			RemainingUnitReservation = FMath::Max(0, RemainingUnitReservation - 1);
		}
	};

	for (int32 i = 0; i < WorkerCount; ++i)
	{
		SpawnTyped(*WorkerClass);
	}
	for (int32 i = 0; i < WalkerCount; ++i)
	{
		SpawnTyped(*WalkerClass);
	}

	AuthorityReleaseLeftoverUnitReservation();
}

void AGP_DropPod::AuthorityReleaseBuildingGridReservation()
{
	if (!HasAuthority() || bGridReservationPromoted || !BuildingGridReservationId.IsValid())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>())
		{
			Grid->ReleaseReservation(BuildingGridReservationId);
		}
	}
	BuildingGridReservationId.Invalidate();
}

void AGP_DropPod::AuthoritySpawnBuildingPayload()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || World == nullptr || bPayloadSpawned)
	{
		return;
	}
	bPayloadSpawned = true;

#if !UE_BUILD_SHIPPING
	if (bDebugSkipPayloadSpawn)
	{
		AuthorityReleaseBuildingGridReservation();
		return;
	}
#endif

	if (PendingBuildingPayloadClass == nullptr)
	{
		AuthorityReleaseBuildingGridReservation();
		return;
	}

	TSubclassOf<AGP_BuildingBase> BuildingClass = PendingBuildingPayloadClass;
	if (BuildingClass == nullptr)
	{
		AuthorityReleaseBuildingGridReservation();
		return;
	}

	const float OffsetZ = GPBuildingGroundPlacement::GetGroundSpawnOffsetZForBuildingClass(*BuildingClass);
	const FVector Loc(LandingLocation.X, LandingLocation.Y, LandingLocation.Z + OffsetZ);
	const FTransform SpawnTM(LandingRotation, Loc);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = RequestingPlayerStateWeak.Get();

	AGP_BuildingBase* Building = World->SpawnActorDeferred<AGP_BuildingBase>(
		BuildingClass,
		SpawnTM,
		Params.Owner,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Building))
	{
		AuthorityReleaseBuildingGridReservation();
		return;
	}

	Building->ConfigureGridPlacement(PendingGridOriginCell, PendingGridFootprintSize);
	if (UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>())
	{
		if (Grid->PromoteReservationToBuilding(
				BuildingGridReservationId,
				Building,
				Building->GetGridOccupantId()))
		{
			bGridReservationPromoted = true;
		}
		else
		{
			Grid->ReleaseReservation(BuildingGridReservationId);
			bGridReservationPromoted = true;
		}
	}
	else
	{
		bGridReservationPromoted = true;
	}

	Building->FinishSpawning(SpawnTM);
	Building->SetTeamId(OwnerTeamId);
}

void AGP_DropPod::AuthorityScheduleCleanup()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (CleanupDelaySeconds <= KINDA_SMALL_NUMBER)
	{
		HandleCleanup();
		return;
	}

	World->GetTimerManager().SetTimer(
		CleanupTimerHandle,
		FTimerDelegate::CreateUObject(this, &AGP_DropPod::HandleCleanup),
		CleanupDelaySeconds,
		false);
}

void AGP_DropPod::HandleCleanup()
{
	if (!HasAuthority())
	{
		return;
	}
	ClearLifecycleTimers();
	Destroy();
}