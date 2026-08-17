// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GPBuildGridSubsystem.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EGP_GridRejectReason : uint8
{
	Free = 0,
	CellOccupied,
	InvalidFootprint,
	NotNavigable,
	WorldBlocked
};

/** Server occupancy record. Identity is FGuid, not a raw pointer address. */
struct FGP_GridCellRecord
{
	FGuid OccupantId;
	TWeakObjectPtr<AActor> OccupantActor;
	bool bIsReservation = false;
};

/**
 * Authoritative BuildGrid (GP-S36G).
 * Cell size 200 cm. Origin XY is world (0,0). Z is not encoded in cell coordinates.
 * Subsystem is not replicated; occupancy is server-only.
 */
UCLASS()
class GPRUNTIME_API UGP_BuildGridSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	float GetCellSize() const { return CellSize; }

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FVector2D GetGridOriginXY() const { return GridOriginXY; }

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FIntPoint WorldToCell(FVector World) const;

	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FVector CellToWorld(FIntPoint Cell, float GroundZ = 0.0f) const;

	/** OriginCell = min/anchor cell of an axis-aligned footprint. */
	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FIntPoint SnapOriginCell(FVector World, FIntPoint FootprintSize) const;

	/** World XY of the footprint center (even sizes sit between cells). */
	UFUNCTION(BlueprintPure, Category = "GP|BuildGrid")
	FVector GetFootprintCenterWorld(FIntPoint OriginCell, FIntPoint FootprintSize, float GroundZ) const;

	void EnumerateFootprintCells(FIntPoint OriginCell, FIntPoint FootprintSize, TArray<FIntPoint>& OutCells) const;

	bool ResolveSnappedPlacement(
		const FVector& RequestedWorld,
		FIntPoint FootprintSize,
		FIntPoint& OutOriginCell,
		FVector& OutSnappedGroundLocation) const;

	bool IsValidFootprintSize(FIntPoint FootprintSize) const;

	bool IsCellOccupied(FIntPoint Cell, AActor* IgnoreActor = nullptr) const;
	AActor* GetActorAtCell(FIntPoint Cell) const;

	bool CanPlaceFootprint(
		FIntPoint OriginCell,
		FIntPoint FootprintSize,
		EGP_GridRejectReason& OutReason,
		AActor* IgnoreActor = nullptr,
		const FGuid& IgnoreReservationId = FGuid());

	bool RegisterFootprint(AActor* Building, FIntPoint OriginCell, FIntPoint FootprintSize, FGuid OccupantId);
	void UnregisterFootprint(AActor* Building);
	void UnregisterOccupant(const FGuid& OccupantId);

	bool TryReserveFootprint(const FGuid& ReservationId, FIntPoint OriginCell, FIntPoint FootprintSize);
	void BindReservationOwner(const FGuid& ReservationId, AActor* OwnerActor);
	bool PromoteReservationToBuilding(const FGuid& ReservationId, AActor* Building, FGuid BuildingOccupantId);
	void ReleaseReservation(const FGuid& ReservationId);
	bool IsReservationActive(const FGuid& ReservationId) const;

	/** NavMesh MVP: project footprint center. Off-nav ground rejects; empty void is allowed. */
	bool IsFootprintNavigable(FIntPoint OriginCell, FIntPoint FootprintSize, float GroundZ) const;

	/** Environmental sanity (WorldStatic/WorldDynamic). Not structure-vs-structure SoT. */
	bool IsFootprintEnvironmentBlocked(
		FIntPoint OriginCell,
		FIntPoint FootprintSize,
		float GroundZ,
		AActor* IgnoreActor = nullptr) const;

	void SweepStaleOccupants();

	static constexpr float DefaultCellSizeCm = 200.0f;

private:
	int32 WorldToCell1D(float WorldCoord) const;
	int32 SnapOrigin1D(float WorldCoord, int32 Size) const;
	bool IsRecordIgnored(const FGP_GridCellRecord& Record, AActor* IgnoreActor, const FGuid& IgnoreReservationId) const;

	float CellSize = DefaultCellSizeCm;
	FVector2D GridOriginXY = FVector2D::ZeroVector;

	TMap<FIntPoint, FGP_GridCellRecord> CellOccupancy;
	TMap<FGuid, TArray<FIntPoint>> OccupantCells;
	TMap<FGuid, TWeakObjectPtr<AActor>> OccupantActors;
	TSet<FGuid> ReservationIds;
};
