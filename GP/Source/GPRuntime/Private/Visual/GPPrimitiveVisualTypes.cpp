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
		// Elongated Cube reads as a direction nose from RTS camera; Cone tip-forward reads as a disc.
		Forward.Shape = EGP_PrimitiveShape::Cube;
		Forward.ParentPartName = TEXT("Body");
		Forward.RelativeLocation = FVector(52.0f, 0.0f, 16.0f);
		Forward.RelativeRotation = FRotator(0.0f, 0.0f, 0.0f);
		Forward.RelativeScale = FVector(0.58f, 0.20f, 0.16f);
		Forward.bFacingIndicator = true;
		Definition.Parts.Add(Forward);

		FGP_PrimitiveVisualPart Weapon;
		Weapon.PartName = TEXT("Weapon");
		Weapon.Shape = EGP_PrimitiveShape::Cube;
		Weapon.ParentPartName = TEXT("Body");
		Weapon.RelativeLocation = FVector(42.0f, 30.0f, 14.0f);
		Weapon.RelativeRotation = FRotator(0.0f, 10.0f, 0.0f);
		Weapon.RelativeScale = FVector(0.90f, 0.16f, 0.16f);
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
