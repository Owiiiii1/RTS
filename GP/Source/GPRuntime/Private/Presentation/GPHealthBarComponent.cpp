// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPHealthBarComponent.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Presentation/GPHealthBarWidget.h"
#include "Settings/GPGameplayPresentationSettings.h"
#include "Units/GPUnitBase.h"

UGP_HealthBarComponent::UGP_HealthBarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(false);
	SetPivot(FVector2D(0.5f, 1.0f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	Space = EWidgetSpace::Screen;

	const UGP_GameplayPresentationSettings* Settings = UGP_GameplayPresentationSettings::Get();
	const float SizeX = Settings != nullptr ? Settings->HealthBarDrawSizeX : 120.0f;
	const float SizeY = Settings != nullptr ? Settings->HealthBarDrawSizeY : 14.0f;
	SetDrawSize(FVector2D(SizeX, SizeY));
	SetWidgetClass(UGP_HealthBarWidget::StaticClass());
}

void UGP_HealthBarComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const UGP_GameplayPresentationSettings* Settings = UGP_GameplayPresentationSettings::Get())
	{
		SetRelativeLocation(Settings->HealthBarWorldOffset);
		SetDrawSize(FVector2D(Settings->HealthBarDrawSizeX, Settings->HealthBarDrawSizeY));
	}

	if (UUserWidget* UserWidget = GetWidget())
	{
		HealthBarWidget = Cast<UGP_HealthBarWidget>(UserWidget);
	}
	else
	{
		InitWidget();
		HealthBarWidget = Cast<UGP_HealthBarWidget>(GetWidget());
	}

	ApplyWidgetColors();
	BindAttributeDelegates();
	RefreshHealthBarFromAttributes();

	if (const AGP_UnitBase* Unit = Cast<AGP_UnitBase>(GetOwner()))
	{
		if (Unit->IsDead())
		{
			SetHealthBarVisible(false);
		}
	}
}

void UGP_HealthBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindAttributeDelegates();
	Super::EndPlay(EndPlayReason);
}

void UGP_HealthBarComponent::ApplyWidgetColors()
{
	UGP_HealthBarWidget* Bar = HealthBarWidget.Get();
	if (Bar == nullptr)
	{
		Bar = Cast<UGP_HealthBarWidget>(GetWidget());
		HealthBarWidget = Bar;
	}
	if (Bar == nullptr)
	{
		return;
	}

	if (const UGP_GameplayPresentationSettings* Settings = UGP_GameplayPresentationSettings::Get())
	{
		Bar->SetColors(
			Settings->HealthBarFillColor,
			Settings->HealthBarFrameColor,
			Settings->HealthBarBackgroundColor);
	}
}

void UGP_HealthBarComponent::BindAttributeDelegates()
{
	UnbindAttributeDelegates();

	AGP_UnitBase* Unit = Cast<AGP_UnitBase>(GetOwner());
	if (Unit == nullptr)
	{
		return;
	}

	UGP_AbilitySystemComponent* ASC = Unit->GetGPAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	BoundASC = ASC;
	HealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UGP_UnitAttributeSet::GetHealthAttribute()).AddUObject(
		this, &UGP_HealthBarComponent::HandleHealthChanged);
	MaxHealthChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UGP_UnitAttributeSet::GetMaxHealthAttribute()).AddUObject(
		this, &UGP_HealthBarComponent::HandleMaxHealthChanged);
}

void UGP_HealthBarComponent::UnbindAttributeDelegates()
{
	if (UGP_AbilitySystemComponent* ASC = BoundASC.Get())
	{
		if (HealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UGP_UnitAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		}
		if (MaxHealthChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UGP_UnitAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		}
	}
	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	BoundASC.Reset();
}

void UGP_HealthBarComponent::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	(void)Data;
	RefreshHealthBarFromAttributes();
}

void UGP_HealthBarComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& Data)
{
	(void)Data;
	RefreshHealthBarFromAttributes();
}

void UGP_HealthBarComponent::RefreshHealthBarFromAttributes()
{
	const AGP_UnitBase* Unit = Cast<AGP_UnitBase>(GetOwner());
	const UGP_UnitAttributeSet* Attrs = Unit != nullptr ? Unit->GetUnitAttributeSet() : nullptr;
	float Ratio = 0.0f;
	if (Attrs != nullptr)
	{
		const float MaxHealth = Attrs->GetMaxHealth();
		const float Health = Attrs->GetHealth();
		Ratio = (MaxHealth > KINDA_SMALL_NUMBER) ? (Health / MaxHealth) : 0.0f;
	}
	DisplayedHealthRatio = FMath::Clamp(Ratio, 0.0f, 1.0f);

	UGP_HealthBarWidget* Bar = HealthBarWidget.Get();
	if (Bar == nullptr)
	{
		Bar = Cast<UGP_HealthBarWidget>(GetWidget());
		HealthBarWidget = Bar;
	}
	if (Bar != nullptr)
	{
		Bar->SetHealthRatio(DisplayedHealthRatio);
	}

	if (Unit != nullptr && Unit->IsDead())
	{
		SetHealthBarVisible(false);
	}
	else if (DisplayedHealthRatio <= 0.0f)
	{
		SetHealthBarVisible(false);
	}
	else
	{
		SetHealthBarVisible(true);
	}
}

void UGP_HealthBarComponent::SetHealthBarVisible(bool bShowHealthBar)
{
	SetVisibility(bShowHealthBar, true);
	SetHiddenInGame(!bShowHealthBar, true);
}
