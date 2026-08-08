// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPSalvageWalker.h"

#include "Tags/GPGameplayTags.h"
#include "Units/GPMovementComponent.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "Visual/GPUnitVisualComponent.h"

AGP_SalvageWalker::AGP_SalvageWalker()
{
	DefaultMaxHealth = 200.0f;
	DefaultHealth = 200.0f;
	DefaultDamage = 20.0f;
	DefaultAttackCooldown = 1.0f;
	DefaultAttackRange = 600.0f;

	if (UGP_MovementComponent* Movement = GetUnitMovementComponent())
	{
		Movement->MoveSpeed = 250.0f;
	}

	if (UGP_UnitVisualComponent* Visual = GetUnitVisualComponent())
	{
		// Prefer AuthoredComponents so operator BP_SalvageWalker does not stack NativeFallback InfantryMelee.
		Visual->SetVisualSourceMode(EGP_VisualSourceMode::AuthoredComponents);
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPTags.Unit_Type_SalvageWalker.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Unit_Type_SalvageWalker);
	}
}
