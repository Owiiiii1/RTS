// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Player/GPPlayerController.h"
#include "Styling/CoreStyle.h"

namespace GPTempS28PHUDPrivate
{
	static FSlateFontInfo MakeStatusFont(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
	}

	static void StyleStatusText(UTextBlock* Text)
	{
		if (Text == nullptr)
		{
			return;
		}
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.95f, 0.85f, 1.0f)));
		Text->SetFont(MakeStatusFont(16));
	}
}

TSharedRef<SWidget> UGP_TEMP_S28P_PlanetaryFerroniteHUD::RebuildWidget()
{
	EnsureWidgetTreeBuilt();
	return Super::RebuildWidget();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::NativeConstruct()
{
	Super::NativeConstruct();
	SetAnchorsInViewport(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	SetAlignmentInViewport(FVector2D::ZeroVector);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RefreshStatusText();
	RefreshOrbitalText();
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
		&& StatusPanel != nullptr
		&& BaseLineText != nullptr
		&& ContainerLinesBox != nullptr
		&& OrbitalLineText != nullptr
		&& LaunchButton != nullptr)
	{
		BindLaunchClickedIdempotent();
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	StatusPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusPanel"));
	StatusPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* StatusSlot = RootCanvas->AddChildToCanvas(StatusPanel))
	{
		StatusSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		StatusSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		StatusSlot->SetAutoSize(true);
		StatusSlot->SetOffsets(FMargin(24.0f, 24.0f, 0.0f, 0.0f));
	}

	BaseLineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BaseLineText"));
	GPTempS28PHUDPrivate::StyleStatusText(BaseLineText);
	BaseLineText->SetFont(GPTempS28PHUDPrivate::MakeStatusFont(18));
	if (UVerticalBoxSlot* BaseSlot = StatusPanel->AddChildToVerticalBox(BaseLineText))
	{
		BaseSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	ContainerLinesBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContainerLinesBox"));
	ContainerLinesBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UVerticalBoxSlot* ContainersSlot = StatusPanel->AddChildToVerticalBox(ContainerLinesBox))
	{
		ContainersSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	OrbitalLineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("OrbitalLineText"));
	GPTempS28PHUDPrivate::StyleStatusText(OrbitalLineText);
	OrbitalLineText->SetFont(GPTempS28PHUDPrivate::MakeStatusFont(18));
	StatusPanel->AddChildToVerticalBox(OrbitalLineText);

	LaunchButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("LaunchButton"));
	LaunchButton->SetVisibility(ESlateVisibility::Visible);
	LaunchButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LaunchButtonLabel"));
	LaunchButtonLabel->SetText(FText::FromString(TEXT("Launch Container")));
	LaunchButtonLabel->SetJustification(ETextJustify::Center);
	LaunchButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LaunchButtonLabel->SetFont(GPTempS28PHUDPrivate::MakeStatusFont(16));
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
	RefreshStatusText();
	RefreshOrbitalText();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::EnsureContainerLineCount(int32 DesiredCount)
{
	DesiredCount = FMath::Max(0, DesiredCount);
	if (ContainerLinesBox == nullptr || WidgetTree == nullptr)
	{
		return;
	}

	while (ContainerLineTexts.Num() > DesiredCount)
	{
		UTextBlock* Line = ContainerLineTexts.Last();
		ContainerLineTexts.Pop();
		if (Line != nullptr)
		{
			ContainerLinesBox->RemoveChild(Line);
		}
	}

	while (ContainerLineTexts.Num() < DesiredCount)
	{
		const int32 Index = ContainerLineTexts.Num();
		const FName LineName(*FString::Printf(TEXT("ContainerLine_%d"), Index));
		UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LineName);
		GPTempS28PHUDPrivate::StyleStatusText(Line);
		if (UVerticalBoxSlot* LineSlot = ContainerLinesBox->AddChildToVerticalBox(Line))
		{
			LineSlot->SetPadding(FMargin(0.0f, 1.0f, 0.0f, 0.0f));
		}
		ContainerLineTexts.Add(Line);
	}
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::RefreshStatusText()
{
	if (BaseLineText != nullptr)
	{
		if (!bHasResolvedBase)
		{
			BaseLineText->SetText(FText::FromString(TEXT("База: -- / --")));
		}
		else
		{
			BaseLineText->SetText(FText::FromString(FString::Printf(
				TEXT("База: %d / %d"),
				FMath::RoundToInt(DisplayStored),
				FMath::RoundToInt(DisplayCapacity))));
		}
	}

	EnsureContainerLineCount(bHasResolvedBase ? DisplayContainerAmounts.Num() : 0);
	for (int32 Index = 0; Index < ContainerLineTexts.Num(); ++Index)
	{
		UTextBlock* Line = ContainerLineTexts[Index];
		if (Line == nullptr)
		{
			continue;
		}
		const float Amount = DisplayContainerAmounts.IsValidIndex(Index) ? DisplayContainerAmounts[Index] : 0.0f;
		Line->SetText(FText::FromString(FString::Printf(
			TEXT("Контейнер %d — %d"),
			Index + 1,
			FMath::RoundToInt(Amount))));
	}
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::RefreshOrbitalText()
{
	if (OrbitalLineText != nullptr)
	{
		OrbitalLineText->SetText(FText::FromString(FString::Printf(
			TEXT("Orbital: %d"),
			FMath::RoundToInt(DisplayOrbital))));
	}
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetStorageDisplay(
	bool bHasBase,
	float TotalStored,
	float TotalCapacity,
	const TArray<float>& ContainerAmounts)
{
	bHasResolvedBase = bHasBase;
	DisplayStored = FMath::IsFinite(TotalStored) ? FMath::Max(0.0f, TotalStored) : 0.0f;
	DisplayCapacity = FMath::IsFinite(TotalCapacity) ? FMath::Max(0.0f, TotalCapacity) : 0.0f;
	DisplayContainerAmounts.Reset();
	if (bHasBase)
	{
		DisplayContainerAmounts = ContainerAmounts;
	}
	RefreshStatusText();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetPlanetaryFerroniteDisplay(float StoredAmount, bool bHasBase)
{
	TArray<float> Empty;
	SetStorageDisplay(bHasBase, StoredAmount, bHasBase ? DisplayCapacity : 0.0f, Empty);
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetOrbitalFerroniteDisplay(float OrbitalAmount)
{
	DisplayOrbital = FMath::IsFinite(OrbitalAmount) ? FMath::Max(0.0f, OrbitalAmount) : 0.0f;
	RefreshOrbitalText();
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
FString UGP_TEMP_S28P_PlanetaryFerroniteHUD::GetBaseLineTextForContract() const
{
	return BaseLineText != nullptr ? BaseLineText->GetText().ToString() : FString();
}

FString UGP_TEMP_S28P_PlanetaryFerroniteHUD::GetOrbitalLineTextForContract() const
{
	return OrbitalLineText != nullptr ? OrbitalLineText->GetText().ToString() : FString();
}

int32 UGP_TEMP_S28P_PlanetaryFerroniteHUD::GetContainerLineCountForContract() const
{
	return ContainerLineTexts.Num();
}

FString UGP_TEMP_S28P_PlanetaryFerroniteHUD::GetContainerLineTextForContract(int32 ZeroBasedIndex) const
{
	if (!ContainerLineTexts.IsValidIndex(ZeroBasedIndex) || ContainerLineTexts[ZeroBasedIndex] == nullptr)
	{
		return FString();
	}
	return ContainerLineTexts[ZeroBasedIndex]->GetText().ToString();
}

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

bool UGP_TEMP_S28P_PlanetaryFerroniteHUD::HasStatusPanelForContract() const
{
	return StatusPanel != nullptr
		&& BaseLineText != nullptr
		&& ContainerLinesBox != nullptr
		&& OrbitalLineText != nullptr
		&& GetWidgetFromName(TEXT("StatusPanel")) == StatusPanel;
}

bool UGP_TEMP_S28P_PlanetaryFerroniteHUD::HasLaunchButtonWidgetForContract() const
{
	return LaunchButton != nullptr && GetWidgetFromName(TEXT("LaunchButton")) == LaunchButton;
}
#endif
