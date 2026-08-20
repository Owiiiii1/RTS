// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPFoWWorldPresentationSubsystem.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Player/GPPlayerController.h"
#include "SceneView.h"
#include "SceneViewExtension.h"
#include "RHI.h"
#include "TextureResource.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFoWWorldPresentation, Log, All);

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
		UGP_FoWWorldPresentationSubsystem* Presentation = Owner.Get();
		if (Presentation == nullptr
			|| !Presentation->IsVisualizationEnabled()
			|| !InView.bIsGameView)
		{
			return;
		}

		ULocalPlayer* LocalPlayer = Presentation->GetLocalPlayer();
		UGameViewportClient* ViewportClient =
			LocalPlayer != nullptr ? LocalPlayer->ViewportClient : nullptr;
		FViewport* Viewport = ViewportClient != nullptr ? ViewportClient->Viewport : nullptr;
		if (Viewport == nullptr || InViewFamily.RenderTarget != Viewport)
		{
			return;
		}

		if (UMaterialInstanceDynamic* MID = Presentation->GetPostProcessMID())
		{
			InView.FinalPostProcessSettings.AddBlendable(MID, 1.0f);
		}
	}

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
	{
		(void)InViewFamily;
	}

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		const UGP_FoWWorldPresentationSubsystem* Presentation = Owner.Get();
		return Presentation != nullptr
			&& Presentation->IsVisualizationEnabled()
			&& Context.GetWorld() != nullptr
			&& Context.GetWorld() == Presentation->GetWorld();
	}

private:
	TWeakObjectPtr<UGP_FoWWorldPresentationSubsystem> Owner;
};

void UGP_FoWWorldPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	EnsureMaskResources();
	ViewExtension = FSceneViewExtensions::NewExtension<FGP_FoWSceneViewExtension>(this);

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PlayerController =
		LocalPlayer != nullptr ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	BindToPlayerController(PlayerController);
}

void UGP_FoWWorldPresentationSubsystem::Deinitialize()
{
	ViewExtension.Reset();
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
		&& bVisualizationEnabled
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

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetMaterialAssetPath()
{
	return GPFoWVisualMask::GetMaterialAssetPath();
}

int32 UGP_FoWWorldPresentationSubsystem::GetMaskTextureResolution()
{
	return GPFoWVisualMask::TextureResolution;
}

float UGP_FoWWorldPresentationSubsystem::GetBlendDurationSeconds()
{
	return GPFoWVisualMask::BlendDurationSeconds;
}

int32 UGP_FoWWorldPresentationSubsystem::GetSpatialBlurRadius()
{
	return GPFoWVisualMask::SpatialBlurRadius;
}

int32 UGP_FoWWorldPresentationSubsystem::GetSpatialBlurPasses()
{
	return GPFoWVisualMask::SpatialBlurPasses;
}

float UGP_FoWWorldPresentationSubsystem::GetExploredDimFactor()
{
	return GPFoWVisualMask::ExploredDimFactor;
}

int32 UGP_FoWWorldPresentationSubsystem::GetMaskBytesPerTexture()
{
	return GPFoWVisualMask::TextureResolution * GPFoWVisualMask::TextureResolution * 4;
}

bool UGP_FoWWorldPresentationSubsystem::IsPostProcessBound() const
{
	return ViewExtension.IsValid() && PostProcessMID != nullptr && TemplateMaterial != nullptr;
}

void UGP_FoWWorldPresentationSubsystem::SetVisualizationEnabled(bool bEnabled)
{
	if (bVisualizationEnabled == bEnabled)
	{
		return;
	}

	bVisualizationEnabled = bEnabled;
	UpdateMaterialParameters();
}

void UGP_FoWWorldPresentationSubsystem::DebugAdvanceBlend(float DeltaSeconds)
{
	GPFoWVisualMask::AdvanceBlend(MaskRuntime, DeltaSeconds);
	UpdateMaterialParameters();
}

void UGP_FoWWorldPresentationSubsystem::BindToPlayerController(
	APlayerController* NewPlayerController)
{
	UnbindMirror();
	EnsureMaskResources();

	AGP_PlayerController* GPPlayerController = Cast<AGP_PlayerController>(NewPlayerController);
	if (GPPlayerController == nullptr || !GPPlayerController->IsLocalController())
	{
		LastUpdateRevision = -1;
		ApplyConservativeBlackMask();
		return;
	}

	UGP_LocalFoWComponent* Mirror = GPPlayerController->GetLocalFogOfWarComponent();
	if (Mirror == nullptr)
	{
		LastUpdateRevision = -1;
		ApplyConservativeBlackMask();
		return;
	}

	BoundMirror = Mirror;
	MirrorUpdatedHandle = Mirror->OnLocalFoWUpdated.AddUObject(
		this,
		&ThisClass::HandleLocalFoWUpdated);
	HandleLocalFoWUpdated(Mirror);
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

void UGP_FoWWorldPresentationSubsystem::EnsureMaskResources()
{
	if (PreviousMaskTexture == nullptr)
	{
		PreviousMaskTexture = CreateMaskTexture(TEXT("GPFoWPreviousMask"));
	}
	if (TargetMaskTexture == nullptr)
	{
		TargetMaskTexture = CreateMaskTexture(TEXT("GPFoWTargetMask"));
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
	PreviousMaskTexture = nullptr;
	TargetMaskTexture = nullptr;
	PostProcessMID = nullptr;
	TemplateMaterial = nullptr;
	PackedPreviousPixels.Reset();
	PackedTargetPixels.Reset();
	GPFoWVisualMask::ResetRuntime(MaskRuntime);
}

UTexture2D* UGP_FoWWorldPresentationSubsystem::CreateMaskTexture(const TCHAR* Name) const
{
	UTexture2D* Texture = UTexture2D::CreateTransient(
		GPFoWVisualMask::TextureResolution,
		GPFoWVisualMask::TextureResolution,
		PF_B8G8R8A8,
		Name);
	if (Texture == nullptr)
	{
		return nullptr;
	}

	Texture->SRGB = false;
	Texture->Filter = TF_Bilinear;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->NeverStream = true;
	Texture->UpdateResource();
	return Texture;
}

void UGP_FoWWorldPresentationSubsystem::UploadTexture(
	UTexture2D* Texture,
	const TArray<FColor>& Pixels)
{
	if (Texture == nullptr || Pixels.Num() != GetMaskTextureResolution() * GetMaskTextureResolution())
	{
		return;
	}

	const int32 Width = GetMaskTextureResolution();
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Width);
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

void UGP_FoWWorldPresentationSubsystem::UploadMaskTextures()
{
	const double UploadStart = FPlatformTime::Seconds();
	GPFoWVisualMask::PackRGBA(MaskRuntime.Previous, PackedPreviousPixels);
	GPFoWVisualMask::PackRGBA(MaskRuntime.Target, PackedTargetPixels);
	UploadTexture(PreviousMaskTexture, PackedPreviousPixels);
	UploadTexture(TargetMaskTexture, PackedTargetPixels);
	MaskRuntime.LastUploadMilliseconds = (FPlatformTime::Seconds() - UploadStart) * 1000.0;
}

void UGP_FoWWorldPresentationSubsystem::UpdateMaterialParameters()
{
	if (PostProcessMID == nullptr)
	{
		return;
	}

	const UGP_LocalFoWComponent* Mirror = BoundMirror.Get();
	const bool bReady = bVisualizationEnabled && MaskRuntime.bReady && Mirror != nullptr && Mirror->IsReady();
	const FVector2D Origin = MaskRuntime.Target.ExtentWorldXY.X > KINDA_SMALL_NUMBER
		? MaskRuntime.Target.OriginWorldXY
		: (Mirror != nullptr ? Mirror->GetGridOriginWorldXY() : FVector2D(-100000.0, -100000.0));
	const FVector2D Extent = MaskRuntime.Target.ExtentWorldXY.X > KINDA_SMALL_NUMBER
		? MaskRuntime.Target.ExtentWorldXY
		: FVector2D(200000.0, 200000.0);
	const FVector2D InvExtent(
		Extent.X > KINDA_SMALL_NUMBER ? 1.0 / Extent.X : 0.0,
		Extent.Y > KINDA_SMALL_NUMBER ? 1.0 / Extent.Y : 0.0);

	PostProcessMID->SetTextureParameterValue(TEXT("FoWPreviousMask"), PreviousMaskTexture);
	PostProcessMID->SetTextureParameterValue(TEXT("FoWTargetMask"), TargetMaskTexture);
	PostProcessMID->SetScalarParameterValue(TEXT("FoWBlendAlpha"), MaskRuntime.BlendAlpha);
	PostProcessMID->SetScalarParameterValue(TEXT("FoWReady"), bReady ? 1.0f : 0.0f);
	PostProcessMID->SetScalarParameterValue(TEXT("FoWExploredDim"), GPFoWVisualMask::ExploredDimFactor);
	PostProcessMID->SetVectorParameterValue(
		TEXT("FoWOriginXY"),
		FLinearColor(Origin.X, Origin.Y, 0.0f, 0.0f));
	PostProcessMID->SetVectorParameterValue(
		TEXT("FoWInvExtentXY"),
		FLinearColor(InvExtent.X, InvExtent.Y, 0.0f, 0.0f));
}

void UGP_FoWWorldPresentationSubsystem::ApplyConservativeBlackMask()
{
	GPFoWVisualMask::ResetRuntime(MaskRuntime);
	FGP_FoWVisualMaskBuffers Black;
	GPFoWVisualMask::ResetBuffers(
		Black,
		GetMaskTextureResolution(),
		GetMaskTextureResolution(),
		FVector2D(-100000.0, -100000.0),
		FVector2D(200000.0, 200000.0));
	MaskRuntime.Previous = Black;
	MaskRuntime.Target = Black;
	MaskRuntime.BlendAlpha = 1.0f;
	MaskRuntime.bReady = false;
	LastUpdateRevision = -1;
	UploadMaskTextures();
	UpdateMaterialParameters();
}

void UGP_FoWWorldPresentationSubsystem::RebuildMaskFromMirror(const UGP_LocalFoWComponent* Mirror)
{
	const double BuildStart = FPlatformTime::Seconds();
	FGP_FoWVisualMaskBuffers NewTarget;
	GPFoWVisualMask::EncodeFromLocalFoW(
		NewTarget,
		Mirror,
		GetMaskTextureResolution(),
		GetMaskTextureResolution());
	GPFoWVisualMask::ApplySpatialFilter(NewTarget);
	MaskRuntime.LastBuildMilliseconds = (FPlatformTime::Seconds() - BuildStart) * 1000.0;
	GPFoWVisualMask::BeginNewTarget(
		MaskRuntime,
		MoveTemp(NewTarget),
		Mirror != nullptr ? Mirror->GetRevision() : -1);
	LastUpdateRevision = MaskRuntime.MaskRevision;
	UploadMaskTextures();
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

#if !UE_BUILD_SHIPPING

void UGP_FoWWorldPresentationSubsystem::DebugDumpToLog() const
{
	const UGP_LocalFoWComponent* Mirror = BoundMirror.Get();
	const FVector2D Origin = MaskRuntime.Target.OriginWorldXY;
	const FVector2D Extent = MaskRuntime.Target.ExtentWorldXY;
	const float SpatialRadiusCm = GetSpatialBlurRadius()
		* (Extent.X > KINDA_SMALL_NUMBER
			? static_cast<float>(Extent.X / GetMaskTextureResolution())
			: 0.0f);

	UE_LOG(LogGPFoWWorldPresentation, Display,
		TEXT("GP FoW VisualDump: Renderer=%s MaskModel=%s TextureResolution=%d WorldOrigin=%s WorldExtent=%s MaskRevision=%lld PreviousRevision=%lld BlendAlpha=%.3f BlendDuration=%.2f SpatialFilter=%s SpatialRadius=%d SpatialRadiusCm=%.1f MaskBuildMs=%.3f MaskUploadMs=%.3f LocalTeam=%d Ready=%s PostProcessBound=%s OldSlateRendererActive=%s CellSize=%.1f Dims=%dx%d Interval=n/a MaskBytes=%d MaskTextures=2 BuildCount=%d Enabled=%s Material=%s"),
		GetRendererName(),
		GetMaskModelName(),
		GetMaskTextureResolution(),
		*Origin.ToString(),
		*Extent.ToString(),
		MaskRuntime.MaskRevision,
		MaskRuntime.PreviousRevision,
		MaskRuntime.BlendAlpha,
		GetBlendDurationSeconds(),
		GetSpatialFilterName(),
		GetSpatialBlurRadius(),
		SpatialRadiusCm,
		MaskRuntime.LastBuildMilliseconds,
		MaskRuntime.LastUploadMilliseconds,
		Mirror != nullptr ? Mirror->GetLocalTeamId() : -1,
		Mirror != nullptr && Mirror->IsReady() && MaskRuntime.bReady ? TEXT("true") : TEXT("false"),
		IsPostProcessBound() ? TEXT("true") : TEXT("false"),
		IsOldSlateRendererActive() ? TEXT("true") : TEXT("false"),
		Mirror != nullptr ? Mirror->GetCellSizeCm() : 0.0f,
		Mirror != nullptr ? Mirror->GetGridDimensions().X : 0,
		Mirror != nullptr ? Mirror->GetGridDimensions().Y : 0,
		GetMaskBytesPerTexture(),
		MaskRuntime.BuildCount,
		bVisualizationEnabled ? TEXT("true") : TEXT("false"),
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
