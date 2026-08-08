// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPDropPod.h"

#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/GPPlayerState.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitBase.h"
#include "Units/GPWorker.h"

AGP_DropPod::AGP_DropPod()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

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

void AGP_DropPod::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGP_DropPod, LandingLocation);
	DOREPLIFETIME(AGP_DropPod, LandingRotation);
	DOREPLIFETIME(AGP_DropPod, StartLocation);
	DOREPLIFETIME(AGP_DropPod, OwnerTeamId);
	DOREPLIFETIME(AGP_DropPod, DescentProgress01);
	DOREPLIFETIME(AGP_DropPod, bDescending);
}

void AGP_DropPod::BeginPlay()
{
	Super::BeginPlay();
}

void AGP_DropPod::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CleanupTimerHandle);
	}
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
	float InCleanupDelaySeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	RequestingPlayerStateWeak = RequestingPlayerState;
	OwnerTeamId = TeamId;
	PendingManifest = Manifest;
	LandingLocation = LandingWorldLocation;
	LandingRotation = LandingWorldRotation;
	DescentDurationSeconds = FMath::Max(0.05f, InDescentDurationSeconds);
	SpawnSpacingCm = FMath::Max(50.0f, InSpawnSpacingCm);
	CleanupDelaySeconds = FMath::Max(0.0f, InCleanupDelaySeconds);
	StartLocation = LandingLocation + FVector(0.0f, 0.0f, FMath::Max(100.0f, SpawnAltitudeCm));
	DescentElapsed = 0.0f;
	DescentProgress01 = 0.0f;
	bLandingCompleted = false;
	bDescending = true;

	SetActorLocationAndRotation(StartLocation, LandingRotation);
	SetActorTickEnabled(true);
	OnDescentStarted();
}

void AGP_DropPod::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bDescending || bLandingCompleted)
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
	bDescending = false;
	DescentProgress01 = 1.0f;
	SetActorLocationAndRotation(LandingLocation, LandingRotation);
	SetActorTickEnabled(false);

	OnImpact();
	AuthoritySpawnUnitPayload();
	OnPayloadDeployed();
	AuthorityScheduleCleanup();
}

void AGP_DropPod::AuthoritySpawnUnitPayload()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || World == nullptr)
	{
		return;
	}

	const int32 WorkerCount = FMath::Max(0, PendingManifest.WorkerCount);
	const int32 WalkerCount = FMath::Max(0, PendingManifest.SalvageWalkerCount);
	const int32 Total = WorkerCount + WalkerCount;
	if (Total <= 0)
	{
		return;
	}

	const FVector Forward = LandingRotation.RotateVector(FVector::ForwardVector).GetSafeNormal2D();
	const FVector Right = LandingRotation.RotateVector(FVector::RightVector).GetSafeNormal2D();
	const FVector BasisF = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward;
	const FVector BasisR = Right.IsNearlyZero() ? FVector::RightVector : Right;

	auto SpawnOffset = [&](int32 Index) -> FVector
	{
		if (Total == 1)
		{
			return LandingLocation;
		}
		const float Angle = (2.0f * PI * static_cast<float>(Index)) / static_cast<float>(Total);
		const FVector Offset = (BasisF * FMath::Cos(Angle) + BasisR * FMath::Sin(Angle)) * SpawnSpacingCm;
		return LandingLocation + Offset;
	};

	int32 SpawnIndex = 0;
	auto SpawnTyped = [&](TSubclassOf<AGP_UnitBase> Class)
	{
		if (*Class == nullptr)
		{
			return;
		}
		const FVector Loc = SpawnOffset(SpawnIndex++);
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
		SpawnTyped(AGP_Worker::StaticClass());
	}
	for (int32 i = 0; i < WalkerCount; ++i)
	{
		SpawnTyped(AGP_SalvageWalker::StaticClass());
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
	Destroy();
}
