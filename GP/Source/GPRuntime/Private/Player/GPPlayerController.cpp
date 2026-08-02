// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GPPlayerController.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Camera/GPCameraPawn.h"
#include "CollisionQueryParams.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Units/GPUnitBase.h"

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

	SelectionMappingContext = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Selection/IMC_GP_Selection.IMC_GP_Selection")));
	SelectionAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(
		TEXT("/Game/GrimProtocol/Input/Selection/IA_Select.IA_Select")));
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
	bSelectionPressActive = false;
	SelectionPressScreenPosition = FVector2D::ZeroVector;

	if (AGP_CameraPawn* CameraPawn = GetCameraPawn())
	{
		CameraPawn->SetRotateActive(false);
	}

	RemoveSelectionInputMapping();
	RemoveCameraInputMapping();

	LoadedSelectionMappingContext = nullptr;
	LoadedSelectionAction = nullptr;

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
	InitializeSelectionInput();
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

	if (!bSelectionInputBindingsInstalled)
	{
		LoadSelectionInputAssets();
		BindSelectionInputActions(*EnhancedInputComponent);
		bSelectionInputBindingsInstalled = true;
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

void AGP_PlayerController::OnSelectionStarted(const FInputActionValue& Value)
{
	(void)Value;

	if (!IsLocalController())
	{
		return;
	}

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

	const FVector2D PressPosition = SelectionPressScreenPosition;
	bSelectionPressActive = false;
	SelectionPressScreenPosition = FVector2D::ZeroVector;

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	const FVector2D ReleasePosition(MouseX, MouseY);
	const float PixelDistance = FVector2D::Distance(PressPosition, ReleasePosition);
	if (PixelDistance > SelectionDragThresholdPixels)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("GP Selection: Result=DragDeferredToB2b Distance=%.1f Threshold=%.1f"),
			PixelDistance, SelectionDragThresholdPixels);
		return;
	}

	ProcessSelectionClickAtScreenPosition(ReleasePosition);
}

void AGP_PlayerController::OnSelectionCanceled(const FInputActionValue& Value)
{
	(void)Value;

	bSelectionPressActive = false;
	SelectionPressScreenPosition = FVector2D::ZeroVector;
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
