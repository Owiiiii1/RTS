// Copyright Epic Games, Inc. All Rights Reserved.

#include "Command/GPCommandComponent.h"

UGP_CommandComponent::UGP_CommandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}
