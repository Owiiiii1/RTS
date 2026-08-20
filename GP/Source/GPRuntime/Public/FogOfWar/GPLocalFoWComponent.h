// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "GPLocalFoWComponent.generated.h"

class UGP_LocalFoWComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnGP_LocalFoWUpdated, UGP_LocalFoWComponent*);

/**
 * Presentation-only Fog of War mirror for exactly one owning player's team.
 *
 * It has no gameplay mutation path and is populated only by AGP_PlayerController's owning-client RPC.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_LocalFoWComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_LocalFoWComponent();

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	int32 GetLocalTeamId() const { return LocalTeamId; }

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	int64 GetRevision() const { return Revision; }

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	FVector2D GetGridOriginWorldXY() const { return GridOriginWorldXY; }

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	FIntPoint GetGridDimensions() const { return GridDimensions; }

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	float GetCellSizeCm() const { return CellSizeCm; }

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	EGP_FoWState GetStateAtWorldLocation(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	bool IsExplored(const FVector& WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	bool IsVisible(const FVector& WorldLocation) const;

	/** Conservative presentation gate: not-ready and non-Visible both deny local placement preview. */
	UFUNCTION(BlueprintPure, Category = "GP|FogOfWar|Local")
	bool AllowsLocalPlacementPreview(const FVector& WorldLocation) const;

	/**
	 * Transport seam used by the owning PlayerController Client RPC.
	 * Calling it locally can only alter presentation state; server gameplay state is unreachable.
	 */
	bool ApplyServerUpdate(const FGP_FoWPresentationUpdate& Update);

	/** Clears all prior-team state. Used on travel, PlayerState replacement, and team changes. */
	void ResetPresentation();

	FOnGP_LocalFoWUpdated OnLocalFoWUpdated;

#if !UE_BUILD_SHIPPING
	int32 DebugGetExploredCellCount() const { return Explored.CountSetBits(); }
	int32 DebugGetVisibleCellCount() const { return Visible.CountSetBits(); }
	void DebugDumpToLog() const;
#endif

private:
	bool IsMetadataValid(const FGP_FoWPresentationUpdate& Update) const;
	bool AreRangesValid(const TArray<FGP_FoWCellRange>& Ranges, int32 NumCells) const;
	void ApplyRanges(TBitArray<>& Bits, const TArray<FGP_FoWCellRange>& Ranges);
	int32 WorldLocationToIndex(const FVector& WorldLocation) const;

	bool bReady = false;
	int32 LocalTeamId = -1;
	int64 Revision = 0;
	FVector2D GridOriginWorldXY = FVector2D::ZeroVector;
	FIntPoint GridDimensions = FIntPoint::ZeroValue;
	float CellSizeCm = 0.0f;
	TBitArray<> Explored;
	TBitArray<> Visible;
};
