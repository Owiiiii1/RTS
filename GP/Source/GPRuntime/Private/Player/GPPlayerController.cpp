// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GPPlayerController.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Camera/GPCameraPawn.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Player/GPSelectionComponent.h"

AGP_PlayerController::AGP_PlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	SelectionComponent = CreateDefaultSubobject<UGP_SelectionComponent>(TEXT("SelectionComponent"));

	CameraMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Camera/IMC_GP_Camera.IMC_GP_Camera")));
	CameraPanAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Camera/IA_Camera_Pan.IA_Camera_Pan")));
	CameraZoomAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Camera/IA_Camera_Zoom.IA_Camera_Zoom")));
	CameraRotateAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Camera/IA_Camera_Rotate.IA_Camera_Rotate")));
	CameraRotateToggleAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Camera/IA_Camera_RotateToggle.IA_Camera_RotateToggle")));
}

UGP_AbilitySystemComponent* AGP_PlayerController::GetGPAbilitySystemComponent() const
{
	APlayerState* PS = GetPlayerState<APlayerState>();
	if (PS == nullptr)
	{
		return nullptr;
	}

	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(PS);
	if (ASI == nullptr)
	{
		return nullptr;
	}

	return Cast<UGP_AbilitySystemComponent>(ASI->GetAbilitySystemComponent());
}

UGP_SelectionComponent* AGP_PlayerController::GetSelectionComponent() const
{
	return SelectionComponent;
}

void AGP_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			TryInitializeLocalPawn(ControlledPawn);
		}
		TryInitializePlayerStateLink();
	}
}

void AGP_PlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bCameraRotateHeld = false;

	if (AGP_CameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->SetRotateActive(false);
	}

	RemoveCameraInputMapping();

	LoadedCameraMappingContext = nullptr;
	LoadedCameraPanAction = nullptr;
	LoadedCameraZoomAction = nullptr;
	LoadedCameraRotateAction = nullptr;
	LoadedCameraRotateToggleAction = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AGP_PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Verbose,
		TEXT("AGP_PlayerController::OnPossess: Pawn=%s Local=%s"),
		*GetNameSafe(InPawn),
		IsLocalController() ? TEXT("true") : TEXT("false"));

	if (IsLocalController())
	{
		TryInitializeLocalPawn(InPawn);
	}

	TryInitializePlayerStateLink();
}

void AGP_PlayerController::OnUnPossess()
{
	if (AGP_CameraPawn* CameraPawn = Cast<AGP_CameraPawn>(GetPawn()))
	{
		CameraPawn->SetRotateActive(false);
	}

	bCameraRotateHeld = false;

	const APawn* CurrentPawn = GetPawn();
	if (LastInitializedLocalPawn.Get() == CurrentPawn)
	{
		LastInitializedLocalPawn.Reset();
	}

	Super::OnUnPossess();
}

void AGP_PlayerController::AcknowledgePossession(APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);

	if (!IsLocalController())
	{
		return;
	}

	TryInitializeLocalPawn(InPawn);
	TryInitializePlayerStateLink();
}

void AGP_PlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	TryInitializePlayerStateLink();
}

void AGP_PlayerController::BeginPlayingState()
{
	Super::BeginPlayingState();

	if (!IsLocalController())
	{
		TryInitializePlayerStateLink();
		return;
	}

	TryInitializeLocalPawn(GetPawn());
	TryInitializePlayerStateLink();

	bShowMouseCursor = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
	SetInputMode(InputMode);

	InitializeCameraInput();
}

void AGP_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (bCameraInputBindingsInstalled)
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInputComponent == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::SetupInputComponent: InputComponent is not UEnhancedInputComponent."));
		return;
	}

	LoadCameraInputAssets();
	BindCameraInputActions(*EnhancedInputComponent);
	bCameraInputBindingsInstalled = true;
}

void AGP_PlayerController::TryInitializeLocalPawn(APawn* InPawn)
{
	if (!IsLocalController() || InPawn == nullptr)
	{
		return;
	}

	if (LastInitializedLocalPawn.Get() == InPawn)
	{
		return;
	}

	LastInitializedLocalPawn = InPawn;
	OnLocalPawnReady(InPawn);
}

void AGP_PlayerController::TryInitializePlayerStateLink()
{
	APlayerState* PS = GetPlayerState<APlayerState>();
	if (PS == nullptr)
	{
		return;
	}

	if (LastInitializedPlayerState.Get() != PS)
	{
		LastInitializedPlayerState = PS;
		LastNotifiedAbilitySystemComponent.Reset();
		OnPlayerStateReady(PS);
	}

	UGP_AbilitySystemComponent* ASC = GetGPAbilitySystemComponent();
	if (ASC == nullptr)
	{
		return;
	}

	if (LastNotifiedAbilitySystemComponent.Get() == ASC)
	{
		return;
	}

	LastNotifiedAbilitySystemComponent = ASC;
	OnAbilitySystemLinkReady(ASC);
}

void AGP_PlayerController::OnLocalPawnReady(APawn* InPawn)
{
	if (Cast<AGP_CameraPawn>(InPawn) == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("Local pawn %s is not GP_CameraPawn; camera input forwarding is unavailable."),
			*GetNameSafe(InPawn));
	}
}

void AGP_PlayerController::OnPlayerStateReady(APlayerState* InPlayerState)
{
	UE_LOG(LogTemp, Log,
		TEXT("AGP_PlayerController::OnPlayerStateReady: PlayerState ready (%s)."),
		*GetNameSafe(InPlayerState));
}

void AGP_PlayerController::OnAbilitySystemLinkReady(UGP_AbilitySystemComponent* InAbilitySystemComponent)
{
	UE_LOG(LogTemp, Log,
		TEXT("AGP_PlayerController::OnAbilitySystemLinkReady: ASC linked (%s)."),
		*GetNameSafe(InAbilitySystemComponent));
}

void AGP_PlayerController::LoadCameraInputAssets()
{
	LoadedCameraPanAction = CameraPanAction.LoadSynchronous();
	if (LoadedCameraPanAction == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IA_Camera_Pan at '%s'."),
			*CameraPanAction.ToSoftObjectPath().ToString());
	}

	LoadedCameraZoomAction = CameraZoomAction.LoadSynchronous();
	if (LoadedCameraZoomAction == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IA_Camera_Zoom at '%s'."),
			*CameraZoomAction.ToSoftObjectPath().ToString());
	}

	LoadedCameraRotateAction = CameraRotateAction.LoadSynchronous();
	if (LoadedCameraRotateAction == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IA_Camera_Rotate at '%s'."),
			*CameraRotateAction.ToSoftObjectPath().ToString());
	}

	LoadedCameraRotateToggleAction = CameraRotateToggleAction.LoadSynchronous();
	if (LoadedCameraRotateToggleAction == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IA_Camera_RotateToggle at '%s'."),
			*CameraRotateToggleAction.ToSoftObjectPath().ToString());
	}
}

void AGP_PlayerController::BindCameraInputActions(UEnhancedInputComponent& EnhancedInput)
{
	if (LoadedCameraPanAction != nullptr)
	{
		EnhancedInput.BindAction(
			LoadedCameraPanAction,
			ETriggerEvent::Triggered,
			this,
			&AGP_PlayerController::OnCameraPan);
	}

	if (LoadedCameraZoomAction != nullptr)
	{
		EnhancedInput.BindAction(
			LoadedCameraZoomAction,
			ETriggerEvent::Triggered,
			this,
			&AGP_PlayerController::OnCameraZoom);
	}

	if (LoadedCameraRotateAction != nullptr)
	{
		EnhancedInput.BindAction(
			LoadedCameraRotateAction,
			ETriggerEvent::Triggered,
			this,
			&AGP_PlayerController::OnCameraRotate);
	}

	if (LoadedCameraRotateToggleAction != nullptr)
	{
		EnhancedInput.BindAction(
			LoadedCameraRotateToggleAction,
			ETriggerEvent::Started,
			this,
			&AGP_PlayerController::OnCameraRotateStarted);
		EnhancedInput.BindAction(
			LoadedCameraRotateToggleAction,
			ETriggerEvent::Completed,
			this,
			&AGP_PlayerController::OnCameraRotateStopped);
		EnhancedInput.BindAction(
			LoadedCameraRotateToggleAction,
			ETriggerEvent::Canceled,
			this,
			&AGP_PlayerController::OnCameraRotateStopped);
	}
}

void AGP_PlayerController::InitializeCameraInput()
{
	if (!IsLocalController() || bCameraMappingContextAdded)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (LocalPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::InitializeCameraInput: LocalPlayer is unavailable."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (Subsystem == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::InitializeCameraInput: Enhanced Input subsystem is unavailable."));
		return;
	}

	LoadedCameraMappingContext = CameraMappingContext.LoadSynchronous();
	if (LoadedCameraMappingContext == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IMC_GP_Camera at '%s'."),
			*CameraMappingContext.ToSoftObjectPath().ToString());
		return;
	}

	Subsystem->AddMappingContext(LoadedCameraMappingContext, CameraMappingPriority);
	bCameraMappingContextAdded = true;
}

void AGP_PlayerController::RemoveCameraInputMapping()
{
	if (!bCameraMappingContextAdded)
	{
		return;
	}

	if (IsLocalController())
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
					LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (LoadedCameraMappingContext != nullptr)
				{
					Subsystem->RemoveMappingContext(LoadedCameraMappingContext);
				}
			}
		}
	}

	bCameraMappingContextAdded = false;
}

AGP_CameraPawn* AGP_PlayerController::GetCameraPawn() const
{
	return Cast<AGP_CameraPawn>(GetPawn());
}

void AGP_PlayerController::OnCameraPan(const FInputActionValue& Value)
{
	if (AGP_CameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->SetPanInput(Value.Get<FVector2D>());
	}
}

void AGP_PlayerController::OnCameraZoom(const FInputActionValue& Value)
{
	if (AGP_CameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->AddZoomInput(Value.Get<float>());
	}
}

void AGP_PlayerController::OnCameraRotate(const FInputActionValue& Value)
{
	if (!bCameraRotateHeld)
	{
		return;
	}

	if (AGP_CameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->AddRotateInput(Value.Get<float>());
	}
}

void AGP_PlayerController::OnCameraRotateStarted(const FInputActionValue& Value)
{
	(void)Value;

	bCameraRotateHeld = true;
	if (AGP_CameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->SetRotateActive(true);
	}
}

void AGP_PlayerController::OnCameraRotateStopped(const FInputActionValue& Value)
{
	(void)Value;

	bCameraRotateHeld = false;
	if (AGP_CameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->SetRotateActive(false);
	}
}
