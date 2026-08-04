// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Visual/GPPrimitiveVisualBuilder.h"
#include "Visual/GPPrimitiveVisualProfile.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "GPUnitVisualComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPUnitVisual, Log, All);

/**
 * Cosmetic composite primitive visual (GP-S26B1 / S26B2A).
 * Builds from editable DataAsset profile with native InfantryMelee fallback.
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
	int32 GetPartCount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool IsDedicatedVisualSuppressed() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool HasBuiltVisual() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool IsUsingFallback() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	EGP_VisualDefinitionSource GetActiveVisualSource() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	FString GetVisualProfilePath() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool IsProfileValid() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	int32 GetProfileValidationErrorCount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	int32 GetProfilePartCount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	bool IsHierarchyValid() const;

	UFUNCTION(BlueprintPure, Category = "GP|Visual")
	int32 GetDuplicatePartNameCount() const;

	FName GetPresentationRootPartName() const;
	void GetPartNames(TArray<FName>& OutNames) const;
	bool AreVisualPartCollisionsDisabled() const;

	UFUNCTION(BlueprintCallable, Category = "GP|Visual")
	void SetVisualProfile(TSoftObjectPtr<UGP_PrimitiveVisualProfile> NewProfile);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GP|Visual")
	void RebuildVisual();

	void ClearVisual();

protected:
	UPROPERTY(EditAnywhere, Category = "GP|Visual")
	TSoftObjectPtr<UGP_PrimitiveVisualProfile> VisualProfile;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Visual")
	EGP_VisualArchetype VisualArchetype = EGP_VisualArchetype::InfantryMelee;

private:
	bool ShouldSuppressVisualConstruction() const;
	void ApplyTeamColorFallback();
	FGP_PrimitiveVisualDefinition ResolveDefinition();

	GPPrimitiveVisualBuilder::FBuildResult BuiltVisual;
	FGP_PrimitiveVisualDefinition CachedDefinition;
	bool bVisualBuilt = false;
	bool bDedicatedVisualSuppressed = false;
	bool bUsingFallback = true;
	bool bProfileValid = false;
	bool bHierarchyValid = true;
	int32 ProfileValidationErrorCount = 0;
	int32 ProfilePartCount = 0;
	int32 DuplicatePartNameCount = 0;
	EGP_VisualDefinitionSource ActiveVisualSource = EGP_VisualDefinitionSource::NativeFallback;
};
