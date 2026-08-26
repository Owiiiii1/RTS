// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GPSelectionViewModel.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EGP_SelectionPresentationMode : uint8
{
	None UMETA(DisplayName = "None"),
	Single UMETA(DisplayName = "Single"),
	Group UMETA(DisplayName = "Group")
};

USTRUCT(BlueprintType)
struct GPUIRUNTIME_API FGP_SelectionGroupRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Selection")
	int32 Index = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Selection")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Selection")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Selection")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Selection")
	float MaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Selection")
	float HealthNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Selection")
	bool bIsUnit = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|Selection")
	bool bIsBuilding = false;

	bool operator==(const FGP_SelectionGroupRow& Other) const
	{
		return Index == Other.Index
			&& DisplayName.IdenticalTo(Other.DisplayName)
			&& Icon == Other.Icon
			&& FMath::IsNearlyEqual(CurrentHealth, Other.CurrentHealth)
			&& FMath::IsNearlyEqual(MaxHealth, Other.MaxHealth)
			&& FMath::IsNearlyEqual(HealthNormalized, Other.HealthNormalized)
			&& bIsUnit == Other.bIsUnit
			&& bIsBuilding == Other.bIsBuilding;
	}

	bool operator!=(const FGP_SelectionGroupRow& Other) const
	{
		return !(*this == Other);
	}
};

DECLARE_MULTICAST_DELEGATE(FOnGPSelectionPresentationChanged);

/**
 * Canonical production HUD selection presentation.
 * Populated only by UGP_SelectionViewModelAdapter. Widgets must not write these fields.
 */
UCLASS(BlueprintType)
class GPUIRUNTIME_API UGP_SelectionViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	EGP_SelectionPresentationMode Mode = EGP_SelectionPresentationMode::None;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	int32 SelectionCount = 0;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float CurrentHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float MaxHealth = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float HealthNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float Damage = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float Armor = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float MoveSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float AttackRange = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	bool bIsUnit = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	bool bIsBuilding = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	bool bHasCargo = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float CargoAmount = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float CargoCapacity = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	float CargoNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	bool bIsInspectPresentation = false;

	/**
	 * Canonical group rows. UE MVVM list binding to TArray<struct> is not the operator seam;
	 * WBP should refresh from UGP_HUDRootWidget::GetSelectionGroupRows after BP_OnSelectionPresentationChanged.
	 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "GP|HUD|Selection")
	TArray<FGP_SelectionGroupRow> GroupRows;

	const TArray<FGP_SelectionGroupRow>& GetGroupRows() const { return GroupRows; }

	FOnGPSelectionPresentationChanged OnSelectionPresentationChanged;

	void ResetPresentation();
	void ApplyNone();
	void ApplySingle(
		const FText& InDisplayName,
		float InCurrentHealth,
		float InMaxHealth,
		float InDamage,
		float InArmor,
		float InMoveSpeed,
		float InAttackRange,
		UTexture2D* InIcon,
		bool bInIsUnit,
		bool bInIsBuilding,
		bool bInHasCargo,
		float InCargoAmount,
		float InCargoCapacity,
		bool bInIsInspectPresentation);
	void ApplyGroup(const TArray<FGP_SelectionGroupRow>& InRows);
	void SetSingleVitals(float InCurrentHealth, float InMaxHealth);
	void SetGroupRowVitals(int32 RowIndex, float InCurrentHealth, float InMaxHealth);
	void SetSingleCargo(bool bInHasCargo, float InCargoAmount, float InCargoCapacity);

private:
	void SetMode(EGP_SelectionPresentationMode Value);
	void SetSelectionCount(int32 Value);
	void SetDisplayName(const FText& Value);
	void SetCurrentHealth(float Value);
	void SetMaxHealth(float Value);
	void SetHealthNormalized(float Value);
	void SetDamage(float Value);
	void SetArmor(float Value);
	void SetMoveSpeed(float Value);
	void SetAttackRange(float Value);
	void SetIcon(UTexture2D* Value);
	void SetIsUnit(bool bValue);
	void SetIsBuilding(bool bValue);
	void SetHasCargo(bool bValue);
	void SetCargoAmount(float Value);
	void SetCargoCapacity(float Value);
	void SetCargoNormalized(float Value);
	void SetIsInspectPresentation(bool bValue);
	void SetGroupRows(const TArray<FGP_SelectionGroupRow>& Value);
	void ClearSingleIdentity();
	void ClearCargo();
	static float SafeNormalized(float Value, float Maximum);
};
