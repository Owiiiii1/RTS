// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Visual/GPPrimitiveVisualBuilder.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "GPUnitVisualComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPUnitVisual, Log, All);

/**
 * Cosmetic unit presentation (GP-S26B1 / S26B2A).
 * NativeFallback builds Engine basic-shape parts into BuiltVisual only.
 * AuthoredComponents leaves Blueprint/SCS meshes alone and never destroys them.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_UnitVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_UnitVisualComponent();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	EGP_VisualArchetype GetVisualArchetype() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	EGP_VisualSourceMode GetVisualSourceMode() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool UsesAuthoredComponents() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	int32 GetPartCount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool IsDedicatedVisualSuppressed() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool HasBuiltVisual() const;

	int32 GetGeneratedPartCount() const;
	int32 GetAuthoredPrimitiveComponentCount() const;
	int32 GetAuthoredCollisionWarningCount() const;
	int32 GetAuthoredNavigationWarningCount() const;
	int32 GetDuplicateGeneratedPartCount() const;
	bool AreGeneratedCollisionsDisabled() const;

	FName GetPresentationRootPartName() const;
	void GetPartNames(TArray<FName>& OutNames) const;
	bool AreVisualPartCollisionsDisabled() const;

	/** NativeFallback rebuild, or clear generated parts only in AuthoredComponents. */
	UFUNCTION(CallInEditor, Category = "GP|Visual")
	void RefreshVisualMode();

	/** Class-defaults / editor helper. Not a networked gameplay RPC. */
	UFUNCTION(BlueprintCallable, Category = "GP|Visual")
	void SetVisualSourceMode(EGP_VisualSourceMode NewMode);

	/** Clear and rebuild native parts when mode is NativeFallback (no-op build on dedicated / authored). */
	void RebuildVisual();

	/** Destroys only BuiltVisual generated parts — never Blueprint/SCS/gameplay components. */
	void ClearVisual();

	/** Re-apply TeamColor from UGP_GameplayPresentationSettings (GP-S29R). */
	void RefreshTeamColorFromPresentation();

protected:
	/**
	 * Class-defaults presentation ownership.
	 * C++ AGP_Unit default = NativeFallback; Blueprint subclasses set AuthoredComponents.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Visual")
	EGP_VisualSourceMode VisualSourceMode = EGP_VisualSourceMode::NativeFallback;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Visual")
	EGP_VisualArchetype VisualArchetype = EGP_VisualArchetype::InfantryMelee;

private:
	bool ShouldSuppressVisualConstruction() const;
	void ApplyTeamColorFallback();
	void RefreshAuthoredDiagnostics() const;

	GPPrimitiveVisualBuilder::FBuildResult BuiltVisual;
	bool bVisualBuilt = false;
	bool bDedicatedVisualSuppressed = false;

	mutable GPAuthoredVisualDiagnostics::FSnapshot CachedAuthoredSnapshot;
	mutable bool bAuthoredSnapshotDirty = true;
};
