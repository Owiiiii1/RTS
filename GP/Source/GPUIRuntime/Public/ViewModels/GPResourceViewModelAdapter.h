// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "UObject/Object.h"
#include "GPResourceViewModelAdapter.generated.h"

class AGP_GameState;
class AGP_MainBase;
class AGP_PlayerState;
class UAbilitySystemComponent;
class UGP_ResourceViewModel;
class UGP_StorageComponent;

/** Push-only GAS attribute adapter for one local player's resource presentation. */
UCLASS()
class GPUIRUNTIME_API UGP_ResourceViewModelAdapter : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(
		UGP_ResourceViewModel* InViewModel,
		AGP_PlayerState* InLocalPlayerState,
		AGP_PlayerState* InOpponentPlayerState,
		AGP_GameState* InGameState,
		int32 InLocalTeamId);
	void Shutdown();

	UGP_ResourceViewModel* GetViewModel() const { return ViewModel; }
	int32 GetBoundDelegateCount() const;

#if !UE_BUILD_SHIPPING
	/** Contract seam using the same production attribute-to-field mapping. */
	void InitializeForContract(UGP_ResourceViewModel* InViewModel);
	void ApplyOwnAttributeForContract(const FGameplayAttribute& Attribute, float NewValue);
	void ApplyOpponentScoreForContract(float NewValue);
#endif

protected:
	virtual void BeginDestroy() override;

private:
	void RefreshSnapshot();
	void RefreshPlanetFerronite();
	void BindLocalMainBaseStorage();
	void UnbindLocalMainBaseStorage();
	void ApplyOwnAttribute(const FGameplayAttribute& Attribute, float NewValue);
	void HandleOrbitalFerroniteChanged(const FOnAttributeChangeData& Data);
	void HandleFerroniteScoreChanged(const FOnAttributeChangeData& Data);
	void HandleCurrentUnitsChanged(const FOnAttributeChangeData& Data);
	void HandleMaxUnitsChanged(const FOnAttributeChangeData& Data);
	void HandleOpponentFerroniteScoreChanged(const FOnAttributeChangeData& Data);
	void HandleResolvedMainBaseChanged(int32 TeamId, AGP_MainBase* Previous, AGP_MainBase* NewBase);

	UFUNCTION()
	void HandleStorageChanged(float PreviousTotalStored, float NewTotalStored, float TotalCapacity);

	UPROPERTY(Transient)
	TObjectPtr<UGP_ResourceViewModel> ViewModel;

	TWeakObjectPtr<UAbilitySystemComponent> BoundLocalASC;
	TWeakObjectPtr<UAbilitySystemComponent> BoundOpponentASC;
	TWeakObjectPtr<AGP_GameState> BoundGameState;
	TWeakObjectPtr<UGP_StorageComponent> BoundStorage;
	int32 LocalTeamId = -1;
	FDelegateHandle OrbitalFerroniteHandle;
	FDelegateHandle FerroniteScoreHandle;
	FDelegateHandle CurrentUnitsHandle;
	FDelegateHandle MaxUnitsHandle;
	FDelegateHandle OpponentFerroniteScoreHandle;
	FDelegateHandle ResolvedMainBaseHandle;
};
