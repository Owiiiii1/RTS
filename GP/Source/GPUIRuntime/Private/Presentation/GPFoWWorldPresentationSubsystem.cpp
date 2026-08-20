// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPFoWWorldPresentationSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Scene.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "CoreGlobals.h"
#include "Player/GPPlayerController.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "RHI.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFoWWorldPresentation, Log, All);

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarFoWVisualDebugMode(
	TEXT("gp.FoW.VisualDebugMode"),
	0,
	TEXT("0=normal FoW composition. 1=fullscreen diagnostic tint proving post-process execution."),
	ECVF_Default);
#endif

class FGP_FoWSceneViewExtension final : public FSceneViewExtensionBase
{
public:
	FGP_FoWSceneViewExtension(
		const FAutoRegister& AutoRegister,
		UGP_FoWWorldPresentationSubsystem* InOwner)
		: FSceneViewExtensionBase(AutoRegister)
		, Owner(InOwner)
	{
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override
	{
		(void)InViewFamily;
	}

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
	{
		(void)InViewFamily;
		UGP_FoWWorldPresentationSubsystem* Presentation = Owner.Get();
		if (Presentation == nullptr)
		{
			return;
		}

		const AActor* ViewActor = InView.ViewActor;
		const FString ViewDebugName = FString::Printf(
			TEXT("PlayerIndex=%d ViewActor=%s GameView=%d SceneCapture=%d"),
			InView.PlayerIndex,
			*GetNameSafe(ViewActor),
			InView.bIsGameView ? 1 : 0,
			InView.bIsSceneCapture ? 1 : 0);
		Presentation->TryInjectOwnedView(
			InView.FinalPostProcessSettings,
			InView.PlayerIndex,
			ViewActor,
			InView.bIsGameView,
			InView.bIsSceneCapture,
			InView.bIsReflectionCapture,
			GFrameCounter,
			*ViewDebugName);
	}

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
	{
		(void)InViewFamily;
	}

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		const UGP_FoWWorldPresentationSubsystem* Presentation = Owner.Get();
		return Presentation != nullptr
			&& Context.GetWorld() != nullptr
			&& Context.GetWorld() == Presentation->GetWorld();
	}

private:
	TWeakObjectPtr<UGP_FoWWorldPresentationSubsystem> Owner;
};

void UGP_FoWWorldPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EnsureMaskResources(
		GPFoWVisualMask::CanonicalMaskResolution,
		GPFoWVisualMask::CanonicalMaskResolution);
	ViewExtension = FSceneViewExtensions::NewExtension<FGP_FoWSceneViewExtension>(this);

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PlayerController =
		LocalPlayer != nullptr ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	BindToPlayerController(PlayerController);
}

void UGP_FoWWorldPresentationSubsystem::Deinitialize()
{
	ViewExtension.Reset();
	UnbindLocalCameraBlendable();
	UnbindMirror();
	ReleaseMaskResources();
	Super::Deinitialize();
}

void UGP_FoWWorldPresentationSubsystem::PlayerControllerChanged(
	APlayerController* NewPlayerController)
{
	BindToPlayerController(NewPlayerController);
}

void UGP_FoWWorldPresentationSubsystem::Tick(float DeltaTime)
{
	RefreshLocalCameraBlendable();
	if (!bVisualizationEnabled)
	{
		return;
	}

	GPFoWVisualMask::AdvanceBlend(MaskRuntime, DeltaTime);
	UpdateMaterialParameters();
}

TStatId UGP_FoWWorldPresentationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGP_FoWWorldPresentationSubsystem, STATGROUP_Tickables);
}

bool UGP_FoWWorldPresentationSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return !HasAnyFlags(RF_ClassDefaultObject)
		&& World != nullptr
		&& World->IsGameWorld();
}

ETickableTickType UGP_FoWWorldPresentationSubsystem::GetTickableTickType() const
{
	return ETickableTickType::Conditional;
}

float UGP_FoWWorldPresentationSubsystem::GetObscurationForState(EGP_FoWState State)
{
	return GPFoWVisualMask::ObscurationForState(State);
}

FLinearColor UGP_FoWWorldPresentationSubsystem::ComposeVisualSceneColor(
	const FLinearColor& SceneColor,
	float Known,
	float Visible,
	bool bReady)
{
	return GPFoWVisualMask::ComposeSceneColor(SceneColor, Known, Visible, bReady);
}

bool UGP_FoWWorldPresentationSubsystem::RequiresConservativeFullObscuration(
	const UGP_LocalFoWComponent* Mirror)
{
	return Mirror == nullptr || !Mirror->IsReady();
}

bool UGP_FoWWorldPresentationSubsystem::IsRendererActive() const
{
	return bVisualizationEnabled && IsPostProcessBound();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetRendererName()
{
	return GPFoWVisualMask::GetRendererName();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetMaskModelName()
{
	return GPFoWVisualMask::GetMaskModelName();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetSpatialFilterName()
{
	return GPFoWVisualMask::GetSpatialFilterName();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetTemporalFilterName()
{
	return GPFoWVisualMask::GetTemporalFilterName();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetMaterialAssetPath()
{
	return GPFoWVisualMask::GetMaterialAssetPath();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetWorldPositionMethodName()
{
	return GPFoWVisualMask::GetWorldPositionMethodName();
}

int32 UGP_FoWWorldPresentationSubsystem::GetCanonicalMaskResolution()
{
	return GPFoWVisualMask::CanonicalMaskResolution;
}

float UGP_FoWWorldPresentationSubsystem::GetBlendDurationSeconds()
{
	return GPFoWVisualMask::BlendDurationSeconds;
}

float UGP_FoWWorldPresentationSubsystem::GetBlurRadiusTexels()
{
	return GPFoWVisualMask::BlurRadiusTexels;
}

float UGP_FoWWorldPresentationSubsystem::GetExploredDimFactor()
{
	return GPFoWVisualMask::ExploredDimFactor;
}

bool UGP_FoWWorldPresentationSubsystem::UsesCpuSpatialBlur()
{
	return GPFoWVisualMask::UsesCpuSpatialBlur();
}

bool UGP_FoWWorldPresentationSubsystem::UsesCpuTemporalLerp()
{
	return GPFoWVisualMask::UsesCpuTemporalLerp();
}

bool UGP_FoWWorldPresentationSubsystem::UsesWorldLocationQueriesForEncode()
{
	return GPFoWVisualMask::UsesWorldLocationQueriesForEncode();
}

int32 UGP_FoWWorldPresentationSubsystem::GetMaskTextureResolution() const
{
	return MaskWidth;
}

int32 UGP_FoWWorldPresentationSubsystem::GetMaskBytesPerTexture() const
{
	return MaskWidth * MaskHeight * GPFoWVisualMask::BytesPerPackedTexel();
}

bool UGP_FoWWorldPresentationSubsystem::IsPostProcessBound() const
{
	return bVisualizationEnabled
		&& PostProcessMID != nullptr
		&& BlendableInjectionCount > 0;
}

int32 UGP_FoWWorldPresentationSubsystem::GetVisualDebugMode() const
{
#if !UE_BUILD_SHIPPING
	if (bHasForcedVisualDebugMode)
	{
		return ForcedVisualDebugMode;
	}
	return CVarFoWVisualDebugMode.GetValueOnGameThread();
#else
	return 0;
#endif
}

float UGP_FoWWorldPresentationSubsystem::GetMidDebugModeValue() const
{
	if (PostProcessMID == nullptr)
	{
		return 0.0f;
	}

	return PostProcessMID->K2_GetScalarParameterValue(TEXT("FoWDebugMode"));
}

void UGP_FoWWorldPresentationSubsystem::SetVisualizationEnabled(bool bEnabled)
{
	if (bVisualizationEnabled == bEnabled)
	{
		return;
	}

	bVisualizationEnabled = bEnabled;
	RefreshLocalCameraBlendable();
	UpdateMaterialParameters();
}

void UGP_FoWWorldPresentationSubsystem::DebugAdvanceBlend(float DeltaSeconds)
{
	GPFoWVisualMask::AdvanceBlend(MaskRuntime, DeltaSeconds);
	UpdateMaterialParameters();
}

void UGP_FoWWorldPresentationSubsystem::DebugSetVisualDebugMode(int32 Mode)
{
	ForcedVisualDebugMode = Mode;
	bHasForcedVisualDebugMode = true;
#if !UE_BUILD_SHIPPING
	if (IConsoleVariable* DebugModeCVar = CVarFoWVisualDebugMode.AsVariable())
	{
		DebugModeCVar->Set(Mode);
	}
#endif
	UpdateMaterialParameters();
}

bool UGP_FoWWorldPresentationSubsystem::DebugPingPongUploadPackedMask(
	const TArray<FColor>& Pixels,
	int32 Width,
	int32 Height)
{
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		return false;
	}

	EnsureMaskResources(Width, Height);
	Swap(PreviousMaskTexture, TargetMaskTexture);
	UploadTargetMask(Pixels);
	UpdateMaterialParameters();
	return PreviousMaskTexture != nullptr
		&& TargetMaskTexture != nullptr
		&& PreviousMaskTexture != TargetMaskTexture;
}

void UGP_FoWWorldPresentationSubsystem::BindToPlayerController(
	APlayerController* NewPlayerController)
{
	UnbindMirror();
	EnsureMaskResources(
		GPFoWVisualMask::CanonicalMaskResolution,
		GPFoWVisualMask::CanonicalMaskResolution);

	AGP_PlayerController* GPPlayerController = Cast<AGP_PlayerController>(NewPlayerController);
	if (GPPlayerController == nullptr || !GPPlayerController->IsLocalController())
	{
		LastUpdateRevision = -1;
		ApplyConservativeBlackMask();
		RefreshLocalCameraBlendable();
		return;
	}

	UGP_LocalFoWComponent* Mirror = GPPlayerController->GetLocalFogOfWarComponent();
	if (Mirror == nullptr)
	{
		LastUpdateRevision = -1;
		ApplyConservativeBlackMask();
		RefreshLocalCameraBlendable();
		return;
	}

	BoundMirror = Mirror;
	MirrorUpdatedHandle = Mirror->OnLocalFoWUpdated.AddUObject(
		this,
		&ThisClass::HandleLocalFoWUpdated);
	HandleLocalFoWUpdated(Mirror);
	RefreshLocalCameraBlendable();
}

void UGP_FoWWorldPresentationSubsystem::UnbindMirror()
{
	if (BoundMirror.IsValid() && MirrorUpdatedHandle.IsValid())
	{
		BoundMirror->OnLocalFoWUpdated.Remove(MirrorUpdatedHandle);
	}
	MirrorUpdatedHandle.Reset();
	BoundMirror.Reset();
}

void UGP_FoWWorldPresentationSubsystem::EnsureMaskResources(int32 Width, int32 Height)
{
	const int32 ClampedWidth = FMath::Max(Width, 1);
	const int32 ClampedHeight = FMath::Max(Height, 1);
	const bool bSizeChanged = MaskWidth != ClampedWidth || MaskHeight != ClampedHeight;
	MaskWidth = ClampedWidth;
	MaskHeight = ClampedHeight;

	if (bSizeChanged || PreviousMaskTexture == nullptr)
	{
		PreviousMaskTexture = CreateMaskTexture(TEXT("GPFoWPreviousMask"), MaskWidth, MaskHeight);
		FillTextureBlack(PreviousMaskTexture, MaskWidth, MaskHeight);
	}
	if (bSizeChanged || TargetMaskTexture == nullptr)
	{
		TargetMaskTexture = CreateMaskTexture(TEXT("GPFoWTargetMask"), MaskWidth, MaskHeight);
		FillTextureBlack(TargetMaskTexture, MaskWidth, MaskHeight);
	}

	if (TemplateMaterial == nullptr)
	{
		TemplateMaterial = LoadObject<UMaterialInterface>(
			nullptr,
			GPFoWVisualMask::GetMaterialAssetPath());
		if (TemplateMaterial == nullptr)
		{
			UE_LOG(LogGPFoWWorldPresentation, Error,
				TEXT("Failed to load FoW post-process material %s."),
				GPFoWVisualMask::GetMaterialAssetPath());
		}
	}

	if (PostProcessMID == nullptr && TemplateMaterial != nullptr)
	{
		PostProcessMID = UMaterialInstanceDynamic::Create(TemplateMaterial, this);
	}

	UpdateMaterialParameters();
}

void UGP_FoWWorldPresentationSubsystem::ReleaseMaskResources()
{
	UnbindLocalCameraBlendable();
	PreviousMaskTexture = nullptr;
	TargetMaskTexture = nullptr;
	PostProcessMID = nullptr;
	TemplateMaterial = nullptr;
	PackedTargetPixels.Reset();
	GPFoWVisualMask::ResetRuntime(MaskRuntime);
}

UTexture2D* UGP_FoWWorldPresentationSubsystem::CreateMaskTexture(const TCHAR* Name, int32 Width, int32 Height) const
{
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, Name);
	if (Texture == nullptr)
	{
		return nullptr;
	}

	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;
	Texture->CompressionSettings = TC_VectorDisplacementmap;
	Texture->UpdateResource();
	return Texture;
}

void UGP_FoWWorldPresentationSubsystem::UploadTexture(
	UTexture2D* Texture,
	const TArray<FColor>& Pixels,
	int32 Width,
	int32 Height)
{
	if (Texture == nullptr || Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		return;
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);
	uint8* Copy = static_cast<uint8*>(FMemory::Malloc(Pixels.Num() * sizeof(FColor)));
	FMemory::Memcpy(Copy, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Texture->UpdateTextureRegions(
		0,
		1,
		Region,
		Width * sizeof(FColor),
		sizeof(FColor),
		Copy,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			delete Regions;
			FMemory::Free(SrcData);
		});
}

void UGP_FoWWorldPresentationSubsystem::FillTextureBlack(UTexture2D* Texture, int32 Width, int32 Height)
{
	TArray<FColor> Black;
	Black.Init(FColor(0, 0, 0, 255), Width * Height);
	UploadTexture(Texture, Black, Width, Height);
}

void UGP_FoWWorldPresentationSubsystem::UploadTargetMask(const TArray<FColor>& Pixels)
{
	const double UploadStart = FPlatformTime::Seconds();
	UploadTexture(TargetMaskTexture, Pixels, MaskWidth, MaskHeight);
	MaskRuntime.LastUploadMilliseconds = (FPlatformTime::Seconds() - UploadStart) * 1000.0;
	++MaskRuntime.TargetUploadCount;
}

void UGP_FoWWorldPresentationSubsystem::UpdateMaterialParameters()
{
	if (PostProcessMID == nullptr)
	{
		return;
	}

	const UGP_LocalFoWComponent* Mirror = BoundMirror.Get();
	const bool bReady = bVisualizationEnabled && MaskRuntime.bReady && Mirror != nullptr && Mirror->IsReady();
	const FVector2D Origin = MaskRuntime.ExtentWorldXY.X > KINDA_SMALL_NUMBER
		? MaskRuntime.OriginWorldXY
		: (Mirror != nullptr ? Mirror->GetGridOriginWorldXY() : FVector2D(-100000.0, -100000.0));
	const FVector2D Extent = MaskRuntime.ExtentWorldXY.X > KINDA_SMALL_NUMBER
		? MaskRuntime.ExtentWorldXY
		: FVector2D(200000.0, 200000.0);
	const FVector2D InvExtent(
		Extent.X > KINDA_SMALL_NUMBER ? 1.0 / Extent.X : 0.0,
		Extent.Y > KINDA_SMALL_NUMBER ? 1.0 / Extent.Y : 0.0);
	const float TexelSize = MaskWidth > 0 ? 1.0f / static_cast<float>(MaskWidth) : 0.001f;

	PostProcessMID->SetTextureParameterValue(TEXT("FoWPreviousMask"), PreviousMaskTexture);
	PostProcessMID->SetTextureParameterValue(TEXT("FoWTargetMask"), TargetMaskTexture);
	PostProcessMID->SetScalarParameterValue(TEXT("FoWBlendAlpha"), MaskRuntime.BlendAlpha);
	PostProcessMID->SetScalarParameterValue(TEXT("FoWReady"), bReady ? 1.0f : 0.0f);
	PostProcessMID->SetScalarParameterValue(TEXT("FoWExploredDim"), GPFoWVisualMask::ExploredDimFactor);
	PostProcessMID->SetScalarParameterValue(TEXT("FoWMaskTexelSize"), TexelSize);
	PostProcessMID->SetScalarParameterValue(TEXT("FoWBlurRadiusTexels"), GPFoWVisualMask::BlurRadiusTexels);
	PostProcessMID->SetScalarParameterValue(
		TEXT("FoWDebugMode"),
		bVisualizationEnabled ? static_cast<float>(GetVisualDebugMode()) : 0.0f);
	PostProcessMID->SetVectorParameterValue(
		TEXT("FoWOriginXY"),
		FLinearColor(Origin.X, Origin.Y, 0.0f, 0.0f));
	PostProcessMID->SetVectorParameterValue(
		TEXT("FoWInvExtentXY"),
		FLinearColor(InvExtent.X, InvExtent.Y, 0.0f, 0.0f));
}

void UGP_FoWWorldPresentationSubsystem::ApplyConservativeBlackMask()
{
	EnsureMaskResources(
		GPFoWVisualMask::CanonicalMaskResolution,
		GPFoWVisualMask::CanonicalMaskResolution);
	GPFoWVisualMask::ResetRuntime(MaskRuntime);
	Swap(PreviousMaskTexture, TargetMaskTexture);
	TArray<FColor> Black;
	Black.Init(FColor(0, 0, 0, 255), MaskWidth * MaskHeight);
	UploadTargetMask(Black);
	MaskRuntime.Width = MaskWidth;
	MaskRuntime.Height = MaskHeight;
	MaskRuntime.OriginWorldXY = FVector2D(-100000.0, -100000.0);
	MaskRuntime.ExtentWorldXY = FVector2D(200000.0, 200000.0);
	MaskRuntime.BlendAlpha = 1.0f;
	MaskRuntime.bReady = false;
	LastUpdateRevision = -1;
	UpdateMaterialParameters();
}

void UGP_FoWWorldPresentationSubsystem::RebuildMaskFromMirror(const UGP_LocalFoWComponent* Mirror)
{
	const double EncodeStart = FPlatformTime::Seconds();
	FVector2D Origin = FVector2D::ZeroVector;
	FVector2D Extent = FVector2D::ZeroVector;
	int32 Width = 0;
	int32 Height = 0;
	const bool bEncoded = GPFoWVisualMask::EncodePackedFromLocalFoW(
		PackedTargetPixels,
		Width,
		Height,
		Origin,
		Extent,
		Mirror);
	MaskRuntime.LastEncodeMilliseconds = (FPlatformTime::Seconds() - EncodeStart) * 1000.0;
	if (!bEncoded)
	{
		ApplyConservativeBlackMask();
		return;
	}

	EnsureMaskResources(Width, Height);
	Swap(PreviousMaskTexture, TargetMaskTexture);
	UploadTargetMask(PackedTargetPixels);
	GPFoWVisualMask::BeginNewTarget(
		MaskRuntime,
		Width,
		Height,
		Origin,
		Extent,
		Mirror != nullptr ? Mirror->GetRevision() : -1);
	LastUpdateRevision = MaskRuntime.MaskRevision;
	UpdateMaterialParameters();
}

void UGP_FoWWorldPresentationSubsystem::HandleLocalFoWUpdated(UGP_LocalFoWComponent* UpdatedMirror)
{
	if (UpdatedMirror == nullptr || UpdatedMirror != BoundMirror.Get())
	{
		return;
	}

	if (!UpdatedMirror->IsReady())
	{
		ApplyConservativeBlackMask();
		return;
	}

	RebuildMaskFromMirror(UpdatedMirror);
}

bool UGP_FoWWorldPresentationSubsystem::OwnsLocalGameView(
	int32 ViewPlayerIndex,
	const AActor* ViewActor,
	bool bIsGameView,
	bool bIsSceneCapture,
	bool bIsReflectionCapture) const
{
	if (!bVisualizationEnabled || !bIsGameView || bIsSceneCapture || bIsReflectionCapture)
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const APlayerController* PlayerController =
		LocalPlayer != nullptr ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	if (LocalPlayer == nullptr || PlayerController == nullptr || !PlayerController->IsLocalController())
	{
		return false;
	}

	if (ViewPlayerIndex == LocalPlayer->GetControllerId()
		|| ViewPlayerIndex == LocalPlayer->GetLocalPlayerIndex())
	{
		return true;
	}

	const AActor* ViewTarget = PlayerController->GetViewTarget();
	return ViewActor != nullptr
		&& (ViewActor == ViewTarget
			|| ViewActor == PlayerController->GetPawn()
			|| ViewActor == PlayerController);
}

bool UGP_FoWWorldPresentationSubsystem::SettingsContainLocalBlendable(
	const FPostProcessSettings& Settings) const
{
	if (PostProcessMID == nullptr)
	{
		return false;
	}

	for (const FWeightedBlendable& Blendable : Settings.WeightedBlendables.Array)
	{
		if (Blendable.Object == PostProcessMID && Blendable.Weight > 0.0f)
		{
			return true;
		}
	}
	return false;
}

bool UGP_FoWWorldPresentationSubsystem::IsBlendableBoundToLocalCamera() const
{
	const UCameraComponent* Camera = BoundCamera.Get();
	return Camera != nullptr
		&& PostProcessMID != nullptr
		&& SettingsContainLocalBlendable(Camera->PostProcessSettings);
}

bool UGP_FoWWorldPresentationSubsystem::TryInjectOwnedView(
	FPostProcessSettings& Settings,
	int32 ViewPlayerIndex,
	const AActor* ViewActor,
	bool bIsGameView,
	bool bIsSceneCapture,
	bool bIsReflectionCapture,
	uint64 FrameNumber,
	const TCHAR* ViewDebugName)
{
	if (!OwnsLocalGameView(
			ViewPlayerIndex,
			ViewActor,
			bIsGameView,
			bIsSceneCapture,
			bIsReflectionCapture))
	{
		return false;
	}

	RecordViewSeen(ViewDebugName);
	if (PostProcessMID == nullptr)
	{
		return false;
	}

	if (!SettingsContainLocalBlendable(Settings))
	{
		Settings.AddBlendable(PostProcessMID, 1.0f);
	}

	RecordSuccessfulInjection(FrameNumber, ViewDebugName);
	return SettingsContainLocalBlendable(Settings);
}

void UGP_FoWWorldPresentationSubsystem::RecordViewSeen(const TCHAR* ViewDebugName)
{
	++ActualViewsSeen;
	(void)ViewDebugName;
}

void UGP_FoWWorldPresentationSubsystem::RecordSuccessfulInjection(
	uint64 FrameNumber,
	const TCHAR* ViewDebugName)
{
	++BlendableInjectionCount;
	LastInjectedFrame = FrameNumber;
	LastInjectedView = ViewDebugName != nullptr ? FString(ViewDebugName) : FString();
}

UCameraComponent* UGP_FoWWorldPresentationSubsystem::ResolveLocalCamera() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const APlayerController* PlayerController =
		LocalPlayer != nullptr ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	AActor* ViewTarget = PlayerController != nullptr ? PlayerController->GetViewTarget() : nullptr;
	if (ViewTarget == nullptr)
	{
		return nullptr;
	}
	return ViewTarget->FindComponentByClass<UCameraComponent>();
}

void UGP_FoWWorldPresentationSubsystem::UnbindLocalCameraBlendable()
{
	if (UCameraComponent* Camera = BoundCamera.Get())
	{
		if (PostProcessMID != nullptr)
		{
			Camera->RemoveBlendable(PostProcessMID);
		}
	}
	BoundCamera.Reset();
}

void UGP_FoWWorldPresentationSubsystem::RefreshLocalCameraBlendable()
{
	UCameraComponent* Camera = ResolveLocalCamera();
	if (BoundCamera.Get() != Camera)
	{
		UnbindLocalCameraBlendable();
		BoundCamera = Camera;
	}

	if (Camera == nullptr || PostProcessMID == nullptr)
	{
		return;
	}

	if (!bVisualizationEnabled)
	{
		Camera->RemoveBlendable(PostProcessMID);
		return;
	}

	Camera->AddOrUpdateBlendable(PostProcessMID, 1.0f);
}

#if !UE_BUILD_SHIPPING

void UGP_FoWWorldPresentationSubsystem::DebugDumpToLog() const
{
	const UGP_LocalFoWComponent* Mirror = BoundMirror.Get();
	const FVector2D Origin = MaskRuntime.OriginWorldXY;
	const FVector2D Extent = MaskRuntime.ExtentWorldXY;

	UE_LOG(LogGPFoWWorldPresentation, Display,
		TEXT("GP FoW VisualDump: Renderer=%s MaskModel=%s TextureResolution=%dx%d WorldOrigin=%s WorldExtent=%s MaskRevision=%lld PreviousRevision=%lld BlendAlpha=%.3f BlendDuration=%.2f SpatialFilter=%s TemporalFilter=%s BlurRadiusTexels=%.2f MaskEncodeMs=%.3f MaskUploadMs=%.3f LocalTeam=%d Ready=%s PostProcessBound=%s ActualViewsSeen=%d BlendableInjectionCount=%d LastInjectedFrame=%llu LastInjectedView=%s CameraBlendable=%s OldSlateRendererActive=%s CellSize=%.1f Dims=%dx%d Interval=n/a MaskBytes=%d MaskTextures=2 TargetUploads=%d BuildCount=%d Enabled=%s DebugMode=%d WorldPosition=%s Material=%s"),
		GetRendererName(),
		GetMaskModelName(),
		MaskWidth,
		MaskHeight,
		*Origin.ToString(),
		*Extent.ToString(),
		MaskRuntime.MaskRevision,
		MaskRuntime.PreviousRevision,
		MaskRuntime.BlendAlpha,
		GetBlendDurationSeconds(),
		GetSpatialFilterName(),
		GetTemporalFilterName(),
		GetBlurRadiusTexels(),
		MaskRuntime.LastEncodeMilliseconds,
		MaskRuntime.LastUploadMilliseconds,
		Mirror != nullptr ? Mirror->GetLocalTeamId() : -1,
		Mirror != nullptr && Mirror->IsReady() && MaskRuntime.bReady ? TEXT("true") : TEXT("false"),
		IsPostProcessBound() ? TEXT("true") : TEXT("false"),
		ActualViewsSeen,
		BlendableInjectionCount,
		LastInjectedFrame,
		*LastInjectedView,
		IsBlendableBoundToLocalCamera() ? TEXT("true") : TEXT("false"),
		IsOldSlateRendererActive() ? TEXT("true") : TEXT("false"),
		Mirror != nullptr ? Mirror->GetCellSizeCm() : 0.0f,
		Mirror != nullptr ? Mirror->GetGridDimensions().X : 0,
		Mirror != nullptr ? Mirror->GetGridDimensions().Y : 0,
		GetMaskBytesPerTexture(),
		MaskRuntime.TargetUploadCount,
		MaskRuntime.BuildCount,
		bVisualizationEnabled ? TEXT("true") : TEXT("false"),
		GetVisualDebugMode(),
		GetWorldPositionMethodName(),
		*GetNameSafe(TemplateMaterial));
}

namespace GPFoWWorldPresentationPrivate
{
	static UGP_FoWWorldPresentationSubsystem* ResolveSubsystem(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		APlayerController* PlayerController = World->GetFirstPlayerController();
		ULocalPlayer* LocalPlayer =
			PlayerController != nullptr ? PlayerController->GetLocalPlayer() : nullptr;
		return LocalPlayer != nullptr
			? LocalPlayer->GetSubsystem<UGP_FoWWorldPresentationSubsystem>()
			: nullptr;
	}

	static void VisualDump(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (UGP_FoWWorldPresentationSubsystem* Subsystem = ResolveSubsystem(World))
		{
			Subsystem->DebugDumpToLog();
			return;
		}

		UE_LOG(LogGPFoWWorldPresentation, Warning,
			TEXT("gp.FoW.VisualDump: no local FoW world presentation subsystem in World=%s."),
			*GetNameSafe(World));
	}

	static void VisualEnable(const TArray<FString>& Args, UWorld* World)
	{
		UGP_FoWWorldPresentationSubsystem* Subsystem = ResolveSubsystem(World);
		if (Subsystem == nullptr || Args.IsEmpty())
		{
			UE_LOG(LogGPFoWWorldPresentation, Warning,
				TEXT("gp.FoW.VisualEnable usage: gp.FoW.VisualEnable <0|1>."));
			return;
		}

		const bool bEnabled = FCString::Atoi(*Args[0]) != 0;
		Subsystem->SetVisualizationEnabled(bEnabled);
		UE_LOG(LogGPFoWWorldPresentation, Display,
			TEXT("gp.FoW.VisualEnable: Enabled=%s World=%s."),
			bEnabled ? TEXT("true") : TEXT("false"),
			*GetNameSafe(World));
	}

	static FAutoConsoleCommandWithWorldAndArgs GVisualDumpCommand(
		TEXT("gp.FoW.VisualDump"),
		TEXT("Dump local world Fog of War renderer state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&VisualDump));

	static FAutoConsoleCommandWithWorldAndArgs GVisualEnableCommand(
		TEXT("gp.FoW.VisualEnable"),
		TEXT("Enable/disable local world Fog of War presentation: gp.FoW.VisualEnable <0|1>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&VisualEnable));
}

#endif
