// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GPTEMP_S28P_PlanetaryFerroniteHUD.generated.h"

class UButton;
class UTextBlock;
class UCanvasPanel;

/**
 * TEMP_S28P_HUD — Planetary + Orbital Ferronite readout + Launch Container button (GP-S28P4 / GP-S30).
 * Local PC-owned. Not production RTS HUD. Root is SelfHitTestInvisible so empty space passes RTS input;
 * only the Launch button consumes mouse hits.
 */
UCLASS()
class GPRUNTIME_API UGP_TEMP_S28P_PlanetaryFerroniteHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	/** bHasBase=false shows "--"; otherwise integer-rounded stored amount. */
	void SetPlanetaryFerroniteDisplay(float StoredAmount, bool bHasBase);

	/** Orbital Ferronite from local PlayerAttributeSet (GAS). Shows 0 when unbound. */
	void SetOrbitalFerroniteDisplay(float OrbitalAmount);

	/** UI affordance only — server revalidates launch. */
	void SetLaunchButtonEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GP|TEMP HUD")
	void HandleLaunchClicked();

#if !UE_BUILD_SHIPPING
	FString GetCountersDisplayTextForContract() const { return CountersDisplayText; }
	bool IsLaunchButtonEnabledForContract() const;
	bool HasInteractiveLaunchButtonForContract() const;
	float GetDisplayedOrbitalForContract() const { return DisplayOrbital; }
	float GetDisplayedPlanetaryForContract() const { return DisplayStored; }
	bool HasResolvedBaseForContract() const { return bHasResolvedBase; }
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void EnsureWidgetTreeBuilt();
	void RefreshCountersText();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountersText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LaunchButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LaunchButtonLabel;

	bool bHasResolvedBase = false;
	float DisplayStored = 0.0f;
	float DisplayOrbital = 0.0f;
	FString CountersDisplayText = TEXT("Ferronite: --     Orbital: 0");
	bool bLaunchEnabled = false;
	bool bTreeBuilt = false;
};
