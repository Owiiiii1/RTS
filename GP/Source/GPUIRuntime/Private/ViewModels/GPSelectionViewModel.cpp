// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPSelectionViewModel.h"

float UGP_SelectionViewModel::SafeNormalized(float Value, float Maximum)
{
	if (!FMath::IsFinite(Value) || !FMath::IsFinite(Maximum) || Maximum <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}
	return FMath::Clamp(Value / Maximum, 0.0f, 1.0f);
}

void UGP_SelectionViewModel::ResetPresentation()
{
	ApplyNone();
}

void UGP_SelectionViewModel::ApplyNone()
{
	SetMode(EGP_SelectionPresentationMode::None);
	SetSelectionCount(0);
	ClearSingleIdentity();
	ClearCargo();
	SetIsInspectPresentation(false);
	SetGroupRows(TArray<FGP_SelectionGroupRow>());
	OnSelectionPresentationChanged.Broadcast();
}

void UGP_SelectionViewModel::ApplySingle(
	const FText& InDisplayName,
	float InCurrentHealth,
	float InMaxHealth,
	float InDamage,
	float InArmor,
	float InMoveSpeed,
	bool bInIsUnit,
	bool bInIsBuilding,
	bool bInHasCargo,
	float InCargoAmount,
	float InCargoCapacity,
	bool bInIsInspectPresentation)
{
	SetMode(EGP_SelectionPresentationMode::Single);
	SetSelectionCount(1);
	SetDisplayName(InDisplayName);
	SetCurrentHealth(InCurrentHealth);
	SetMaxHealth(InMaxHealth);
	SetHealthNormalized(SafeNormalized(InCurrentHealth, InMaxHealth));
	SetDamage(InDamage);
	SetArmor(InArmor);
	SetMoveSpeed(InMoveSpeed);
	SetIsUnit(bInIsUnit);
	SetIsBuilding(bInIsBuilding);
	SetSingleCargo(bInHasCargo, InCargoAmount, InCargoCapacity);
	SetIsInspectPresentation(bInIsInspectPresentation);
	SetGroupRows(TArray<FGP_SelectionGroupRow>());
	OnSelectionPresentationChanged.Broadcast();
}

void UGP_SelectionViewModel::ApplyGroup(const TArray<FGP_SelectionGroupRow>& InRows)
{
	SetMode(EGP_SelectionPresentationMode::Group);
	SetSelectionCount(InRows.Num());
	ClearSingleIdentity();
	ClearCargo();
	SetIsInspectPresentation(false);
	SetGroupRows(InRows);
	OnSelectionPresentationChanged.Broadcast();
}

void UGP_SelectionViewModel::SetSingleVitals(float InCurrentHealth, float InMaxHealth)
{
	SetCurrentHealth(InCurrentHealth);
	SetMaxHealth(InMaxHealth);
	SetHealthNormalized(SafeNormalized(InCurrentHealth, InMaxHealth));
}

void UGP_SelectionViewModel::SetGroupRowVitals(int32 RowIndex, float InCurrentHealth, float InMaxHealth)
{
	if (!GroupRows.IsValidIndex(RowIndex))
	{
		return;
	}

	TArray<FGP_SelectionGroupRow> NextRows = GroupRows;
	FGP_SelectionGroupRow& Row = NextRows[RowIndex];
	Row.CurrentHealth = InCurrentHealth;
	Row.MaxHealth = InMaxHealth;
	Row.HealthNormalized = SafeNormalized(InCurrentHealth, InMaxHealth);
	SetGroupRows(NextRows);
	OnSelectionPresentationChanged.Broadcast();
}

void UGP_SelectionViewModel::SetSingleCargo(bool bInHasCargo, float InCargoAmount, float InCargoCapacity)
{
	SetHasCargo(bInHasCargo);
	SetCargoAmount(bInHasCargo ? InCargoAmount : 0.0f);
	SetCargoCapacity(bInHasCargo ? InCargoCapacity : 0.0f);
	SetCargoNormalized(bInHasCargo ? SafeNormalized(InCargoAmount, InCargoCapacity) : 0.0f);
}

void UGP_SelectionViewModel::ClearSingleIdentity()
{
	SetDisplayName(FText::GetEmpty());
	SetCurrentHealth(0.0f);
	SetMaxHealth(0.0f);
	SetHealthNormalized(0.0f);
	SetDamage(0.0f);
	SetArmor(0.0f);
	SetMoveSpeed(0.0f);
	SetIsUnit(false);
	SetIsBuilding(false);
}

void UGP_SelectionViewModel::ClearCargo()
{
	SetSingleCargo(false, 0.0f, 0.0f);
}

void UGP_SelectionViewModel::SetMode(EGP_SelectionPresentationMode Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(Mode, Value);
}

void UGP_SelectionViewModel::SetSelectionCount(int32 Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(SelectionCount, Value);
}

void UGP_SelectionViewModel::SetDisplayName(const FText& Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Value);
}

void UGP_SelectionViewModel::SetCurrentHealth(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? Value : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(CurrentHealth, SafeValue);
}

void UGP_SelectionViewModel::SetMaxHealth(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? Value : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(MaxHealth, SafeValue);
}

void UGP_SelectionViewModel::SetHealthNormalized(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(HealthNormalized, SafeValue);
}

void UGP_SelectionViewModel::SetDamage(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? Value : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(Damage, SafeValue);
}

void UGP_SelectionViewModel::SetArmor(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? Value : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(Armor, SafeValue);
}

void UGP_SelectionViewModel::SetMoveSpeed(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? Value : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(MoveSpeed, SafeValue);
}

void UGP_SelectionViewModel::SetIsUnit(bool bValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsUnit, bValue);
}

void UGP_SelectionViewModel::SetIsBuilding(bool bValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsBuilding, bValue);
}

void UGP_SelectionViewModel::SetHasCargo(bool bValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bHasCargo, bValue);
}

void UGP_SelectionViewModel::SetCargoAmount(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? Value : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(CargoAmount, SafeValue);
}

void UGP_SelectionViewModel::SetCargoCapacity(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? Value : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(CargoCapacity, SafeValue);
}

void UGP_SelectionViewModel::SetCargoNormalized(float Value)
{
	const float SafeValue = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f;
	UE_MVVM_SET_PROPERTY_VALUE(CargoNormalized, SafeValue);
}

void UGP_SelectionViewModel::SetIsInspectPresentation(bool bValue)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsInspectPresentation, bValue);
}

void UGP_SelectionViewModel::SetGroupRows(const TArray<FGP_SelectionGroupRow>& Value)
{
	UE_MVVM_SET_PROPERTY_VALUE(GroupRows, Value);
}
