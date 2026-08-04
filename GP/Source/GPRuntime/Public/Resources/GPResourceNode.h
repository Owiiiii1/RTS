// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Resources/GPResourceTypes.h"
#include "GPResourceNode.generated.h"

class UBoxComponent;
class UGP_ResourceNodeVisualComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogGPResourceNode, Log, All);

/**
 * Server-authoritative ore deposit (GP-S27A1).
 * Not a unit/pawn. No gather loop, GAS, or command integration in this slice.
 */
UCLASS(Blueprintable)
class GPRUNTIME_API AGP_ResourceNode : public AActor
{
	GENERATED_BODY()

public:
	AGP_ResourceNode();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	EGP_ResourceType GetResourceType() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	int32 GetMaxAmount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	int32 GetCurrentAmount() const;

	UFUNCTION(BlueprintPure, Category = "GP|Resource")
	bool IsDepleted() const;

	/**
	 * Authority-only. Consumes up to RequestedAmount; returns actual consumed.
	 * Does not destroy the actor or change visuals on depletion.
	 */
	int32 ConsumeResource(int32 RequestedAmount);

	UGP_ResourceNodeVisualComponent* GetResourceNodeVisualComponent() const;
	UBoxComponent* GetCollisionBox() const;

protected:
	UFUNCTION()
	void OnRep_CurrentAmount();

	void ClampCurrentAmountToMax();

	/** Gameplay collision root — blocks movement/nav. Visual parts are separate NoCollision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Resource", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Resource|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_ResourceNodeVisualComponent> ResourceNodeVisualComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "GP|Resource")
	EGP_ResourceType ResourceType = EGP_ResourceType::Ore;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "GP|Resource", meta = (ClampMin = "0"))
	int32 MaxAmount = 5000;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentAmount, Category = "GP|Resource", meta = (ClampMin = "0"))
	int32 CurrentAmount = 5000;

private:
	void NormalizeAmountsOnConstruction();
};
