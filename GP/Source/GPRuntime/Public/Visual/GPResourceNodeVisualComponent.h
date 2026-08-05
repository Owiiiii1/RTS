// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Visual/GPPrimitiveVisualBuilder.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "GPResourceNodeVisualComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPResourceNodeVisual, Log, All);

/**
 * Cosmetic resource-node presentation (GP-S27A1 / S26B2A).
 * NativeFallback builds Ore primitives into BuiltVisual only.
 * AuthoredComponents keeps Blueprint/SCS meshes; gameplay Box stays authoritative.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_ResourceNodeVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_ResourceNodeVisualComponent();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	EGP_VisualSourceMode GetVisualSourceMode() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool UsesAuthoredComponents() const;

	int32 GetPartCount() const;
	bool IsDedicatedVisualSuppressed() const;
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

	UFUNCTION(CallInEditor, Category = "GP|Visual")
	void RefreshVisualMode();

	UFUNCTION(BlueprintCallable, Category = "GP|Visual")
	void SetVisualSourceMode(EGP_VisualSourceMode NewMode);

	void RebuildVisual();
	void ClearVisual();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Visual")
	EGP_VisualSourceMode VisualSourceMode = EGP_VisualSourceMode::NativeFallback;

private:
	bool ShouldSuppressVisualConstruction() const;
	void RefreshAuthoredDiagnostics() const;

	GPPrimitiveVisualBuilder::FBuildResult BuiltVisual;
	bool bVisualBuilt = false;
	bool bDedicatedVisualSuppressed = false;

	mutable GPAuthoredVisualDiagnostics::FSnapshot CachedAuthoredSnapshot;
	mutable bool bAuthoredSnapshotDirty = true;
};
