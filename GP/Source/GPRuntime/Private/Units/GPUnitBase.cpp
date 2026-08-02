// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitBase.h"

AGP_UnitBase::AGP_UnitBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
}
