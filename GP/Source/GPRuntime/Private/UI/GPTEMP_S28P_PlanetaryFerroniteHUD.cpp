// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Player/GPPlayerController.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UGP_TEMP_S28P_PlanetaryFerroniteHUD::RebuildWidget()
{
	// Must populate WidgetTree->RootWidget before Super builds Slate from it.
	EnsureWidgetTreeBuilt();
	return Super::RebuildWidget();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::NativeConstruct()
{
	Super::NativeConstruct();
	SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	SetAlignmentInViewport(FVector2D::ZeroVector);
	// SelfHitTestInvisible: empty fullscreen area does not block RTS selection; children can still hit-test.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshCountersText();
	SetLaunchButtonEnabled(bLaunchEnabled);
	BindLaunchClickedIdempotent();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::NativeDestruct()
{
	if (LaunchButton != nullptr)
	{
		LaunchButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleLaunchClicked);
	}
	Super::NativeDestruct();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::BindLaunchClickedIdempotent()
{
	if (LaunchButton == nullptr)
	{
		return;
	}
	LaunchButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleLaunchClicked);
	LaunchButton->OnClicked.AddDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleLaunchClicked);
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::EnsureWidgetTreeBuilt()
{
	if (WidgetTree == nullptr)
	{
		return;
	}

	if (bTreeBuilt
		&& RootCanvas != nullptr
		&& WidgetTree->RootWidget == RootCanvas
		&& CountersText != nullptr
		&& LaunchButton != nullptr)
	{
		BindLaunchClickedIdempotent();
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	CountersText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CountersText"));
	CountersText->SetVisibility(ESlateVisibility::HitTestInvisible);
	CountersText->SetText(FText::FromString(CountersDisplayText));
	CountersText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.95f, 0.85f, 1.0f)));
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18);
	CountersText->SetFont(Font);

	if (UCanvasPanelSlot* CounterSlot = RootCanvas->AddChildToCanvas(CountersText))
	{
		CounterSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		CounterSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CounterSlot->SetAutoSize(true);
		CounterSlot->SetOffsets(FMargin(24.0f, 24.0f, 0.0f, 0.0f));
	}

	LaunchButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LaunchButton"));
	LaunchButton->SetVisibility(ESlateVisibility::Visible);
	LaunchButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LaunchButtonLabel"));
	LaunchButtonLabel->SetText(FText::FromString(TEXT("Launch Container")));
	LaunchButtonLabel->SetJustification(ETextJustify::Center);
	LaunchButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LaunchButtonLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16));
	LaunchButton->SetContent(LaunchButtonLabel);
	BindLaunchClickedIdempotent();

	if (UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(LaunchButton))
	{
		ButtonSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		ButtonSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		ButtonSlot->SetAutoSize(false);
		ButtonSlot->SetSize(FVector2D(220.0f, 44.0f));
		ButtonSlot->SetPosition(FVector2D(0.0f, -28.0f));
	}

	bTreeBuilt = true;
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::RefreshCountersText()
{
	const FString FerronitePart = bHasResolvedBase
		? FString::Printf(TEXT("Ferronite: %d"), FMath::RoundToInt(DisplayStored))
		: FString(TEXT("Ferronite: --"));
	const FString OrbitalPart = FString::Printf(TEXT("Orbital: %d"), FMath::RoundToInt(DisplayOrbital));
	CountersDisplayText = FerronitePart + TEXT("     ") + OrbitalPart;

	if (CountersText != nullptr)
	{
		CountersText->SetText(FText::FromString(CountersDisplayText));
	}
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetPlanetaryFerroniteDisplay(float StoredAmount, bool bHasBase)
{
	bHasResolvedBase = bHasBase;
	DisplayStored = StoredAmount;
	RefreshCountersText();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetOrbitalFerroniteDisplay(float OrbitalAmount)
{
	DisplayOrbital = FMath::IsFinite(OrbitalAmount) ? FMath::Max(0.0f, OrbitalAmount) : 0.0f;
	RefreshCountersText();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetLaunchButtonEnabled(bool bEnabled)
{
	bLaunchEnabled = bEnabled;
	if (LaunchButton != nullptr)
	{
		LaunchButton->SetIsEnabled(bEnabled);
	}
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleLaunchClicked()
{
	AGP_PlayerController* PC = Cast<AGP_PlayerController>(GetOwningPlayer());
	if (PC == nullptr)
	{
		return;
	}
	PC->RequestLaunchReadyContainer();
}

#if !UE_BUILD_SHIPPING
bool UGP_TEMP_S28P_PlanetaryFerroniteHUD::IsLaunchButtonEnabledForContract() const
{
	return LaunchButton != nullptr && LaunchButton->GetIsEnabled();
}

bool UGP_TEMP_S28P_PlanetaryFerroniteHUD::HasInteractiveLaunchButtonForContract() const
{
	return LaunchButton != nullptr
		&& LaunchButton->GetVisibility() == ESlateVisibility::Visible;
}

bool UGP_TEMP_S28P_PlanetaryFerroniteHUD::HasWidgetTreeRootForContract() const
{
	return GetRootWidget() != nullptr && RootCanvas != nullptr && GetRootWidget() == RootCanvas;
}

bool UGP_TEMP_S28P_PlanetaryFerroniteHUD::HasCountersWidgetForContract() const
{
	return CountersText != nullptr && GetWidgetFromName(TEXT("CountersText")) == CountersText;
}

bool UGP_TEMP_S28P_PlanetaryFerroniteHUD::HasLaunchButtonWidgetForContract() const
{
	return LaunchButton != nullptr && GetWidgetFromName(TEXT("LaunchButton")) == LaunchButton;
}
#endif
