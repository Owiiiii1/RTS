// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPResourceNodeSearch.generated.h"

class AGP_ResourceNode;
class AActor;
class UGP_ResourceDefinition;

/** Authority search query for ResourceNode registry (GP-S28P2). */
USTRUCT(BlueprintType)
struct FGP_ResourceNodeSearchQuery
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	float SearchRadiusCm = 3000.0f;

	UPROPERTY(BlueprintReadWrite, Category = "GP|Resource|Search")
	float MaxPathLengthCm = 6000.0f;

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
};

/** Deterministic scored candidate from registry search. */
USTRUCT(BlueprintType)
struct FGP_ResourceNodeCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|Resource|Search")
	TObjectPtr<AGP_ResourceNode> Node = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Resource|Search")
	float PathLengthCm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Resource|Search")
	float DirectDistanceCm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|Resource|Search")
	bool bHasFreeSlot = false;
};
