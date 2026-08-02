// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GPPlayerController.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Algo/Sort.h"
#include "Blueprint/UserWidget.h"
#include "Camera/GPCameraPawn.h"
#include "CollisionQueryParams.h"
#include "Command/GPCommandComponent.h"
#include "Command/GPCommandRequest.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EngineUtils.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "UI/GPMarqueeSelectionWidget.h"
#include "Units/GPUnitBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPCommandInput, Log, All);

AGP_PlayerController::AGP_PlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	SelectionComponent = CreateDefaultSubobject<UGP_SelectionComponent>(TEXT("SelectionComponent"));
	CommandComponent = CreateDefaultSubobject<UGP_CommandComponent>(TEXT("CommandComponent"));

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

	SelectionMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Selection/IMC_GP_Selection.IMC_GP_Selection")));
	SelectionAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Selection/IA_Select.IA_Select")));
	ControlGroupAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Selection/IA_ControlGroup.IA_ControlGroup")));

	CommandMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Commands/IMC_GP_Commands.IMC_GP_Commands")));
	CommandAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Commands/IA_Command.IA_Command")));
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

UGP_CommandComponent* AGP_PlayerController::GetCommandComponent() const
{
	return CommandComponent;
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
	CancelActiveMarquee(/*bLogCanceled=*/false);
	bSelectionPressActive = false;
	SelectionPressScreenPosition = FVector2D::ZeroVector;
	DestroyMarqueeWidget();

	bCameraRotateHeld = false;

	if (AGP_CameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->SetRotateActive(false);
	}

	RemoveCommandInputMapping();
	RemoveSelectionInputMapping();
	RemoveCameraInputMapping();

	LoadedSelectionMappingContext = nullptr;
	LoadedSelectionAction = nullptr;
	LoadedControlGroupAction = nullptr;

	LoadedCommandMappingContext = nullptr;
	LoadedCommandAction = nullptr;

	LoadedCameraMappingContext = nullptr;
	LoadedCameraPanAction = nullptr;
	LoadedCameraZoomAction = nullptr;
	LoadedCameraRotateAction = nullptr;
	LoadedCameraRotateToggleAction = nullptr;

	Super::EndPlay(EndPlayReason);
}

void AGP_PlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocalController())
	{
		return;
	}

	// Temporary validation-only boxes; not marquee work and not a world scan.
	DrawLocalSelectionDebugVisualization();

	if (!bSelectionPressActive)
	{
		return;
	}

	UpdatePendingSelectionDrag();
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
	if (bSelectionPressActive || bMarqueeActive)
	{
		CancelActiveMarquee(/*bLogCanceled=*/true);
		bSelectionPressActive = false;
		SelectionPressScreenPosition = FVector2D::ZeroVector;
	}

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
	InitializeSelectionInput();
	InitializeCommandInput();
}

void AGP_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInputComponent == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::SetupInputComponent: InputComponent is not UEnhancedInputComponent."));
		return;
	}

	if (!bCameraInputBindingsInstalled)
	{
		LoadCameraInputAssets();
		BindCameraInputActions(*EnhancedInputComponent);
		bCameraInputBindingsInstalled = true;
	}

	LoadSelectionInputAssets();

	if (!bSelectionActionBindingInstalled)
	{
		BindSelectionInputActions(*EnhancedInputComponent);
		bSelectionActionBindingInstalled = true;
	}

	if (!bControlGroupActionBindingInstalled)
	{
		BindControlGroupInputActions(*EnhancedInputComponent);
		bControlGroupActionBindingInstalled = true;
	}

	LoadCommandInputAssets();

	if (!bCommandActionBindingInstalled)
	{
		BindCommandInputActions(*EnhancedInputComponent);
		bCommandActionBindingInstalled = true;
	}
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

void AGP_PlayerController::LoadSelectionInputAssets()
{
	LoadedSelectionAction = SelectionAction.LoadSynchronous();
	if (LoadedSelectionAction == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IA_Select at '%s'."),
			*SelectionAction.ToSoftObjectPath().ToString());
	}

	LoadedControlGroupAction = ControlGroupAction.LoadSynchronous();
	if (LoadedControlGroupAction == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IA_ControlGroup at '%s'."),
			*ControlGroupAction.ToSoftObjectPath().ToString());
	}
}


void AGP_PlayerController::BindSelectionInputActions(UEnhancedInputComponent& EnhancedInput)
{
	if (LoadedSelectionAction == nullptr)
	{
		return;
	}

	EnhancedInput.BindAction(
		LoadedSelectionAction,
		ETriggerEvent::Started,
		this,
		&AGP_PlayerController::OnSelectionStarted);
	EnhancedInput.BindAction(
		LoadedSelectionAction,
		ETriggerEvent::Completed,
		this,
		&AGP_PlayerController::OnSelectionCompleted);
	EnhancedInput.BindAction(
		LoadedSelectionAction,
		ETriggerEvent::Canceled,
		this,
		&AGP_PlayerController::OnSelectionCanceled);
}

void AGP_PlayerController::BindControlGroupInputActions(UEnhancedInputComponent& EnhancedInput)
{
	if (LoadedControlGroupAction == nullptr)
	{
		return;
	}

	EnhancedInput.BindAction(
		LoadedControlGroupAction,
		ETriggerEvent::Started,
		this,
		&AGP_PlayerController::OnControlGroupStarted);
}

void AGP_PlayerController::InitializeSelectionInput()
{
	if (!IsLocalController() || bSelectionMappingContextAdded)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (LocalPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::InitializeSelectionInput: LocalPlayer is unavailable."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (Subsystem == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::InitializeSelectionInput: Enhanced Input subsystem is unavailable."));
		return;
	}

	LoadedSelectionMappingContext = SelectionMappingContext.LoadSynchronous();
	if (LoadedSelectionMappingContext == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IMC_GP_Selection at '%s'."),
			*SelectionMappingContext.ToSoftObjectPath().ToString());
		return;
	}

	Subsystem->AddMappingContext(LoadedSelectionMappingContext, SelectionMappingPriority);
	bSelectionMappingContextAdded = true;
}

void AGP_PlayerController::RemoveSelectionInputMapping()
{
	if (!bSelectionMappingContextAdded)
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
				if (LoadedSelectionMappingContext != nullptr)
				{
					Subsystem->RemoveMappingContext(LoadedSelectionMappingContext);
				}
			}
		}
	}

	bSelectionMappingContextAdded = false;
}

void AGP_PlayerController::LoadCommandInputAssets()
{
	LoadedCommandAction = CommandAction.LoadSynchronous();
	if (LoadedCommandAction == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IA_Command at '%s'."),
			*CommandAction.ToSoftObjectPath().ToString());
	}
}

void AGP_PlayerController::BindCommandInputActions(UEnhancedInputComponent& EnhancedInput)
{
	if (LoadedCommandAction == nullptr)
	{
		return;
	}

	EnhancedInput.BindAction(
		LoadedCommandAction,
		ETriggerEvent::Started,
		this,
		&AGP_PlayerController::OnCommandInputStarted);
}

void AGP_PlayerController::InitializeCommandInput()
{
	if (!IsLocalController() || bCommandMappingContextAdded)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (LocalPlayer == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::InitializeCommandInput: LocalPlayer is unavailable."));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (Subsystem == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::InitializeCommandInput: Enhanced Input subsystem is unavailable."));
		return;
	}

	LoadedCommandMappingContext = CommandMappingContext.LoadSynchronous();
	if (LoadedCommandMappingContext == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController: missing IMC_GP_Commands at '%s'."),
			*CommandMappingContext.ToSoftObjectPath().ToString());
		return;
	}

	Subsystem->AddMappingContext(LoadedCommandMappingContext, CommandMappingPriority);
	bCommandMappingContextAdded = true;
}

void AGP_PlayerController::RemoveCommandInputMapping()
{
	if (!bCommandMappingContextAdded)
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
				if (LoadedCommandMappingContext != nullptr)
				{
					Subsystem->RemoveMappingContext(LoadedCommandMappingContext);
				}
			}
		}
	}

	bCommandMappingContextAdded = false;
}

void AGP_PlayerController::OnCommandInputStarted(const FInputActionValue& Value)
{
	(void)Value;

	if (!IsLocalController() || CommandComponent == nullptr)
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	if (!DeprojectScreenPositionToWorld(MouseX, MouseY, WorldOrigin, WorldDirection))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GPCommandClick), /*bTraceComplex=*/false);
	QueryParams.bReturnPhysicalMaterial = false;
	if (APawn* ControlledPawn = GetPawn())
	{
		QueryParams.AddIgnoredActor(ControlledPawn);
	}

	const FVector TraceEnd = WorldOrigin + (WorldDirection * SelectionTraceDistance);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(
			Hit,
			WorldOrigin,
			TraceEnd,
			ECC_Visibility,
			QueryParams))
	{
		return;
	}

	AActor* TargetActor = Hit.GetActor();
	const FVector TargetLocation = Hit.ImpactPoint;
	const bool bQueue = IsShiftModifierDown();

	FGP_CommandRequest Request;
	if (!CommandComponent->BuildSmartCommand(TargetActor, TargetLocation, bQueue, Request))
	{
		return;
	}

	const AGP_PlayerState* GPPlayerState = GetPlayerState<AGP_PlayerState>();
	const int32 LocalTeamId = GPPlayerState != nullptr ? GPPlayerState->GetTeamId() : -1;

	const TCHAR* NetModeText = TEXT("Unknown");
	switch (World->GetNetMode())
	{
	case NM_Standalone:
		NetModeText = TEXT("Standalone");
		break;
	case NM_DedicatedServer:
		NetModeText = TEXT("DedicatedServer");
		break;
	case NM_ListenServer:
		NetModeText = TEXT("ListenServer");
		break;
	case NM_Client:
		NetModeText = TEXT("Client");
		break;
	default:
		break;
	}

	const TCHAR* RoleText = TEXT("Unknown");
	switch (GetLocalRole())
	{
	case ROLE_None:
		RoleText = TEXT("None");
		break;
	case ROLE_SimulatedProxy:
		RoleText = TEXT("SimulatedProxy");
		break;
	case ROLE_AutonomousProxy:
		RoleText = TEXT("AutonomousProxy");
		break;
	case ROLE_Authority:
		RoleText = TEXT("Authority");
		break;
	default:
		break;
	}

	UE_LOG(LogGPCommandInput, Log,
		TEXT("GP CommandInput: Tag=%s Units=%d TargetActor=%s Loc=%s Queue=%s LocalTeam=%d NetMode=%s Role=%s"),
		*Request.CommandTag.ToString(),
		Request.IssuingUnits.Num(),
		*GetNameSafe(Request.TargetActor),
		*Request.TargetLocation.ToCompactString(),
		Request.bQueue ? TEXT("true") : TEXT("false"),
		LocalTeamId,
		NetModeText,
		RoleText);
}

void AGP_PlayerController::OnSelectionStarted(const FInputActionValue& Value)
{
	(void)Value;

	if (!IsLocalController())
	{
		return;
	}

	if (bMarqueeActive
		|| (SelectionComponent != nullptr && SelectionComponent->IsMarqueeActive())
		|| (MarqueeWidget != nullptr && MarqueeWidget->HasActiveMarqueeRect()))
	{
		CancelActiveMarquee(/*bLogCanceled=*/false);
	}
	bMarqueeActive = false;

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		bSelectionPressActive = false;
		SelectionPressScreenPosition = FVector2D::ZeroVector;
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_PlayerController::OnSelectionStarted: GetMousePosition failed."));
		return;
	}

	SelectionPressScreenPosition = FVector2D(MouseX, MouseY);
	bSelectionPressActive = true;
}

void AGP_PlayerController::OnSelectionCompleted(const FInputActionValue& Value)
{
	(void)Value;

	if (!IsLocalController() || !bSelectionPressActive)
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		CancelActiveMarquee(/*bLogCanceled=*/true);
		bSelectionPressActive = false;
		SelectionPressScreenPosition = FVector2D::ZeroVector;
		return;
	}

	const FVector2D ReleasePosition(MouseX, MouseY);

	if (bMarqueeActive)
	{
		CompleteActiveMarquee(ReleasePosition);
		return;
	}

	const float PixelDistance =
		FVector2D::Distance(SelectionPressScreenPosition, ReleasePosition);
	if (PixelDistance <= SelectionDragThresholdPixels)
	{
		bSelectionPressActive = false;
		SelectionPressScreenPosition = FVector2D::ZeroVector;
		ProcessSelectionClickAtScreenPosition(ReleasePosition);
		return;
	}

	// Fast-release / low-FPS: Tick may not have crossed the threshold yet.
	BeginActiveMarquee(ReleasePosition);
	CompleteActiveMarquee(ReleasePosition);
}

void AGP_PlayerController::OnSelectionCanceled(const FInputActionValue& Value)
{
	(void)Value;

	CancelActiveMarquee(/*bLogCanceled=*/true);
	bSelectionPressActive = false;
	SelectionPressScreenPosition = FVector2D::ZeroVector;
}

void AGP_PlayerController::OnControlGroupStarted(const FInputActionValue& Value)
{
	if (!IsLocalController() || SelectionComponent == nullptr)
	{
		return;
	}

	const float AxisValue = Value.Get<float>();
	const int32 GroupNumber = FMath::RoundToInt(AxisValue);
	if (GroupNumber < 1 || GroupNumber > 9)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP ControlGroup: Result=InvalidGroup Axis=%.3f"),
			AxisValue);
		return;
	}

	const int32 BeforeCount = SelectionComponent->GetSelectionCount();
	const bool bCtrl = IsControlModifierDown();
	const bool bShift = IsShiftModifierDown();

	const TCHAR* OperationTag = TEXT("Recall");
	if (bCtrl && bShift)
	{
		SelectionComponent->AppendToControlGroup(GroupNumber);
		OperationTag = TEXT("AppendToGroup");
	}
	else if (bCtrl)
	{
		SelectionComponent->AssignControlGroup(GroupNumber);
		OperationTag = TEXT("Assign");
	}
	else if (bShift)
	{
		TArray<TWeakObjectPtr<AGP_UnitBase>> BeforeSelection =
			SelectionComponent->GetSelectedUnits();
		SelectionComponent->AppendControlGroupToSelection(GroupNumber);
		const TArray<TWeakObjectPtr<AGP_UnitBase>>& AfterSelection =
			SelectionComponent->GetSelectedUnits();

		auto AreSelectionIdentitiesEqual =
			[](const TArray<TWeakObjectPtr<AGP_UnitBase>>& A,
				const TArray<TWeakObjectPtr<AGP_UnitBase>>& B) -> bool
		{
			if (A.Num() != B.Num())
			{
				return false;
			}

			for (int32 Index = 0; Index < A.Num(); ++Index)
			{
				if (A[Index].Get() != B[Index].Get())
				{
					return false;
				}
			}

			return true;
		};

		if (!AreSelectionIdentitiesEqual(BeforeSelection, AfterSelection)
			&& SelectionComponent->GetInspectedTarget() != nullptr)
		{
			SelectionComponent->ClearInspectedTarget();
		}

		OperationTag = TEXT("AppendRecall");
	}
	else
	{
		if (SelectionComponent->GetInspectedTarget() != nullptr)
		{
			SelectionComponent->ClearInspectedTarget();
		}
		SelectionComponent->RecallControlGroup(GroupNumber);
		OperationTag = TEXT("Recall");
	}

	UE_LOG(LogTemp, Log,
		TEXT("GP ControlGroup: Group=%d Operation=%s Before=%d After=%d"),
		GroupNumber,
		OperationTag,
		BeforeCount,
		SelectionComponent->GetSelectionCount());
}

void AGP_PlayerController::DrawLocalSelectionDebugVisualization() const
{
	// Temporary developer validation visualization.
	// Must be replaced by production selection highlight in a later UI/visual slice.
	if (!IsLocalController() || SelectionComponent == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	static constexpr float BoundsPadding = 5.0f;
	static constexpr float BoxThickness = 2.0f;
	static constexpr float InspectBoxThickness = 2.5f;
	static constexpr float BoxLifeTime = 0.0f;

	const UGP_SelectionComponent* LocalSelection = SelectionComponent;
	const TArray<TWeakObjectPtr<AGP_UnitBase>>& SelectedUnits = LocalSelection->GetSelectedUnits();

	TSet<const AGP_UnitBase*> DrawnSelectedUnits;
	DrawnSelectedUnits.Reserve(SelectedUnits.Num());

	for (const TWeakObjectPtr<AGP_UnitBase>& WeakUnit : SelectedUnits)
	{
		const AGP_UnitBase* Unit = WeakUnit.Get();
		if (!IsValid(Unit))
		{
			continue;
		}

		FVector BoundsOrigin = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		Unit->GetActorBounds(false, BoundsOrigin, BoundsExtent);
		BoundsExtent += FVector(BoundsPadding);

		DrawDebugBox(
			World,
			BoundsOrigin,
			BoundsExtent,
			FColor::Green,
			/*bPersistentLines=*/false,
			BoxLifeTime,
			/*DepthPriority=*/0,
			BoxThickness);

		DrawnSelectedUnits.Add(Unit);
	}

	AActor* InspectedActor = LocalSelection->GetInspectedTarget();
	const AGP_UnitBase* InspectedUnit = Cast<AGP_UnitBase>(InspectedActor);
	if (!IsValid(InspectedUnit) || DrawnSelectedUnits.Contains(InspectedUnit))
	{
		return;
	}

	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;
	InspectedUnit->GetActorBounds(false, BoundsOrigin, BoundsExtent);
	BoundsExtent += FVector(BoundsPadding);

	DrawDebugBox(
		World,
		BoundsOrigin,
		BoundsExtent,
		FColor::Yellow,
		/*bPersistentLines=*/false,
		BoxLifeTime,
		/*DepthPriority=*/0,
		InspectBoxThickness);
}

void AGP_PlayerController::UpdatePendingSelectionDrag()
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	const FVector2D CurrentPosition(MouseX, MouseY);
	if (!bMarqueeActive)
	{
		const float PixelDistance =
			FVector2D::Distance(SelectionPressScreenPosition, CurrentPosition);
		if (PixelDistance > SelectionDragThresholdPixels)
		{
			BeginActiveMarquee(CurrentPosition);
		}
		return;
	}

	UpdateActiveMarquee(CurrentPosition);
}

void AGP_PlayerController::BeginActiveMarquee(const FVector2D& CurrentScreenPosition)
{
	if (!IsLocalController() || SelectionComponent == nullptr || bMarqueeActive)
	{
		return;
	}

	bMarqueeActive = true;
	SelectionComponent->BeginMarquee(SelectionPressScreenPosition);
	SelectionComponent->UpdateMarquee(CurrentScreenPosition);

	EnsureMarqueeWidget();
	if (MarqueeWidget != nullptr)
	{
		MarqueeWidget->SetMarqueeRect(SelectionPressScreenPosition, CurrentScreenPosition);
	}
}

void AGP_PlayerController::UpdateActiveMarquee(const FVector2D& CurrentScreenPosition)
{
	if (!IsLocalController() || !bMarqueeActive || SelectionComponent == nullptr)
	{
		return;
	}

	SelectionComponent->UpdateMarquee(CurrentScreenPosition);
	if (MarqueeWidget != nullptr)
	{
		MarqueeWidget->SetMarqueeRect(SelectionPressScreenPosition, CurrentScreenPosition);
	}
}

void AGP_PlayerController::CompleteActiveMarquee(const FVector2D& ReleaseScreenPosition)
{
	if (!IsLocalController())
	{
		return;
	}

	const FVector2D StartPosition = SelectionPressScreenPosition;
	UpdateActiveMarquee(ReleaseScreenPosition);
	ResolveAndApplyMarqueeSelection(StartPosition, ReleaseScreenPosition);

	if (SelectionComponent != nullptr)
	{
		SelectionComponent->EndMarquee();
	}

	HideMarqueeWidget();
	bMarqueeActive = false;
	bSelectionPressActive = false;
	SelectionPressScreenPosition = FVector2D::ZeroVector;
}

void AGP_PlayerController::CancelActiveMarquee(bool bLogCanceled)
{
	const bool bHadMarquee =
		bMarqueeActive
		|| (SelectionComponent != nullptr && SelectionComponent->IsMarqueeActive());

	if (SelectionComponent != nullptr && SelectionComponent->IsMarqueeActive())
	{
		SelectionComponent->CancelMarquee();
	}

	HideMarqueeWidget();
	bMarqueeActive = false;

	if (bLogCanceled && bHadMarquee)
	{
		UE_LOG(LogTemp, Verbose, TEXT("GP Marquee: Result=Canceled"));
	}
}

void AGP_PlayerController::EnsureMarqueeWidget()
{
	if (!IsLocalController())
	{
		return;
	}

	if (MarqueeWidget != nullptr)
	{
		if (!MarqueeWidget->IsInViewport())
		{
			MarqueeWidget->AddToViewport(MarqueeWidgetZOrder);
		}
		MarqueeWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	MarqueeWidget = CreateWidget<UGP_MarqueeSelectionWidget>(
		this,
		UGP_MarqueeSelectionWidget::StaticClass());
	if (MarqueeWidget == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::EnsureMarqueeWidget: CreateWidget failed."));
		return;
	}

	MarqueeWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	MarqueeWidget->AddToViewport(MarqueeWidgetZOrder);
}

void AGP_PlayerController::HideMarqueeWidget()
{
	if (MarqueeWidget != nullptr)
	{
		MarqueeWidget->ClearMarqueeRect();
	}
}

void AGP_PlayerController::DestroyMarqueeWidget()
{
	if (MarqueeWidget == nullptr)
	{
		return;
	}

	MarqueeWidget->ClearMarqueeRect();
	MarqueeWidget->RemoveFromParent();
	MarqueeWidget = nullptr;
}

void AGP_PlayerController::ResolveAndApplyMarqueeSelection(
	const FVector2D& ScreenStart,
	const FVector2D& ScreenEnd)
{
	if (!IsLocalController() || SelectionComponent == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const AGP_PlayerState* GPPlayerState = GetPlayerState<AGP_PlayerState>();
	if (GPPlayerState == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP Marquee: Result=BlockedUnassignedTeam LocalTeam=None"));
		return;
	}

	const int32 LocalTeamId = GPPlayerState->GetTeamId();
	if (LocalTeamId < 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP Marquee: LocalTeam=%d Rect=(%.1f,%.1f)-(%.1f,%.1f) Candidates=0 SelectedCount=%d Modifier=Replace Result=BlockedUnassignedTeam"),
			LocalTeamId,
			ScreenStart.X, ScreenStart.Y,
			ScreenEnd.X, ScreenEnd.Y,
			SelectionComponent->GetSelectionCount());
		return;
	}

	const float MinX = FMath::Min(ScreenStart.X, ScreenEnd.X);
	const float MaxX = FMath::Max(ScreenStart.X, ScreenEnd.X);
	const float MinY = FMath::Min(ScreenStart.Y, ScreenEnd.Y);
	const float MaxY = FMath::Max(ScreenStart.Y, ScreenEnd.Y);

	TArray<AGP_UnitBase*> EligibleUnits;
	for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
	{
		AGP_UnitBase* Unit = *It;
		if (!IsValid(Unit))
		{
			continue;
		}

		if (Unit->GetTeamId() != LocalTeamId || !Unit->IsGameplaySelectable())
		{
			continue;
		}

		FVector2D ProjectedScreen = FVector2D::ZeroVector;
		if (!ProjectWorldLocationToScreen(Unit->GetActorLocation(), ProjectedScreen, false))
		{
			continue;
		}

		if (ProjectedScreen.X < MinX || ProjectedScreen.X > MaxX
			|| ProjectedScreen.Y < MinY || ProjectedScreen.Y > MaxY)
		{
			continue;
		}

		EligibleUnits.Add(Unit);
	}

	Algo::Sort(EligibleUnits, [](const AGP_UnitBase* A, const AGP_UnitBase* B)
	{
		if (A == nullptr || B == nullptr)
		{
			return A != nullptr;
		}
		return A->GetPathName() < B->GetPathName();
	});

	const int32 CandidateCount = EligibleUnits.Num();
	const bool bCtrl = IsControlModifierDown();
	const bool bShift = !bCtrl && IsShiftModifierDown();
	const TCHAR* ModifierTag = TEXT("Replace");

	if (bCtrl)
	{
		ModifierTag = TEXT("Toggle");
		if (CandidateCount == 0)
		{
			UE_LOG(LogTemp, Log,
				TEXT("GP Marquee: LocalTeam=%d Rect=(%.1f,%.1f)-(%.1f,%.1f) Candidates=0 SelectedCount=%d Modifier=Toggle Result=NoOpEmpty"),
				LocalTeamId,
				MinX, MinY, MaxX, MaxY,
				SelectionComponent->GetSelectionCount());
			return;
		}

		TArray<TWeakObjectPtr<AGP_UnitBase>> FinalUnits = SelectionComponent->GetSelectedUnits();
		for (AGP_UnitBase* Candidate : EligibleUnits)
		{
			bool bAlreadySelected = false;
			for (int32 Index = FinalUnits.Num() - 1; Index >= 0; --Index)
			{
				if (FinalUnits[Index].Get() == Candidate)
				{
					FinalUnits.RemoveAt(Index);
					bAlreadySelected = true;
					break;
				}
			}

			if (!bAlreadySelected)
			{
				FinalUnits.Add(Candidate);
			}
		}

		if (SelectionComponent->GetInspectedTarget() != nullptr)
		{
			SelectionComponent->ClearInspectedTarget();
		}
		SelectionComponent->SetSelectionFromUnits(FinalUnits);
	}
	else if (bShift)
	{
		ModifierTag = TEXT("Add");
		if (CandidateCount == 0)
		{
			UE_LOG(LogTemp, Log,
				TEXT("GP Marquee: LocalTeam=%d Rect=(%.1f,%.1f)-(%.1f,%.1f) Candidates=0 SelectedCount=%d Modifier=Add Result=NoOpEmpty"),
				LocalTeamId,
				MinX, MinY, MaxX, MaxY,
				SelectionComponent->GetSelectionCount());
			return;
		}

		TArray<TWeakObjectPtr<AGP_UnitBase>> FinalUnits = SelectionComponent->GetSelectedUnits();
		for (AGP_UnitBase* Candidate : EligibleUnits)
		{
			bool bAlreadySelected = false;
			for (const TWeakObjectPtr<AGP_UnitBase>& Existing : FinalUnits)
			{
				if (Existing.Get() == Candidate)
				{
					bAlreadySelected = true;
					break;
				}
			}

			if (!bAlreadySelected)
			{
				FinalUnits.Add(Candidate);
			}
		}

		if (SelectionComponent->GetInspectedTarget() != nullptr)
		{
			SelectionComponent->ClearInspectedTarget();
		}
		SelectionComponent->SetSelectionFromUnits(FinalUnits);
	}
	else
	{
		TArray<TWeakObjectPtr<AGP_UnitBase>> FinalUnits;
		FinalUnits.Reserve(EligibleUnits.Num());
		for (AGP_UnitBase* Candidate : EligibleUnits)
		{
			FinalUnits.Add(Candidate);
		}

		if (SelectionComponent->GetInspectedTarget() != nullptr)
		{
			SelectionComponent->ClearInspectedTarget();
		}
		SelectionComponent->SetSelectionFromUnits(FinalUnits);
	}

	UE_LOG(LogTemp, Log,
		TEXT("GP Marquee: LocalTeam=%d Rect=(%.1f,%.1f)-(%.1f,%.1f) Candidates=%d SelectedCount=%d Modifier=%s Result=Applied"),
		LocalTeamId,
		MinX, MinY, MaxX, MaxY,
		CandidateCount,
		SelectionComponent->GetSelectionCount(),
		ModifierTag);
}

void AGP_PlayerController::ProcessSelectionClickAtScreenPosition(const FVector2D& ScreenPosition)
{
	if (!IsLocalController() || SelectionComponent == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const AGP_PlayerState* GPPlayerState = GetPlayerState<AGP_PlayerState>();
	if (GPPlayerState == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP Selection: Result=BlockedMissingPlayerState"));
		return;
	}

	const int32 LocalTeamId = GPPlayerState->GetTeamId();
	if (LocalTeamId < 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP Selection: Result=BlockedUnassignedTeam LocalTeam=%d"),
			LocalTeamId);
		return;
	}

	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	if (!DeprojectScreenPositionToWorld(
			ScreenPosition.X,
			ScreenPosition.Y,
			WorldOrigin,
			WorldDirection))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_PlayerController::ProcessSelectionClickAtScreenPosition: deproject failed."));
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GPSelectionClick), /*bTraceComplex=*/false);
	QueryParams.bReturnPhysicalMaterial = false;
	if (APawn* ControlledPawn = GetPawn())
	{
		QueryParams.AddIgnoredActor(ControlledPawn);
	}

	const FVector TraceEnd = WorldOrigin + (WorldDirection * SelectionTraceDistance);
	FHitResult Hit;
	World->LineTraceSingleByChannel(
		Hit,
		WorldOrigin,
		TraceEnd,
		ECC_Visibility,
		QueryParams);

	AActor* HitActor = Hit.GetActor();
	AGP_UnitBase* HitUnit = Cast<AGP_UnitBase>(HitActor);
	if (HitUnit == nullptr)
	{
		SelectionComponent->ClearAllSelectionState();
		LogSelectionClickResult(TEXT("Clear"), HitActor, /*bHasHitTeam=*/false, 0, LocalTeamId);
		return;
	}

	const int32 HitTeamId = HitUnit->GetTeamId();
	if (HitTeamId < 0)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("GP Selection: Result=BlockedUnassignedUnit Hit=%s HitTeam=%d"),
			*GetNameSafe(HitUnit), HitTeamId);
		LogSelectionClickResult(
			TEXT("BlockedUnassignedUnit"), HitUnit, /*bHasHitTeam=*/true, HitTeamId, LocalTeamId);
		return;
	}

	const bool bFriendly = (HitTeamId == LocalTeamId);
	const bool bNeutral = HitUnit->IsNeutral();
	const bool bSelectable = HitUnit->IsGameplaySelectable();
	const bool bInspectable = HitUnit->IsGameplayInspectable();

	if (bFriendly && bSelectable)
	{
		// Selection mutators do not clear inspect; clear first (may broadcast separately).
		if (SelectionComponent->GetInspectedTarget() != nullptr)
		{
			SelectionComponent->ClearInspectedTarget();
		}

		const TCHAR* ResultTag = TEXT("Replace");
		if (IsControlModifierDown())
		{
			SelectionComponent->ToggleUnitSelection(HitUnit);
			ResultTag = TEXT("Toggle");
		}
		else if (IsShiftModifierDown())
		{
			SelectionComponent->AddUnitToSelection(HitUnit);
			ResultTag = TEXT("Add");
		}
		else
		{
			SelectionComponent->ReplaceSelectionWithUnit(HitUnit);
		}

		LogSelectionClickResult(ResultTag, HitUnit, /*bHasHitTeam=*/true, HitTeamId, LocalTeamId);
		return;
	}

	if (bFriendly && !bSelectable)
	{
		if (bInspectable)
		{
			SelectionComponent->SetInspectedTarget(HitUnit);
			LogSelectionClickResult(
				TEXT("Inspect"), HitUnit, /*bHasHitTeam=*/true, HitTeamId, LocalTeamId);
		}
		else
		{
			LogSelectionClickResult(
				TEXT("NoOp"), HitUnit, /*bHasHitTeam=*/true, HitTeamId, LocalTeamId);
		}
		return;
	}

	// Enemy (assigned, not local) or Neutral
	if ((bNeutral || HitTeamId != LocalTeamId) && bInspectable)
	{
		SelectionComponent->SetInspectedTarget(HitUnit);
		LogSelectionClickResult(
			TEXT("Inspect"), HitUnit, /*bHasHitTeam=*/true, HitTeamId, LocalTeamId);
		return;
	}

	LogSelectionClickResult(TEXT("NoOp"), HitUnit, /*bHasHitTeam=*/true, HitTeamId, LocalTeamId);
}

void AGP_PlayerController::LogSelectionClickResult(
	const TCHAR* ResultTag,
	const AActor* HitActor,
	bool bHasHitTeam,
	int32 HitTeamId,
	int32 LocalTeamId) const
{
	if (SelectionComponent == nullptr)
	{
		return;
	}

	const AActor* Inspected = SelectionComponent->GetInspectedTarget();
	const FString HitTeamText = bHasHitTeam ? FString::FromInt(HitTeamId) : FString(TEXT("None"));

	UE_LOG(LogTemp, Log,
		TEXT("GP Selection: LocalTeam=%d Hit=%s HitTeam=%s SelectedCount=%d Inspected=%s Result=%s"),
		LocalTeamId,
		*GetNameSafe(HitActor),
		*HitTeamText,
		SelectionComponent->GetSelectionCount(),
		*GetNameSafe(Inspected),
		ResultTag);
}

bool AGP_PlayerController::IsControlModifierDown() const
{
	return IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl);
}

bool AGP_PlayerController::IsShiftModifierDown() const
{
	return IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
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
