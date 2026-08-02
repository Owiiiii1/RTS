// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GPPlayerController.generated.h"

class UGP_AbilitySystemComponent;
class AGP_CameraPawn;
class UGP_SelectionComponent;
class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
struct FInputActionValue;

/**
 * Network-correct PlayerController.
 * Forwards Enhanced Input camera intents to possessed AGP_CameraPawn; queries ASC from PlayerState.
 * Owns local UGP_SelectionComponent state shell. Does not own camera math, ASC creation, or UI.
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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	AGP_CameraPawn* GetCameraPawn() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Selection", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UGP_SelectionComponent> SelectionComponent;

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

	/** Lifecycle guards only — not replicated / not authoritative gameplay state. */
	TWeakObjectPtr<APawn> LastInitializedLocalPawn;
	TWeakObjectPtr<APlayerState> LastInitializedPlayerState;
	TWeakObjectPtr<UGP_AbilitySystemComponent> LastNotifiedAbilitySystemComponent;

	static constexpr int32 CameraMappingPriority = 100;

	bool bCameraMappingContextAdded = false;
	bool bCameraInputBindingsInstalled = false;
	bool bCameraRotateHeld = false;
};
