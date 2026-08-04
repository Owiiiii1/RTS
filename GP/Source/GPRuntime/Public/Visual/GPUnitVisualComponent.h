// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "GPUnitVisualComponent.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogGPUnitVisual, Log, All);

/**
 * Cosmetic composite primitive visual (GP-S26B1).
 * Builds Engine basic-shape parts from a native definition. No gameplay authority.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_UnitVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_UnitVisualComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	EGP_VisualArchetype GetVisualArchetype() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	int32 GetPartCount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool IsDedicatedVisualSuppressed() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool HasBuiltVisual() const;

	FName GetPresentationRootPartName() const;
	void GetPartNames(TArray<FName>& OutNames) const;
	bool AreVisualPartCollisionsDisabled() const;

	/** Clear and rebuild from current archetype definition (no-op on dedicated). */
	void RebuildVisual();

	void ClearVisual();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "GP|Visual")
	EGP_VisualArchetype VisualArchetype = EGP_VisualArchetype::InfantryMelee;

private:
	bool ShouldSuppressVisualConstruction() const;
	void BuildVisualFromDefinition(const FGP_PrimitiveVisualDefinition& Definition);
	UStaticMesh* ResolveShapeMesh(EGP_PrimitiveShape Shape) const;
	USceneComponent* ResolveAttachParent(const FGP_PrimitiveVisualPart& Part, USceneComponent* FallbackRoot) const;
	void ApplyTeamColorFallback();
	static FString GetEngineShapePath(EGP_PrimitiveShape Shape);

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PresentationRootComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PartComponents;

	TMap<FName, TObjectPtr<UStaticMeshComponent>> PartLookup;

	FName PresentationRootPartName;
	bool bVisualBuilt = false;
	bool bDedicatedVisualSuppressed = false;
};
