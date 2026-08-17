// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GPBuildGridSubsystem.generated.h"

class AActor;
class AGP_BuildingBase;
class UBoxComponent;
class UGP_BuildingDefinition;

/** Shared runtime footprint after bounds/DA/class fallback resolution (GP-S36G). */
USTRUCT()
struct GPRUNTIME_API FGP_ResolvedBuildingFootprint
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint SizeCells = FIntPoint::ZeroValue;

	/** Authored box RelativeLocation XY vs actor/root (cm), not world. Zero when using DA/class fallback. */
	UPROPERTY()
	FVector2D LocalCenterOffsetCm = FVector2D::ZeroVector;

	UPROPERTY()
	bool bFromAuthoredBounds = false;

	bool IsValid() const { return SizeCells.X > 0 && SizeCells.Y > 0; }
};

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

	void GetFootprintWorldAABB(
		FIntPoint OriginCell,
		FIntPoint FootprintSize,
		float GroundZ,
		FVector& OutMin,
		FVector& OutMax) const;

	bool DoFootprintsOverlap(FIntPoint OriginA, FIntPoint SizeA, FIntPoint OriginB, FIntPoint SizeB) const;

	bool ResolveSnappedPlacement(
		const FVector& RequestedWorld,
		FIntPoint FootprintSize,
		FIntPoint& OutOriginCell,
		FVector& OutSnappedGroundLocation) const;

	bool IsValidFootprintSize(FIntPoint FootprintSize) const;

	/** XY half-extent → cell count. WidthCm = 2*Extent.X. Ceil, minimum 1×1. CellSize 200. */
	FIntPoint ConvertAuthoredBoundsToFootprintCells(FVector BoxExtent) const;

	/**
	 * Component-local authored half-extent (cm): |UnscaledBoxExtent * RelativeScale3D|.
	 * Does not use GetScaledBoxExtent() / component world scale (those include actor/parent scale).
	 * RelativeRotation is ignored (BuildGrid is axis-aligned).
	 */
	static FVector GetAuthoredPlacementHalfExtentCm(const UBoxComponent* Bounds);

	static bool ArePlacementFootprintBoundsUsable(const UBoxComponent* Bounds);

	/** Native archetype XY half-extent (cm): MainBase 500, Hub 400, generic 100. */
	static FVector GetNativeDefaultPlacementHalfExtentCm(TSubclassOf<AGP_BuildingBase> PayloadClass);

	/**
	 * True when authored XY half-extent and RelativeLocation match the native class default.
	 * Used to detect stale level-instance snapshots of native 5×5 / 4×4 / 1×1.
	 */
	static bool LooksLikeNativeDefaultPlacementBounds(
		TSubclassOf<AGP_BuildingBase> PayloadClass,
		const UBoxComponent* Bounds);

	/**
	 * Preferred: payload CDO/instance PlacementFootprintBounds when effective XY half-extent >= 1 cm.
	 * Actor resolve always prefers the LIVE PlacementFootprintBounds when usable.
	 * Class CDO is design data used to synchronize pre-placed live components, not a hidden
	 * occupancy source that can differ from the visible box.
	 * Fallback: BuildingDefinition.FootprintCells when both axes > 0.
	 * If a BuildingDefinition is present but FootprintCells is invalid, do not class-fallback
	 * (keeps InvalidFootprint deploy rejection).
	 * If no definition: MainBase 5×5, LogisticsHub 4×4, else 1×1.
	 */
	FGP_ResolvedBuildingFootprint ResolveBuildingFootprint(
		TSubclassOf<AGP_BuildingBase> PayloadClass,
		const UGP_BuildingDefinition* BuildingDef) const;

	FGP_ResolvedBuildingFootprint ResolveActorFootprint(
		const AGP_BuildingBase* Building,
		const UGP_BuildingDefinition* BuildingDef = nullptr) const;

	/**
	 * Rotate authored local XY offset by actor/root rotation. Does not apply actor/world scale.
	 * Uses FTransform::TransformVectorNoScale on a rotation-only transform (scale = 1).
	 */
	static FVector TransformFootprintLocalOffsetToWorld(
		FVector2D LocalCenterOffsetCm,
		FRotator ActorRotation);

	/** ActorLocation + rotation-only world offset. Footprint size stays world-axis-aligned. */
	static FVector MakeWorldFootprintCenter(
		const FVector& ActorLocation,
		FRotator ActorRotation,
		FVector2D LocalCenterOffsetCm);

	/** Visible live box center (GetComponentLocation). Occupancy snaps this XY. */
	static FVector GetLivePlacementFootprintCenterWorld(const UBoxComponent* Bounds);

	FVector MakeActorLocationFromFootprintCenter(
		const FVector& FootprintCenterWorld,
		FVector2D LocalCenterOffsetCm,
		FRotator ActorRotation = FRotator::ZeroRotator) const;

	/** Semantic deploy/preview ground Z at XY. Not first-hit Visibility/WorldStatic. */
	float ResolveDeployGroundZ(const FVector& HintLocation, AActor* ExtraIgnoreActor = nullptr) const;

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
	bool TryResolveFromPlacementBounds(const UBoxComponent* Bounds, FGP_ResolvedBuildingFootprint& OutResolved) const;
	FGP_ResolvedBuildingFootprint ResolveDefinitionOrClassFallback(
		TSubclassOf<AGP_BuildingBase> PayloadClass,
		const UGP_BuildingDefinition* BuildingDef) const;

	float CellSize = DefaultCellSizeCm;
	FVector2D GridOriginXY = FVector2D::ZeroVector;

	TMap<FIntPoint, FGP_GridCellRecord> CellOccupancy;
	TMap<FGuid, TArray<FIntPoint>> OccupantCells;
	TMap<FGuid, TWeakObjectPtr<AActor>> OccupantActors;
	TSet<FGuid> ReservationIds;
};
