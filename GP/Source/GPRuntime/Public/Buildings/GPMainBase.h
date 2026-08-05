// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/GPBuildingBase.h"
#include "GPMainBase.generated.h"

class UCapsuleComponent;
class UGP_StorageComponent;

/**
 * Minimal MainBase host for StorageComponent + Worker drop-off (GP-S28).
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

	UFUNCTION(BlueprintPure, Category = "GP|MainBase")
	UGP_StorageComponent* GetStorageComponent() const;

	UFUNCTION(BlueprintPure, Category = "GP|MainBase")
	UCapsuleComponent* GetCapsuleComponent() const;

	/**
	 * Drop-off interaction range (cm). BuildingDefinition not present yet —
	 * TDD/07 / TDD/10 placeholder 400 cm.
	 */
	UFUNCTION(BlueprintPure, Category = "GP|MainBase")
	float GetDropOffRangeCm() const { return DropOffRangeCm; }

	bool ValidateMainBaseContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCapsuleComponent> CapsuleComponent;

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
