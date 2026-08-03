// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPMobileUnit.h"

#include "Units/GPMovementComponent.h"

AGP_MobileUnit::AGP_MobileUnit()
{
	MovementComponent = CreateDefaultSubobject<UGP_MovementComponent>(TEXT("MovementComponent"));
}

UGP_MovementComponent* AGP_MobileUnit::GetUnitMovementComponent() const
{
	return MovementComponent;
}
