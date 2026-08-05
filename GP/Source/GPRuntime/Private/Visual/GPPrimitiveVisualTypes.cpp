// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPPrimitiveVisualTypes.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

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

	FGP_PrimitiveVisualDefinition MakeOreNodeDefinition()
	{
		FGP_PrimitiveVisualDefinition Definition;
		Definition.Archetype = EGP_VisualArchetype::InfantryMelee; // unused for resource nodes

		// Accents/Core parent to Base: keep Base scale uniform so Cone +Z tips are not
		// squashed into discs and tilted accents are not sheared. Sink Base so only a
		// low pedestal reads above ground; crystals dominate the RTS silhouette.
		// Engine Cone axis = +Z (tip up). Pitch leans tip toward +X; Roll toward +Y.

		FGP_PrimitiveVisualPart Base;
		Base.PartName = TEXT("Base");
		Base.Shape = EGP_PrimitiveShape::Cylinder;
		Base.RelativeLocation = FVector(0.0f, 0.0f, -40.0f);
		Base.RelativeScale = FVector(0.56f, 0.56f, 0.56f);
		Base.bPresentationRoot = true;
		Base.bBody = true;
		Definition.Parts.Add(Base);

		FGP_PrimitiveVisualPart Core;
		Core.PartName = TEXT("Core");
		Core.Shape = EGP_PrimitiveShape::Cone;
		Core.ParentPartName = TEXT("Base");
		Core.RelativeLocation = FVector(0.0f, 0.0f, 98.0f);
		Core.RelativeRotation = FRotator(0.0f, 0.0f, 0.0f);
		Core.RelativeScale = FVector(0.52f, 0.52f, 3.85f);
		Definition.Parts.Add(Core);

		FGP_PrimitiveVisualPart AccentA;
		AccentA.PartName = TEXT("AccentA");
		AccentA.Shape = EGP_PrimitiveShape::Cone;
		AccentA.ParentPartName = TEXT("Base");
		AccentA.RelativeLocation = FVector(52.0f, 8.0f, 82.0f);
		AccentA.RelativeRotation = FRotator(28.0f, 0.0f, 0.0f); // lean +X
		AccentA.RelativeScale = FVector(0.36f, 0.36f, 2.75f);
		Definition.Parts.Add(AccentA);

		FGP_PrimitiveVisualPart AccentB;
		AccentB.PartName = TEXT("AccentB");
		AccentB.Shape = EGP_PrimitiveShape::Cone;
		AccentB.ParentPartName = TEXT("Base");
		AccentB.RelativeLocation = FVector(-10.0f, -54.0f, 76.0f);
		AccentB.RelativeRotation = FRotator(0.0f, 0.0f, -30.0f); // lean -Y
		AccentB.RelativeScale = FVector(0.32f, 0.32f, 2.45f);
		Definition.Parts.Add(AccentB);

		FGP_PrimitiveVisualPart AccentC;
		AccentC.PartName = TEXT("AccentC");
		AccentC.Shape = EGP_PrimitiveShape::Cone;
		AccentC.ParentPartName = TEXT("Base");
		AccentC.RelativeLocation = FVector(-46.0f, 40.0f, 68.0f);
		AccentC.RelativeRotation = FRotator(-18.0f, 12.0f, 24.0f); // lean +Y / -X
		AccentC.RelativeScale = FVector(0.28f, 0.28f, 1.95f);
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

	const TCHAR* VisualSourceModeToString(EGP_VisualSourceMode Mode)
	{
		switch (Mode)
		{
		case EGP_VisualSourceMode::AuthoredComponents:
			return TEXT("AuthoredComponents");
		case EGP_VisualSourceMode::NativeFallback:
		default:
			return TEXT("NativeFallback");
		}
	}
}

namespace GPAuthoredVisualDiagnostics
{
	static bool IsGameplayRootCollision(const AActor* Owner, const UActorComponent* Component)
	{
		if (Owner == nullptr || Component == nullptr)
		{
			return false;
		}

		if (Component == Owner->GetRootComponent())
		{
			return Component->IsA<UCapsuleComponent>() || Component->IsA<UBoxComponent>();
		}

		return false;
	}

	void Collect(
		const AActor* Owner,
		const TSet<const UActorComponent*>& GeneratedComponents,
		FSnapshot& OutSnapshot)
	{
		OutSnapshot = FSnapshot();
		if (Owner == nullptr)
		{
			OutSnapshot.bMissingVisibleAuthoredMesh = true;
			return;
		}

		TInlineComponentArray<UPrimitiveComponent*> Primitives(Owner);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive == nullptr || GeneratedComponents.Contains(Primitive))
			{
				continue;
			}

			const bool bMeshPresentation =
				Primitive->IsA<UStaticMeshComponent>() || Primitive->IsA<USkeletalMeshComponent>();
			if (!bMeshPresentation)
			{
				continue;
			}

			if (IsGameplayRootCollision(Owner, Primitive))
			{
				continue;
			}

			++OutSnapshot.AuthoredPrimitiveComponentCount;

			if (Primitive->IsVisible()
				&& ((Cast<UStaticMeshComponent>(Primitive) != nullptr
						&& Cast<UStaticMeshComponent>(Primitive)->GetStaticMesh() != nullptr)
					|| (Cast<USkeletalMeshComponent>(Primitive) != nullptr
						&& Cast<USkeletalMeshComponent>(Primitive)->GetSkeletalMeshAsset() != nullptr)))
			{
				++OutSnapshot.VisibleAuthoredMeshCount;
			}

			if (Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				++OutSnapshot.AuthoredCollisionWarnings;
			}

			if (Primitive->GetGenerateOverlapEvents())
			{
				++OutSnapshot.AuthoredOverlapWarnings;
			}

			if (Primitive->CanEverAffectNavigation())
			{
				++OutSnapshot.AuthoredNavigationWarnings;
			}
		}

		OutSnapshot.bMissingVisibleAuthoredMesh = OutSnapshot.VisibleAuthoredMeshCount <= 0;
	}
}
