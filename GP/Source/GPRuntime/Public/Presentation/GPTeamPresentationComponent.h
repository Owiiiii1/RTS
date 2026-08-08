// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GPTeamPresentationComponent.generated.h"

/**
 * Applies configured TeamId → TeamColor tint to owner mesh primitives (GP-S29R).
 * Presentation-only; does not mutate TeamId gameplay authority.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_TeamPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_TeamPresentationComponent();

	virtual void BeginPlay() override;

	/** Resolve settings color for current owner TeamId and apply to meshes. */
	UFUNCTION(BlueprintCallable, Category = "GP|Presentation|Team")
	void RefreshTeamPresentation();

	UFUNCTION(BlueprintPure, Category = "GP|Presentation|Team")
	FLinearColor GetAppliedTeamColor() const { return AppliedTeamColor; }

	UFUNCTION(BlueprintPure, Category = "GP|Presentation|Team")
	FLinearColor GetTeamPresentationColor() const;

private:
	void ApplyColorToMeshComponents(const FLinearColor& Color);

	UPROPERTY(VisibleInstanceOnly, Category = "GP|Presentation|Team")
	FLinearColor AppliedTeamColor = FLinearColor::White;

	bool bHasAppliedOnce = false;
};
