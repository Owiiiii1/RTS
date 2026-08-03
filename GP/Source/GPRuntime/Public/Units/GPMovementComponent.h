// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GPMovementComponent.generated.h"

/**
 * Server-authoritative straight-line movement backend (GP-S20).
 * Authority mutates owner transform; component state is not replicated.
 * No Held Command integration, NavMesh, AIController, or RPC.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_MovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_MovementComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Authority-only. Returns true only when the move request is accepted. */
	bool RequestMove(const FVector& Destination, uint32 CommandSerial);

	/** Authority-only. Stops active movement without callbacks. */
	void StopMove();

	bool IsMoving() const;
	uint32 GetActiveMoveSerial() const;
	const FVector& GetMoveDestination() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement")
	float AcceptanceRadius = 50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement")
	float RotationSpeed = 360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Movement")
	bool bRotateToMovement = true;

private:
	void ClearActiveMovementState();

	FVector MoveDestination = FVector::ZeroVector;
	uint32 ActiveMoveSerial = 0;
	bool bIsMoving = false;
};
