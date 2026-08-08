// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GPTEMP_S28P_PlanetaryFerroniteHUD.generated.h"

class UButton;
class UTextBlock;
class UCanvasPanel;
class UVerticalBox;
class UHorizontalBox;

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
	float GetDisplayedOrbitalForContract() const { return DisplayOrbital; }
	float GetDisplayedStoredForContract() const { return DisplayStored; }
	float GetDisplayedCapacityForContract() const { return DisplayCapacity; }
	bool HasResolvedBaseForContract() const { return bHasResolvedBase; }
	int32 GetWorkerCountForContract() const { return WorkerCount; }
	int32 GetSalvageWalkerCountForContract() const { return SalvageWalkerCount; }
	bool IsConfirmDropEnabledForContract() const;
	int32 GetBuildingReadyForContract() const { return ReadyLogisticsHubCount; }
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
	void EnsureContainerLineCount(int32 DesiredCount);
	void RefreshStatusText();
	void RefreshOrbitalText();
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

	bool bHasResolvedBase = false;
	float DisplayStored = 0.0f;
	float DisplayCapacity = 0.0f;
	TArray<float> DisplayContainerAmounts;
	float DisplayOrbital = 0.0f;
	bool bLaunchEnabled = false;
	bool bTreeBuilt = false;
	int32 WorkerCount = 0;
	int32 SalvageWalkerCount = 0;
	int32 ReadyLogisticsHubCount = 0;
};
