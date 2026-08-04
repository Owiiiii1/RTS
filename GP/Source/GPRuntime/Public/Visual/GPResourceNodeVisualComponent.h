// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Visual/GPPrimitiveVisualBuilder.h"
#include "Visual/GPPrimitiveVisualProfile.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "GPResourceNodeVisualComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPResourceNodeVisual, Log, All);

/**
 * Cosmetic Ore primitive composition for AGP_ResourceNode (GP-S27A1 / S26B2A).
 * Editable DataAsset profile with native Ore fallback. No team tint.
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

	int32 GetPartCount() const;
	bool IsDedicatedVisualSuppressed() const;
	bool HasBuiltVisual() const;
	bool IsUsingFallback() const;
	EGP_VisualDefinitionSource GetActiveVisualSource() const;
	FString GetVisualProfilePath() const;
	bool IsProfileValid() const;
	int32 GetProfileValidationErrorCount() const;
	int32 GetProfilePartCount() const;
	bool IsHierarchyValid() const;
	int32 GetDuplicatePartNameCount() const;
	FName GetPresentationRootPartName() const;
	void GetPartNames(TArray<FName>& OutNames) const;
	bool AreVisualPartCollisionsDisabled() const;

	void SetVisualProfile(TSoftObjectPtr<UGP_PrimitiveVisualProfile> NewProfile);

	UFUNCTION(CallInEditor, Category = "GP|Visual")
	void RebuildVisual();

	void ClearVisual();

protected:
	UPROPERTY(EditAnywhere, Category = "GP|Visual")
	TSoftObjectPtr<UGP_PrimitiveVisualProfile> VisualProfile;

private:
	bool ShouldSuppressVisualConstruction() const;
	FGP_PrimitiveVisualDefinition ResolveDefinition();

	GPPrimitiveVisualBuilder::FBuildResult BuiltVisual;
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
