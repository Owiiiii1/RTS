// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPPrimitiveVisualTypes.generated.h"

class AActor;
class UActorComponent;

/** Engine basic-shape kind used by primitive MVP visuals (GP-S26B1). */
UENUM()
enum class EGP_PrimitiveShape : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone,
	/** Resolved as Engine Cylinder with capsule-like scale (no Engine Capsule basic mesh). */
	Capsule
};

/** Visual archetype identity (cosmetic only; not used by combat gameplay). */
UENUM()
enum class EGP_VisualArchetype : uint8
{
	InfantryMelee UMETA(DisplayName = "InfantryMelee")
	// Future: InfantryRanged, HeavyInfantry, Worker, Tank, Artillery, Turret, Monsters, Buildings, ResourceNode
};

/**
 * Who owns presentation for a unit/resource visual component (GP-S26B2A).
 * NativeFallback: C++ builds Engine basic-shape parts into BuiltVisual.
 * AuthoredComponents: Blueprint/SCS meshes are the presentation; generated native parts stay cleared.
 */
UENUM(BlueprintType)
enum class EGP_VisualSourceMode : uint8
{
	NativeFallback UMETA(DisplayName = "NativeFallback"),
	AuthoredComponents UMETA(DisplayName = "AuthoredComponents")
};

/** Collision policy for visual parts (MVP: always NoCollision). */
UENUM()
enum class EGP_PrimitiveVisualCollisionPolicy : uint8
{
	NoCollision
};

/** Visibility policy for visual parts. */
UENUM()
enum class EGP_PrimitiveVisualVisibilityPolicy : uint8
{
	AlwaysVisible,
	HideOnDedicatedServer
};

/** Single primitive part in a composite visual definition. */
USTRUCT()
struct GPRUNTIME_API FGP_PrimitiveVisualPart
{
	GENERATED_BODY()

	UPROPERTY()
	FName PartName;

	UPROPERTY()
	EGP_PrimitiveShape Shape = EGP_PrimitiveShape::Cube;

	UPROPERTY()
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector RelativeScale = FVector::OneVector;

	/** Empty = attach under unit root (presentation attach parent). */
	UPROPERTY()
	FName ParentPartName;

	UPROPERTY()
	bool bPresentationRoot = false;

	UPROPERTY()
	bool bBody = false;

	UPROPERTY()
	bool bFacingIndicator = false;

	UPROPERTY()
	bool bWeapon = false;

	UPROPERTY()
	bool bTurret = false;

	UPROPERTY()
	bool bAnimated = false;

	UPROPERTY()
	EGP_PrimitiveVisualCollisionPolicy CollisionPolicy = EGP_PrimitiveVisualCollisionPolicy::NoCollision;

	UPROPERTY()
	EGP_PrimitiveVisualVisibilityPolicy VisibilityPolicy = EGP_PrimitiveVisualVisibilityPolicy::HideOnDedicatedServer;
};

/** Native composite visual definition (no DataAsset instance in B1). */
USTRUCT()
struct GPRUNTIME_API FGP_PrimitiveVisualDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	EGP_VisualArchetype Archetype = EGP_VisualArchetype::InfantryMelee;

	UPROPERTY()
	TArray<FGP_PrimitiveVisualPart> Parts;
};

namespace GPPrimitiveVisualDefaults
{
	/** Native InfantryMelee prototype (≤4 parts). Cosmetic only. */
	GPRUNTIME_API FGP_PrimitiveVisualDefinition MakeInfantryMeleeDefinition();

	/** Native Ore resource-node pile (≤6 parts). Cosmetic only. */
	GPRUNTIME_API FGP_PrimitiveVisualDefinition MakeOreNodeDefinition();

	GPRUNTIME_API FGP_PrimitiveVisualDefinition MakeDefinitionForArchetype(EGP_VisualArchetype Archetype);

	GPRUNTIME_API const TCHAR* ArchetypeToString(EGP_VisualArchetype Archetype);
	GPRUNTIME_API const TCHAR* ShapeToString(EGP_PrimitiveShape Shape);
	GPRUNTIME_API const TCHAR* VisualSourceModeToString(EGP_VisualSourceMode Mode);
}

/**
 * Read-only diagnostics for Blueprint/SCS presentation meshes.
 * Does not mutate components. Excludes generated BuiltVisual parts and gameplay root collision.
 */
namespace GPAuthoredVisualDiagnostics
{
	struct FSnapshot
	{
		int32 AuthoredPrimitiveComponentCount = 0;
		int32 VisibleAuthoredMeshCount = 0;
		int32 AuthoredCollisionWarnings = 0;
		int32 AuthoredNavigationWarnings = 0;
		int32 AuthoredOverlapWarnings = 0;
		bool bMissingVisibleAuthoredMesh = false;
	};

	GPRUNTIME_API void Collect(
		const AActor* Owner,
		const TSet<const UActorComponent*>& GeneratedComponents,
		FSnapshot& OutSnapshot);
}
