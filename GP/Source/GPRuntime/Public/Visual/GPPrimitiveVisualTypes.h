// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPPrimitiveVisualTypes.generated.h"

/** Engine basic-shape kind used by primitive MVP visuals (GP-S26B1 / S26B2A). */
UENUM(BlueprintType)
enum class EGP_PrimitiveShape : uint8
{
	Cube UMETA(DisplayName = "Cube"),
	Sphere UMETA(DisplayName = "Sphere"),
	Cylinder UMETA(DisplayName = "Cylinder"),
	Cone UMETA(DisplayName = "Cone"),
	/** Resolved as Engine Cylinder with capsule-like scale (no Engine Capsule basic mesh). */
	Capsule UMETA(DisplayName = "Capsule")
};

/** Visual archetype identity (cosmetic only; not used by combat gameplay). */
UENUM(BlueprintType)
enum class EGP_VisualArchetype : uint8
{
	InfantryMelee UMETA(DisplayName = "InfantryMelee")
	// Future: InfantryRanged, HeavyInfantry, Worker, Tank, Artillery, Turret, Monsters, Buildings, ResourceNode
};

/** Collision policy for visual parts (MVP: always NoCollision). */
UENUM(BlueprintType)
enum class EGP_PrimitiveVisualCollisionPolicy : uint8
{
	NoCollision UMETA(DisplayName = "NoCollision")
};

/** Visibility policy for visual parts. */
UENUM(BlueprintType)
enum class EGP_PrimitiveVisualVisibilityPolicy : uint8
{
	AlwaysVisible UMETA(DisplayName = "AlwaysVisible"),
	HideOnDedicatedServer UMETA(DisplayName = "HideOnDedicatedServer")
};

/** Cosmetic profile category (not gameplay). */
UENUM(BlueprintType)
enum class EGP_PrimitiveVisualProfileCategory : uint8
{
	Generic UMETA(DisplayName = "Generic"),
	Unit UMETA(DisplayName = "Unit"),
	Resource UMETA(DisplayName = "Resource"),
	Building UMETA(DisplayName = "Building")
};

/** Where the active visual definition came from. */
UENUM(BlueprintType)
enum class EGP_VisualDefinitionSource : uint8
{
	NativeFallback UMETA(DisplayName = "NativeFallback"),
	DataAsset UMETA(DisplayName = "DataAsset")
};

/** Single primitive part in a composite visual definition. */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_PrimitiveVisualPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	FName PartName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	EGP_PrimitiveShape Shape = EGP_PrimitiveShape::Cube;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	FVector RelativeScale = FVector::OneVector;

	/**
	 * Empty = attach under root / presentation root (see builder policy).
	 * Explicit parent uses that part; unresolved parent falls back to actor root with warning.
	 * Caution: non-uniform parent scale inherits to children (Ore uses uniform Base for this reason).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	FName ParentPartName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	bool bPresentationRoot = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual|Roles")
	bool bBody = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual|Roles")
	bool bFacingIndicator = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual|Roles")
	bool bWeapon = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual|Roles")
	bool bTurret = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual|Roles")
	bool bAnimated = false;

	/** When true, unit team tint DMI may be applied to this part. Ore parts keep false. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual|Team")
	bool bTeamTintEligible = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	bool bCastShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	bool bVisible = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	EGP_PrimitiveVisualCollisionPolicy CollisionPolicy = EGP_PrimitiveVisualCollisionPolicy::NoCollision;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	EGP_PrimitiveVisualVisibilityPolicy VisibilityPolicy = EGP_PrimitiveVisualVisibilityPolicy::HideOnDedicatedServer;
};

/** Native / resolved composite visual definition. */
USTRUCT(BlueprintType)
struct GPRUNTIME_API FGP_PrimitiveVisualDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	EGP_VisualArchetype Archetype = EGP_VisualArchetype::InfantryMelee;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GP|Visual")
	TArray<FGP_PrimitiveVisualPart> Parts;
};

struct FGP_PrimitiveVisualValidationResult
{
	bool bValid = false;
	bool bHierarchyValid = true;
	int32 DuplicatePartNameCount = 0;
	TArray<FString> Errors;
	TArray<FString> Warnings;
	FGP_PrimitiveVisualDefinition SanitizedDefinition;
};

namespace GPPrimitiveVisualDefaults
{
	static constexpr int32 MaxParts = 32;

	/** Soft paths for default cooked profiles (GP-S26B2A). */
	inline const TCHAR* DefaultInfantryMeleeProfilePath()
	{
		return TEXT("/Game/GrimProtocol/VisualProfiles/DA_Visual_InfantryMelee.DA_Visual_InfantryMelee");
	}

	inline const TCHAR* DefaultOreProfilePath()
	{
		return TEXT("/Game/GrimProtocol/VisualProfiles/DA_Visual_Ore.DA_Visual_Ore");
	}

	/** Native InfantryMelee prototype (≤4 parts). Cosmetic only. */
	GPRUNTIME_API FGP_PrimitiveVisualDefinition MakeInfantryMeleeDefinition();

	/** Native Ore resource-node pile (≤6 parts). Cosmetic only. */
	GPRUNTIME_API FGP_PrimitiveVisualDefinition MakeOreNodeDefinition();

	GPRUNTIME_API FGP_PrimitiveVisualDefinition MakeDefinitionForArchetype(EGP_VisualArchetype Archetype);

	GPRUNTIME_API const TCHAR* ArchetypeToString(EGP_VisualArchetype Archetype);
	GPRUNTIME_API const TCHAR* ShapeToString(EGP_PrimitiveShape Shape);
	GPRUNTIME_API const TCHAR* VisualSourceToString(EGP_VisualDefinitionSource Source);

	GPRUNTIME_API bool IsSupportedShape(EGP_PrimitiveShape Shape);

	/** Validate + sanitize a definition. Hard failures → bValid=false. */
	GPRUNTIME_API FGP_PrimitiveVisualValidationResult ValidateAndSanitizeDefinition(
		const FGP_PrimitiveVisualDefinition& InDefinition);
}
