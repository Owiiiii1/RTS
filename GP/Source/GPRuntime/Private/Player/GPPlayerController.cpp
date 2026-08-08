// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GPPlayerController.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
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
#include "GameplayEffectTypes.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "UI/GPMarqueeSelectionWidget.h"
#include "UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h"
#include "Buildings/GPMainBase.h"
#include "Game/GPGameState.h"
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPBuildingPlacementGhost.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPDropPod.h"
#include "Resources/GPStorageComponent.h"
#include "Units/GPUnitBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPCommandInput, Log, All);

namespace GPCommandServerPrivate
{
	static const TCHAR* RejectReasonToString(EGP_CommandRejectReason Reason)
	{
		switch (Reason)
		{
		case EGP_CommandRejectReason::None:
			return TEXT("None");
		case EGP_CommandRejectReason::InvalidController:
			return TEXT("InvalidController");
		case EGP_CommandRejectReason::InvalidPlayerState:
			return TEXT("InvalidPlayerState");
		case EGP_CommandRejectReason::InvalidRequestingTeam:
			return TEXT("InvalidRequestingTeam");
		case EGP_CommandRejectReason::InvalidCommandTag:
			return TEXT("InvalidCommandTag");
		case EGP_CommandRejectReason::UnsupportedCommandTag:
			return TEXT("UnsupportedCommandTag");
		case EGP_CommandRejectReason::NoCommandableUnits:
			return TEXT("NoCommandableUnits");
		case EGP_CommandRejectReason::InvalidTarget:
			return TEXT("InvalidTarget");
		case EGP_CommandRejectReason::FriendlyAttackTarget:
			return TEXT("FriendlyAttackTarget");
		case EGP_CommandRejectReason::InvalidResourceTarget:
			return TEXT("InvalidResourceTarget");
		case EGP_CommandRejectReason::InvalidTargetLocation:
			return TEXT("InvalidTargetLocation");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* NetModeToString(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}
}

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
	CancelBuildingPlacement();
	DestroyMarqueeWidget();
	DestroyPlanetaryFerroniteHUD();

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

	if (bBuildingPlacementActive)
	{
		UpdateBuildingPlacementGhost();

		const bool bRMBDown = IsInputKeyDown(EKeys::RightMouseButton);
		if (bRMBDown && !bBuildingPlacementRMBWasDown)
		{
			CancelBuildingPlacement();
		}
		bBuildingPlacementRMBWasDown = bRMBDown;
	}
	else
	{
		bBuildingPlacementRMBWasDown = false;
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

	EnsurePlanetaryFerroniteHUD();
	RefreshPlanetaryFerroniteHUDBinding();
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

	if (IsLocalController())
	{
		EnsurePlanetaryFerroniteHUD();
		RefreshPlanetaryFerroniteHUDBinding();
	}
}

void AGP_PlayerController::OnAbilitySystemLinkReady(UGP_AbilitySystemComponent* InAbilitySystemComponent)
{
	UE_LOG(LogTemp, Log,
		TEXT("AGP_PlayerController::OnAbilitySystemLinkReady: ASC linked (%s)."),
		*GetNameSafe(InAbilitySystemComponent));

	if (IsLocalController())
	{
		EnsurePlanetaryFerroniteHUD();
		BindOrbitalFerroniteAttribute();
		SyncOrbitalFerroniteHUDFromAttributes();
	}
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

	Server_RequestCommand(Request);
}

void AGP_PlayerController::Server_RequestCommand_Implementation(const FGP_CommandRequest& Request)
{
	if (CommandComponent == nullptr)
	{
		UE_LOG(LogGPCommandServer, Warning,
			TEXT("GP CommandServer Rejected: PC=%s Team=invalid Reason=InvalidController Tag=%s ReceivedUnits=%d NetMode=%s"),
			*GetName(),
			*Request.CommandTag.ToString(),
			Request.IssuingUnits.Num(),
			GPCommandServerPrivate::NetModeToString(GetNetMode()));
		return;
	}

	const int32 ReceivedUnits = Request.IssuingUnits.Num();
	FGP_CommandRequest ValidatedRequest;
	EGP_CommandRejectReason RejectReason = EGP_CommandRejectReason::None;

	if (!CommandComponent->ValidateAndNormalizeCommand(Request, ValidatedRequest, RejectReason))
	{
		const AGP_PlayerState* GPPlayerState = GetPlayerState<AGP_PlayerState>();
		const FString TeamText = GPPlayerState != nullptr
			? FString::FromInt(GPPlayerState->GetTeamId())
			: FString(TEXT("invalid"));

		UE_LOG(LogGPCommandServer, Log,
			TEXT("GP CommandServer Rejected: PC=%s Team=%s Reason=%s Tag=%s ReceivedUnits=%d NetMode=%s"),
			*GetName(),
			*TeamText,
			GPCommandServerPrivate::RejectReasonToString(RejectReason),
			*Request.CommandTag.ToString(),
			ReceivedUnits,
			GPCommandServerPrivate::NetModeToString(GetNetMode()));
		return;
	}

	const AGP_PlayerState* GPPlayerState = GetPlayerState<AGP_PlayerState>();
	const int32 TeamId = GPPlayerState != nullptr ? GPPlayerState->GetTeamId() : -1;

	UE_LOG(LogGPCommandServer, Log,
		TEXT("GP CommandServer Accepted: PC=%s Team=%d Tag=%s ReceivedUnits=%d AcceptedUnits=%d TargetActor=%s Loc=%s Queue=%s NetMode=%s"),
		*GetName(),
		TeamId,
		*ValidatedRequest.CommandTag.ToString(),
		ReceivedUnits,
		ValidatedRequest.IssuingUnits.Num(),
		*GetNameSafe(ValidatedRequest.TargetActor),
		*ValidatedRequest.TargetLocation.ToCompactString(),
		ValidatedRequest.bQueue ? TEXT("true") : TEXT("false"),
		GPCommandServerPrivate::NetModeToString(GetNetMode()));

	const int32 RequestedUnits = ValidatedRequest.IssuingUnits.Num();
	const int32 DeliveredUnits = CommandComponent->DispatchValidatedCommand(ValidatedRequest);
	const int32 SkippedUnits = RequestedUnits - DeliveredUnits;

	if (DeliveredUnits == 0)
	{
		UE_LOG(LogGPCommandServer, Warning,
			TEXT("GP CommandDispatch: PC=%s Team=%d Tag=%s RequestedUnits=%d DeliveredUnits=0 SkippedUnits=%d TargetActor=%s Queue=%s NetMode=%s"),
			*GetName(),
			TeamId,
			*ValidatedRequest.CommandTag.ToString(),
			RequestedUnits,
			SkippedUnits,
			*GetNameSafe(ValidatedRequest.TargetActor),
			ValidatedRequest.bQueue ? TEXT("true") : TEXT("false"),
			GPCommandServerPrivate::NetModeToString(GetNetMode()));
	}
	else
	{
		UE_LOG(LogGPCommandServer, Log,
			TEXT("GP CommandDispatch: PC=%s Team=%d Tag=%s RequestedUnits=%d DeliveredUnits=%d SkippedUnits=%d TargetActor=%s Queue=%s NetMode=%s"),
			*GetName(),
			TeamId,
			*ValidatedRequest.CommandTag.ToString(),
			RequestedUnits,
			DeliveredUnits,
			SkippedUnits,
			*GetNameSafe(ValidatedRequest.TargetActor),
			ValidatedRequest.bQueue ? TEXT("true") : TEXT("false"),
			GPCommandServerPrivate::NetModeToString(GetNetMode()));
	}
}

void AGP_PlayerController::RequestLaunchReadyContainer()
{
	if (!IsLocalController())
	{
		return;
	}

	Server_RequestLaunchReadyContainer();
}

void AGP_PlayerController::RequestUnitDrop(const FGP_UnitDropManifest& Manifest)
{
	if (!IsLocalController())
	{
		return;
	}

	Server_RequestUnitDrop(Manifest);
}

bool AGP_PlayerController::Server_RequestUnitDrop_Validate(const FGP_UnitDropManifest& Manifest)
{
	(void)Manifest;
	return true;
}

void AGP_PlayerController::Server_RequestUnitDrop_Implementation(const FGP_UnitDropManifest& Manifest)
{
	AuthorityTryRequestUnitDrop(Manifest);
}

bool AGP_PlayerController::AuthorityTryRequestUnitDrop(const FGP_UnitDropManifest& Manifest)
{
	if (!HasAuthority())
	{
		return false;
	}

	AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	if (PS == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP UnitDrop Rejected: PC=%s Reason=MissingPlayerState"),
			*GetName());
		return false;
	}

	const GPUnitDropAuthority::FEvalResult Result =
		GPUnitDropAuthority::AuthorityRequestUnitDrop(GetWorld(), PS, Manifest);
	UE_LOG(LogTemp, Log,
		TEXT("GP UnitDrop Result: PC=%s Team=%d Accepted=%s Reason=%d Slots=%d Cost=%.3f Units=%d Pod=%s"),
		*GetName(),
		PS->GetTeamId(),
		Result.bAccepted ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Result.RejectReason),
		Result.SlotCost,
		Result.OrbitalCost,
		Result.UnitCount,
		*GetNameSafe(Result.SpawnedPod.Get()));
	return Result.bAccepted;
}

void AGP_PlayerController::RequestBuildingPurchase(EGP_OrbitalBuildingType BuildingType)
{
	if (!IsLocalController())
	{
		return;
	}
	Server_RequestBuildingPurchase(BuildingType);
}

bool AGP_PlayerController::Server_RequestBuildingPurchase_Validate(EGP_OrbitalBuildingType BuildingType)
{
	(void)BuildingType;
	return true;
}

void AGP_PlayerController::Server_RequestBuildingPurchase_Implementation(EGP_OrbitalBuildingType BuildingType)
{
	AuthorityTryPurchaseBuilding(BuildingType);
}

bool AGP_PlayerController::AuthorityTryPurchaseBuilding(EGP_OrbitalBuildingType BuildingType)
{
	if (!HasAuthority())
	{
		return false;
	}

	AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	if (PS == nullptr)
	{
		return false;
	}

	const GPBuildingDropAuthority::FPurchaseResult Result =
		GPBuildingDropAuthority::AuthorityPurchaseBuilding(GetWorld(), PS, BuildingType);
	UE_LOG(LogTemp, Log,
		TEXT("GP BuildingPurchase Result: PC=%s Team=%d Accepted=%s Reason=%d Cost=%.3f Ready=%d"),
		*GetName(),
		PS->GetTeamId(),
		Result.bAccepted ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Result.RejectReason),
		Result.OrbitalCost,
		Result.ReadyAfter);
	return Result.bAccepted;
}

void AGP_PlayerController::RequestBuildingDeploy(EGP_OrbitalBuildingType BuildingType, const FTransform& WorldTransform)
{
	if (!IsLocalController())
	{
		return;
	}
	Server_RequestBuildingDeploy(BuildingType, WorldTransform);
}

bool AGP_PlayerController::Server_RequestBuildingDeploy_Validate(
	EGP_OrbitalBuildingType BuildingType,
	const FTransform& WorldTransform)
{
	(void)BuildingType;
	const FVector Loc = WorldTransform.GetLocation();
	return !Loc.ContainsNaN()
		&& FMath::IsFinite(Loc.X) && FMath::IsFinite(Loc.Y) && FMath::IsFinite(Loc.Z);
}

void AGP_PlayerController::Server_RequestBuildingDeploy_Implementation(
	EGP_OrbitalBuildingType BuildingType,
	const FTransform& WorldTransform)
{
	AuthorityTryDeployBuilding(BuildingType, WorldTransform);
}

bool AGP_PlayerController::AuthorityTryDeployBuilding(
	EGP_OrbitalBuildingType BuildingType,
	const FTransform& WorldTransform)
{
	if (!HasAuthority())
	{
		return false;
	}

	AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	if (PS == nullptr)
	{
		return false;
	}

	const float OrbitalBefore = PS->GetPlayerAttributeSet() != nullptr
		? PS->GetPlayerAttributeSet()->GetOrbitalFerronite()
		: 0.0f;

	const GPBuildingDropAuthority::FDeployResult Result =
		GPBuildingDropAuthority::AuthorityDeployBuilding(GetWorld(), PS, BuildingType, WorldTransform);

	const float OrbitalAfter = PS->GetPlayerAttributeSet() != nullptr
		? PS->GetPlayerAttributeSet()->GetOrbitalFerronite()
		: 0.0f;

	UE_LOG(LogTemp, Log,
		TEXT("GP BuildingDeploy Result: PC=%s Team=%d Accepted=%s Reason=%d Ready=%d Pod=%s OrbitalUnchanged=%s"),
		*GetName(),
		PS->GetTeamId(),
		Result.bAccepted ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Result.RejectReason),
		Result.ReadyAfter,
		*GetNameSafe(Result.SpawnedPod.Get()),
		FMath::IsNearlyEqual(OrbitalBefore, OrbitalAfter, 0.05f) ? TEXT("true") : TEXT("false"));
	return Result.bAccepted;
}

void AGP_PlayerController::EnterBuildingPlacementMode(EGP_OrbitalBuildingType BuildingType)
{
	if (!IsLocalController() || BuildingType == EGP_OrbitalBuildingType::None)
	{
		return;
	}

	AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	UGP_OrbitalBuildingInventoryComponent* Inventory =
		PS != nullptr ? PS->GetOrbitalBuildingInventoryComponent() : nullptr;
	if (Inventory == nullptr || Inventory->GetReadyCount(BuildingType) <= 0)
	{
		return;
	}

	ActiveBuildingPlacementType = BuildingType;
	bBuildingPlacementActive = true;
	bBuildingPlacementRMBWasDown = IsInputKeyDown(EKeys::RightMouseButton);

	if (BuildingPlacementGhost == nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = this;
		BuildingPlacementGhost = GetWorld()->SpawnActor<AGP_BuildingPlacementGhost>(
			AGP_BuildingPlacementGhost::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			Params);
	}

	if (BuildingPlacementGhost != nullptr)
	{
		BuildingPlacementGhost->SetGhostVisible(true);
		UpdateBuildingPlacementGhost();
	}
}

void AGP_PlayerController::CancelBuildingPlacement()
{
	if (!IsLocalController())
	{
		return;
	}

	bBuildingPlacementActive = false;
	ActiveBuildingPlacementType = EGP_OrbitalBuildingType::None;
	DestroyBuildingPlacementGhost();
}

void AGP_PlayerController::ConfirmBuildingPlacement()
{
	if (!IsLocalController() || !bBuildingPlacementActive)
	{
		return;
	}

	FVector GroundLoc = FVector::ZeroVector;
	FRotator GroundRot = FRotator::ZeroRotator;
	if (!TraceGroundUnderCursor(GroundLoc, GroundRot))
	{
		return;
	}

	const EGP_OrbitalBuildingType Type = ActiveBuildingPlacementType;
	const FTransform DeployTransform(GroundRot, GroundLoc);
	CancelBuildingPlacement();
	RequestBuildingDeploy(Type, DeployTransform);
}

bool AGP_PlayerController::TraceGroundUnderCursor(FVector& OutGroundLocation, FRotator& OutGroundRotation) const
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return false;
	}

	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	if (!DeprojectScreenPositionToWorld(MouseX, MouseY, WorldOrigin, WorldDirection))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GPBuildingPlacementTrace), false);
	QueryParams.bReturnPhysicalMaterial = false;
	if (APawn* ControlledPawn = GetPawn())
	{
		QueryParams.AddIgnoredActor(ControlledPawn);
	}
	if (BuildingPlacementGhost != nullptr)
	{
		QueryParams.AddIgnoredActor(BuildingPlacementGhost);
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
		return false;
	}

	OutGroundLocation = Hit.ImpactPoint;
	OutGroundRotation = FRotator(0.0f, GetControlRotation().Yaw, 0.0f);
	return true;
}

void AGP_PlayerController::UpdateBuildingPlacementGhost()
{
	if (!bBuildingPlacementActive || BuildingPlacementGhost == nullptr)
	{
		return;
	}

	FVector GroundLoc = FVector::ZeroVector;
	FRotator GroundRot = FRotator::ZeroRotator;
	if (TraceGroundUnderCursor(GroundLoc, GroundRot))
	{
		BuildingPlacementGhost->UpdateGhostTransform(FTransform(GroundRot, GroundLoc));
	}
}

void AGP_PlayerController::DestroyBuildingPlacementGhost()
{
	if (BuildingPlacementGhost != nullptr)
	{
		BuildingPlacementGhost->Destroy();
		BuildingPlacementGhost = nullptr;
	}
}

void AGP_PlayerController::BindBuildingInventoryEvents()
{
	UnbindBuildingInventoryEvents();

	AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	if (PS == nullptr)
	{
		SyncBuildingReadyHUDFromInventory();
		return;
	}

	if (UGP_OrbitalBuildingInventoryComponent* Inventory = PS->GetOrbitalBuildingInventoryComponent())
	{
		BuildingReadyChangedHandle = Inventory->OnReadyChanged.AddUObject(
			this,
			&AGP_PlayerController::HandleBuildingReadyChanged);
	}
	SyncBuildingReadyHUDFromInventory();
}

void AGP_PlayerController::UnbindBuildingInventoryEvents()
{
	AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	if (PS != nullptr)
	{
		if (UGP_OrbitalBuildingInventoryComponent* Inventory = PS->GetOrbitalBuildingInventoryComponent())
		{
			if (BuildingReadyChangedHandle.IsValid())
			{
				Inventory->OnReadyChanged.Remove(BuildingReadyChangedHandle);
			}
		}
	}
	BuildingReadyChangedHandle.Reset();
}

void AGP_PlayerController::SyncBuildingReadyHUDFromInventory()
{
	EnsurePlanetaryFerroniteHUD();
	if (PlanetaryFerroniteHUD == nullptr)
	{
		return;
	}

	AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	int32 ReadyLogisticsHub = 0;
	if (PS != nullptr)
	{
		if (UGP_OrbitalBuildingInventoryComponent* Inventory = PS->GetOrbitalBuildingInventoryComponent())
		{
			ReadyLogisticsHub = Inventory->GetReadyCount(EGP_OrbitalBuildingType::LogisticsHub);
		}
	}
	PlanetaryFerroniteHUD->SetBuildingReadyDisplay(ReadyLogisticsHub);
}

void AGP_PlayerController::HandleBuildingReadyChanged(EGP_OrbitalBuildingType BuildingType, int32 NewReadyCount)
{
	(void)BuildingType;
	EnsurePlanetaryFerroniteHUD();
	if (PlanetaryFerroniteHUD != nullptr)
	{
		PlanetaryFerroniteHUD->SetBuildingReadyDisplay(NewReadyCount);
	}
}

bool AGP_PlayerController::Server_RequestLaunchReadyContainer_Validate()
{
	// Intent-only RPC (no client payload). Authority revalidates ownership/Ready/ASC.
	return true;
}

void AGP_PlayerController::Server_RequestLaunchReadyContainer_Implementation()
{
	AuthorityTryLaunchReadyContainerForOwningTeam();
}

bool AGP_PlayerController::AuthorityTryLaunchReadyContainerForOwningTeam()
{
	if (!HasAuthority())
	{
		return false;
	}

	const AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	if (PS == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP LaunchRequest Rejected: PC=%s Reason=MissingPlayerState"),
			*GetName());
		return false;
	}

	const int32 TeamId = PS->GetTeamId();
	if (TeamId < 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP LaunchRequest Rejected: PC=%s Reason=InvalidTeamId Team=%d"),
			*GetName(),
			TeamId);
		return false;
	}

	UWorld* World = GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	if (GS == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP LaunchRequest Rejected: PC=%s Team=%d Reason=MissingGameState"),
			*GetName(),
			TeamId);
		return false;
	}

	AGP_MainBase* MainBase = GS->FindMainBaseForTeam(TeamId);
	if (!IsValid(MainBase))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP LaunchRequest Rejected: PC=%s Team=%d Reason=MissingMainBase"),
			*GetName(),
			TeamId);
		return false;
	}

	UGP_StorageComponent* Storage = MainBase->GetStorageComponent();
	if (!IsValid(Storage))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP LaunchRequest Rejected: PC=%s Team=%d Reason=MissingStorage"),
			*GetName(),
			TeamId);
		return false;
	}

	const FGP_ContainerLaunchResult Result = Storage->TryLaunchReadyContainer();
	UE_LOG(LogTemp, Log,
		TEXT("GP LaunchRequest Result: PC=%s Team=%d Base=%s Accepted=%s Reason=%d Index=%d Amount=%.3f"),
		*GetName(),
		TeamId,
		*GetNameSafe(MainBase),
		Result.bAccepted ? TEXT("true") : TEXT("false"),
		static_cast<int32>(Result.RejectReason),
		Result.ContainerIndex,
		Result.LaunchedPlanetaryAmount);
	return Result.bAccepted;
}

void AGP_PlayerController::OnSelectionStarted(const FInputActionValue& Value)
{
	(void)Value;

	if (!IsLocalController())
	{
		return;
	}

	if (bBuildingPlacementActive)
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

	if (bBuildingPlacementActive)
	{
		const float PixelDistance =
			FVector2D::Distance(SelectionPressScreenPosition, ReleasePosition);
		if (PixelDistance <= SelectionDragThresholdPixels)
		{
			ConfirmBuildingPlacement();
		}
		bSelectionPressActive = false;
		SelectionPressScreenPosition = FVector2D::ZeroVector;
		return;
	}

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

	if (bBuildingPlacementActive)
	{
		CancelBuildingPlacement();
	}

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

void AGP_PlayerController::EnsurePlanetaryFerroniteHUD()
{
	if (!IsLocalController())
	{
		return;
	}

	if (PlanetaryFerroniteHUD != nullptr)
	{
		if (!PlanetaryFerroniteHUD->IsInViewport())
		{
			PlanetaryFerroniteHUD->AddToViewport(PlanetaryFerroniteHUDZOrder);
		}
		// Visibility owned by HUD NativeConstruct (SelfHitTestInvisible + Visible button).
		return;
	}

	PlanetaryFerroniteHUD = CreateWidget<UGP_TEMP_S28P_PlanetaryFerroniteHUD>(
		this,
		UGP_TEMP_S28P_PlanetaryFerroniteHUD::StaticClass());
	if (PlanetaryFerroniteHUD == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_PlayerController::EnsurePlanetaryFerroniteHUD: CreateWidget failed."));
		return;
	}

	PlanetaryFerroniteHUD->AddToViewport(PlanetaryFerroniteHUDZOrder);
	PlanetaryFerroniteHUD->SetStorageDisplay(false, 0.0f, 0.0f, TArray<float>());
	PlanetaryFerroniteHUD->SetOrbitalFerroniteDisplay(0.0f);
	PlanetaryFerroniteHUD->SetLaunchButtonEnabled(false);
}

void AGP_PlayerController::DestroyPlanetaryFerroniteHUD()
{
	ClearPlanetaryFerroniteHUDBindings();
	if (PlanetaryFerroniteHUD == nullptr)
	{
		return;
	}
	PlanetaryFerroniteHUD->RemoveFromParent();
	PlanetaryFerroniteHUD = nullptr;
}

void AGP_PlayerController::ClearPlanetaryFerroniteHUDBindings()
{
	UnbindPlanetaryFerroniteStorage();
	UnbindOrbitalFerroniteAttribute();
	UnbindBuildingInventoryEvents();

	if (AGP_GameState* GS = BoundPlanetaryGameState.Get())
	{
		if (ResolvedMainBaseChangedHandle.IsValid())
		{
			GS->OnResolvedMainBaseChanged.Remove(ResolvedMainBaseChangedHandle);
		}
	}
	ResolvedMainBaseChangedHandle.Reset();
	BoundPlanetaryGameState.Reset();

	if (AGP_PlayerState* PS = BoundPlanetaryPlayerState.Get())
	{
		if (PlayerTeamIdChangedHandle.IsValid())
		{
			PS->OnTeamIdChanged.Remove(PlayerTeamIdChangedHandle);
		}
	}
	PlayerTeamIdChangedHandle.Reset();
	BoundPlanetaryPlayerState.Reset();
	BoundPlanetaryTeamId = -1;
}

void AGP_PlayerController::UnbindPlanetaryFerroniteStorage()
{
	if (UGP_StorageComponent* Storage = BoundPlanetaryStorage.Get())
	{
		Storage->OnStorageChanged.RemoveDynamic(this, &AGP_PlayerController::HandleStorageChangedForHUD);
	}
	BoundPlanetaryStorage.Reset();
}

void AGP_PlayerController::BindPlanetaryFerroniteStorage(AGP_MainBase* MainBase)
{
	UnbindPlanetaryFerroniteStorage();
	UGP_StorageComponent* Storage = IsValid(MainBase) ? MainBase->GetStorageComponent() : nullptr;
	if (!IsValid(Storage))
	{
		SyncPlanetaryFerroniteHUDFromStorage();
		SyncLaunchButtonFromStorage();
		return;
	}

	BoundPlanetaryStorage = Storage;
	Storage->OnStorageChanged.AddDynamic(this, &AGP_PlayerController::HandleStorageChangedForHUD);
	SyncPlanetaryFerroniteHUDFromStorage();
	SyncLaunchButtonFromStorage();
}

void AGP_PlayerController::SyncPlanetaryFerroniteHUDFromStorage()
{
	EnsurePlanetaryFerroniteHUD();
	if (PlanetaryFerroniteHUD == nullptr)
	{
		return;
	}

	if (UGP_StorageComponent* Storage = BoundPlanetaryStorage.Get())
	{
		TArray<float> Amounts;
		const TArray<FGP_StorageContainer>& Containers = Storage->GetContainers();
		Amounts.Reserve(Containers.Num());
		for (const FGP_StorageContainer& Container : Containers)
		{
			Amounts.Add(Container.CurrentAmount);
		}
		PlanetaryFerroniteHUD->SetStorageDisplay(
			true,
			Storage->GetTotalStored(),
			Storage->GetTotalCapacity(),
			Amounts);
	}
	else
	{
		PlanetaryFerroniteHUD->SetStorageDisplay(false, 0.0f, 0.0f, TArray<float>());
	}
}

void AGP_PlayerController::SyncLaunchButtonFromStorage()
{
	EnsurePlanetaryFerroniteHUD();
	if (PlanetaryFerroniteHUD == nullptr)
	{
		return;
	}

	bool bEnabled = false;
	if (UGP_StorageComponent* Storage = BoundPlanetaryStorage.Get())
	{
		bEnabled = Storage->GetReadyCount() > 0 && !Storage->IsLaunchInFlight();
	}
	PlanetaryFerroniteHUD->SetLaunchButtonEnabled(bEnabled);
}

void AGP_PlayerController::UnbindOrbitalFerroniteAttribute()
{
	if (UGP_AbilitySystemComponent* ASC = BoundOrbitalASC.Get())
	{
		if (OrbitalFerroniteChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(
				UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute()).Remove(OrbitalFerroniteChangedHandle);
		}
	}
	OrbitalFerroniteChangedHandle.Reset();
	BoundOrbitalASC.Reset();
}

void AGP_PlayerController::BindOrbitalFerroniteAttribute()
{
	UnbindOrbitalFerroniteAttribute();

	AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	UGP_AbilitySystemComponent* ASC = PS != nullptr ? PS->GetGPAbilitySystemComponent() : nullptr;
	if (ASC == nullptr || PS->GetPlayerAttributeSet() == nullptr)
	{
		SyncOrbitalFerroniteHUDFromAttributes();
		return;
	}

	BoundOrbitalASC = ASC;
	OrbitalFerroniteChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute()).AddUObject(
		this, &AGP_PlayerController::HandleOrbitalFerroniteAttributeChanged);
	SyncOrbitalFerroniteHUDFromAttributes();
}

void AGP_PlayerController::SyncOrbitalFerroniteHUDFromAttributes()
{
	EnsurePlanetaryFerroniteHUD();
	if (PlanetaryFerroniteHUD == nullptr)
	{
		return;
	}

	float Orbital = 0.0f;
	if (const AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>())
	{
		if (const UGP_PlayerAttributeSet* AttrSet = PS->GetPlayerAttributeSet())
		{
			Orbital = AttrSet->GetOrbitalFerronite();
		}
	}
	PlanetaryFerroniteHUD->SetOrbitalFerroniteDisplay(Orbital);
}

void AGP_PlayerController::HandleOrbitalFerroniteAttributeChanged(const FOnAttributeChangeData& Data)
{
	EnsurePlanetaryFerroniteHUD();
	if (PlanetaryFerroniteHUD != nullptr)
	{
		PlanetaryFerroniteHUD->SetOrbitalFerroniteDisplay(Data.NewValue);
	}
}

void AGP_PlayerController::HandleStorageChangedForHUD(
	float PreviousTotalStored,
	float NewTotalStored,
	float TotalCapacity)
{
	(void)PreviousTotalStored;
	(void)NewTotalStored;
	(void)TotalCapacity;
	SyncPlanetaryFerroniteHUDFromStorage();
	SyncLaunchButtonFromStorage();
}

void AGP_PlayerController::HandleResolvedMainBaseChanged(
	int32 TeamId,
	AGP_MainBase* PreviousMainBase,
	AGP_MainBase* NewMainBase)
{
	(void)PreviousMainBase;
	if (TeamId != BoundPlanetaryTeamId || BoundPlanetaryTeamId < 1)
	{
		return;
	}
	BindPlanetaryFerroniteStorage(NewMainBase);
}

void AGP_PlayerController::HandlePlayerTeamIdChanged(int32 OldTeamId, int32 NewTeamId)
{
	(void)OldTeamId;
	(void)NewTeamId;
	RefreshPlanetaryFerroniteHUDBinding();
}

void AGP_PlayerController::RefreshPlanetaryFerroniteHUDBinding()
{
	if (!IsLocalController())
	{
		return;
	}

	EnsurePlanetaryFerroniteHUD();

	AGP_PlayerState* PS = GetPlayerState<AGP_PlayerState>();
	UWorld* World = GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;

	if (BoundPlanetaryPlayerState.Get() != PS)
	{
		if (AGP_PlayerState* OldPS = BoundPlanetaryPlayerState.Get())
		{
			if (PlayerTeamIdChangedHandle.IsValid())
			{
				OldPS->OnTeamIdChanged.Remove(PlayerTeamIdChangedHandle);
			}
		}
		PlayerTeamIdChangedHandle.Reset();
		BoundPlanetaryPlayerState = PS;
		if (IsValid(PS))
		{
			PlayerTeamIdChangedHandle = PS->OnTeamIdChanged.AddUObject(
				this, &AGP_PlayerController::HandlePlayerTeamIdChanged);
		}
	}

	if (BoundPlanetaryGameState.Get() != GS)
	{
		if (AGP_GameState* OldGS = BoundPlanetaryGameState.Get())
		{
			if (ResolvedMainBaseChangedHandle.IsValid())
			{
				OldGS->OnResolvedMainBaseChanged.Remove(ResolvedMainBaseChangedHandle);
			}
		}
		ResolvedMainBaseChangedHandle.Reset();
		BoundPlanetaryGameState = GS;
		if (IsValid(GS))
		{
			ResolvedMainBaseChangedHandle = GS->OnResolvedMainBaseChanged.AddUObject(
				this, &AGP_PlayerController::HandleResolvedMainBaseChanged);
		}
	}

	BoundPlanetaryTeamId = IsValid(PS) ? PS->GetTeamId() : -1;
	AGP_MainBase* Base = (IsValid(GS) && BoundPlanetaryTeamId >= 1)
		? GS->FindMainBaseForTeamClientSafe(BoundPlanetaryTeamId)
		: nullptr;
	BindPlanetaryFerroniteStorage(Base);
	BindOrbitalFerroniteAttribute();
	BindBuildingInventoryEvents();
	SyncOrbitalFerroniteHUDFromAttributes();
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
