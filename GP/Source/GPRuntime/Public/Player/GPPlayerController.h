// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Command/GPCommandRequest.h"
#include "GameFramework/PlayerController.h"
#include "GPPlayerController.generated.h"

class UGP_AbilitySystemComponent;
class AGP_CameraPawn;
class UGP_CommandComponent;
class UGP_MarqueeSelectionWidget;
class UGP_TEMP_S28P_PlanetaryFerroniteHUD;
class UGP_SelectionComponent;
class AGP_MainBase;
class AGP_GameState;
class AGP_PlayerState;
class UGP_StorageComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
struct FInputActionValue;

/**
 * Network-correct PlayerController.
 * Forwards Enhanced Input camera intents to possessed AGP_CameraPawn; queries ASC from PlayerState.
 * Owns local UGP_SelectionComponent / Phase B2 select policy and Phase A UGP_CommandComponent shell.
 */
UCLASS()
class GPRUNTIME_API AGP_PlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGP_PlayerController();

	UFUNCTION(BlueprintPure, Category = "GP|AbilitySystem")
	UGP_AbilitySystemComponent* GetGPAbilitySystemComponent() const;

	UGP_SelectionComponent* GetSelectionComponent() const;
	UGP_CommandComponent* GetCommandComponent() const;

	/** Phase D: submit candidate command for authoritative validate/normalize. No execution. */
	UFUNCTION(Server, Reliable)
	void Server_RequestCommand(const FGP_CommandRequest& Request);

	/**
	 * Local TEMP HUD Launch Container intent (GP-S30).
	 * Client calls Server_RequestLaunchReadyContainer — does not mutate Storage locally.
	 */
	void RequestLaunchReadyContainer();

	/** Authority launch intent: resolve own-team MainBase → TryLaunchReadyContainer. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestLaunchReadyContainer();

	/** Authority helper used by Server RPC and non-shipping contracts. */
	bool AuthorityTryLaunchReadyContainerForOwningTeam();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void AcknowledgePossession(APawn* InPawn) override;
	virtual void OnRep_PlayerState() override;
	virtual void BeginPlayingState() override;
	virtual void SetupInputComponent() override;

	virtual void OnLocalPawnReady(APawn* InPawn);
	virtual void OnPlayerStateReady(APlayerState* InPlayerState);
	virtual void OnAbilitySystemLinkReady(UGP_AbilitySystemComponent* InAbilitySystemComponent);

	void TryInitializeLocalPawn(APawn* InPawn);
	void TryInitializePlayerStateLink();

private:
	void InitializeCameraInput();
	void RemoveCameraInputMapping();
	void LoadCameraInputAssets();
	void BindCameraInputActions(UEnhancedInputComponent& EnhancedInput);

	void InitializeSelectionInput();
	void RemoveSelectionInputMapping();
	void LoadSelectionInputAssets();
	void BindSelectionInputActions(UEnhancedInputComponent& EnhancedInput);
	void BindControlGroupInputActions(UEnhancedInputComponent& EnhancedInput);

	void InitializeCommandInput();
	void RemoveCommandInputMapping();
	void LoadCommandInputAssets();
	void BindCommandInputActions(UEnhancedInputComponent& EnhancedInput);

	void OnSelectionStarted(const FInputActionValue& Value);
	void OnSelectionCompleted(const FInputActionValue& Value);
	void OnSelectionCanceled(const FInputActionValue& Value);
	void OnControlGroupStarted(const FInputActionValue& Value);
	void OnCommandInputStarted(const FInputActionValue& Value);

	void ProcessSelectionClickAtScreenPosition(const FVector2D& ScreenPosition);
	void LogSelectionClickResult(
		const TCHAR* ResultTag,
		const AActor* HitActor,
		bool bHasHitTeam,
		int32 HitTeamId,
		int32 LocalTeamId) const;

	void EnsureMarqueeWidget();
	void HideMarqueeWidget();
	void DestroyMarqueeWidget();

	/** TEMP_S28P_HUD — Planetary + Orbital Ferronite + Launch (GP-S28P4 / GP-S30). Local-only, event-driven. */
	void EnsurePlanetaryFerroniteHUD();
	void DestroyPlanetaryFerroniteHUD();
	void RefreshPlanetaryFerroniteHUDBinding();
	void ClearPlanetaryFerroniteHUDBindings();
	void BindPlanetaryFerroniteStorage(AGP_MainBase* MainBase);
	void UnbindPlanetaryFerroniteStorage();
	void SyncPlanetaryFerroniteHUDFromStorage();
	void SyncLaunchButtonFromStorage();
	void BindOrbitalFerroniteAttribute();
	void UnbindOrbitalFerroniteAttribute();
	void SyncOrbitalFerroniteHUDFromAttributes();
	void HandleOrbitalFerroniteAttributeChanged(const struct FOnAttributeChangeData& Data);
	void HandleResolvedMainBaseChanged(int32 TeamId, AGP_MainBase* PreviousMainBase, AGP_MainBase* NewMainBase);
	void HandlePlayerTeamIdChanged(int32 OldTeamId, int32 NewTeamId);
	UFUNCTION()
	void HandleStorageChangedForHUD(float PreviousTotalStored, float NewTotalStored, float TotalCapacity);

	void UpdatePendingSelectionDrag();
	void BeginActiveMarquee(const FVector2D& CurrentScreenPosition);
	void UpdateActiveMarquee(const FVector2D& CurrentScreenPosition);
	void CompleteActiveMarquee(const FVector2D& ReleaseScreenPosition);
	void CancelActiveMarquee(bool bLogCanceled);

	void ResolveAndApplyMarqueeSelection(
		const FVector2D& ScreenStart,
		const FVector2D& ScreenEnd);

	/**
	 * Temporary developer validation visualization (green selected / yellow inspected wireframe).
	 * Must be replaced by production selection highlight in a later UI/visual slice.
	 * Local-only; reads SelectionComponent only; no world scan / RPC / unit mutation.
	 */
	void DrawLocalSelectionDebugVisualization() const;

	bool IsControlModifierDown() const;
	bool IsShiftModifierDown() const;

	AGP_CameraPawn* GetCameraPawn() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Selection", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_SelectionComponent> SelectionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Commands", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_CommandComponent> CommandComponent;

	void OnCameraPan(const FInputActionValue& Value);
	void OnCameraZoom(const FInputActionValue& Value);
	void OnCameraRotate(const FInputActionValue& Value);
	void OnCameraRotateStarted(const FInputActionValue& Value);
	void OnCameraRotateStopped(const FInputActionValue& Value);

	UPROPERTY(EditDefaultsOnly, Category = "GP|Camera|Input")
	TSoftObjectPtr<UInputMappingContext> CameraMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Camera|Input")
	TSoftObjectPtr<UInputAction> CameraPanAction;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Camera|Input")
	TSoftObjectPtr<UInputAction> CameraZoomAction;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Camera|Input")
	TSoftObjectPtr<UInputAction> CameraRotateAction;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Camera|Input")
	TSoftObjectPtr<UInputAction> CameraRotateToggleAction;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Selection|Input")
	TSoftObjectPtr<UInputMappingContext> SelectionMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Selection|Input")
	TSoftObjectPtr<UInputAction> SelectionAction;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Selection|Input")
	TSoftObjectPtr<UInputAction> ControlGroupAction;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Commands|Input")
	TSoftObjectPtr<UInputMappingContext> CommandMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "GP|Commands|Input")
	TSoftObjectPtr<UInputAction> CommandAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LoadedCameraMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedCameraPanAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedCameraZoomAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedCameraRotateAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedCameraRotateToggleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LoadedSelectionMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedSelectionAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedControlGroupAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> LoadedCommandMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LoadedCommandAction;

	UPROPERTY(Transient)
	TObjectPtr<UGP_MarqueeSelectionWidget> MarqueeWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGP_TEMP_S28P_PlanetaryFerroniteHUD> PlanetaryFerroniteHUD;

	TWeakObjectPtr<AGP_GameState> BoundPlanetaryGameState;
	TWeakObjectPtr<AGP_PlayerState> BoundPlanetaryPlayerState;
	TWeakObjectPtr<UGP_StorageComponent> BoundPlanetaryStorage;
	TWeakObjectPtr<UGP_AbilitySystemComponent> BoundOrbitalASC;
	FDelegateHandle ResolvedMainBaseChangedHandle;
	FDelegateHandle PlayerTeamIdChangedHandle;
	FDelegateHandle OrbitalFerroniteChangedHandle;
	int32 BoundPlanetaryTeamId = -1;

	/** Lifecycle guards only — not replicated / not authoritative gameplay state. */
	TWeakObjectPtr<APawn> LastInitializedLocalPawn;
	TWeakObjectPtr<APlayerState> LastInitializedPlayerState;
	TWeakObjectPtr<UGP_AbilitySystemComponent> LastNotifiedAbilitySystemComponent;

	static constexpr int32 CameraMappingPriority = 100;
	static constexpr int32 SelectionMappingPriority = 110;
	static constexpr int32 CommandMappingPriority = 120;
	static constexpr float SelectionDragThresholdPixels = 8.0f;
	static constexpr float SelectionTraceDistance = 1000000.0f;
	static constexpr int32 MarqueeWidgetZOrder = 1000;
	static constexpr int32 PlanetaryFerroniteHUDZOrder = 900;

	bool bCameraMappingContextAdded = false;
	bool bCameraInputBindingsInstalled = false;
	bool bCameraRotateHeld = false;

	bool bSelectionMappingContextAdded = false;
	bool bSelectionActionBindingInstalled = false;
	bool bControlGroupActionBindingInstalled = false;
	bool bCommandMappingContextAdded = false;
	bool bCommandActionBindingInstalled = false;
	bool bSelectionPressActive = false;
	bool bMarqueeActive = false;
	FVector2D SelectionPressScreenPosition = FVector2D::ZeroVector;
};
