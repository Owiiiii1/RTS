// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPPrimitiveVisualTypes.h"

namespace GPPrimitiveVisualDefaults
{
	FGP_PrimitiveVisualDefinition MakeInfantryMeleeDefinition()
	{
		FGP_PrimitiveVisualDefinition Definition;
		Definition.Archetype = EGP_VisualArchetype::InfantryMelee;

		// Tuned for AGP_Unit capsule (r≈42, hh≈88). Actor forward = +X.
		FGP_PrimitiveVisualPart Body;
		Body.PartName = TEXT("Body");
		Body.Shape = EGP_PrimitiveShape::Cylinder;
		Body.RelativeLocation = FVector(0.0f, 0.0f, -8.0f);
		Body.RelativeScale = FVector(0.72f, 0.72f, 1.45f);
		Body.bPresentationRoot = true;
		Body.bBody = true;
		Body.bAnimated = true;
		Definition.Parts.Add(Body);

		FGP_PrimitiveVisualPart Forward;
		Forward.PartName = TEXT("Forward");
		Forward.Shape = EGP_PrimitiveShape::Cone;
		Forward.ParentPartName = TEXT("Body");
		// Cone default axis is +Z; roll so tip faces actor +X.
		Forward.RelativeLocation = FVector(42.0f, 0.0f, 18.0f);
		Forward.RelativeRotation = FRotator(90.0f, 0.0f, 0.0f);
		Forward.RelativeScale = FVector(0.22f, 0.22f, 0.38f);
		Forward.bFacingIndicator = true;
		Definition.Parts.Add(Forward);

		FGP_PrimitiveVisualPart Weapon;
		Weapon.PartName = TEXT("Weapon");
		Weapon.Shape = EGP_PrimitiveShape::Cube;
		Weapon.ParentPartName = TEXT("Body");
		Weapon.RelativeLocation = FVector(28.0f, 20.0f, 10.0f);
		Weapon.RelativeRotation = FRotator(0.0f, 12.0f, 0.0f);
		Weapon.RelativeScale = FVector(0.50f, 0.10f, 0.10f);
		Weapon.bWeapon = true;
		Definition.Parts.Add(Weapon);

		return Definition;
	}

	FGP_PrimitiveVisualDefinition MakeDefinitionForArchetype(EGP_VisualArchetype Archetype)
	{
		switch (Archetype)
		{
		case EGP_VisualArchetype::InfantryMelee:
		default:
			return MakeInfantryMeleeDefinition();
		}
	}

	const TCHAR* ArchetypeToString(EGP_VisualArchetype Archetype)
	{
		switch (Archetype)
		{
		case EGP_VisualArchetype::InfantryMelee:
			return TEXT("InfantryMelee");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* ShapeToString(EGP_PrimitiveShape Shape)
	{
		switch (Shape)
		{
		case EGP_PrimitiveShape::Cube:
			return TEXT("Cube");
		case EGP_PrimitiveShape::Sphere:
			return TEXT("Sphere");
		case EGP_PrimitiveShape::Cylinder:
			return TEXT("Cylinder");
		case EGP_PrimitiveShape::Cone:
			return TEXT("Cone");
		case EGP_PrimitiveShape::Capsule:
			return TEXT("Capsule");
		default:
			return TEXT("Unknown");
		}
	}
}
