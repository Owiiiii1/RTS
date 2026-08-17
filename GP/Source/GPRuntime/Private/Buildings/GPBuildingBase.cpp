// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPBuildingBase.h"

#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GPGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPBuildGridRegister, Log, All);

AGP_BuildingBase::AGP_BuildingBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	NavigationObstacle = CreateDefaultSubobject<UBoxComponent>(TEXT("NavigationObstacle"));
	ConfigureNavigationObstacleDefaults();
	// Attachment deferred until derived classes set Capsule root (PostInitializeComponents).

	PlacementFootprintBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("PlacementFootprintBounds"));
	ConfigurePlacementFootprintBoundsDefaults();

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
	if (GPTags.Selection_Type_Building.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Selection_Type_Building);
	}
	if (GPTags.Unit_Type_Building.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Unit_Type_Building);
	}
}

void AGP_BuildingBase::ConfigureNavigationObstacleDefaults()
{
	if (!NavigationObstacle)
	{
		return;
	}

	NavigationObstacle->SetBoxExtent(FVector(140.0f, 140.0f, 120.0f));
	NavigationObstacle->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	NavigationObstacle->SetCollisionObjectType(ECC_WorldStatic);
	NavigationObstacle->SetCollisionResponseToAllChannels(ECR_Ignore);
	NavigationObstacle->SetGenerateOverlapEvents(false);
	NavigationObstacle->SetSimulatePhysics(false);
	NavigationObstacle->SetHiddenInGame(true);
	NavigationObstacle->SetVisibility(true);
	NavigationObstacle->SetCanEverAffectNavigation(true);
	NavigationObstacle->bDynamicObstacle = true;
	NavigationObstacle->SetAreaClassOverride(UNavArea_Null::StaticClass());
	NavigationObstacle->bEditableWhenInherited = true;
	// Intentionally inherits actor/root scale: nav volume tracks presentation size.
}

void AGP_BuildingBase::ConfigurePlacementFootprintBoundsDefaults()
{
	if (!PlacementFootprintBounds)
	{
		return;
	}

	// Visible native default: generic 1×1 (200×200 cm). Derived classes override XY.
	PlacementFootprintBounds->SetBoxExtent(FVector(100.0f, 100.0f, 20.0f));
	PlacementFootprintBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlacementFootprintBounds->SetCollisionObjectType(ECC_WorldStatic);
	PlacementFootprintBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	PlacementFootprintBounds->SetGenerateOverlapEvents(false);
	PlacementFootprintBounds->SetSimulatePhysics(false);
	PlacementFootprintBounds->SetHiddenInGame(true);
	PlacementFootprintBounds->SetVisibility(true);
	PlacementFootprintBounds->SetCanEverAffectNavigation(false);
	PlacementFootprintBounds->bDynamicObstacle = false;
	PlacementFootprintBounds->SetAreaClassOverride(nullptr);
	PlacementFootprintBounds->ShapeColor = FColor(0, 180, 255);
	PlacementFootprintBounds->SetLineThickness(2.0f);
	PlacementFootprintBounds->bEditableWhenInherited = true;
	ApplyPlacementFootprintWorldAxisPolicy();
}

void AGP_BuildingBase::ApplyPlacementFootprintWorldAxisPolicy()
{
	if (PlacementFootprintBounds == nullptr)
	{
		return;
	}

	// Verified UE 5.8 USceneComponent::SetAbsolute(bAbsLoc, bAbsRot, bAbsScale):
	// CalcNewComponentToWorld_GeneralCase copies RelativeRotation / RelativeScale3D into
	// world rotation/scale when those axes are absolute. Location stays parent-relative,
	// so Rel * Parent still applies actor yaw to the authored local offset.
	// GP-S36G: no rotated footprints — force world-zero box axes. Rotation support deferred.
	PlacementFootprintBounds->SetRelativeRotation(FRotator::ZeroRotator);
	PlacementFootprintBounds->SetAbsolute(false, true, true);
}

void AGP_BuildingBase::AttachDeferredComponentToRoot(USceneComponent* Component)
{
	if (Component == nullptr)
	{
		return;
	}

	USceneComponent* Root = GetRootComponent();
	if (Root == nullptr || Component == Root)
	{
		return;
	}

	if (Component->GetAttachParent() == Root)
	{
		return;
	}

	if (Component->IsRegistered())
	{
		Component->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		Component->SetupAttachment(Root);
	}
}

void AGP_BuildingBase::AttachPlacementFootprintBoundsToRoot()
{
	AttachDeferredComponentToRoot(PlacementFootprintBounds);
}

void AGP_BuildingBase::AttachNavigationObstacleToRoot()
{
	AttachDeferredComponentToRoot(NavigationObstacle);
}

void AGP_BuildingBase::AttachDeferredSceneComponentsToRoot()
{
	AttachNavigationObstacleToRoot();
	AttachPlacementFootprintBoundsToRoot();
}

void AGP_BuildingBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AttachDeferredSceneComponentsToRoot();
	ApplyPlacementFootprintWorldAxisPolicy();
	TryApplyClassDesignToLivePlacementFootprintBounds();
}

void AGP_BuildingBase::PostLoad()
{
	Super::PostLoad();
	ApplyPlacementFootprintWorldAxisPolicy();
	TryApplyClassDesignToLivePlacementFootprintBounds();
}

void AGP_BuildingBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	AttachDeferredSceneComponentsToRoot();
	ApplyPlacementFootprintWorldAxisPolicy();
	TryApplyClassDesignToLivePlacementFootprintBounds();
}

void AGP_BuildingBase::ApplyClassDesignToLivePlacementFootprintBounds()
{
	if (PlacementFootprintBounds == nullptr || HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	const AGP_BuildingBase* ClassCDO = GetClass() != nullptr
		? GetClass()->GetDefaultObject<AGP_BuildingBase>()
		: nullptr;
	const UBoxComponent* Design = ClassCDO != nullptr ? ClassCDO->GetPlacementFootprintBounds() : nullptr;
	if (Design == nullptr || Design == PlacementFootprintBounds)
	{
		return;
	}

	PlacementFootprintBounds->SetBoxExtent(Design->GetUnscaledBoxExtent());
	PlacementFootprintBounds->SetRelativeLocation(Design->GetRelativeLocation());
	PlacementFootprintBounds->SetRelativeScale3D(Design->GetRelativeScale3D());
	ApplyPlacementFootprintWorldAxisPolicy();
}

void AGP_BuildingBase::TryApplyClassDesignToLivePlacementFootprintBounds()
{
	if (IsNetStartupActor())
	{
		ApplyClassDesignToLivePlacementFootprintBounds();
	}
}

void AGP_BuildingBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGP_BuildingBase, GridOriginCell);
	DOREPLIFETIME(AGP_BuildingBase, GridFootprintSize);
}

void AGP_BuildingBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyPlacementFootprintWorldAxisPolicy();
	TryApplyClassDesignToLivePlacementFootprintBounds();
	TryRegisterWithBuildGrid();
}

void AGP_BuildingBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TryUnregisterFromBuildGrid();
	Super::EndPlay(EndPlayReason);
}

void AGP_BuildingBase::ConfigureGridPlacement(FIntPoint OriginCell, FIntPoint FootprintSize)
{
	GridOriginCell = OriginCell;
	GridFootprintSize = FootprintSize;
	bGridPlacementConfigured = FootprintSize.X > 0 && FootprintSize.Y > 0;
	if (!GridOccupantId.IsValid())
	{
		GridOccupantId = FGuid::NewGuid();
	}
}

FIntPoint AGP_BuildingBase::ResolveFallbackFootprintSize() const
{
	if (GridFootprintSize.X > 0 && GridFootprintSize.Y > 0)
	{
		return GridFootprintSize;
	}
	if (IsA(AGP_MainBase::StaticClass()))
	{
		return FIntPoint(5, 5);
	}
	if (IsA(AGP_LogisticsHub::StaticClass()))
	{
		return FIntPoint(4, 4);
	}
	return FIntPoint(1, 1);
}

void AGP_BuildingBase::TryRegisterWithBuildGrid()
{
	if (bGridRegistered || !HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	UGP_BuildGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UGP_BuildGridSubsystem>() : nullptr;
	if (Grid == nullptr)
	{
		return;
	}

	const FGP_ResolvedBuildingFootprint Resolved = Grid->ResolveActorFootprint(this, nullptr);
	const FIntPoint Footprint = Resolved.IsValid() ? Resolved.SizeCells : ResolveFallbackFootprintSize();
	if (Footprint.X <= 0 || Footprint.Y <= 0)
	{
		return;
	}

	if (!bGridPlacementConfigured)
	{
		FIntPoint Origin = FIntPoint::ZeroValue;
		FVector Snapped = FVector::ZeroVector;
		const FVector FootprintCenterHint = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(
			GetPlacementFootprintBounds());
		Grid->ResolveSnappedPlacement(FootprintCenterHint, Footprint, Origin, Snapped);
		GridOriginCell = Origin;
		GridFootprintSize = Footprint;
		bGridPlacementConfigured = true;
	}

	if (!GridOccupantId.IsValid())
	{
		GridOccupantId = FGuid::NewGuid();
	}

	if (Grid->RegisterFootprint(this, GridOriginCell, GridFootprintSize, GridOccupantId))
	{
		bGridRegistered = true;
	}

#if !UE_BUILD_SHIPPING
	const UBoxComponent* LiveBounds = GetPlacementFootprintBounds();
	const FVector LiveCenter = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(LiveBounds);
	const FVector LiveHalf = UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(LiveBounds);
	const FVector LiveRel = LiveBounds != nullptr ? LiveBounds->GetRelativeLocation() : FVector::ZeroVector;
	const FVector ActorScale = GetActorScale3D();
	const FVector RootWorldScale = GetRootComponent() != nullptr
		? GetRootComponent()->GetComponentScale()
		: FVector::ZeroVector;
	const FVector BoundsRelScale = LiveBounds != nullptr ? LiveBounds->GetRelativeScale3D() : FVector::ZeroVector;
	const FVector BoundsCompScale = LiveBounds != nullptr ? LiveBounds->GetComponentScale() : FVector::ZeroVector;
	const FVector Unscaled = LiveBounds != nullptr ? LiveBounds->GetUnscaledBoxExtent() : FVector::ZeroVector;
	const FRotator BoundsRelRot = LiveBounds != nullptr ? LiveBounds->GetRelativeRotation() : FRotator::ZeroRotator;
	const FRotator BoundsWorldRot = LiveBounds != nullptr ? LiveBounds->GetComponentRotation() : FRotator::ZeroRotator;
	const FVector VisualHalf = LiveBounds != nullptr ? LiveBounds->GetScaledBoxExtent() : FVector::ZeroVector;
	UE_LOG(
		LogGPBuildGridRegister,
		Log,
		TEXT("BuildGrid occupancy %s liveCenter=(%.1f,%.1f,%.1f) liveRel=(%.1f,%.1f) actorYaw=%.1f boundsRelYaw=%.1f boundsWorldYaw=%.1f actorScale=(%.3f,%.3f,%.3f) rootWorldScale=(%.3f,%.3f,%.3f) boundsRelScale=(%.3f,%.3f,%.3f) boundsCompScale=(%.3f,%.3f,%.3f) unscaled=(%.1f,%.1f) authoredHalf=(%.1f,%.1f) visualHalf=(%.1f,%.1f) resolved=%dx%d origin=%d,%d registered=%dx%d fromLive=%s absRot=%s absScale=%s"),
		*GetName(),
		LiveCenter.X,
		LiveCenter.Y,
		LiveCenter.Z,
		LiveRel.X,
		LiveRel.Y,
		GetActorRotation().Yaw,
		BoundsRelRot.Yaw,
		BoundsWorldRot.Yaw,
		ActorScale.X,
		ActorScale.Y,
		ActorScale.Z,
		RootWorldScale.X,
		RootWorldScale.Y,
		RootWorldScale.Z,
		BoundsRelScale.X,
		BoundsRelScale.Y,
		BoundsRelScale.Z,
		BoundsCompScale.X,
		BoundsCompScale.Y,
		BoundsCompScale.Z,
		Unscaled.X,
		Unscaled.Y,
		LiveHalf.X,
		LiveHalf.Y,
		FMath::Abs(VisualHalf.X),
		FMath::Abs(VisualHalf.Y),
		Resolved.SizeCells.X,
		Resolved.SizeCells.Y,
		GridOriginCell.X,
		GridOriginCell.Y,
		GridFootprintSize.X,
		GridFootprintSize.Y,
		Resolved.bFromAuthoredBounds ? TEXT("1") : TEXT("0"),
		LiveBounds != nullptr && LiveBounds->IsUsingAbsoluteRotation() ? TEXT("1") : TEXT("0"),
		LiveBounds != nullptr && LiveBounds->IsUsingAbsoluteScale() ? TEXT("1") : TEXT("0"));
#endif
}

#if !UE_BUILD_SHIPPING
FString AGP_BuildingBase::GetBuildGridOccupancyDebugString() const
{
	FVector2D Offset = FVector2D::ZeroVector;
	FIntPoint ResolvedSize = FIntPoint::ZeroValue;
	if (const UWorld* World = GetWorld())
	{
		if (const UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>())
		{
			const FGP_ResolvedBuildingFootprint Resolved = Grid->ResolveActorFootprint(this, nullptr);
			ResolvedSize = Resolved.SizeCells;
			Offset = Resolved.LocalCenterOffsetCm;
		}
	}

	const FVector LiveCenter = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(
		GetPlacementFootprintBounds());
	return FString::Printf(
		TEXT("%s liveCenter=(%.1f,%.1f) yaw=%.1f local=(%.1f,%.1f) origin=%d,%d registered=%dx%d resolved=%dx%d"),
		*GetName(),
		LiveCenter.X,
		LiveCenter.Y,
		GetActorRotation().Yaw,
		Offset.X,
		Offset.Y,
		GridOriginCell.X,
		GridOriginCell.Y,
		GridFootprintSize.X,
		GridFootprintSize.Y,
		ResolvedSize.X,
		ResolvedSize.Y);
}
#endif

void AGP_BuildingBase::TryUnregisterFromBuildGrid()
{
	if (!bGridRegistered && !GridOccupantId.IsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (UGP_BuildGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UGP_BuildGridSubsystem>() : nullptr)
	{
		if (GridOccupantId.IsValid())
		{
			Grid->UnregisterOccupant(GridOccupantId);
		}
		else
		{
			Grid->UnregisterFootprint(this);
		}
	}

	bGridRegistered = false;
}
