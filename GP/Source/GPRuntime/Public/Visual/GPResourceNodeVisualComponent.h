// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Visual/GPPrimitiveVisualBuilder.h"
#include "GPResourceNodeVisualComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGPResourceNodeVisual, Log, All);

/**
 * Cosmetic Ore primitive composition for AGP_ResourceNode (GP-S27A1).
 * Reuses shared primitive builder/mesh helpers. No team tint. No unit coupling.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_ResourceNodeVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_ResourceNodeVisualComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	int32 GetPartCount() const;
	bool IsDedicatedVisualSuppressed() const;
	bool HasBuiltVisual() const;
	FName GetPresentationRootPartName() const;
	void GetPartNames(TArray<FName>& OutNames) const;
	bool AreVisualPartCollisionsDisabled() const;

	void RebuildVisual();
	void ClearVisual();

private:
	bool ShouldSuppressVisualConstruction() const;

	GPPrimitiveVisualBuilder::FBuildResult BuiltVisual;
	bool bVisualBuilt = false;
	bool bDedicatedVisualSuppressed = false;
};
