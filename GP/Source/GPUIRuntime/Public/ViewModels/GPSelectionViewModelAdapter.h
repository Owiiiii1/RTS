// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "UObject/Object.h"
#include "ViewModels/GPSelectionViewModel.h"
#include "GPSelectionViewModelAdapter.generated.h"

class AGP_UnitBase;
class UAbilitySystemComponent;
class UGP_CargoComponent;
class UGP_SelectionComponent;
class UTexture2D;

/** Push-only adapter from local UGP_SelectionComponent + GAS/cargo delegates into SelectionVM. */
UCLASS()
class GPUIRUNTIME_API UGP_SelectionViewModelAdapter : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(UGP_SelectionViewModel* InViewModel, UGP_SelectionComponent* InSelectionComponent);
	void Shutdown();

	UGP_SelectionViewModel* GetViewModel() const { return ViewModel; }
	int32 GetBoundDelegateCount() const;

protected:
	virtual void BeginDestroy() override;

private:
	struct FBoundPresentationUnit
	{
		TWeakObjectPtr<AGP_UnitBase> Unit;
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		TWeakObjectPtr<UGP_CargoComponent> Cargo;
		FDelegateHandle HealthHandle;
		FDelegateHandle MaxHealthHandle;
		FDelegateHandle DiedHandle;
	};

	void HandleSelectionChanged();
	void RebuildPresentation();
	void UnbindPresentedUnits();
	void BindPresentedUnits(const TArray<AGP_UnitBase*>& Units);
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void HandleUnitDied(AGP_UnitBase* Unit);

	UFUNCTION()
	void HandleBoundActorDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleCargoChanged(float PreviousAmount, float NewAmount, float Capacity, float Delta);

	void PushVitalsForUnit(AGP_UnitBase* Unit);
	static void CollectLiveSelectedUnits(const UGP_SelectionComponent* Selection, TArray<AGP_UnitBase*>& OutUnits);
	static AGP_UnitBase* ResolveInspectedUnit(const UGP_SelectionComponent* Selection);
	static void FillGroupRow(AGP_UnitBase* Unit, int32 Index, FGP_SelectionGroupRow& OutRow);
	static void ReadVitals(const AGP_UnitBase* Unit, float& OutCurrentHealth, float& OutMaxHealth);
	static void ReadStaticStats(
		const AGP_UnitBase* Unit,
		FText& OutDisplayName,
		float& OutDamage,
		float& OutArmor,
		float& OutMoveSpeed,
		float& OutAttackRange,
		UTexture2D*& OutIcon,
		bool& bOutIsUnit,
		bool& bOutIsBuilding);
	static UGP_CargoComponent* ResolveWorkerCargo(AGP_UnitBase* Unit);

	UPROPERTY(Transient)
	TObjectPtr<UGP_SelectionViewModel> ViewModel;

	TWeakObjectPtr<UGP_SelectionComponent> BoundSelection;
	FDelegateHandle SelectionChangedHandle;
	TArray<FBoundPresentationUnit> BoundUnits;
};
