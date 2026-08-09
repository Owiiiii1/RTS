// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/GPBuildingBase.h"
#include "GPMainBase.generated.h"

class UCapsuleComponent;
class USceneComponent;
class UGP_StorageComponent;

/**
 * Minimal MainBase host for StorageComponent + Worker drop-off (GP-S28).
 * GP-S28P1: PresentationRoot + DropOffVisualAnchor for Blueprint meshes.
 * Full content/visual MainBase remains GP-S39. No production/construction/launch.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_MainBase : public AGP_BuildingBase
{
	GENERATED_BODY()

public:
	AGP_MainBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void NotifyTeamIdChanged(int32 OldTeamId, int32 NewTeamId) override;

	/** Authority refresh: register only when TeamId is playable (>=1). */
	void RefreshMainBaseRegistration();

	UFUNCTION(BlueprintPure, Category = "GP|MainBase")
	UGP_StorageComponent* GetStorageComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|MainBase")
	UCapsuleComponent* GetCapsuleComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|MainBase|Presentation")
	USceneComponent* GetPresentationRoot() const;

	UFUNCTION(BlueprintPure, Category = "GP|MainBase|Presentation")
	USceneComponent* GetDropOffVisualAnchor() const;

	/** Authored Unit Drop Zone (unit pods only). Reposition in BP-derived MainBase. */
	UFUNCTION(BlueprintPure, Category = "GP|MainBase|Orbital")
	USceneComponent* GetUnitDropZone() const;

	UFUNCTION(BlueprintPure, Category = "GP|MainBase|Storage")
	float GetPlanetaryStored() const;

	UFUNCTION(BlueprintPure, Category = "GP|MainBase|Storage")
	float GetPlanetaryCapacity() const;

	/**
	 * Drop-off interaction range (cm). BuildingDefinition not present yet —
	 * TDD/07 / TDD/10 placeholder 400 cm. Not driven by DropOffVisualAnchor.
	 */
	UFUNCTION(BlueprintPure, Category = "GP|MainBase")
	float GetDropOffRangeCm() const { return DropOffRangeCm; }

	/**
	 * Canonical MainBase drop-off distance (GP-S33M): ground-plane Dist2D to actor center.
	 * Actor-origin Z must not consume DropOffRange budget.
	 */
	UFUNCTION(BlueprintPure, Category = "GP|MainBase")
	float ComputeDropOffDistance2D(const FVector& FromLocation) const;

	UFUNCTION(BlueprintPure, Category = "GP|MainBase")
	bool IsWithinDropOffRange2D(const FVector& FromLocation) const;

	bool ValidateMainBaseContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

	/** BP mesh attach parent under Capsule. No StaticMesh in C++. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PresentationRoot;

	/** Presentation-only drop-off marker. Gameplay DropOffRangeCm is independent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> DropOffVisualAnchor;

	/**
	 * Unit DropPod landing anchor (GP-S31R). Authored-relative; not used by building pods later.
	 * Default offset from PresentationRoot — reposition in BP without C++ hardcoded world coords.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Orbital", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> UnitDropZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Storage", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_StorageComponent> StorageComponent;

	/** Temporary canonical placeholder until UGP_BuildingDefinition exists (TDD 400 cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "GP|MainBase", meta = (ClampMin = "1.0"))
	float DropOffRangeCm = 400.0f;

private:
	void RegisterWithGameState();
	void UnregisterFromGameState();
	bool bRegisteredWithGameState = false;
};
