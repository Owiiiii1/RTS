// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPFoWWorldPresentationSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "Game/GPGameState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Presentation/GPFoWPresentationRaster.h"
#include "Presentation/GPLocalFoWUnitPresentationSubsystem.h"
#include "Widgets/GPFoWWorldOverlayWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFoWWorldPresentation, Log, All);

void UGP_FoWWorldPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	APlayerController* PlayerController =
		LocalPlayer != nullptr ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
	BindToPlayerController(PlayerController);
	EnsureOverlayWidget(PlayerController);
}

void UGP_FoWWorldPresentationSubsystem::Deinitialize()
{
	RemoveOverlayWidget();
	UnbindMirror();
	Super::Deinitialize();
}

void UGP_FoWWorldPresentationSubsystem::PlayerControllerChanged(
	APlayerController* NewPlayerController)
{
	RemoveOverlayWidget();
	BindToPlayerController(NewPlayerController);
	EnsureOverlayWidget(NewPlayerController);
}

float UGP_FoWWorldPresentationSubsystem::GetObscurationForState(EGP_FoWState State)
{
	return GPFoWPresentationRaster::ObscurationForState(State);
}

FLinearColor UGP_FoWWorldPresentationSubsystem::GetOverlayColorForState(EGP_FoWState State)
{
	return GPFoWPresentationRaster::OverlayColorForObscuration(GetObscurationForState(State));
}

FLinearColor UGP_FoWWorldPresentationSubsystem::GetOverlayColorForObscuration(float Obscuration)
{
	return GPFoWPresentationRaster::OverlayColorForObscuration(Obscuration);
}

bool UGP_FoWWorldPresentationSubsystem::RequiresConservativeFullObscuration(
	const UGP_LocalFoWComponent* Mirror)
{
	return Mirror == nullptr || !Mirror->IsReady();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetRendererName()
{
	return GPFoWPresentationRaster::GetRendererName();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetPresentationAlgorithmName()
{
	return GPFoWPresentationRaster::GetAlgorithmName();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetMaskModelName()
{
	return GPFoWPresentationRaster::GetMaskModelName();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetInterpolationName()
{
	return GPFoWPresentationRaster::GetInterpolationName();
}

const TCHAR* UGP_FoWWorldPresentationSubsystem::GetBlurName()
{
	return GPFoWPresentationRaster::GetBlurName();
}

float UGP_FoWWorldPresentationSubsystem::GetFeatherFraction()
{
	return GPFoWPresentationRaster::FeatherFraction;
}

float UGP_FoWWorldPresentationSubsystem::GetInnerFeatherFraction()
{
	return GPFoWPresentationRaster::InnerFeatherFraction;
}

float UGP_FoWWorldPresentationSubsystem::GetRevealFadeSeconds()
{
	return GPFoWPresentationRaster::RevealFadeSeconds;
}

float UGP_FoWWorldPresentationSubsystem::GetHideFadeSeconds()
{
	return GPFoWPresentationRaster::HideFadeSeconds;
}

int32 UGP_FoWWorldPresentationSubsystem::GetMaximumOverlayQuads()
{
	return GPFoWPresentationRaster::MaximumOverlayQuads;
}

void UGP_FoWWorldPresentationSubsystem::SetVisualizationEnabled(bool bEnabled)
{
	if (bVisualizationEnabled == bEnabled)
	{
		return;
	}

	bVisualizationEnabled = bEnabled;
	++RenderSerial;
	if (OverlayWidget != nullptr)
	{
		OverlayWidget->HandlePresentationDataChanged();
	}
}

bool UGP_FoWWorldPresentationSubsystem::IsRendererActive() const
{
	return bVisualizationEnabled && OverlayWidget != nullptr && OverlayWidget->IsInViewport();
}

bool UGP_FoWWorldPresentationSubsystem::IsVisualDataDirty() const
{
	return OverlayWidget == nullptr || OverlayWidget->GetConsumedRenderSerial() != RenderSerial;
}

void UGP_FoWWorldPresentationSubsystem::RecordOverlayStats(const FGP_FoWWorldOverlayStats& Stats)
{
	LastStats = Stats;
	LastConsumedRenderSerial = Stats.ConsumedSerial;
}

void UGP_FoWWorldPresentationSubsystem::BindToPlayerController(
	APlayerController* NewPlayerController)
{
	UnbindMirror();

	AGP_PlayerController* GPPlayerController = Cast<AGP_PlayerController>(NewPlayerController);
	if (GPPlayerController == nullptr || !GPPlayerController->IsLocalController())
	{
		++RenderSerial;
		LastUpdateRevision = -1;
		return;
	}

	UGP_LocalFoWComponent* Mirror = GPPlayerController->GetLocalFogOfWarComponent();
	if (Mirror == nullptr)
	{
		++RenderSerial;
		LastUpdateRevision = -1;
		return;
	}

	BoundMirror = Mirror;
	MirrorUpdatedHandle = Mirror->OnLocalFoWUpdated.AddUObject(
		this,
		&ThisClass::HandleLocalFoWUpdated);
	LastUpdateRevision = Mirror->IsReady() ? Mirror->GetRevision() : -1;
	++RenderSerial;
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

void UGP_FoWWorldPresentationSubsystem::EnsureOverlayWidget(
	APlayerController* OwningController)
{
	if (OwningController == nullptr || !OwningController->IsLocalController())
	{
		return;
	}

	if (OverlayWidget == nullptr)
	{
		OverlayWidget = CreateWidget<UGP_FoWWorldOverlayWidget>(
			OwningController,
			UGP_FoWWorldOverlayWidget::StaticClass());
		if (OverlayWidget == nullptr)
		{
			UE_LOG(LogGPFoWWorldPresentation, Error,
				TEXT("Failed to create source-only FoW world overlay for %s."),
				*GetNameSafe(OwningController));
			return;
		}
		OverlayWidget->InitializeWithPresentationOwner(this);
	}

	if (!OverlayWidget->IsInViewport())
	{
		OverlayWidget->AddToPlayerScreen(-10000);
	}
	OverlayWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	OverlayWidget->HandlePresentationDataChanged();
}

void UGP_FoWWorldPresentationSubsystem::RemoveOverlayWidget()
{
	if (OverlayWidget != nullptr)
	{
		OverlayWidget->RemoveFromParent();
		OverlayWidget = nullptr;
	}
}

void UGP_FoWWorldPresentationSubsystem::HandleLocalFoWUpdated(
	UGP_LocalFoWComponent* UpdatedMirror)
{
	if (UpdatedMirror == nullptr || UpdatedMirror != BoundMirror.Get())
	{
		return;
	}

	LastUpdateRevision = UpdatedMirror->IsReady() ? UpdatedMirror->GetRevision() : -1;
	++RenderSerial;
	if (OverlayWidget != nullptr)
	{
		OverlayWidget->HandlePresentationDataChanged();
	}
}

#if !UE_BUILD_SHIPPING

void UGP_FoWWorldPresentationSubsystem::DebugDumpToLog() const
{
	const UGP_LocalFoWComponent* Mirror = BoundMirror.Get();
	const float CellSize = Mirror != nullptr ? Mirror->GetCellSizeCm() : 0.0f;
	const FIntPoint Dims = Mirror != nullptr ? Mirror->GetGridDimensions() : FIntPoint::ZeroValue;
	const AGP_GameState* GameState =
		GetWorld() != nullptr ? GetWorld()->GetGameState<AGP_GameState>() : nullptr;
	const UGP_FogOfWarComponent* AuthorityFoW =
		GameState != nullptr ? GameState->GetFogOfWarComponent() : nullptr;
	const float Interval = AuthorityFoW != nullptr ? AuthorityFoW->GetUpdateIntervalSeconds() : 0.0f;
	const UGP_LocalFoWUnitPresentationSubsystem* UnitPresentation =
		GetWorld() != nullptr
			? GetWorld()->GetSubsystem<UGP_LocalFoWUnitPresentationSubsystem>()
			: nullptr;

	UE_LOG(LogGPFoWWorldPresentation, Display,
		TEXT("GP FoW VisualDump: Renderer=%s PostProcessActive=%s MaskProjectionActive=%s World=%s Active=%s Enabled=%s Ready=%s LocalTeam=%d Algorithm=%s CellSize=%.1f Dims=%dx%d Interval=%.2f SampledGameplayCells=%d CellTiles=%d VisibleCellsSkipped=%d FeatherQuads=%d FeatherCm=%.1f FeatherFraction=%.2f InnerFeatherFraction=%.2f OverlayQuads=%d OverlayVertices=%d MaxSampledCells=%d MaxQuads=%d MaskRevision=%lld CameraResample=%s FallbackActive=%s RegisteredEnemyPresentation=%d RebuildMs=%.3f Blur=%s PadCells=%d RegionMin=%s RegionMax=%s Dirty=%s LastUpdateRevision=%lld ConsumedSerial=%llu RenderSerial=%llu"),
		GetRendererName(),
		IsPostProcessActive() ? TEXT("true") : TEXT("false"),
		IsMaskProjectionActive() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(GetWorld()),
		IsRendererActive() ? TEXT("true") : TEXT("false"),
		bVisualizationEnabled ? TEXT("true") : TEXT("false"),
		Mirror != nullptr && Mirror->IsReady() ? TEXT("true") : TEXT("false"),
		Mirror != nullptr ? Mirror->GetLocalTeamId() : -1,
		GetPresentationAlgorithmName(),
		CellSize,
		Dims.X,
		Dims.Y,
		Interval,
		LastStats.SampledGameplayCells,
		LastStats.CellTiles,
		LastStats.VisibleCellsSkipped,
		LastStats.FeatherQuads,
		LastStats.FeatherCm,
		GetFeatherFraction(),
		GetInnerFeatherFraction(),
		LastStats.OverlayQuads,
		LastStats.OverlayVertices,
		GetMaximumSampledCells(),
		GetMaximumOverlayQuads(),
		LastStats.MaskRevision,
		LastStats.bCameraResample ? TEXT("true") : TEXT("false"),
		LastStats.bFallbackActive ? TEXT("true") : TEXT("false"),
		UnitPresentation != nullptr ? UnitPresentation->GetRegisteredUnitCount() : 0,
		LastStats.RebuildMilliseconds,
		GetBlurName(),
		GetSamplePadCells(),
		*LastStats.MinCell.ToString(),
		*LastStats.MaxCell.ToString(),
		IsVisualDataDirty() ? TEXT("true") : TEXT("false"),
		LastUpdateRevision,
		LastConsumedRenderSerial,
		RenderSerial);
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
