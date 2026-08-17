// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/Grid/GPBuildGridSubsystem.h"

#include "Buildings/GPBuildingBase.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "Orbital/GPBuildingPlacementGhost.h"
#include "Orbital/GPDropPod.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPBuildGrid, Log, All);

bool UGP_BuildGridSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World != nullptr && World->IsGameWorld();
}

void UGP_BuildGridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CellSize = DefaultCellSizeCm;
	GridOriginXY = FVector2D::ZeroVector;
	CellOccupancy.Reset();
	OccupantCells.Reset();
	OccupantActors.Reset();
	ReservationIds.Reset();
}

void UGP_BuildGridSubsystem::Deinitialize()
{
	CellOccupancy.Reset();
	OccupantCells.Reset();
	OccupantActors.Reset();
	ReservationIds.Reset();
	Super::Deinitialize();
}

int32 UGP_BuildGridSubsystem::WorldToCell1D(float WorldCoord) const
{
	const float Origin = 0.0f;
	const float Size = FMath::Max(1.0f, CellSize);
	return FMath::FloorToInt(((WorldCoord - Origin) / Size) + 0.5f);
}

int32 UGP_BuildGridSubsystem::SnapOrigin1D(float WorldCoord, int32 Size) const
{
	const int32 SafeSize = FMath::Max(1, Size);
	const float OffsetCells = 0.5f * static_cast<float>(SafeSize - 1);
	const float GridSize = FMath::Max(1.0f, CellSize);
	return FMath::FloorToInt((WorldCoord / GridSize) - OffsetCells + 0.5f);
}

FIntPoint UGP_BuildGridSubsystem::WorldToCell(FVector World) const
{
	return FIntPoint(WorldToCell1D(World.X), WorldToCell1D(World.Y));
}

FVector UGP_BuildGridSubsystem::CellToWorld(FIntPoint Cell, float GroundZ) const
{
	return FVector(
		GridOriginXY.X + static_cast<float>(Cell.X) * CellSize,
		GridOriginXY.Y + static_cast<float>(Cell.Y) * CellSize,
		GroundZ);
}

FIntPoint UGP_BuildGridSubsystem::SnapOriginCell(FVector World, FIntPoint FootprintSize) const
{
	return FIntPoint(
		SnapOrigin1D(World.X, FootprintSize.X),
		SnapOrigin1D(World.Y, FootprintSize.Y));
}

FVector UGP_BuildGridSubsystem::GetFootprintCenterWorld(
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	float GroundZ) const
{
	const int32 Width = FMath::Max(1, FootprintSize.X);
	const int32 Height = FMath::Max(1, FootprintSize.Y);
	const FVector MinCenter = CellToWorld(OriginCell, GroundZ);
	return FVector(
		MinCenter.X + 0.5f * static_cast<float>(Width - 1) * CellSize,
		MinCenter.Y + 0.5f * static_cast<float>(Height - 1) * CellSize,
		GroundZ);
}

void UGP_BuildGridSubsystem::EnumerateFootprintCells(
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	TArray<FIntPoint>& OutCells) const
{
	OutCells.Reset();
	if (!IsValidFootprintSize(FootprintSize))
	{
		return;
	}

	OutCells.Reserve(FootprintSize.X * FootprintSize.Y);
	for (int32 Y = 0; Y < FootprintSize.Y; ++Y)
	{
		for (int32 X = 0; X < FootprintSize.X; ++X)
		{
			OutCells.Add(FIntPoint(OriginCell.X + X, OriginCell.Y + Y));
		}
	}
}

void UGP_BuildGridSubsystem::GetFootprintWorldAABB(
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	float GroundZ,
	FVector& OutMin,
	FVector& OutMax) const
{
	const int32 Width = FMath::Max(1, FootprintSize.X);
	const int32 Height = FMath::Max(1, FootprintSize.Y);
	const FVector FirstCenter = CellToWorld(OriginCell, GroundZ);
	OutMin = FirstCenter - FVector(CellSize * 0.5f, CellSize * 0.5f, 0.0f);
	OutMax = OutMin + FVector(static_cast<float>(Width) * CellSize, static_cast<float>(Height) * CellSize, 0.0f);
}

bool UGP_BuildGridSubsystem::DoFootprintsOverlap(
	FIntPoint OriginA,
	FIntPoint SizeA,
	FIntPoint OriginB,
	FIntPoint SizeB) const
{
	if (!IsValidFootprintSize(SizeA) || !IsValidFootprintSize(SizeB))
	{
		return false;
	}
	return OriginA.X < OriginB.X + SizeB.X
		&& OriginB.X < OriginA.X + SizeA.X
		&& OriginA.Y < OriginB.Y + SizeB.Y
		&& OriginB.Y < OriginA.Y + SizeA.Y;
}

bool UGP_BuildGridSubsystem::ResolveSnappedPlacement(
	const FVector& RequestedWorld,
	FIntPoint FootprintSize,
	FIntPoint& OutOriginCell,
	FVector& OutSnappedGroundLocation) const
{
	if (!IsValidFootprintSize(FootprintSize)
		|| RequestedWorld.ContainsNaN()
		|| !FMath::IsFinite(RequestedWorld.X)
		|| !FMath::IsFinite(RequestedWorld.Y)
		|| !FMath::IsFinite(RequestedWorld.Z))
	{
		OutOriginCell = FIntPoint::ZeroValue;
		OutSnappedGroundLocation = FVector::ZeroVector;
		return false;
	}

	OutOriginCell = SnapOriginCell(RequestedWorld, FootprintSize);
	OutSnappedGroundLocation = GetFootprintCenterWorld(OutOriginCell, FootprintSize, RequestedWorld.Z);
	return true;
}

bool UGP_BuildGridSubsystem::IsValidFootprintSize(FIntPoint FootprintSize) const
{
	return FootprintSize.X > 0 && FootprintSize.Y > 0;
}

bool UGP_BuildGridSubsystem::IsRecordIgnored(
	const FGP_GridCellRecord& Record,
	AActor* IgnoreActor,
	const FGuid& IgnoreReservationId) const
{
	if (IgnoreReservationId.IsValid() && Record.OccupantId == IgnoreReservationId)
	{
		return true;
	}
	if (IsValid(IgnoreActor) && Record.OccupantActor.Get() == IgnoreActor)
	{
		return true;
	}
	return false;
}

bool UGP_BuildGridSubsystem::IsCellOccupied(FIntPoint Cell, AActor* IgnoreActor) const
{
	const FGP_GridCellRecord* Record = CellOccupancy.Find(Cell);
	if (Record == nullptr)
	{
		return false;
	}
	if (IsRecordIgnored(*Record, IgnoreActor, FGuid()))
	{
		return false;
	}
	if (Record->bIsReservation)
	{
		return true;
	}
	return Record->OccupantActor.IsValid();
}

AActor* UGP_BuildGridSubsystem::GetActorAtCell(FIntPoint Cell) const
{
	const FGP_GridCellRecord* Record = CellOccupancy.Find(Cell);
	if (Record == nullptr)
	{
		return nullptr;
	}
	return Record->OccupantActor.Get();
}

bool UGP_BuildGridSubsystem::CanPlaceFootprint(
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	EGP_GridRejectReason& OutReason,
	AActor* IgnoreActor,
	const FGuid& IgnoreReservationId)
{
	SweepStaleOccupants();
	OutReason = EGP_GridRejectReason::Free;
	if (!IsValidFootprintSize(FootprintSize))
	{
		OutReason = EGP_GridRejectReason::InvalidFootprint;
		return false;
	}

	TArray<FIntPoint> Cells;
	EnumerateFootprintCells(OriginCell, FootprintSize, Cells);
	for (const FIntPoint& Cell : Cells)
	{
		const FGP_GridCellRecord* Record = CellOccupancy.Find(Cell);
		if (Record == nullptr)
		{
			continue;
		}
		if (IsRecordIgnored(*Record, IgnoreActor, IgnoreReservationId))
		{
			continue;
		}
		if (Record->bIsReservation || Record->OccupantActor.IsValid() || OccupantCells.Contains(Record->OccupantId))
		{
			OutReason = EGP_GridRejectReason::CellOccupied;
			return false;
		}
	}

	return true;
}

bool UGP_BuildGridSubsystem::RegisterFootprint(
	AActor* Building,
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	FGuid OccupantId)
{
	if (!IsValid(Building) || !OccupantId.IsValid() || !IsValidFootprintSize(FootprintSize))
	{
		return false;
	}

	SweepStaleOccupants();

	TArray<FIntPoint> Cells;
	EnumerateFootprintCells(OriginCell, FootprintSize, Cells);

	if (const TArray<FIntPoint>* Existing = OccupantCells.Find(OccupantId))
	{
		if (*Existing == Cells)
		{
			OccupantActors.Add(OccupantId, Building);
			for (const FIntPoint& Cell : Cells)
			{
				FGP_GridCellRecord& Record = CellOccupancy.FindOrAdd(Cell);
				Record.OccupantId = OccupantId;
				Record.OccupantActor = Building;
				Record.bIsReservation = false;
			}
			ReservationIds.Remove(OccupantId);
			return true;
		}
		UnregisterOccupant(OccupantId);
	}

	EGP_GridRejectReason Reason = EGP_GridRejectReason::Free;
	if (!CanPlaceFootprint(OriginCell, FootprintSize, Reason, Building, OccupantId))
	{
		return false;
	}

	OccupantCells.Add(OccupantId, Cells);
	OccupantActors.Add(OccupantId, Building);
	ReservationIds.Remove(OccupantId);
	for (const FIntPoint& Cell : Cells)
	{
		FGP_GridCellRecord Record;
		Record.OccupantId = OccupantId;
		Record.OccupantActor = Building;
		Record.bIsReservation = false;
		CellOccupancy.Add(Cell, Record);
	}
	return true;
}

void UGP_BuildGridSubsystem::UnregisterFootprint(AActor* Building)
{
	if (!IsValid(Building))
	{
		return;
	}

	TArray<FGuid> ToRemove;
	for (const TPair<FGuid, TWeakObjectPtr<AActor>>& Pair : OccupantActors)
	{
		if (Pair.Value.Get() == Building)
		{
			ToRemove.Add(Pair.Key);
		}
	}
	for (const FGuid& Id : ToRemove)
	{
		UnregisterOccupant(Id);
	}
}

void UGP_BuildGridSubsystem::UnregisterOccupant(const FGuid& OccupantId)
{
	if (!OccupantId.IsValid())
	{
		return;
	}

	if (const TArray<FIntPoint>* Cells = OccupantCells.Find(OccupantId))
	{
		for (const FIntPoint& Cell : *Cells)
		{
			if (const FGP_GridCellRecord* Record = CellOccupancy.Find(Cell))
			{
				if (Record->OccupantId == OccupantId)
				{
					CellOccupancy.Remove(Cell);
				}
			}
		}
	}
	OccupantCells.Remove(OccupantId);
	OccupantActors.Remove(OccupantId);
	ReservationIds.Remove(OccupantId);
}

bool UGP_BuildGridSubsystem::TryReserveFootprint(
	const FGuid& ReservationId,
	FIntPoint OriginCell,
	FIntPoint FootprintSize)
{
	if (!ReservationId.IsValid() || !IsValidFootprintSize(FootprintSize))
	{
		return false;
	}

	SweepStaleOccupants();

	if (ReservationIds.Contains(ReservationId) || OccupantCells.Contains(ReservationId))
	{
		return true;
	}

	EGP_GridRejectReason Reason = EGP_GridRejectReason::Free;
	if (!CanPlaceFootprint(OriginCell, FootprintSize, Reason))
	{
		return false;
	}

	TArray<FIntPoint> Cells;
	EnumerateFootprintCells(OriginCell, FootprintSize, Cells);
	OccupantCells.Add(ReservationId, Cells);
	ReservationIds.Add(ReservationId);
	for (const FIntPoint& Cell : Cells)
	{
		FGP_GridCellRecord Record;
		Record.OccupantId = ReservationId;
		Record.bIsReservation = true;
		CellOccupancy.Add(Cell, Record);
	}
	return true;
}

void UGP_BuildGridSubsystem::BindReservationOwner(const FGuid& ReservationId, AActor* OwnerActor)
{
	if (!ReservationId.IsValid() || !ReservationIds.Contains(ReservationId) || !IsValid(OwnerActor))
	{
		return;
	}

	OccupantActors.Add(ReservationId, OwnerActor);
	if (const TArray<FIntPoint>* Cells = OccupantCells.Find(ReservationId))
	{
		for (const FIntPoint& Cell : *Cells)
		{
			if (FGP_GridCellRecord* Record = CellOccupancy.Find(Cell))
			{
				if (Record->OccupantId == ReservationId)
				{
					Record->OccupantActor = OwnerActor;
					Record->bIsReservation = true;
				}
			}
		}
	}
}

bool UGP_BuildGridSubsystem::PromoteReservationToBuilding(
	const FGuid& ReservationId,
	AActor* Building,
	FGuid BuildingOccupantId)
{
	if (!ReservationId.IsValid() || !BuildingOccupantId.IsValid() || !IsValid(Building))
	{
		return false;
	}
	if (!ReservationIds.Contains(ReservationId))
	{
		return false;
	}

	TArray<FIntPoint> Cells;
	if (const TArray<FIntPoint>* Existing = OccupantCells.Find(ReservationId))
	{
		Cells = *Existing;
	}
	if (Cells.Num() == 0)
	{
		ReleaseReservation(ReservationId);
		return false;
	}

	OccupantCells.Remove(ReservationId);
	OccupantActors.Remove(ReservationId);
	ReservationIds.Remove(ReservationId);

	OccupantCells.Add(BuildingOccupantId, Cells);
	OccupantActors.Add(BuildingOccupantId, Building);
	for (const FIntPoint& Cell : Cells)
	{
		FGP_GridCellRecord Record;
		Record.OccupantId = BuildingOccupantId;
		Record.OccupantActor = Building;
		Record.bIsReservation = false;
		CellOccupancy.Add(Cell, Record);
	}
	return true;
}

void UGP_BuildGridSubsystem::ReleaseReservation(const FGuid& ReservationId)
{
	if (!ReservationId.IsValid() || !ReservationIds.Contains(ReservationId))
	{
		return;
	}
	UnregisterOccupant(ReservationId);
}

bool UGP_BuildGridSubsystem::IsReservationActive(const FGuid& ReservationId) const
{
	return ReservationId.IsValid() && ReservationIds.Contains(ReservationId);
}

bool UGP_BuildGridSubsystem::IsFootprintNavigable(
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	float GroundZ) const
{
	UWorld* World = GetWorld();
	if (World == nullptr || !IsValidFootprintSize(FootprintSize))
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSys == nullptr || NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) == nullptr)
	{
		return true;
	}

	const FVector Center = GetFootprintCenterWorld(OriginCell, FootprintSize, GroundZ);
	FNavLocation Projected;
	const FVector Extent(CellSize * 0.5f, CellSize * 0.5f, 300.0f);
	if (NavSys->ProjectPointToNavigation(Center, Projected, Extent))
	{
		return true;
	}

	FHitResult GroundHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GPBuildGridNavGround), false);
	const FVector TraceStart(Center.X, Center.Y, Center.Z + 200.0f);
	const FVector TraceEnd(Center.X, Center.Y, Center.Z - 400.0f);
	const bool bHitGround = World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_WorldStatic,
		QueryParams);
	if (bHitGround)
	{
		UE_LOG(LogGPBuildGrid, Verbose,
			TEXT("BuildGrid NotNavigable: Origin=(%d,%d) Center=%s"),
			OriginCell.X, OriginCell.Y, *Center.ToCompactString());
		return false;
	}

	// Empty void (contract isolation far from authored nav/geometry) is allowed.
	return true;
}

bool UGP_BuildGridSubsystem::IsFootprintEnvironmentBlocked(
	FIntPoint OriginCell,
	FIntPoint FootprintSize,
	float GroundZ,
	AActor* IgnoreActor) const
{
	UWorld* World = GetWorld();
	if (World == nullptr || !IsValidFootprintSize(FootprintSize))
	{
		return true;
	}

	const FVector Center = GetFootprintCenterWorld(OriginCell, FootprintSize, GroundZ);
	const float HalfX = FMath::Max(10.0f, 0.5f * static_cast<float>(FootprintSize.X) * CellSize - 8.0f);
	const float HalfY = FMath::Max(10.0f, 0.5f * static_cast<float>(FootprintSize.Y) * CellSize - 8.0f);
	const FCollisionShape Shape = FCollisionShape::MakeBox(FVector(HalfX, HalfY, 40.0f));
	const FVector QueryLoc(Center.X, Center.Y, GroundZ + 55.0f);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GPBuildGridWorldBlock), false);
	QueryParams.bFindInitialOverlaps = true;
	if (IsValid(IgnoreActor))
	{
		QueryParams.AddIgnoredActor(IgnoreActor);
	}

	TArray<FOverlapResult> Overlaps;
	if (!World->OverlapMultiByObjectType(Overlaps, QueryLoc, FQuat::Identity, ObjectParams, Shape, QueryParams))
	{
		return false;
	}

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!IsValid(HitActor))
		{
			continue;
		}
		if (HitActor->IsA(AGP_BuildingBase::StaticClass())
			|| HitActor->IsA(AGP_DropPod::StaticClass())
			|| HitActor->IsA(AGP_BuildingPlacementGhost::StaticClass())
			|| HitActor->IsA(APawn::StaticClass()))
		{
			continue;
		}
		return true;
	}

	return false;
}

void UGP_BuildGridSubsystem::SweepStaleOccupants()
{
	TArray<FGuid> Stale;
	for (const TPair<FGuid, TArray<FIntPoint>>& Pair : OccupantCells)
	{
		const FGuid& Id = Pair.Key;
		const bool bReservation = ReservationIds.Contains(Id);
		const TWeakObjectPtr<AActor>* ActorPtr = OccupantActors.Find(Id);
		const AActor* Actor = ActorPtr != nullptr ? ActorPtr->Get() : nullptr;
		if (bReservation)
		{
			if (ActorPtr != nullptr && !ActorPtr->IsValid())
			{
				Stale.Add(Id);
			}
			continue;
		}
		if (!IsValid(Actor))
		{
			Stale.Add(Id);
		}
	}

	for (const FGuid& Id : Stale)
	{
		UE_LOG(LogGPBuildGrid, Verbose, TEXT("BuildGrid sweep stale occupant %s"), *Id.ToString());
		UnregisterOccupant(Id);
	}
}
