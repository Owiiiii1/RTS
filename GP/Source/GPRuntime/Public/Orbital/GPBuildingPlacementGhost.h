// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GPBuildingPlacementGhost.generated.h"

class UStaticMeshComponent;

/**
 * Local-only translucent placement ghost for orbital building deploy (GP-S32R).
 * Non-replicated; owned/spawned by local PlayerController during placement mode.
 */
UCLASS(NotPlaceable)
class GPRUNTIME_API AGP_BuildingPlacementGhost : public AActor
{
	GENERATED_BODY()

public:
	AGP_BuildingPlacementGhost();

	void SetGhostVisible(bool bVisible);
	void UpdateGhostTransform(const FTransform& WorldTransform);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Building|Ghost")
	TObjectPtr<UStaticMeshComponent> GhostMesh;
};
