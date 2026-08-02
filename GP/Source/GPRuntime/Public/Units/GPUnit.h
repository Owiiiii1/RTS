// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Units/GPUnitBase.h"
#include "GPUnit.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;

/**
 * First production-oriented concrete unit actor.
 * Placeable placeholder with Visibility-traceable capsule + basic cylinder mesh.
 * Not a full GP-S18 gameplay unit.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_Unit : public AGP_UnitBase
{
	GENERATED_BODY()

public:
	AGP_Unit();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> VisualMesh;
};
