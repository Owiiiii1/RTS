// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Components/WidgetComponent.h"
#include "GPHealthBarComponent.generated.h"

class UGP_AbilitySystemComponent;
class UGP_HealthBarWidget;

/**
 * World-space health bar bound to GAS Health/MaxHealth (GP-S29R).
 * Event-driven; no Tick / no polling.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_HealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UGP_HealthBarComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "GP|Presentation|Health")
	void RefreshHealthBarFromAttributes();

	UFUNCTION(BlueprintPure, Category = "GP|Presentation|Health")
	float GetDisplayedHealthRatio() const { return DisplayedHealthRatio; }

	/** Hide bar (death). Does not change gameplay death. */
	UFUNCTION(BlueprintCallable, Category = "GP|Presentation|Health")
	void SetHealthBarVisible(bool bShowHealthBar);

private:
	void BindAttributeDelegates();
	void UnbindAttributeDelegates();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void ApplyWidgetColors();

	float DisplayedHealthRatio = 1.0f;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	TWeakObjectPtr<UGP_AbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UGP_HealthBarWidget> HealthBarWidget;
};
