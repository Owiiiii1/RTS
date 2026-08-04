// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPPrimitiveVisualTypes.h"

namespace GPPrimitiveVisualDefaults
{
	static bool HasHierarchyCycle(
		const TMap<FName, FName>& ParentByPart,
		const FName& StartPart)
	{
		TSet<FName> Visited;
		FName Current = StartPart;
		while (!Current.IsNone())
		{
			if (Visited.Contains(Current))
			{
				return true;
			}
			Visited.Add(Current);
			const FName* Parent = ParentByPart.Find(Current);
			if (Parent == nullptr)
			{
				break;
			}
			Current = *Parent;
		}
		return false;
	}

	FGP_PrimitiveVisualDefinition MakeInfantryMeleeDefinition()
	{
		FGP_PrimitiveVisualDefinition Definition;
		Definition.Archetype = EGP_VisualArchetype::InfantryMelee;

		FGP_PrimitiveVisualPart Body;
		Body.PartName = TEXT("Body");
		Body.Shape = EGP_PrimitiveShape::Cylinder;
		Body.RelativeLocation = FVector(0.0f, 0.0f, -8.0f);
		Body.RelativeScale = FVector(0.72f, 0.72f, 1.45f);
		Body.bPresentationRoot = true;
		Body.bBody = true;
		Body.bAnimated = true;
		Body.bTeamTintEligible = true;
		Definition.Parts.Add(Body);

		FGP_PrimitiveVisualPart Forward;
		Forward.PartName = TEXT("Forward");
		Forward.Shape = EGP_PrimitiveShape::Cube;
		Forward.ParentPartName = TEXT("Body");
		Forward.RelativeLocation = FVector(52.0f, 0.0f, 16.0f);
		Forward.RelativeRotation = FRotator(0.0f, 0.0f, 0.0f);
		Forward.RelativeScale = FVector(0.58f, 0.20f, 0.16f);
		Forward.bFacingIndicator = true;
		Forward.bTeamTintEligible = true;
		Definition.Parts.Add(Forward);

		FGP_PrimitiveVisualPart Weapon;
		Weapon.PartName = TEXT("Weapon");
		Weapon.Shape = EGP_PrimitiveShape::Cube;
		Weapon.ParentPartName = TEXT("Body");
		Weapon.RelativeLocation = FVector(42.0f, 30.0f, 14.0f);
		Weapon.RelativeRotation = FRotator(0.0f, 10.0f, 0.0f);
		Weapon.RelativeScale = FVector(0.90f, 0.16f, 0.16f);
		Weapon.bWeapon = true;
		Weapon.bTeamTintEligible = true;
		Definition.Parts.Add(Weapon);

		return Definition;
	}

	FGP_PrimitiveVisualDefinition MakeOreNodeDefinition()
	{
		FGP_PrimitiveVisualDefinition Definition;
		Definition.Archetype = EGP_VisualArchetype::InfantryMelee; // unused for resource nodes

		FGP_PrimitiveVisualPart Base;
		Base.PartName = TEXT("Base");
		Base.Shape = EGP_PrimitiveShape::Cylinder;
		Base.RelativeLocation = FVector(0.0f, 0.0f, -40.0f);
		Base.RelativeScale = FVector(0.56f, 0.56f, 0.56f);
		Base.bPresentationRoot = true;
		Base.bBody = true;
		Base.bTeamTintEligible = false;
		Definition.Parts.Add(Base);

		FGP_PrimitiveVisualPart Core;
		Core.PartName = TEXT("Core");
		Core.Shape = EGP_PrimitiveShape::Cone;
		Core.ParentPartName = TEXT("Base");
		Core.RelativeLocation = FVector(0.0f, 0.0f, 98.0f);
		Core.RelativeRotation = FRotator(0.0f, 0.0f, 0.0f);
		Core.RelativeScale = FVector(0.52f, 0.52f, 3.85f);
		Core.bTeamTintEligible = false;
		Definition.Parts.Add(Core);

		FGP_PrimitiveVisualPart AccentA;
		AccentA.PartName = TEXT("AccentA");
		AccentA.Shape = EGP_PrimitiveShape::Cone;
		AccentA.ParentPartName = TEXT("Base");
		AccentA.RelativeLocation = FVector(52.0f, 8.0f, 82.0f);
		AccentA.RelativeRotation = FRotator(28.0f, 0.0f, 0.0f);
		AccentA.RelativeScale = FVector(0.36f, 0.36f, 2.75f);
		AccentA.bTeamTintEligible = false;
		Definition.Parts.Add(AccentA);

		FGP_PrimitiveVisualPart AccentB;
		AccentB.PartName = TEXT("AccentB");
		AccentB.Shape = EGP_PrimitiveShape::Cone;
		AccentB.ParentPartName = TEXT("Base");
		AccentB.RelativeLocation = FVector(-10.0f, -54.0f, 76.0f);
		AccentB.RelativeRotation = FRotator(0.0f, 0.0f, -30.0f);
		AccentB.RelativeScale = FVector(0.32f, 0.32f, 2.45f);
		AccentB.bTeamTintEligible = false;
		Definition.Parts.Add(AccentB);

		FGP_PrimitiveVisualPart AccentC;
		AccentC.PartName = TEXT("AccentC");
		AccentC.Shape = EGP_PrimitiveShape::Cone;
		AccentC.ParentPartName = TEXT("Base");
		AccentC.RelativeLocation = FVector(-46.0f, 40.0f, 68.0f);
		AccentC.RelativeRotation = FRotator(-18.0f, 12.0f, 24.0f);
		AccentC.RelativeScale = FVector(0.28f, 0.28f, 1.95f);
		AccentC.bTeamTintEligible = false;
		Definition.Parts.Add(AccentC);

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

	const TCHAR* VisualSourceToString(EGP_VisualDefinitionSource Source)
	{
		switch (Source)
		{
		case EGP_VisualDefinitionSource::DataAsset:
			return TEXT("DataAsset");
		case EGP_VisualDefinitionSource::NativeFallback:
		default:
			return TEXT("NativeFallback");
		}
	}

	bool IsSupportedShape(EGP_PrimitiveShape Shape)
	{
		switch (Shape)
		{
		case EGP_PrimitiveShape::Cube:
		case EGP_PrimitiveShape::Sphere:
		case EGP_PrimitiveShape::Cylinder:
		case EGP_PrimitiveShape::Cone:
		case EGP_PrimitiveShape::Capsule:
			return true;
		default:
			return false;
		}
	}

	FGP_PrimitiveVisualValidationResult ValidateAndSanitizeDefinition(
		const FGP_PrimitiveVisualDefinition& InDefinition)
	{
		FGP_PrimitiveVisualValidationResult Result;
		Result.SanitizedDefinition.Archetype = InDefinition.Archetype;

		if (InDefinition.Parts.Num() <= 0)
		{
			Result.Errors.Add(TEXT("Parts array is empty"));
			return Result;
		}

		if (InDefinition.Parts.Num() > MaxParts)
		{
			Result.Errors.Add(FString::Printf(TEXT("Parts count %d exceeds max %d"), InDefinition.Parts.Num(), MaxParts));
			return Result;
		}

		TSet<FName> SeenNames;
		TMap<FName, FName> ParentByPart;
		int32 PresentationRootCount = 0;
		TArray<FGP_PrimitiveVisualPart> WorkingParts;

		for (const FGP_PrimitiveVisualPart& Part : InDefinition.Parts)
		{
			FGP_PrimitiveVisualPart Sanitized = Part;

			if (Sanitized.PartName.IsNone())
			{
				Result.Errors.Add(TEXT("PartName is empty"));
				continue;
			}

			if (SeenNames.Contains(Sanitized.PartName))
			{
				++Result.DuplicatePartNameCount;
				Result.Errors.Add(FString::Printf(TEXT("Duplicate PartName=%s"), *Sanitized.PartName.ToString()));
				continue;
			}
			SeenNames.Add(Sanitized.PartName);

			if (!IsSupportedShape(Sanitized.Shape))
			{
				Result.Errors.Add(FString::Printf(
					TEXT("Unsupported shape on Part=%s"),
					*Sanitized.PartName.ToString()));
				continue;
			}

			if (Sanitized.RelativeLocation.ContainsNaN()
				|| !FMath::IsFinite(Sanitized.RelativeLocation.X)
				|| !FMath::IsFinite(Sanitized.RelativeLocation.Y)
				|| !FMath::IsFinite(Sanitized.RelativeLocation.Z))
			{
				Result.Errors.Add(FString::Printf(TEXT("Non-finite location on Part=%s"), *Sanitized.PartName.ToString()));
				continue;
			}

			if (!FMath::IsFinite(Sanitized.RelativeRotation.Pitch)
				|| !FMath::IsFinite(Sanitized.RelativeRotation.Yaw)
				|| !FMath::IsFinite(Sanitized.RelativeRotation.Roll))
			{
				Result.Errors.Add(FString::Printf(TEXT("Non-finite rotation on Part=%s"), *Sanitized.PartName.ToString()));
				continue;
			}

			if (!FMath::IsFinite(Sanitized.RelativeScale.X)
				|| !FMath::IsFinite(Sanitized.RelativeScale.Y)
				|| !FMath::IsFinite(Sanitized.RelativeScale.Z)
				|| FMath::IsNearlyZero(Sanitized.RelativeScale.X)
				|| FMath::IsNearlyZero(Sanitized.RelativeScale.Y)
				|| FMath::IsNearlyZero(Sanitized.RelativeScale.Z))
			{
				Result.Errors.Add(FString::Printf(TEXT("Invalid scale on Part=%s"), *Sanitized.PartName.ToString()));
				continue;
			}

			if (Sanitized.ParentPartName == Sanitized.PartName)
			{
				Result.bHierarchyValid = false;
				Result.Errors.Add(FString::Printf(TEXT("Self-parent on Part=%s"), *Sanitized.PartName.ToString()));
				continue;
			}

			// Visual parts never carry gameplay collision.
			Sanitized.CollisionPolicy = EGP_PrimitiveVisualCollisionPolicy::NoCollision;

			if (Sanitized.bPresentationRoot)
			{
				++PresentationRootCount;
			}

			ParentByPart.Add(Sanitized.PartName, Sanitized.ParentPartName);
			WorkingParts.Add(Sanitized);
		}

		if (WorkingParts.Num() <= 0)
		{
			Result.Errors.Add(TEXT("No usable parts after sanitize"));
			return Result;
		}

		if (PresentationRootCount == 0)
		{
			WorkingParts[0].bPresentationRoot = true;
			Result.Warnings.Add(TEXT("No PresentationRoot flagged; first part promoted"));
		}
		else if (PresentationRootCount > 1)
		{
			bool bKept = false;
			for (FGP_PrimitiveVisualPart& Part : WorkingParts)
			{
				if (Part.bPresentationRoot)
				{
					if (!bKept)
					{
						bKept = true;
					}
					else
					{
						Part.bPresentationRoot = false;
					}
				}
			}
			Result.Warnings.Add(TEXT("Multiple PresentationRoot flags; kept first only"));
		}

		for (FGP_PrimitiveVisualPart& Part : WorkingParts)
		{
			if (Part.ParentPartName.IsNone())
			{
				continue;
			}

			if (!SeenNames.Contains(Part.ParentPartName))
			{
				Result.Warnings.Add(FString::Printf(
					TEXT("Missing parent %s for Part=%s; will attach to actor root"),
					*Part.ParentPartName.ToString(),
					*Part.PartName.ToString()));
				Part.ParentPartName = NAME_None;
				ParentByPart[Part.PartName] = NAME_None;
				continue;
			}

			if (HasHierarchyCycle(ParentByPart, Part.PartName))
			{
				Result.bHierarchyValid = false;
				Result.Errors.Add(FString::Printf(TEXT("Hierarchy cycle involving Part=%s"), *Part.PartName.ToString()));
			}
		}

		if (Result.Errors.Num() > 0)
		{
			return Result;
		}

		Result.SanitizedDefinition.Parts = WorkingParts;
		Result.bValid = true;
		return Result;
	}
}
