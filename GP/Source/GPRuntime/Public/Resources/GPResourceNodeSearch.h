// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Resources/GPResourceApproach.h"
#include "GPResourceNodeSearch.generated.h"

class AGP_ResourceNode;
class AActor;
class UGP_ResourceDefinition;

/** Authority search query for ResourceNode registry (GP-S28P2). */
USTRUCT(BlueprintType)
struct FGP_ResourceNodeSearchQuery
{
	GENERATED_BODY()

	/**
	 * Spatial cluster center for ResourceSearchRadiusCm filtering.
	 * For post-haul reassignment this is the Mine search anchor (original deposit zone),
	 * NOT the Worker's current location at MainBase.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	FVector SearchCenter = FVector::ZeroVector;

	/**
	 * Navigation path start (current Worker location).
	 * Reachability / MaxPathLengthCm are evaluated from here to each candidate approach point.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	FVector PathStart = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	float SearchRadiusCm = 3000.0f;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	float MaxPathLengthCm = 6000.0f;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	float InteractionRangeCm = 200.0f;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	float AcceptanceRadiusCm = 50.0f;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	float ApproachSafetyMarginCm = 25.0f;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	int32 ApproachDirectionCount = 8;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	TObjectPtr<AGP_ResourceNode> ExcludeNode = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	TObjectPtr<UGP_ResourceDefinition> CompatibleDefinition = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	bool bRequireFreeSlot = false;

	/** Pawn/actor used for navigation path queries (Worker). */
	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	TObjectPtr<AActor> PathfindingActor = nullptr;

	/** Prefer candidates with free mining slots first (sort key). */
	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	bool bPreferFreeSlot = true;

#if !UE_BUILD_SHIPPING
	/** Optional event label for diagnostics (not Tick). */
	FName SearchReason = NAME_None;
	bool bLogDiagnostics = false;
#endif
};

/** Deterministic scored candidate from registry search. */
USTRUCT(BlueprintType)
struct FGP_ResourceNodeCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Resource|Search")
	TObjectPtr<AGP_ResourceNode> Node = nullptr;

	/** Best projected approach location used for path scoring. */
	UPROPERTY(BlueprintReadOnly, Category = "GP|Resource|Search")
	FVector BestApproachLocation = FVector::ZeroVector;

	/** Navigable path length from PathStart to BestApproachLocation. */
	UPROPERTY(BlueprintReadOnly, Category = "GP|Resource|Search")
	float PathLengthCm = 0.0f;

	/** Euclidean distance from SearchCenter to node (radius metric). */
	UPROPERTY(BlueprintReadOnly, Category = "GP|Resource|Search")
	float DirectDistanceCm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Resource|Search")
	bool bHasFreeSlot = false;

#if !UE_BUILD_SHIPPING
	EGP_ResourceCandidateRejectReason RejectReason = EGP_ResourceCandidateRejectReason::None;
#endif
};
