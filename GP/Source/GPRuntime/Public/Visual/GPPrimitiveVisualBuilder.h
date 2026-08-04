// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Visual/GPPrimitiveVisualTypes.h"

class AActor;
class USceneComponent;
class UStaticMeshComponent;

/** Shared builder for NoCollision Engine-shape visual parts (units, resource nodes, …). */
namespace GPPrimitiveVisualBuilder
{
	struct FBuildResult
	{
		TArray<TObjectPtr<UStaticMeshComponent>> PartComponents;
		TMap<FName, TObjectPtr<UStaticMeshComponent>> PartLookup;
		TObjectPtr<USceneComponent> PresentationRootComponent;
		FName PresentationRootPartName = NAME_None;
	};

	/**
	 * Creates transient UStaticMeshComponent parts under Owner.
	 * Parts use NoCollision and do not affect navigation.
	 * Definition should list parents before children.
	 */
	GPRUNTIME_API bool BuildFromDefinition(
		AActor* Owner,
		USceneComponent* AttachRoot,
		const FGP_PrimitiveVisualDefinition& Definition,
		FBuildResult& OutResult,
		const TCHAR* LogOwnerLabel);

	GPRUNTIME_API void DestroyBuiltParts(FBuildResult& InOutResult);
}
