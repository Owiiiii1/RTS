// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GPTEMP_S28P_PlanetaryFerroniteHUD.generated.h"

/**
 * TEMP_S28P_HUD — minimal Planetary Ferronite readout (GP-S28P4).
 * Local PC-owned; paints stored amount only. Not production RTS HUD.
 */
UCLASS()
class GPRUNTIME_API UGP_TEMP_S28P_PlanetaryFerroniteHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	/** bHasBase=false shows "--"; otherwise integer-rounded stored amount. */
	void SetPlanetaryFerroniteDisplay(float StoredAmount, bool bHasBase);

protected:
	virtual void NativeConstruct() override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	bool bHasResolvedBase = false;
	float DisplayStored = 0.0f;
	FString DisplayText = TEXT("Ferronite: --");

	static constexpr float PadX = 24.0f;
	static constexpr float PadY = 24.0f;
};
