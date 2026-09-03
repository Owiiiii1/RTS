// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/GPCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Camera/GPCameraBoundsVolume.h"
#include "Camera/GPCameraConfigDataAsset.h"
#include "Components/SceneComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"

namespace GPCameraPawnPrivate
{
	static void ClampVector2DMagnitude(FVector2D& Value, float MaxMagnitude)
	{
		const float MaxSquared = MaxMagnitude * MaxMagnitude;
		if (Value.SizeSquared() > MaxSquared)
		{
			Value = Value.GetSafeNormal() * MaxMagnitude;
		}
	}
}

AGP_CameraPawn::AGP_CameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	bOnlyRelevantToOwner = true;
	SetReplicateMovement(false);

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);
	RootScene->SetRelativeRotation(FRotator::ZeroRotator);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootScene);
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;
	SpringArm->TargetArmLength = 2500.0f;
	SpringArm->SetRelativeRotation(FRotator(-45.0f, 0.0f, 0.0f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
}

AGP_CameraPawn::~AGP_CameraPawn() = default;

void AGP_CameraPawn::BeginPlay()
{
	Super::BeginPlay();

	const UGP_CameraConfigDataAsset* Config = GetActiveConfig();
	if (Config == nullptr || RootScene == nullptr || SpringArm == nullptr)
	{
		return;
	}

	CurrentYaw = RootScene->GetRelativeRotation().Yaw;
	CurrentArmLength = Config->DefaultArmLength;
	TargetArmLength = CurrentArmLength;
	SpringArm->TargetArmLength = CurrentArmLength;

	ApplyPitch(*Config);
	FindCameraBoundsVolume();
	ClampToBounds(*Config);
	SyncSpringArmPresentation(0.0f);
	NotifyResolvedCameraBoundsChanged();
	NotifyCameraPresentationChangedIfNeeded();

	if (!ConfigRef.IsNull())
	{
		BeginLoadConfig();
	}
}

void AGP_CameraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ConfigLoadHandle.IsValid())
	{
		if (ConfigLoadHandle->IsLoadingInProgress())
		{
			ConfigLoadHandle->CancelHandle();
		}

		ConfigLoadHandle.Reset();
	}

	CachedConfig = nullptr;
	ResetFrameInput();

	Super::EndPlay(EndPlayReason);
}

void AGP_CameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocallyControlled())
	{
		ResetFrameInput();
		return;
	}

	const UGP_CameraConfigDataAsset* Config = GetActiveConfig();
	if (Config == nullptr)
	{
		ResetFrameInput();
		return;
	}

	const FVector2D EdgeInput = CalculateEdgeScrollInput();
	ApplyZoom(*Config, DeltaSeconds);
	ApplyPitch(*Config);
	ApplyRotation(*Config);
	ApplyPan(*Config, EdgeInput, DeltaSeconds);
	ClampToBounds(*Config);
	SyncSpringArmPresentation(DeltaSeconds);
	NotifyCameraPresentationChangedIfNeeded();
	ResetFrameInput();
}

void AGP_CameraPawn::SetPanInput(const FVector2D& AxisXY)
{
	PendingPanInput.X = FMath::Clamp(AxisXY.X, -1.0f, 1.0f);
	PendingPanInput.Y = FMath::Clamp(AxisXY.Y, -1.0f, 1.0f);
	GPCameraPawnPrivate::ClampVector2DMagnitude(PendingPanInput, 1.0f);
}

void AGP_CameraPawn::AddZoomInput(float WheelDelta)
{
	PendingZoomInput += WheelDelta;
}

void AGP_CameraPawn::AddRotateInput(float MouseDeltaX)
{
	PendingRotateInput += MouseDeltaX;
}

void AGP_CameraPawn::SetRotateActive(bool bActive)
{
	bRotateActive = bActive;
}

const UGP_CameraConfigDataAsset* AGP_CameraPawn::GetActiveConfig() const
{
	if (CachedConfig != nullptr)
	{
		return CachedConfig;
	}

	return GetDefault<UGP_CameraConfigDataAsset>();
}

void AGP_CameraPawn::BeginLoadConfig()
{
	if (ConfigLoadHandle.IsValid())
	{
		return;
	}

	if (ConfigRef.IsNull())
	{
		return;
	}

	const FSoftObjectPath SoftPath = ConfigRef.ToSoftObjectPath();
	ConfigLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		SoftPath,
		FStreamableDelegate::CreateUObject(this, &AGP_CameraPawn::HandleConfigLoaded));

	if (!ConfigLoadHandle.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_CameraPawn::BeginLoadConfig: RequestAsyncLoad failed for '%s'. Keeping CDO fallback."),
			*SoftPath.ToString());
	}
}

void AGP_CameraPawn::HandleConfigLoaded()
{
	const UGP_CameraConfigDataAsset* OldConfig = GetActiveConfig();
	float CurrentFraction = 0.0f;
	float TargetFraction = 0.0f;
	bool bOldRangeValid = false;

	if (OldConfig != nullptr)
	{
		const float OldRange = OldConfig->MaxArmLength - OldConfig->MinArmLength;
		bOldRangeValid = OldRange > UE_SMALL_NUMBER;
		if (bOldRangeValid)
		{
			CurrentFraction = CalculateZoomFraction(*OldConfig, CurrentArmLength);
			TargetFraction = CalculateZoomFraction(*OldConfig, TargetArmLength);
		}
	}

	UGP_CameraConfigDataAsset* LoadedConfig = ConfigRef.Get();
	if (LoadedConfig == nullptr)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_CameraPawn::HandleConfigLoaded: failed to resolve '%s'. Keeping CDO fallback."),
			*ConfigRef.ToSoftObjectPath().ToString());
		ConfigLoadHandle.Reset();
		return;
	}

	CachedConfig = LoadedConfig;

	if (bOldRangeValid)
	{
		CurrentArmLength = FMath::Lerp(LoadedConfig->MinArmLength, LoadedConfig->MaxArmLength, CurrentFraction);
		TargetArmLength = FMath::Lerp(LoadedConfig->MinArmLength, LoadedConfig->MaxArmLength, TargetFraction);
	}
	else
	{
		CurrentArmLength = LoadedConfig->DefaultArmLength;
		TargetArmLength = LoadedConfig->DefaultArmLength;
	}

	CurrentArmLength = FMath::Clamp(CurrentArmLength, LoadedConfig->MinArmLength, LoadedConfig->MaxArmLength);
	TargetArmLength = FMath::Clamp(TargetArmLength, LoadedConfig->MinArmLength, LoadedConfig->MaxArmLength);
	if (SpringArm != nullptr)
	{
		SpringArm->TargetArmLength = CurrentArmLength;
	}

	ConfigLoadHandle.Reset();
	NotifyResolvedCameraBoundsChanged();
	SyncSpringArmPresentation(0.0f);
	NotifyCameraPresentationChangedIfNeeded();
}

float AGP_CameraPawn::CalculateZoomFraction(
	const UGP_CameraConfigDataAsset& Config,
	float ArmLength) const
{
	const float Range = Config.MaxArmLength - Config.MinArmLength;
	if (Range <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp((ArmLength - Config.MinArmLength) / Range, 0.0f, 1.0f);
}

float AGP_CameraPawn::CalculateEdgeStrength(
	float DistanceToEdge,
	int32 ThresholdPx,
	int32 FalloffPx) const
{
	ThresholdPx = FMath::Max(ThresholdPx, 0);
	FalloffPx = FMath::Max(FalloffPx, 0);

	if (DistanceToEdge <= static_cast<float>(ThresholdPx))
	{
		return 1.0f;
	}

	if (FalloffPx == 0)
	{
		return 0.0f;
	}

	const float FalloffStart = static_cast<float>(ThresholdPx);
	const float FalloffEnd = FalloffStart + static_cast<float>(FalloffPx);
	if (DistanceToEdge >= FalloffEnd)
	{
		return 0.0f;
	}

	const float Strength = 1.0f - ((DistanceToEdge - FalloffStart) / static_cast<float>(FalloffPx));
	return FMath::Clamp(Strength, 0.0f, 1.0f);
}

FVector2D AGP_CameraPawn::CalculateEdgeScrollInput() const
{
	const UGP_CameraConfigDataAsset* Config = GetActiveConfig();
	if (Config == nullptr || !Config->bEdgeScrollEnabled)
	{
		return FVector2D::ZeroVector;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController == nullptr || !PlayerController->IsLocalController())
	{
		return FVector2D::ZeroVector;
	}

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0)
	{
		return FVector2D::ZeroVector;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!PlayerController->GetMousePosition(MouseX, MouseY))
	{
		return FVector2D::ZeroVector;
	}

	const float LeftDistance = MouseX;
	const float RightDistance = static_cast<float>(ViewportWidth) - MouseX;
	const float TopDistance = MouseY;
	const float BottomDistance = static_cast<float>(ViewportHeight) - MouseY;

	const float LeftStrength = CalculateEdgeStrength(LeftDistance, Config->EdgeThresholdPx, Config->EdgeFalloffPx);
	const float RightStrength = CalculateEdgeStrength(RightDistance, Config->EdgeThresholdPx, Config->EdgeFalloffPx);
	const float TopStrength = CalculateEdgeStrength(TopDistance, Config->EdgeThresholdPx, Config->EdgeFalloffPx);
	const float BottomStrength = CalculateEdgeStrength(BottomDistance, Config->EdgeThresholdPx, Config->EdgeFalloffPx);

	FVector2D Result(RightStrength - LeftStrength, TopStrength - BottomStrength);
	GPCameraPawnPrivate::ClampVector2DMagnitude(Result, 1.0f);
	return Result;
}

void AGP_CameraPawn::ApplyZoom(const UGP_CameraConfigDataAsset& Config, float DeltaSeconds)
{
	if (SpringArm == nullptr)
	{
		return;
	}

	TargetArmLength -= PendingZoomInput * Config.ZoomStep;
	TargetArmLength = FMath::Clamp(TargetArmLength, Config.MinArmLength, Config.MaxArmLength);

	CurrentArmLength = FMath::FInterpTo(
		CurrentArmLength,
		TargetArmLength,
		DeltaSeconds,
		Config.ZoomInterpSpeed);
	CurrentArmLength = FMath::Clamp(CurrentArmLength, Config.MinArmLength, Config.MaxArmLength);

	SpringArm->TargetArmLength = CurrentArmLength;
}

void AGP_CameraPawn::SyncSpringArmPresentation(float DeltaSeconds)
{
	if (SpringArm == nullptr)
	{
		return;
	}

	SpringArm->TickComponent(DeltaSeconds, LEVELTICK_All, nullptr);
}

void AGP_CameraPawn::ApplyPitch(const UGP_CameraConfigDataAsset& Config)
{
	if (SpringArm == nullptr)
	{
		return;
	}

	float Pitch = Config.PitchAtMinZoom;
	if (Config.bPitchInterpEnabled)
	{
		const float ZoomFraction = CalculateZoomFraction(Config, CurrentArmLength);
		Pitch = FMath::Lerp(Config.PitchAtMinZoom, Config.PitchAtMaxZoom, ZoomFraction);
	}

	SpringArm->SetRelativeRotation(FRotator(Pitch, 0.0f, 0.0f));
}

void AGP_CameraPawn::ApplyRotation(const UGP_CameraConfigDataAsset& Config)
{
	if (!bRotateActive || RootScene == nullptr)
	{
		return;
	}

	float RotationDelta = PendingRotateInput * Config.RotateSpeed;
	if (Config.bInvertRotate)
	{
		RotationDelta *= -1.0f;
	}

	CurrentYaw = FMath::UnwindDegrees(CurrentYaw + RotationDelta);
	RootScene->SetRelativeRotation(FRotator(0.0f, CurrentYaw, 0.0f));
}

void AGP_CameraPawn::ApplyPan(
	const UGP_CameraConfigDataAsset& Config,
	const FVector2D& EdgeInput,
	float DeltaSeconds)
{
	FVector2D TargetPanInput = PendingPanInput + EdgeInput * Config.EdgeScrollSpeed;
	GPCameraPawnPrivate::ClampVector2DMagnitude(TargetPanInput, 1.0f);

	const bool bAccelerating = TargetPanInput.SizeSquared() > SmoothedPanInput.SizeSquared();
	const float InterpTime = bAccelerating ? Config.MoveAccelTime : Config.MoveDecelTime;

	if (InterpTime <= UE_SMALL_NUMBER)
	{
		SmoothedPanInput = TargetPanInput;
	}
	else
	{
		const float InterpSpeed = 1.0f / InterpTime;
		SmoothedPanInput = FMath::Vector2DInterpTo(SmoothedPanInput, TargetPanInput, DeltaSeconds, InterpSpeed);
	}

	GPCameraPawnPrivate::ClampVector2DMagnitude(SmoothedPanInput, 1.0f);

	const FRotator YawRotation(0.0f, CurrentYaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector PanWorld = Forward * SmoothedPanInput.Y + Right * SmoothedPanInput.X;

	const float ZoomFraction = CalculateZoomFraction(Config, CurrentArmLength);
	const float EffectivePanSpeed = Config.PanSpeed * (1.0f + Config.ZoomPanScale * ZoomFraction);

	FVector NewLocation = GetActorLocation();
	const float PreservedZ = NewLocation.Z;
	NewLocation += PanWorld * EffectivePanSpeed * DeltaSeconds;
	NewLocation.Z = PreservedZ;

	SetActorLocation(NewLocation, false);
}

void AGP_CameraPawn::FindCameraBoundsVolume()
{
	CameraBoundsVolume.Reset();

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	int32 VolumeCount = 0;
	for (TActorIterator<AGP_CameraBoundsVolume> It(World); It; ++It)
	{
		AGP_CameraBoundsVolume* Volume = *It;
		if (Volume == nullptr)
		{
			continue;
		}

		++VolumeCount;
		if (VolumeCount == 1)
		{
			CameraBoundsVolume = Volume;
		}
	}

	if (VolumeCount > 1)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Found multiple GP_CameraBoundsVolume actors. Using the first actor found. The level should contain at most one."));
	}
}

FBox AGP_CameraPawn::ResolveCameraBounds(const UGP_CameraConfigDataAsset& Config) const
{
	if (CameraBoundsVolume.IsValid())
	{
		const FBox VolumeBounds = CameraBoundsVolume->GetCameraBounds();
		if (IsUsableResolvedCameraBounds(VolumeBounds))
		{
			return VolumeBounds;
		}

		if (!bInvalidCameraBoundsWarningLogged)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("AGP_CameraPawn: CameraBoundsVolume '%s' returned invalid FBox. Using Config FallbackBounds."),
				*GetNameSafe(CameraBoundsVolume.Get()));
			bInvalidCameraBoundsWarningLogged = true;
		}
	}

	return Config.FallbackBounds;
}

bool AGP_CameraPawn::IsUsableResolvedCameraBounds(const FBox& Bounds)
{
	return Bounds.IsValid
		&& FMath::IsFinite(Bounds.Min.X) && FMath::IsFinite(Bounds.Max.X)
		&& FMath::IsFinite(Bounds.Min.Y) && FMath::IsFinite(Bounds.Max.Y)
		&& FMath::IsFinite(Bounds.Min.Z) && FMath::IsFinite(Bounds.Max.Z)
		&& Bounds.Min.X < Bounds.Max.X
		&& Bounds.Min.Y < Bounds.Max.Y
		&& Bounds.Min.Z < Bounds.Max.Z;
}

bool AGP_CameraPawn::GetResolvedCameraBounds(FBox& OutBounds) const
{
	const UGP_CameraConfigDataAsset* Config = GetActiveConfig();
	if (Config == nullptr)
	{
		OutBounds = FBox(ForceInit);
		return false;
	}

	OutBounds = ResolveCameraBounds(*Config);
	if (!IsUsableResolvedCameraBounds(OutBounds))
	{
		OutBounds = FBox(ForceInit);
		return false;
	}

	return true;
}

void AGP_CameraPawn::NotifyResolvedCameraBoundsChanged()
{
	OnResolvedCameraBoundsChanged.Broadcast();
}

float AGP_CameraPawn::GetGroundReferencePlaneZ() const
{
	const FVector Location = GetActorLocation();
	return FMath::IsFinite(Location.Z) ? Location.Z : 0.0f;
}

bool AGP_CameraPawn::SetCameraAnchorWorldXY(const FVector2D& TargetXY)
{
	if (!FMath::IsFinite(TargetXY.X) || !FMath::IsFinite(TargetXY.Y))
	{
		return false;
	}

	const FVector CurrentLocation = GetActorLocation();
	const float PreservedZ = FMath::IsFinite(CurrentLocation.Z) ? CurrentLocation.Z : 0.0f;
	SetActorLocation(FVector(TargetXY.X, TargetXY.Y, PreservedZ), false);

	if (const UGP_CameraConfigDataAsset* Config = GetActiveConfig())
	{
		ClampToBounds(*Config);
	}

	const FVector ClampedLocation = GetActorLocation();
	if (FMath::IsFinite(PreservedZ) && !FMath::IsNearlyEqual(ClampedLocation.Z, PreservedZ))
	{
		SetActorLocation(FVector(ClampedLocation.X, ClampedLocation.Y, PreservedZ), false);
	}

	SyncSpringArmPresentation(0.0f);
	if (RootScene != nullptr)
	{
		RootScene->UpdateComponentToWorld();
	}
	if (SpringArm != nullptr)
	{
		SpringArm->UpdateComponentToWorld();
	}
	if (Camera != nullptr)
	{
		Camera->UpdateComponentToWorld();
	}
	NotifyCameraPresentationChangedIfNeeded();
	return true;
}

bool AGP_CameraPawn::GetPresentationView(FVector& OutLocation, FRotator& OutRotation, float& OutFOV) const
{
	if (Camera == nullptr)
	{
		OutLocation = GetActorLocation();
		OutRotation = GetActorRotation();
		OutFOV = 90.0f;
		return false;
	}

	OutLocation = Camera->GetComponentLocation();
	OutRotation = Camera->GetComponentRotation();
	OutFOV = Camera->FieldOfView;
	return !OutLocation.ContainsNaN()
		&& FMath::IsFinite(OutRotation.Pitch)
		&& FMath::IsFinite(OutRotation.Yaw)
		&& FMath::IsFinite(OutRotation.Roll)
		&& FMath::IsFinite(OutFOV)
		&& OutFOV > KINDA_SMALL_NUMBER;
}

bool AGP_CameraPawn::CaptureCameraPresentationFingerprint(
	FVector& OutActorLocation,
	float& OutYaw,
	float& OutArmLength,
	float& OutPitch,
	FVector& OutCameraLocation,
	FRotator& OutCameraRotation,
	int32& OutViewportX,
	int32& OutViewportY) const
{
	OutActorLocation = GetActorLocation();
	OutYaw = CurrentYaw;
	OutArmLength = CurrentArmLength;
	OutPitch = SpringArm != nullptr ? SpringArm->GetRelativeRotation().Pitch : 0.0f;
	OutCameraLocation = Camera != nullptr ? Camera->GetComponentLocation() : OutActorLocation;
	OutCameraRotation = Camera != nullptr ? Camera->GetComponentRotation() : FRotator::ZeroRotator;
	OutViewportX = 0;
	OutViewportY = 0;

	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		PlayerController->GetViewportSize(OutViewportX, OutViewportY);
	}

	return FMath::IsFinite(OutActorLocation.X)
		&& FMath::IsFinite(OutActorLocation.Y)
		&& FMath::IsFinite(OutActorLocation.Z)
		&& FMath::IsFinite(OutYaw)
		&& FMath::IsFinite(OutArmLength)
		&& FMath::IsFinite(OutPitch)
		&& !OutCameraLocation.ContainsNaN()
		&& FMath::IsFinite(OutCameraRotation.Pitch)
		&& FMath::IsFinite(OutCameraRotation.Yaw)
		&& FMath::IsFinite(OutCameraRotation.Roll);
}

void AGP_CameraPawn::NotifyCameraPresentationChangedIfNeeded()
{
	FVector ActorLocation = FVector::ZeroVector;
	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	float Yaw = 0.0f;
	float ArmLength = 0.0f;
	float Pitch = 0.0f;
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	if (!CaptureCameraPresentationFingerprint(
			ActorLocation,
			Yaw,
			ArmLength,
			Pitch,
			CameraLocation,
			CameraRotation,
			ViewportX,
			ViewportY))
	{
		return;
	}

	const bool bChanged =
		!bHasCameraPresentationFingerprint
		|| !ActorLocation.Equals(LastPresentationActorLocation, 0.05f)
		|| !FMath::IsNearlyEqual(Yaw, LastPresentationYaw, 0.01f)
		|| !FMath::IsNearlyEqual(ArmLength, LastPresentationArmLength, 0.05f)
		|| !FMath::IsNearlyEqual(Pitch, LastPresentationPitch, 0.01f)
		|| !CameraLocation.Equals(LastPresentationCameraLocation, 0.05f)
		|| !CameraRotation.Equals(LastPresentationCameraRotation, 0.01f)
		|| ViewportX != LastPresentationViewportX
		|| ViewportY != LastPresentationViewportY;

	if (!bChanged)
	{
		return;
	}

	LastPresentationActorLocation = ActorLocation;
	LastPresentationCameraLocation = CameraLocation;
	LastPresentationCameraRotation = CameraRotation;
	LastPresentationYaw = Yaw;
	LastPresentationArmLength = ArmLength;
	LastPresentationPitch = Pitch;
	LastPresentationViewportX = ViewportX;
	LastPresentationViewportY = ViewportY;
	bHasCameraPresentationFingerprint = true;
	OnCameraPresentationChanged.Broadcast();
}

#if !UE_BUILD_SHIPPING
void AGP_CameraPawn::ContractSetCameraBoundsVolume(AGP_CameraBoundsVolume* Volume)
{
	CameraBoundsVolume = Volume;
	bInvalidCameraBoundsWarningLogged = false;
	NotifyResolvedCameraBoundsChanged();
}

void AGP_CameraPawn::ContractClearCameraBoundsVolume()
{
	CameraBoundsVolume.Reset();
	bInvalidCameraBoundsWarningLogged = false;
	NotifyResolvedCameraBoundsChanged();
}

void AGP_CameraPawn::ContractNotifyCameraPresentationChanged()
{
	bHasCameraPresentationFingerprint = false;
	NotifyCameraPresentationChangedIfNeeded();
}
#endif

void AGP_CameraPawn::ClampToBounds(const UGP_CameraConfigDataAsset& Config)
{
	const FBox Bounds = ResolveCameraBounds(Config);
	if (!Bounds.IsValid)
	{
		return;
	}

	const FVector Location = GetActorLocation();
	const FVector ClampedLocation(
		FMath::Clamp(Location.X, Bounds.Min.X, Bounds.Max.X),
		FMath::Clamp(Location.Y, Bounds.Min.Y, Bounds.Max.Y),
		FMath::Clamp(Location.Z, Bounds.Min.Z, Bounds.Max.Z));

	if (!Location.Equals(ClampedLocation))
	{
		SetActorLocation(ClampedLocation, false);
	}
}

void AGP_CameraPawn::ResetFrameInput()
{
	PendingPanInput = FVector2D::ZeroVector;
	PendingZoomInput = 0.0f;
	PendingRotateInput = 0.0f;
}
