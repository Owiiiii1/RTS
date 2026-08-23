// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPSelectionViewModelAdapter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Player/GPSelectionComponent.h"
#include "Resources/GPCargoComponent.h"
#include "Units/GPUnitBase.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPWorker.h"

bool UGP_SelectionViewModelAdapter::Initialize(
	UGP_SelectionViewModel* InViewModel,
	UGP_SelectionComponent* InSelectionComponent)
{
	Shutdown();
	ViewModel = InViewModel;
	if (ViewModel == nullptr || !IsValid(InSelectionComponent))
	{
		if (ViewModel != nullptr)
		{
			ViewModel->ResetPresentation();
		}
		return false;
	}

	BoundSelection = InSelectionComponent;
	SelectionChangedHandle = InSelectionComponent->OnSelectionChanged().AddUObject(
		this, &ThisClass::HandleSelectionChanged);
	RebuildPresentation();
	return true;
}

void UGP_SelectionViewModelAdapter::Shutdown()
{
	UnbindPresentedUnits();
	if (UGP_SelectionComponent* Selection = BoundSelection.Get())
	{
		Selection->OnSelectionChanged().Remove(SelectionChangedHandle);
	}
	SelectionChangedHandle.Reset();
	BoundSelection.Reset();
	if (ViewModel != nullptr)
	{
		ViewModel->ResetPresentation();
	}
	ViewModel = nullptr;
}

int32 UGP_SelectionViewModelAdapter::GetBoundDelegateCount() const
{
	int32 Count = SelectionChangedHandle.IsValid() ? 1 : 0;
	for (const FBoundPresentationUnit& Bound : BoundUnits)
	{
		Count += Bound.HealthHandle.IsValid() ? 1 : 0;
		Count += Bound.MaxHealthHandle.IsValid() ? 1 : 0;
		Count += Bound.DiedHandle.IsValid() ? 1 : 0;
		Count += Bound.Unit.IsValid() ? 1 : 0;
		Count += Bound.Cargo.IsValid() ? 1 : 0;
	}
	return Count;
}

void UGP_SelectionViewModelAdapter::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UGP_SelectionViewModelAdapter::HandleSelectionChanged()
{
	RebuildPresentation();
}

void UGP_SelectionViewModelAdapter::RebuildPresentation()
{
	if (ViewModel == nullptr)
	{
		return;
	}

	UnbindPresentedUnits();

	UGP_SelectionComponent* Selection = BoundSelection.Get();
	if (!IsValid(Selection))
	{
		ViewModel->ApplyNone();
		return;
	}

	TArray<AGP_UnitBase*> LiveSelected;
	CollectLiveSelectedUnits(Selection, LiveSelected);
	if (LiveSelected.Num() >= 1)
	{
		if (LiveSelected.Num() == 1)
		{
			AGP_UnitBase* Unit = LiveSelected[0];
			FText Name;
			float Damage = 0.0f;
			float Armor = 0.0f;
			float MoveSpeed = 0.0f;
			bool bIsUnit = false;
			bool bIsBuilding = false;
			ReadStaticStats(Unit, Name, Damage, Armor, MoveSpeed, bIsUnit, bIsBuilding);
			float CurrentHealth = 0.0f;
			float MaxHealth = 0.0f;
			ReadVitals(Unit, CurrentHealth, MaxHealth);
			UGP_CargoComponent* Cargo = ResolveWorkerCargo(Unit);
			const bool bHasCargo = Cargo != nullptr && Cargo->GetCargoCapacity() > KINDA_SMALL_NUMBER;
			ViewModel->ApplySingle(
				Name,
				CurrentHealth,
				MaxHealth,
				Damage,
				Armor,
				MoveSpeed,
				bIsUnit,
				bIsBuilding,
				bHasCargo,
				bHasCargo ? Cargo->GetCurrentCargoAmount() : 0.0f,
				bHasCargo ? Cargo->GetCargoCapacity() : 0.0f,
				false);
		}
		else
		{
			TArray<FGP_SelectionGroupRow> Rows;
			Rows.Reserve(LiveSelected.Num());
			for (int32 Index = 0; Index < LiveSelected.Num(); ++Index)
			{
				FGP_SelectionGroupRow Row;
				FillGroupRow(LiveSelected[Index], Index, Row);
				Rows.Add(Row);
			}
			ViewModel->ApplyGroup(Rows);
		}
		BindPresentedUnits(LiveSelected);
		return;
	}

	if (AGP_UnitBase* Inspected = ResolveInspectedUnit(Selection))
	{
		FText Name;
		float Damage = 0.0f;
		float Armor = 0.0f;
		float MoveSpeed = 0.0f;
		bool bIsUnit = false;
		bool bIsBuilding = false;
		ReadStaticStats(Inspected, Name, Damage, Armor, MoveSpeed, bIsUnit, bIsBuilding);
		float CurrentHealth = 0.0f;
		float MaxHealth = 0.0f;
		ReadVitals(Inspected, CurrentHealth, MaxHealth);
		UGP_CargoComponent* Cargo = ResolveWorkerCargo(Inspected);
		const bool bHasCargo = Cargo != nullptr && Cargo->GetCargoCapacity() > KINDA_SMALL_NUMBER;
		TArray<AGP_UnitBase*> InspectedUnits;
		InspectedUnits.Add(Inspected);
		ViewModel->ApplySingle(
			Name,
			CurrentHealth,
			MaxHealth,
			Damage,
			Armor,
			MoveSpeed,
			bIsUnit,
			bIsBuilding,
			bHasCargo,
			bHasCargo ? Cargo->GetCurrentCargoAmount() : 0.0f,
			bHasCargo ? Cargo->GetCargoCapacity() : 0.0f,
			true);
		BindPresentedUnits(InspectedUnits);
		return;
	}

	ViewModel->ApplyNone();
}

void UGP_SelectionViewModelAdapter::UnbindPresentedUnits()
{
	for (FBoundPresentationUnit& Bound : BoundUnits)
	{
		if (UAbilitySystemComponent* ASC = Bound.ASC.Get())
		{
			if (Bound.HealthHandle.IsValid())
			{
				ASC->GetGameplayAttributeValueChangeDelegate(
					UGP_UnitAttributeSet::GetHealthAttribute()).Remove(Bound.HealthHandle);
			}
			if (Bound.MaxHealthHandle.IsValid())
			{
				ASC->GetGameplayAttributeValueChangeDelegate(
					UGP_UnitAttributeSet::GetMaxHealthAttribute()).Remove(Bound.MaxHealthHandle);
			}
		}
		if (AGP_UnitBase* Unit = Bound.Unit.Get())
		{
			if (Bound.DiedHandle.IsValid())
			{
				Unit->OnUnitDied().Remove(Bound.DiedHandle);
			}
			Unit->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleBoundActorDestroyed);
		}
		if (UGP_CargoComponent* Cargo = Bound.Cargo.Get())
		{
			Cargo->OnCargoAmountChanged.RemoveDynamic(this, &ThisClass::HandleCargoChanged);
		}
		Bound.HealthHandle.Reset();
		Bound.MaxHealthHandle.Reset();
		Bound.DiedHandle.Reset();
	}
	BoundUnits.Reset();
}

void UGP_SelectionViewModelAdapter::BindPresentedUnits(const TArray<AGP_UnitBase*>& Units)
{
	BoundUnits.Reserve(Units.Num());
	for (AGP_UnitBase* Unit : Units)
	{
		if (!IsValid(Unit))
		{
			continue;
		}

		FBoundPresentationUnit Bound;
		Bound.Unit = Unit;
		Unit->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleBoundActorDestroyed);
		Bound.DiedHandle = Unit->OnUnitDied().AddUObject(this, &ThisClass::HandleUnitDied);

		if (UAbilitySystemComponent* ASC = Unit->GetGPAbilitySystemComponent())
		{
			Bound.ASC = ASC;
			Bound.HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(
				UGP_UnitAttributeSet::GetHealthAttribute()).AddUObject(
					this, &ThisClass::HandleHealthChanged);
			Bound.MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(
				UGP_UnitAttributeSet::GetMaxHealthAttribute()).AddUObject(
					this, &ThisClass::HandleMaxHealthChanged);
		}

		if (UGP_CargoComponent* Cargo = ResolveWorkerCargo(Unit))
		{
			Bound.Cargo = Cargo;
			Cargo->OnCargoAmountChanged.AddUniqueDynamic(this, &ThisClass::HandleCargoChanged);
		}

		BoundUnits.Add(MoveTemp(Bound));
	}
}

void UGP_SelectionViewModelAdapter::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	(void)Data;
	for (const FBoundPresentationUnit& Bound : BoundUnits)
	{
		PushVitalsForUnit(Bound.Unit.Get());
	}
}

void UGP_SelectionViewModelAdapter::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	HandleHealthChanged(Data);
}

void UGP_SelectionViewModelAdapter::HandleUnitDied(AGP_UnitBase* Unit)
{
	(void)Unit;
	RebuildPresentation();
}

void UGP_SelectionViewModelAdapter::HandleBoundActorDestroyed(AActor* DestroyedActor)
{
	(void)DestroyedActor;
	RebuildPresentation();
}

void UGP_SelectionViewModelAdapter::HandleCargoChanged(
	float PreviousAmount,
	float NewAmount,
	float Capacity,
	float Delta)
{
	(void)PreviousAmount;
	(void)Delta;
	if (ViewModel == nullptr || ViewModel->Mode != EGP_SelectionPresentationMode::Single)
	{
		return;
	}
	ViewModel->SetSingleCargo(Capacity > KINDA_SMALL_NUMBER, NewAmount, Capacity);
}

void UGP_SelectionViewModelAdapter::PushVitalsForUnit(AGP_UnitBase* Unit)
{
	if (!IsValid(Unit) || ViewModel == nullptr)
	{
		return;
	}

	float CurrentHealth = 0.0f;
	float MaxHealth = 0.0f;
	ReadVitals(Unit, CurrentHealth, MaxHealth);

	if (ViewModel->Mode == EGP_SelectionPresentationMode::Single)
	{
		ViewModel->SetSingleVitals(CurrentHealth, MaxHealth);
		return;
	}

	if (ViewModel->Mode == EGP_SelectionPresentationMode::Group)
	{
		for (int32 Index = 0; Index < BoundUnits.Num(); ++Index)
		{
			if (BoundUnits[Index].Unit.Get() == Unit)
			{
				ViewModel->SetGroupRowVitals(Index, CurrentHealth, MaxHealth);
				break;
			}
		}
	}
}

void UGP_SelectionViewModelAdapter::CollectLiveSelectedUnits(
	const UGP_SelectionComponent* Selection,
	TArray<AGP_UnitBase*>& OutUnits)
{
	OutUnits.Reset();
	if (Selection == nullptr)
	{
		return;
	}

	for (const TWeakObjectPtr<AGP_UnitBase>& WeakUnit : Selection->GetSelectedUnits())
	{
		AGP_UnitBase* Unit = WeakUnit.Get();
		if (IsValid(Unit) && !Unit->IsDead())
		{
			OutUnits.Add(Unit);
		}
	}
}

AGP_UnitBase* UGP_SelectionViewModelAdapter::ResolveInspectedUnit(const UGP_SelectionComponent* Selection)
{
	if (Selection == nullptr)
	{
		return nullptr;
	}

	AGP_UnitBase* Inspected = Cast<AGP_UnitBase>(Selection->GetInspectedTarget());
	if (!IsValid(Inspected) || Inspected->IsDead() || !Inspected->IsGameplayInspectable())
	{
		return nullptr;
	}
	return Inspected;
}

void UGP_SelectionViewModelAdapter::FillGroupRow(AGP_UnitBase* Unit, int32 Index, FGP_SelectionGroupRow& OutRow)
{
	OutRow.Index = Index;
	FText Name;
	float Damage = 0.0f;
	float Armor = 0.0f;
	float MoveSpeed = 0.0f;
	ReadStaticStats(Unit, Name, Damage, Armor, MoveSpeed, OutRow.bIsUnit, OutRow.bIsBuilding);
	OutRow.DisplayName = Name;
	ReadVitals(Unit, OutRow.CurrentHealth, OutRow.MaxHealth);
	OutRow.HealthNormalized = (OutRow.MaxHealth > KINDA_SMALL_NUMBER)
		? FMath::Clamp(OutRow.CurrentHealth / OutRow.MaxHealth, 0.0f, 1.0f)
		: 0.0f;
}

void UGP_SelectionViewModelAdapter::ReadVitals(
	const AGP_UnitBase* Unit,
	float& OutCurrentHealth,
	float& OutMaxHealth)
{
	OutCurrentHealth = 0.0f;
	OutMaxHealth = 0.0f;
	if (Unit == nullptr)
	{
		return;
	}

	if (const UGP_UnitAttributeSet* Attrs = Unit->GetUnitAttributeSet())
	{
		OutCurrentHealth = Attrs->GetHealth();
		OutMaxHealth = Attrs->GetMaxHealth();
	}

	if (OutMaxHealth <= KINDA_SMALL_NUMBER)
	{
		if (const UGP_UnitDefinition* Definition = Unit->ResolveLoadedUnitDefinition())
		{
			OutMaxHealth = Definition->MaxHealth;
		}
	}
}

void UGP_SelectionViewModelAdapter::ReadStaticStats(
	const AGP_UnitBase* Unit,
	FText& OutDisplayName,
	float& OutDamage,
	float& OutArmor,
	float& OutMoveSpeed,
	bool& bOutIsUnit,
	bool& bOutIsBuilding)
{
	OutDisplayName = FText::GetEmpty();
	OutDamage = 0.0f;
	OutArmor = 0.0f;
	OutMoveSpeed = 0.0f;
	bOutIsUnit = false;
	bOutIsBuilding = false;
	if (Unit == nullptr)
	{
		return;
	}

	bOutIsUnit = Unit->IsSelectionTypeUnit();
	bOutIsBuilding = Unit->IsSelectionTypeBuilding();

	if (const UGP_UnitDefinition* Definition = Unit->ResolveLoadedUnitDefinition())
	{
		OutDisplayName = Definition->DisplayName;
		OutDamage = Definition->Damage;
		OutArmor = Definition->Armor;
		OutMoveSpeed = Definition->MoveSpeedCmPerSecond;
		return;
	}

	if (const UGP_UnitAttributeSet* Attrs = Unit->GetUnitAttributeSet())
	{
		OutDamage = Attrs->GetDamage();
		OutArmor = Attrs->GetArmor();
		OutMoveSpeed = Attrs->GetMoveSpeed();
	}
}

UGP_CargoComponent* UGP_SelectionViewModelAdapter::ResolveWorkerCargo(AGP_UnitBase* Unit)
{
	const AGP_Worker* Worker = Cast<AGP_Worker>(Unit);
	return Worker != nullptr ? Worker->GetCargoComponent() : nullptr;
}
