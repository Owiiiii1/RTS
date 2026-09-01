// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/GPContextActionPresenter.h"
#include "ViewModels/GPLaunchMenuPresenter.h"
#include "ViewModels/GPMinimapPresenter.h"
#include "ViewModels/GPSelectionViewModel.h"
#include "Widgets/GPUserWidgetBase.h"
#include "GPHUDRootWidget.generated.h"

class UGP_ContextActionPresenter;
class UGP_LaunchMenuPresenter;
class UGP_MinimapPresenter;
class UGP_SelectionViewModel;

/** Native lifetime root for the authored WBP_GP_HUD production HUD. */
UCLASS(Abstract, Blueprintable)
class GPUIRUNTIME_API UGP_HUDRootWidget : public UGP_UserWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GP|HUD|LaunchMenu")
	TArray<FGP_LaunchContainerRow> GetLaunchContainerRows() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|LaunchMenu")
	TArray<FGP_LaunchContainerRow> GetLaunchContainerPresentations() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|LaunchMenu")
	bool CanLaunchReadyContainer() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|LaunchMenu")
	int32 GetReadyLaunchContainerCount() const;

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|LaunchMenu")
	void RequestLaunchReadyContainer();

	UFUNCTION(BlueprintPure, Category = "GP|HUD|Selection")
	TArray<FGP_SelectionGroupRow> GetSelectionGroupRows() const;

	/**
	 * Local HUD: replace the current group selection with the unit shown in RowIndex.
	 * RowIndex matches FGP_SelectionGroupRow.Index (live GetSelectedUnits() order).
	 * Invalid/stale rows are a no-op. Does not write SelectionVM directly.
	 */
	UFUNCTION(BlueprintCallable, Category = "GP|HUD|Selection")
	void RequestSelectGroupRow(int32 RowIndex);

	UFUNCTION(BlueprintPure, Category = "GP|HUD|ContextActions")
	TArray<FGP_ContextActionPresentation> GetContextActionPresentations() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|ContextActions")
	EGP_ContextActionMode GetContextActionMode() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|ContextActions")
	EGP_ContextActionPanelState GetContextActionPanelState() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|ContextActions")
	FText GetCommandTargetingPrompt() const;

	/** Targeting prompt while command targeting is active; otherwise purchase contextual text. */
	UFUNCTION(BlueprintPure, Category = "GP|HUD|ContextActions")
	FText GetContextMessage() const;

	/** Harmless leftover mapping. Visual targeting feedback is the native software overlay. */
	UFUNCTION(BlueprintPure, Category = "GP|HUD|ContextActions")
	EMouseCursor::Type GetCommandTargetingCursor() const;

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|ContextActions")
	void RequestContextAction(EGP_ContextActionId ActionId);

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|ContextActions")
	void RequestOpenMainBasePurchase();

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|ContextActions")
	void RequestOpenPurchaseCategory(EGP_PurchaseCategory Category);

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|ContextActions")
	void RequestPurchaseBack();

	UFUNCTION(BlueprintPure, Category = "GP|HUD|PurchaseCatalog")
	TArray<FGP_PurchaseCatalogRow> GetPurchaseCatalogRows() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|PurchaseCatalog")
	FGP_PurchaseCatalogRow GetSelectedPurchaseItem() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|PurchaseCatalog")
	FGP_PurchaseUnitManifestPresentation GetPurchaseUnitManifestPresentation() const;

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|PurchaseCatalog")
	void RequestPurchaseRowPrimary(FPrimaryAssetId ItemId);

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|PurchaseCatalog")
	void RequestPurchaseRowSecondary(FPrimaryAssetId ItemId);

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|PurchaseCatalog")
	void RequestLaunchUnitShuttle();

	UFUNCTION(BlueprintCallable, Category = "GP|HUD|PurchaseCatalog")
	void RequestLaunchSelectedPurchaseItem();

	UFUNCTION(BlueprintPure, Category = "GP|HUD|Minimap")
	bool IsMinimapReady() const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|Minimap")
	FVector2D WorldToMinimapNormalized(FVector WorldLocation) const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|Minimap")
	FVector MinimapNormalizedToWorld(FVector2D Normalized, float WorldZ) const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|Minimap")
	EGP_FoWState GetMinimapFoWStateNormalized(FVector2D Normalized) const;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|Minimap")
	FGP_MinimapPresentation GetMinimapPresentation() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|HUD|LaunchMenu")
	void BP_OnLaunchMenuChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|HUD|Selection")
	void BP_OnSelectionPresentationChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|HUD|ContextActions")
	void BP_OnContextActionsChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "GP|HUD|Minimap")
	void BP_OnMinimapChanged();

private:
	void TryAssignOwnedViewModels();
	void BindLaunchMenuPresenter();
	void UnbindLaunchMenuPresenter();
	void HandleLaunchMenuPresentationChanged();
	const UGP_LaunchMenuPresenter* ResolveLaunchMenuPresenter() const;
	void BindSelectionViewModel();
	void UnbindSelectionViewModel();
	void HandleSelectionPresentationChanged();
	const UGP_SelectionViewModel* ResolveSelectionViewModel() const;
	void BindContextActionPresenter();
	void UnbindContextActionPresenter();
	void HandleContextActionsChanged();
	const UGP_ContextActionPresenter* ResolveContextActionPresenter() const;
	void BindMinimapPresenter();
	void UnbindMinimapPresenter();
	void HandleMinimapPresentationChanged();
	const UGP_MinimapPresenter* ResolveMinimapPresenter() const;

	TWeakObjectPtr<UGP_LaunchMenuPresenter> BoundLaunchMenuPresenter;
	TWeakObjectPtr<UGP_SelectionViewModel> BoundSelectionViewModel;
	TWeakObjectPtr<UGP_ContextActionPresenter> BoundContextActionPresenter;
	TWeakObjectPtr<UGP_MinimapPresenter> BoundMinimapPresenter;
};
