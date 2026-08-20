// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/BitArray.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "GPFogOfWarComponent.generated.h"

class AGP_UnitBase;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnGP_FoWTeamStateChanged,
	int32 /*TeamId*/,
	int64 /*Revision*/);

/** Authoritative three-state Fog of War state for one team at one world location. */
UENUM(BlueprintType)
enum class EGP_FoWState : uint8
{
	Unexplored = 0,
	Explored,
	Visible
};

/**
 * Server-authoritative Fog of War grid owned by AGP_GameState.
 *
 * Runtime state is deliberately not replicated in this foundation slice. Later client presentation,
 * last-known state, and relevancy work consume the query API without becoming gameplay authority.
 * Canonical MVP grid: 50 cm cells, 4000 x 4000, 0.10 s (10 Hz) authority recompute.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_FogOfWarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_FogOfWarComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Safe authoritative query. Invalid teams, clients, non-finite locations, and out-of-grid locations are Unexplored. */
	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar")
	EGP_FoWState GetStateForTeamAtWorldLocation(int32 TeamId, const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar")
	bool IsExploredByTeam(int32 TeamId, const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar")
	bool IsVisibleToTeam(int32 TeamId, const FVector& WorldLocation) const;

	bool IsCellVisibleToTeam(int32 TeamId, const FIntPoint& Cell) const;
	bool WorldToCell(const FVector& WorldLocation, FIntPoint& OutCell) const;
	FVector GetCellCenterWorld(const FIntPoint& Cell, float Z = 0.0f) const;

	/** Authority-only sight-source registry. Units may register before receiving a playable TeamId. */
	bool RegisterSightSource(AGP_UnitBase* Source);
	void UnregisterSightSource(AGP_UnitBase* Source);

	/** Immediate bounded recompute for lifecycle changes and deterministic contracts. Authority only. */
	void RecomputeVisibilityNow();

	float GetCellSizeCm() const { return CellSizeCm; }
	FVector2D GetGridOriginWorldXY() const { return GridOriginWorldXY; }
	FIntPoint GetGridDimensions() const { return GridDimensions; }
	float GetUpdateIntervalSeconds() const { return UpdateIntervalSeconds; }

	/**
	 * Authority-only read-only extraction for one owning-client presentation stream.
	 * Initial snapshots include all explored cells; deltas include only cells first explored in Revision.
	 */
	bool BuildPresentationUpdate(
		int32 TeamId,
		bool bInitialSnapshot,
		FGP_FoWPresentationUpdate& OutUpdate) const;

	FOnGP_FoWTeamStateChanged OnTeamStateChanged;

#if !UE_BUILD_SHIPPING
	int32 DebugGetRegisteredSightSourceCount() const;
	int32 DebugGetVisibleCellCountForTeam(int32 TeamId) const;
	int32 DebugGetExploredCellCountForTeam(int32 TeamId) const;
	void DebugDumpToLog() const;
	void DebugResetAllState();
#endif

private:
	struct FTeamGrid
	{
		TBitArray<> Explored;
		TBitArray<> Visible;
		TArray<int32> NewlyExplored;
		int64 Revision = 0;
	};

	bool HasAuthoritativeOwner() const;
	int32 CellToIndex(const FIntPoint& Cell) const;
	bool IsCellInBounds(const FIntPoint& Cell) const;
	FTeamGrid& FindOrAddTeamGrid(int32 TeamId);
	const FTeamGrid* FindTeamGrid(int32 TeamId) const;
	void PruneSightSources();
	void MarkVisibleCircle(FTeamGrid& TeamGrid, const FVector& CenterWorld, float RadiusCm);
	static void BuildRangesFromBits(const TBitArray<>& Bits, TArray<FGP_FoWCellRange>& OutRanges);
	static void BuildRangesFromIndices(
		const TArray<int32>& Indices,
		TArray<FGP_FoWCellRange>& OutRanges);

	/** Foundation-owned deterministic bounds until a canonical playable-area owner is introduced. */
	UPROPERTY(EditDefaultsOnly, Category = "GP|FogOfWar|Grid", meta = (ClampMin = "50.0"))
	float CellSizeCm = 50.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GP|FogOfWar|Grid")
	FVector2D GridOriginWorldXY = FVector2D(-100000.0f, -100000.0f);

	UPROPERTY(EditDefaultsOnly, Category = "GP|FogOfWar|Grid", meta = (ClampMin = "1"))
	FIntPoint GridDimensions = FIntPoint(4000, 4000);

	UPROPERTY(EditDefaultsOnly, Category = "GP|FogOfWar|Runtime", meta = (ClampMin = "0.05"))
	float UpdateIntervalSeconds = 0.1f;

	TMap<int32, FTeamGrid> TeamGrids;
	TArray<TWeakObjectPtr<AGP_UnitBase>> SightSources;
};
