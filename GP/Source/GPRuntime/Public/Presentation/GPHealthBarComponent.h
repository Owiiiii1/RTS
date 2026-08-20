// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Components/WidgetComponent.h"
#include "GPHealthBarComponent.generated.h"

class UGP_AbilitySystemComponent;
class UGP_HealthBarWidget;

/**
 * Screen-space health bar bound to GAS Health/MaxHealth (GP-S29R).
 * Attribute updates are event-driven (no health polling).
 * WidgetComponent Automatic tick is presentation-only (Screen-space draw).
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_HealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UGP_HealthBarComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Attach to owner root and apply configured world offset. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category = "GP|Presentation|Health")
	void EnsureAttachedToOwnerRoot();

	UFUNCTION(BlueprintCallable, Category = "GP|Presentation|Health")
	void RefreshHealthBarFromAttributes();

	UFUNCTION(BlueprintPure, Category = "GP|Presentation|Health")
	float GetDisplayedHealthRatio() const { return DisplayedHealthRatio; }

	/** Hide bar (death). Does not change gameplay death. */
	UFUNCTION(BlueprintCallable, Category = "GP|Presentation|Health")
	void SetHealthBarVisible(bool bShowHealthBar);

	/** Local-only FoW composition gate. A damaged bar cannot override this gate. */
	void SetFoWPresentationAllowed(bool bAllowed);
	bool IsFoWPresentationAllowed() const { return bFoWPresentationAllowed; }
	bool DoesHealthPolicyAllowVisibility() const { return bHealthPolicyAllowsVisibility; }
	bool IsComposedHealthBarVisible() const;

private:
	void BindAttributeDelegates();
	void UnbindAttributeDelegates();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& Data);
	void ApplyWidgetColors();
	void EnsureWidgetInstance();
	void ApplyComposedVisibility();

	float DisplayedHealthRatio = 1.0f;
	bool bOwnerAllowsVisibility = true;
	bool bFoWPresentationAllowed = true;
	bool bHealthPolicyAllowsVisibility = false;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	TWeakObjectPtr<UGP_AbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UGP_HealthBarWidget> HealthBarWidget;
};
