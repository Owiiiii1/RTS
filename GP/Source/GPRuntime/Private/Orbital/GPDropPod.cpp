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
	Super::EndPlay(EndPlayReason);
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
	PendingBuildingType = EGP_OrbitalBuildingType::None;
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
	EGP_OrbitalBuildingType BuildingType,
	const FVector& LandingWorldLocation,
	const FRotator& LandingWorldRotation,
	float InDescentDurationSeconds,
	float SpawnAltitudeCm,
	float InPayloadDeployDelaySeconds,
	float InCleanupDelaySeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	RequestingPlayerStateWeak = RequestingPlayerState;
	OwnerTeamId = TeamId;
	PayloadKind = EGP_DropPodPayloadKind::Building;
	PendingBuildingType = BuildingType;
	PendingManifest = FGP_UnitDropManifest();
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
	};

	for (int32 i = 0; i < WorkerCount; ++i)
	{
		SpawnTyped(*WorkerClass);
	}
	for (int32 i = 0; i < WalkerCount; ++i)
	{
		SpawnTyped(*WalkerClass);
	}

	// Soft-open: only mutate CurrentUnits when MaxUnits is an active ceiling.
	if (AGP_PlayerState* PS = RequestingPlayerStateWeak.Get())
	{
		if (UGP_PlayerAttributeSet* Attr = const_cast<UGP_PlayerAttributeSet*>(PS->GetPlayerAttributeSet()))
		{
			const float MaxUnits = Attr->GetMaxUnits();
			if (MaxUnits > KINDA_SMALL_NUMBER)
			{
				Attr->SetCurrentUnits(Attr->GetCurrentUnits() + static_cast<float>(Total));
			}
		}
	}
}

void AGP_DropPod::AuthoritySpawnBuildingPayload()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || World == nullptr || bPayloadSpawned)
	{
		return;
	}
	bPayloadSpawned = true;

	if (PendingBuildingType == EGP_OrbitalBuildingType::None)
	{
		return;
	}

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	TSubclassOf<AGP_BuildingBase> BuildingClass = AGP_LogisticsHub::StaticClass();
	if (PendingBuildingType == EGP_OrbitalBuildingType::LogisticsHub)
	{
		BuildingClass = Settings != nullptr
			? Settings->ResolveBuildingPayloadClass()
			: TSubclassOf<AGP_BuildingBase>(AGP_LogisticsHub::StaticClass());
	}

	if (BuildingClass == nullptr)
	{
		return;
	}

	const float OffsetZ = GPBuildingGroundPlacement::GetGroundSpawnOffsetZForBuildingClass(*BuildingClass);
	const FVector Loc(LandingLocation.X, LandingLocation.Y, LandingLocation.Z + OffsetZ);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Owner = RequestingPlayerStateWeak.Get();

	AGP_BuildingBase* Building = World->SpawnActor<AGP_BuildingBase>(BuildingClass, Loc, LandingRotation, Params);
	if (!IsValid(Building))
	{
		return;
	}
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