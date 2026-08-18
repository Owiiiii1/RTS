// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "GPTEMP_S28P_PlanetaryFerroniteHUD.generated.h"

class UButton;
class UTextBlock;
class UCanvasPanel;
class UVerticalBox;
class UHorizontalBox;
class UGP_TEMP_S28P_PlanetaryFerroniteHUD;

struct FGP_BuildingHudCatalogRow
{
	FPrimaryAssetId DropDefinitionId;
	FString DisplayName;
	float Cost = 0.0f;
	int32 ReadyCount = 0;
	bool bCanDeploy = false;
};

UCLASS()
class UGP_TEMP_BuildingCatalogRowBinder : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UGP_TEMP_S28P_PlanetaryFerroniteHUD> OwnerHUD;
	FPrimaryAssetId DropDefinitionId;

	UFUNCTION()
	void HandlePurchaseClicked();

	UFUNCTION()
	void HandleDeployClicked();
};

/**
 * TEMP_S28P_HUD — Base storage + Orbital + Launch + Unit Drop (GP-S28P4 / GP-S30 / GP-S31R).
 * Local PC-owned. Not production RTS HUD. Root is SelfHitTestInvisible so empty space passes RTS input;
 * Launch / Unit Drop controls consume mouse hits.
 *
 * Programmatic WidgetTree must be built in RebuildWidget (before Super), not only in NativeConstruct.
 */
UCLASS()
class GPRUNTIME_API UGP_TEMP_S28P_PlanetaryFerroniteHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Planetary storage snapshot from UGP_StorageComponent.
	 * ContainerAmounts[i] maps to stable container index i (1-based label for user).
	 */
	void SetStorageDisplay(
		bool bHasBase,
		float TotalStored,
		float TotalCapacity,
		const TArray<float>& ContainerAmounts);

	/** Backward-compatible total-only update (clears per-container lines when unbound). */
	void SetPlanetaryFerroniteDisplay(float StoredAmount, bool bHasBase);

	/** Orbital Ferronite from local PlayerAttributeSet (GAS). Shows 0 when unbound. */
	void SetOrbitalFerroniteDisplay(float OrbitalAmount);

	/** Living player units / MaxUnits from local PlayerAttributeSet. */
	void SetUnitCapDisplay(int32 CurrentUnits, int32 MaxUnits);

	/** TEMP feedback when a unit order is rejected for cap (or local manifest would exceed). */
	void SetUnitCapReachedFeedback(bool bReached);

	/** TEMP match status from replicated GameState + local PlayerState FerroniteScore. No client win calc. */
	void SetMatchPlayingDisplay(float TimeRemainingSeconds, float FerroniteScore, float DeliveryQuota);
	void SetMatchFinishedDisplay(bool bLocalVictory, FGameplayTag WinReason, int32 WinnerTeamId);
	void ClearMatchFinishedDisplay();

	/** UI affordance only — server revalidates launch. */
	void SetLaunchButtonEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleLaunchClicked();

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleConfirmUnitDropClicked();

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleWorkerMinusClicked();

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleWorkerPlusClicked();

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleWalkerMinusClicked();

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleWalkerPlusClicked();

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandlePurchaseLogisticsHubClicked();

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleDeployLogisticsHubClicked();

	void SetBuildingReadyDisplay(int32 ReadyLogisticsHubCount);
	void SetBuildingCatalogDisplay(const TArray<FGP_BuildingHudCatalogRow>& Rows);
	void RequestBuildingPurchaseById(FPrimaryAssetId DropDefinitionId);
	void RequestBuildingDeployById(FPrimaryAssetId DropDefinitionId);

	void SetWallPackageDisplay(
		int32 Stock,
		bool bPending,
		bool bCanBuy,
		bool bCanBuild,
		float PackageCost,
		bool bDefinitionReady);

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleBuyWallPackageClicked();

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleBuildWallClicked();

#if !UE_BUILD_SHIPPING
	FString GetBaseLineTextForContract() const;
	FString GetOrbitalLineTextForContract() const;
	int32 GetContainerLineCountForContract() const;
	FString GetContainerLineTextForContract(int32 ZeroBasedIndex) const;
	bool IsLaunchButtonEnabledForContract() const;
	bool HasInteractiveLaunchButtonForContract() const;
	bool HasWidgetTreeRootForContract() const;
	bool HasStatusPanelForContract() const;
	bool HasLaunchButtonWidgetForContract() const;
	bool HasResourceBarForContract() const;
	bool IsResourceBarTopRightAnchoredForContract() const;
	bool AreOrbitalAndUnitsOnResourceBarForContract() const;
	bool IsStatusPanelBottomLeftAnchoredForContract() const;
	bool IsProcurementGroupBottomRightAnchoredForContract() const;
	bool IsLaunchButtonBottomCenterAnchoredForContract() const;
	bool HasNoDuplicateOrbitalOrUnitsWidgetsForContract() const;
	float GetDisplayedOrbitalForContract() const { return DisplayOrbital; }
	FString GetUnitsLineTextForContract() const;
	int32 GetDisplayedCurrentUnitsForContract() const { return DisplayCurrentUnits; }
	int32 GetDisplayedMaxUnitsForContract() const { return DisplayMaxUnits; }
	FString GetMatchStatusTextForContract() const;
	FString GetMatchResultTitleForContract() const;
	FString GetMatchResultReasonForContract() const;
	int32 GetDisplayedWinnerTeamIdForContract() const { return DisplayWinnerTeamId; }
	bool IsMatchResultVisibleForContract() const;
	float GetDisplayedStoredForContract() const { return DisplayStored; }
	float GetDisplayedCapacityForContract() const { return DisplayCapacity; }
	bool HasResolvedBaseForContract() const { return bHasResolvedBase; }
	int32 GetWorkerCountForContract() const { return WorkerCount; }
	int32 GetSalvageWalkerCountForContract() const { return SalvageWalkerCount; }
	bool IsConfirmDropEnabledForContract() const;
	int32 GetBuildingReadyForContract() const { return ReadyLogisticsHubCount; }
	int32 GetWallStockForContract() const { return WallStock; }
	bool IsWallPackagePendingForContract() const { return bWallPackagePending; }
	bool IsBuyWallPackageEnabledForContract() const;
	bool IsBuildWallAvailableForContract() const { return bWallBuildAvailable; }
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void EnsureWidgetTreeBuilt();
	void BindLaunchClickedIdempotent();
	void BindUnitDropClickedIdempotent();
	void BindBuildingPanelClickedIdempotent();
	void BindWallPackageClickedIdempotent();
	void RefreshWallPackagePanel();
	void EnsureContainerLineCount(int32 DesiredCount);
	void RefreshStatusText();
	void RefreshOrbitalText();
	void RefreshUnitCapText();
	void RefreshMatchStatusText();
	void RefreshMatchResultText();
	void RefreshUnitDropPanel();
	void RefreshBuildingPanel();
	void AdjustWorkerCount(int32 Delta);
	void AdjustWalkerCount(int32 Delta);
	int32 ComputeSlotCost() const;
	float ComputeOrbitalCost() const;
	int32 GetPodCapacity() const;
	float GetBuildingPurchaseCost() const;
	bool CanConfirmLocally() const;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ResourceBar;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ProcurementPanel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> StatusPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BaseLineText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ContainerLinesBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ContainerLineTexts;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OrbitalLineText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UnitsLineText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> MatchInfoPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MatchStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MatchResultTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MatchResultReasonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MatchResultWinnerText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UnitCapFeedbackText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LaunchButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LaunchButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> UnitDropPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UnitDropTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WorkerCountText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WalkerCountText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SlotsCostText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WorkerMinusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WorkerPlusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WalkerMinusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WalkerPlusButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmDropButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmDropLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BuildingPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuildingTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuildingLogisticsHubLineText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PurchaseLogisticsHubButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DeployLogisticsHubButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PurchaseLogisticsHubLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeployLogisticsHubLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BuildingExtraRowsBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGP_TEMP_BuildingCatalogRowBinder>> ExtraRowBinders;

	TArray<FGP_BuildingHudCatalogRow> BuildingCatalogRows;

	void RebuildExtraBuildingRows();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> WallPackagePanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WallStockLineText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BuyWallPackageButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuyWallPackageLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuildWallAvailabilityText;

	int32 WallStock = 0;
	bool bWallPackagePending = false;
	bool bWallCanBuy = false;
	bool bWallBuildAvailable = false;
	float WallPackageCost = 0.0f;
	bool bWallDefinitionReady = false;

	bool bHasResolvedBase = false;
	float DisplayStored = 0.0f;
	float DisplayCapacity = 0.0f;
	TArray<float> DisplayContainerAmounts;
	float DisplayOrbital = 0.0f;
	int32 DisplayCurrentUnits = 0;
	int32 DisplayMaxUnits = 0;
	float DisplayMatchTimeRemaining = 0.0f;
	float DisplayFerroniteScore = 0.0f;
	float DisplayDeliveryQuota = 5000.0f;
	bool bMatchFinishedDisplay = false;
	bool bLocalVictoryDisplay = false;
	FGameplayTag DisplayWinReason;
	int32 DisplayWinnerTeamId = -1;
	bool bUnitCapReachedFeedback = false;
	bool bLaunchEnabled = false;
	bool bTreeBuilt = false;
	int32 WorkerCount = 0;
	int32 SalvageWalkerCount = 0;
	int32 ReadyLogisticsHubCount = 0;
};
