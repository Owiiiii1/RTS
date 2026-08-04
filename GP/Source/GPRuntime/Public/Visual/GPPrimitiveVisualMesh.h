// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Visual/GPPrimitiveVisualTypes.h"

class UStaticMesh;

/** Shared Engine BasicShapes lookup for primitive MVP visuals. */
namespace GPPrimitiveVisualMesh
{
	GPRUNTIME_API FString GetEngineShapePath(EGP_PrimitiveShape Shape);
	GPRUNTIME_API UStaticMesh* ResolveShapeMesh(EGP_PrimitiveShape Shape);
}
