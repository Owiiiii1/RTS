// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Orbital/GPOrbitalBuildingType.h"
#include "Player/GPPlayerController.h"
#include "Settings/GPOrbitalDeliverySettings.h"
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

	static UButton* MakeSmallButton(UWidgetTree* Tree, const FName& Name, const FString& Label, UTextBlock*& OutLabel)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetVisibility(ESlateVisibility::Visible);
		OutLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%s_Label"), *Name.ToString()));
		OutLabel->SetText(FText::FromString(Label));
		OutLabel->SetJustification(ETextJustify::Center);
		OutLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		OutLabel->SetFont(MakeStatusFont(14));
		Button->SetContent(OutLabel);
		return Button;
	}

	static UHorizontalBox* MakeStepperRow(
		UWidgetTree* Tree,
		const FName& RowName,
		const FString& Title,
		UButton*& OutMinus,
		UTextBlock*& OutCount,
		UButton*& OutPlus,
		UTextBlock*& OutMinusLabel,
		UTextBlock*& OutPlusLabel)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), RowName);
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UTextBlock* TitleText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%s_Title"), *RowName.ToString()));
		StyleStatusText(TitleText);
		TitleText->SetText(FText::FromString(Title));
		if (UHorizontalBoxSlot* TitleSlot = Row->AddChildToHorizontalBox(TitleText))
		{
			TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			TitleSlot->SetVerticalAlignment(VAlign_Center);
		}

		OutMinus = MakeSmallButton(Tree, *FString::Printf(TEXT("%s_Minus"), *RowName.ToString()), TEXT("-"), OutMinusLabel);
		if (UHorizontalBoxSlot* MinusSlot = Row->AddChildToHorizontalBox(OutMinus))
		{
			MinusSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
		}

		OutCount = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%s_Count"), *RowName.ToString()));
		StyleStatusText(OutCount);
		OutCount->SetMinDesiredWidth(28.0f);
		OutCount->SetJustification(ETextJustify::Center);
		if (UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(OutCount))
		{
			CountSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
			CountSlot->SetVerticalAlignment(VAlign_Center);
		}

		OutPlus = MakeSmallButton(Tree, *FString::Printf(TEXT("%s_Plus"), *RowName.ToString()), TEXT("+"), OutPlusLabel);
		Row->AddChildToHorizontalBox(OutPlus);
		return Row;
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
	RefreshUnitCapText();
	RefreshUnitDropPanel();
	RefreshBuildingPanel();
	SetLaunchButtonEnabled(bLaunchEnabled);
	BindLaunchClickedIdempotent();
	BindUnitDropClickedIdempotent();
	BindBuildingPanelClickedIdempotent();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::NativeDestruct()
{
	if (LaunchButton != nullptr)
	{
		LaunchButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleLaunchClicked);
	}
	if (ConfirmDropButton != nullptr)
	{
		ConfirmDropButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleConfirmUnitDropClicked);
	}
	if (WorkerMinusButton != nullptr)
	{
		WorkerMinusButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWorkerMinusClicked);
	}
	if (WorkerPlusButton != nullptr)
	{
		WorkerPlusButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWorkerPlusClicked);
	}
	if (WalkerMinusButton != nullptr)
	{
		WalkerMinusButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWalkerMinusClicked);
	}
	if (WalkerPlusButton != nullptr)
	{
		WalkerPlusButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWalkerPlusClicked);
	}
	if (PurchaseLogisticsHubButton != nullptr)
	{
		PurchaseLogisticsHubButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandlePurchaseLogisticsHubClicked);
	}
	if (DeployLogisticsHubButton != nullptr)
	{
		DeployLogisticsHubButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleDeployLogisticsHubClicked);
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

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::BindUnitDropClickedIdempotent()
{
	if (WorkerMinusButton != nullptr)
	{
		WorkerMinusButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWorkerMinusClicked);
		WorkerMinusButton->OnClicked.AddDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWorkerMinusClicked);
	}
	if (WorkerPlusButton != nullptr)
	{
		WorkerPlusButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWorkerPlusClicked);
		WorkerPlusButton->OnClicked.AddDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWorkerPlusClicked);
	}
	if (WalkerMinusButton != nullptr)
	{
		WalkerMinusButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWalkerMinusClicked);
		WalkerMinusButton->OnClicked.AddDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWalkerMinusClicked);
	}
	if (WalkerPlusButton != nullptr)
	{
		WalkerPlusButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWalkerPlusClicked);
		WalkerPlusButton->OnClicked.AddDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWalkerPlusClicked);
	}
	if (ConfirmDropButton != nullptr)
	{
		ConfirmDropButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleConfirmUnitDropClicked);
		ConfirmDropButton->OnClicked.AddDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleConfirmUnitDropClicked);
	}
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::BindBuildingPanelClickedIdempotent()
{
	if (PurchaseLogisticsHubButton != nullptr)
	{
		PurchaseLogisticsHubButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandlePurchaseLogisticsHubClicked);
		PurchaseLogisticsHubButton->OnClicked.AddDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandlePurchaseLogisticsHubClicked);
	}
	if (DeployLogisticsHubButton != nullptr)
	{
		DeployLogisticsHubButton->OnClicked.RemoveDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleDeployLogisticsHubClicked);
		DeployLogisticsHubButton->OnClicked.AddDynamic(this, &UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleDeployLogisticsHubClicked);
	}
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
		&& LaunchButton != nullptr
		&& UnitDropPanel != nullptr
		&& ConfirmDropButton != nullptr
		&& BuildingPanel != nullptr
		&& PurchaseLogisticsHubButton != nullptr
		&& DeployLogisticsHubButton != nullptr)
	{
		BindLaunchClickedIdempotent();
		BindUnitDropClickedIdempotent();
		BindBuildingPanelClickedIdempotent();
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

	UnitsLineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UnitsLineText"));
	GPTempS28PHUDPrivate::StyleStatusText(UnitsLineText);
	UnitsLineText->SetFont(GPTempS28PHUDPrivate::MakeStatusFont(18));
	if (UVerticalBoxSlot* UnitsSlot = StatusPanel->AddChildToVerticalBox(UnitsLineText))
	{
		UnitsSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
	}

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

	UnitDropPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("UnitDropPanel"));
	UnitDropPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* DropSlot = RootCanvas->AddChildToCanvas(UnitDropPanel))
	{
		DropSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
		DropSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		DropSlot->SetAutoSize(true);
		DropSlot->SetOffsets(FMargin(0.0f, 24.0f, 24.0f, 0.0f));
	}

	UnitDropTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UnitDropTitle"));
	GPTempS28PHUDPrivate::StyleStatusText(UnitDropTitleText);
	UnitDropTitleText->SetFont(GPTempS28PHUDPrivate::MakeStatusFont(18));
	UnitDropTitleText->SetText(FText::FromString(TEXT("Unit Drop")));
	UnitDropPanel->AddChildToVerticalBox(UnitDropTitleText);

	UTextBlock* WorkerMinusLabel = nullptr;
	UTextBlock* WorkerPlusLabel = nullptr;
	UButton* WorkerMinusRaw = nullptr;
	UButton* WorkerPlusRaw = nullptr;
	UTextBlock* WorkerCountRaw = nullptr;
	UHorizontalBox* WorkerRow = GPTempS28PHUDPrivate::MakeStepperRow(
		WidgetTree,
		TEXT("WorkerRow"),
		TEXT("Worker:"),
		WorkerMinusRaw,
		WorkerCountRaw,
		WorkerPlusRaw,
		WorkerMinusLabel,
		WorkerPlusLabel);
	WorkerMinusButton = WorkerMinusRaw;
	WorkerPlusButton = WorkerPlusRaw;
	WorkerCountText = WorkerCountRaw;
	if (UVerticalBoxSlot* WorkerSlot = UnitDropPanel->AddChildToVerticalBox(WorkerRow))
	{
		WorkerSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}

	UTextBlock* WalkerMinusLabel = nullptr;
	UTextBlock* WalkerPlusLabel = nullptr;
	UButton* WalkerMinusRaw = nullptr;
	UButton* WalkerPlusRaw = nullptr;
	UTextBlock* WalkerCountRaw = nullptr;
	UHorizontalBox* WalkerRow = GPTempS28PHUDPrivate::MakeStepperRow(
		WidgetTree,
		TEXT("WalkerRow"),
		TEXT("Salvage Walker:"),
		WalkerMinusRaw,
		WalkerCountRaw,
		WalkerPlusRaw,
		WalkerMinusLabel,
		WalkerPlusLabel);
	WalkerMinusButton = WalkerMinusRaw;
	WalkerPlusButton = WalkerPlusRaw;
	WalkerCountText = WalkerCountRaw;
	if (UVerticalBoxSlot* WalkerSlot = UnitDropPanel->AddChildToVerticalBox(WalkerRow))
	{
		WalkerSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	}

	SlotsCostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotsCostText"));
	GPTempS28PHUDPrivate::StyleStatusText(SlotsCostText);
	if (UVerticalBoxSlot* SlotsSlot = UnitDropPanel->AddChildToVerticalBox(SlotsCostText))
	{
		SlotsSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 4.0f));
	}

	UnitCapFeedbackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UnitCapFeedbackText"));
	GPTempS28PHUDPrivate::StyleStatusText(UnitCapFeedbackText);
	UnitCapFeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.35f, 0.2f)));
	if (UVerticalBoxSlot* CapSlot = UnitDropPanel->AddChildToVerticalBox(UnitCapFeedbackText))
	{
		CapSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	ConfirmDropButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmDropButton"));
	ConfirmDropButton->SetVisibility(ESlateVisibility::Visible);
	ConfirmDropLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmDropLabel"));
	ConfirmDropLabel->SetText(FText::FromString(TEXT("Confirm Drop")));
	ConfirmDropLabel->SetJustification(ETextJustify::Center);
	ConfirmDropLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ConfirmDropLabel->SetFont(GPTempS28PHUDPrivate::MakeStatusFont(15));
	ConfirmDropButton->SetContent(ConfirmDropLabel);
	UnitDropPanel->AddChildToVerticalBox(ConfirmDropButton);

	BindUnitDropClickedIdempotent();

	BuildingPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BuildingPanel"));
	BuildingPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* BuildingSlot = RootCanvas->AddChildToCanvas(BuildingPanel))
	{
		BuildingSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
		BuildingSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		BuildingSlot->SetAutoSize(true);
		BuildingSlot->SetOffsets(FMargin(0.0f, 280.0f, 24.0f, 0.0f));
	}

	BuildingTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildingTitle"));
	GPTempS28PHUDPrivate::StyleStatusText(BuildingTitleText);
	BuildingTitleText->SetFont(GPTempS28PHUDPrivate::MakeStatusFont(18));
	BuildingTitleText->SetText(FText::FromString(TEXT("BUILDINGS")));
	BuildingPanel->AddChildToVerticalBox(BuildingTitleText);

	BuildingLogisticsHubLineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuildingLogisticsHubLine"));
	GPTempS28PHUDPrivate::StyleStatusText(BuildingLogisticsHubLineText);
	if (UVerticalBoxSlot* HubLineSlot = BuildingPanel->AddChildToVerticalBox(BuildingLogisticsHubLineText))
	{
		HubLineSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 4.0f));
	}

	PurchaseLogisticsHubButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PurchaseLogisticsHubButton"));
	PurchaseLogisticsHubButton->SetVisibility(ESlateVisibility::Visible);
	PurchaseLogisticsHubLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PurchaseLogisticsHubLabel"));
	PurchaseLogisticsHubLabel->SetText(FText::FromString(TEXT("Purchase Logistics Hub")));
	PurchaseLogisticsHubLabel->SetJustification(ETextJustify::Center);
	PurchaseLogisticsHubLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PurchaseLogisticsHubLabel->SetFont(GPTempS28PHUDPrivate::MakeStatusFont(14));
	PurchaseLogisticsHubButton->SetContent(PurchaseLogisticsHubLabel);
	if (UVerticalBoxSlot* PurchaseSlot = BuildingPanel->AddChildToVerticalBox(PurchaseLogisticsHubButton))
	{
		PurchaseSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	DeployLogisticsHubButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DeployLogisticsHubButton"));
	DeployLogisticsHubButton->SetVisibility(ESlateVisibility::Visible);
	DeployLogisticsHubLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeployLogisticsHubLabel"));
	DeployLogisticsHubLabel->SetText(FText::FromString(TEXT("Deploy READY")));
	DeployLogisticsHubLabel->SetJustification(ETextJustify::Center);
	DeployLogisticsHubLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DeployLogisticsHubLabel->SetFont(GPTempS28PHUDPrivate::MakeStatusFont(14));
	DeployLogisticsHubButton->SetContent(DeployLogisticsHubLabel);
	BuildingPanel->AddChildToVerticalBox(DeployLogisticsHubButton);

	BindBuildingPanelClickedIdempotent();

	bTreeBuilt = true;
	RefreshStatusText();
	RefreshOrbitalText();
	RefreshUnitCapText();
	RefreshUnitDropPanel();
	RefreshBuildingPanel();
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
	RefreshUnitDropPanel();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::RefreshUnitCapText()
{
	if (UnitsLineText != nullptr)
	{
		UnitsLineText->SetText(FText::FromString(FString::Printf(
			TEXT("UNITS %d / %d"),
			DisplayCurrentUnits,
			DisplayMaxUnits)));
		const bool bOverCap = DisplayMaxUnits > 0 && DisplayCurrentUnits > DisplayMaxUnits;
		UnitsLineText->SetColorAndOpacity(FSlateColor(
			bOverCap ? FLinearColor(1.0f, 0.35f, 0.2f) : FLinearColor::White));
	}
	RefreshUnitDropPanel();
}

int32 UGP_TEMP_S28P_PlanetaryFerroniteHUD::GetPodCapacity() const
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	return Settings != nullptr ? FMath::Max(1, Settings->PodTransportSlotCapacity) : 4;
}

int32 UGP_TEMP_S28P_PlanetaryFerroniteHUD::ComputeSlotCost() const
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	if (Settings == nullptr)
	{
		return WorkerCount + SalvageWalkerCount * 2;
	}
	return WorkerCount * FMath::Max(1, Settings->WorkerTransportSlotCost)
		+ SalvageWalkerCount * FMath::Max(1, Settings->SalvageWalkerTransportSlotCost);
}

float UGP_TEMP_S28P_PlanetaryFerroniteHUD::ComputeOrbitalCost() const
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	if (Settings == nullptr)
	{
		return static_cast<float>(WorkerCount) * 25.0f + static_cast<float>(SalvageWalkerCount) * 50.0f;
	}
	return static_cast<float>(WorkerCount) * Settings->WorkerOrbitalDropCost
		+ static_cast<float>(SalvageWalkerCount) * Settings->SalvageWalkerOrbitalDropCost;
}

bool UGP_TEMP_S28P_PlanetaryFerroniteHUD::CanConfirmLocally() const
{
	if (WorkerCount + SalvageWalkerCount <= 0)
	{
		return false;
	}
	if (ComputeSlotCost() > GetPodCapacity())
	{
		return false;
	}
	if (ComputeOrbitalCost() > DisplayOrbital + KINDA_SMALL_NUMBER)
	{
		return false;
	}
	if (WorkerCount + SalvageWalkerCount + DisplayCurrentUnits > DisplayMaxUnits)
	{
		return false;
	}
	return true;
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::RefreshUnitDropPanel()
{
	if (WorkerCountText != nullptr)
	{
		WorkerCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), WorkerCount)));
	}
	if (WalkerCountText != nullptr)
	{
		WalkerCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), SalvageWalkerCount)));
	}
	if (SlotsCostText != nullptr)
	{
		SlotsCostText->SetText(FText::FromString(FString::Printf(
			TEXT("Slots: %d / %d\nCost: %d Orbital"),
			ComputeSlotCost(),
			GetPodCapacity(),
			FMath::RoundToInt(ComputeOrbitalCost()))));
	}
	if (ConfirmDropButton != nullptr)
	{
		ConfirmDropButton->SetIsEnabled(CanConfirmLocally());
	}
	if (UnitCapFeedbackText != nullptr)
	{
		const int32 ManifestCount = WorkerCount + SalvageWalkerCount;
		const bool bWouldExceed =
			ManifestCount > 0 && ManifestCount + DisplayCurrentUnits > DisplayMaxUnits;
		if (bWouldExceed || bUnitCapReachedFeedback)
		{
			UnitCapFeedbackText->SetText(FText::FromString(TEXT("Unit Cap reached")));
			UnitCapFeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			UnitCapFeedbackText->SetText(FText::GetEmpty());
			UnitCapFeedbackText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

float UGP_TEMP_S28P_PlanetaryFerroniteHUD::GetBuildingPurchaseCost() const
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	return Settings != nullptr ? FMath::Max(0.0f, Settings->BuildingOrbitalPurchaseCost) : 100.0f;
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::RefreshBuildingPanel()
{
	if (BuildingLogisticsHubLineText != nullptr)
	{
		BuildingLogisticsHubLineText->SetText(FText::FromString(FString::Printf(
			TEXT("Logistics Hub — Cost: %d — READY: %d"),
			FMath::RoundToInt(GetBuildingPurchaseCost()),
			ReadyLogisticsHubCount)));
	}
	if (DeployLogisticsHubButton != nullptr)
	{
		DeployLogisticsHubButton->SetIsEnabled(ReadyLogisticsHubCount > 0);
	}
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetBuildingReadyDisplay(int32 InReadyLogisticsHubCount)
{
	ReadyLogisticsHubCount = FMath::Max(0, InReadyLogisticsHubCount);
	RefreshBuildingPanel();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::AdjustWorkerCount(int32 Delta)
{
	const int32 Next = FMath::Max(0, WorkerCount + Delta);
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const int32 WorkerSlots = Settings != nullptr ? FMath::Max(1, Settings->WorkerTransportSlotCost) : 1;
	const int32 WalkerSlots = Settings != nullptr ? FMath::Max(1, Settings->SalvageWalkerTransportSlotCost) : 2;
	const int32 Cap = GetPodCapacity();
	const int32 UsedWithoutWorker = SalvageWalkerCount * WalkerSlots;
	const int32 MaxWorkers = FMath::Max(0, (Cap - UsedWithoutWorker) / WorkerSlots);
	WorkerCount = FMath::Clamp(Next, 0, MaxWorkers);
	RefreshUnitDropPanel();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::AdjustWalkerCount(int32 Delta)
{
	const int32 Next = FMath::Max(0, SalvageWalkerCount + Delta);
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const int32 WorkerSlots = Settings != nullptr ? FMath::Max(1, Settings->WorkerTransportSlotCost) : 1;
	const int32 WalkerSlots = Settings != nullptr ? FMath::Max(1, Settings->SalvageWalkerTransportSlotCost) : 2;
	const int32 Cap = GetPodCapacity();
	const int32 UsedWithoutWalker = WorkerCount * WorkerSlots;
	const int32 MaxWalkers = FMath::Max(0, (Cap - UsedWithoutWalker) / WalkerSlots);
	SalvageWalkerCount = FMath::Clamp(Next, 0, MaxWalkers);
	RefreshUnitDropPanel();
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

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetUnitCapDisplay(int32 CurrentUnits, int32 MaxUnits)
{
	DisplayCurrentUnits = FMath::Max(0, CurrentUnits);
	DisplayMaxUnits = FMath::Max(0, MaxUnits);
	if (DisplayCurrentUnits <= DisplayMaxUnits)
	{
		bUnitCapReachedFeedback = false;
	}
	RefreshUnitCapText();
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::SetUnitCapReachedFeedback(bool bReached)
{
	bUnitCapReachedFeedback = bReached;
	RefreshUnitDropPanel();
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

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleConfirmUnitDropClicked()
{
	if (!CanConfirmLocally())
	{
		return;
	}
	AGP_PlayerController* PC = Cast<AGP_PlayerController>(GetOwningPlayer());
	if (PC == nullptr)
	{
		return;
	}
	FGP_UnitDropManifest Manifest;
	Manifest.WorkerCount = WorkerCount;
	Manifest.SalvageWalkerCount = SalvageWalkerCount;
	PC->RequestUnitDrop(Manifest);
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWorkerMinusClicked()
{
	AdjustWorkerCount(-1);
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWorkerPlusClicked()
{
	AdjustWorkerCount(1);
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWalkerMinusClicked()
{
	AdjustWalkerCount(-1);
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleWalkerPlusClicked()
{
	AdjustWalkerCount(1);
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandlePurchaseLogisticsHubClicked()
{
	AGP_PlayerController* PC = Cast<AGP_PlayerController>(GetOwningPlayer());
	if (PC == nullptr)
	{
		return;
	}
	PC->RequestBuildingPurchase(EGP_OrbitalBuildingType::LogisticsHub);
}

void UGP_TEMP_S28P_PlanetaryFerroniteHUD::HandleDeployLogisticsHubClicked()
{
	AGP_PlayerController* PC = Cast<AGP_PlayerController>(GetOwningPlayer());
	if (PC == nullptr)
	{
		return;
	}
	PC->EnterBuildingPlacementMode(EGP_OrbitalBuildingType::LogisticsHub);
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

FString UGP_TEMP_S28P_PlanetaryFerroniteHUD::GetUnitsLineTextForContract() const
{
	return UnitsLineText != nullptr ? UnitsLineText->GetText().ToString() : FString();
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

bool UGP_TEMP_S28P_PlanetaryFerroniteHUD::IsConfirmDropEnabledForContract() const
{
	return ConfirmDropButton != nullptr && ConfirmDropButton->GetIsEnabled();
}
#endif
