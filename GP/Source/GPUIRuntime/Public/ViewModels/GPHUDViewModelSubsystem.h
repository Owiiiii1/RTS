// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GPHUDViewModelSubsystem.generated.h"

class AGameStateBase;
class APlayerController;
class APlayerState;
class AGP_GameState;
class AGP_PlayerController;
class AGP_PlayerState;
class UGP_ContextActionPresenter;
class UGP_HUDRootWidget;
class UGP_LaunchMenuPresenter;
class UGP_MatchViewModel;
class UGP_MinimapPresenter;
class UGP_MatchViewModelAdapter;
class UGP_ResourceViewModel;
class UGP_ResourceViewModelAdapter;
class UGP_SelectionViewModel;
class UGP_SelectionViewModelAdapter;

/**
 * GPUIRuntime-owned local-player lifetime for production HUD ViewModels, push adapters,
 * and the production HUD root widget bootstrap. It never ticks and never performs a
 * world actor scan.
 */
UCLASS()
class GPUIRUNTIME_API UGP_HUDViewModelSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	UFUNCTION(BlueprintPure, Category = "GP|HUD|MVVM")
	UGP_ResourceViewModel* GetResourceViewModel() const { return ResourceViewModel; }

	UFUNCTION(BlueprintPure, Category = "GP|HUD|MVVM")
	UGP_MatchViewModel* GetMatchViewModel() const { return MatchViewModel; }

	UFUNCTION(BlueprintPure, Category = "GP|HUD|MVVM")
	UGP_SelectionViewModel* GetSelectionViewModel() const { return SelectionViewModel; }

	UFUNCTION(BlueprintPure, Category = "GP|HUD|MVVM")
	bool IsReady() const { return bReady; }

	UFUNCTION(BlueprintPure, Category = "GP|HUD|MVVM")
	int32 GetLocalTeamId() const { return LocalTeamId; }

	UFUNCTION(BlueprintPure, Category = "GP|HUD")
	UGP_HUDRootWidget* GetProductionHUDWidget() const { return ProductionHUDWidget; }

	UFUNCTION(BlueprintPure, Category = "GP|HUD|LaunchMenu")
	UGP_LaunchMenuPresenter* GetLaunchMenuPresenter() const { return LaunchMenuPresenter; }

	UFUNCTION(BlueprintPure, Category = "GP|HUD|ContextActions")
	UGP_ContextActionPresenter* GetContextActionPresenter() const { return ContextActionPresenter; }

	UFUNCTION(BlueprintPure, Category = "GP|HUD|Minimap")
	UGP_MinimapPresenter* GetMinimapPresenter() const { return MinimapPresenter; }

	int32 GetResourceDelegateCount() const;
	int32 GetMatchDelegateCount() const;
	int32 GetSelectionDelegateCount() const;

	void EnsureProductionHUD();
	void TeardownProductionHUD();

#if !UE_BUILD_SHIPPING
	void EnsureProductionHUDWithClassForContract(TSubclassOf<UGP_HUDRootWidget> WidgetClass);
	void DebugDumpToLog() const;
	void DebugDumpHUDStatusToLog() const;
#endif

private:
	void Rebind();
	void ResetViewModels();
	void BindPlayerController(AGP_PlayerController* PlayerController);
	void UnbindPlayerController();
	void BindSelectionAdapter(AGP_PlayerController* PlayerController);
	void BindContextActionPresenter(AGP_PlayerController* PlayerController);
	void BindMinimapPresenter(AGP_PlayerController* PlayerController);
	void BindGameState(AGP_GameState* GameState);
	void UnbindGameState();
	void RebuildPlayerStateTeamBindings(AGP_GameState* GameState);
	void ClearPlayerStateTeamBindings();
	AGP_PlayerState* ResolveOpponentPlayerState(
		const AGP_GameState* GameState,
		const AGP_PlayerState* LocalPlayerState,
		int32 InLocalTeamId) const;
	void HandleGameStateSet(AGameStateBase* NewGameState);
	void HandlePlayerStatePresentationReady(APlayerState* PlayerState);
	void HandlePlayerStateRosterChanged(APlayerState* PlayerState, bool bAdded);
	void HandleAnyPlayerTeamIdChanged(int32 OldTeamId, int32 NewTeamId);
	TSubclassOf<UGP_HUDRootWidget> ResolveConfiguredProductionHUDClass() const;
	void EnsureProductionHUDInternal(
		TSubclassOf<UGP_HUDRootWidget> WidgetClass,
		bool bWarnIfUnconfigured);

	UPROPERTY(Transient)
	TObjectPtr<UGP_HUDRootWidget> ProductionHUDWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGP_ResourceViewModel> ResourceViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UGP_MatchViewModel> MatchViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UGP_ResourceViewModelAdapter> ResourceAdapter;

	UPROPERTY(Transient)
	TObjectPtr<UGP_MatchViewModelAdapter> MatchAdapter;

	UPROPERTY(Transient)
	TObjectPtr<UGP_SelectionViewModel> SelectionViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UGP_SelectionViewModelAdapter> SelectionAdapter;

	UPROPERTY(Transient)
	TObjectPtr<UGP_LaunchMenuPresenter> LaunchMenuPresenter;

	UPROPERTY(Transient)
	TObjectPtr<UGP_ContextActionPresenter> ContextActionPresenter;

	UPROPERTY(Transient)
	TObjectPtr<UGP_MinimapPresenter> MinimapPresenter;

	TWeakObjectPtr<AGP_GameState> BoundGameState;
	TWeakObjectPtr<AGP_PlayerController> BoundPlayerController;
	FDelegateHandle GameStateSetHandle;
	FDelegateHandle PlayerStatePresentationReadyHandle;
	FDelegateHandle PlayerStateRosterHandle;
	TMap<TWeakObjectPtr<AGP_PlayerState>, FDelegateHandle> PlayerStateTeamHandles;
	int32 LocalTeamId = -1;
	bool bReady = false;
	bool bLoggedUnconfiguredHUDClass = false;
};
