// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPFoWWorldPresentationSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
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
	switch (State)
	{
	case EGP_FoWState::Visible:
		return 0.0f;
	case EGP_FoWState::Explored:
		return 0.68f;
	default:
		return 1.0f;
	}
}

FLinearColor UGP_FoWWorldPresentationSubsystem::GetOverlayColorForState(EGP_FoWState State)
{
	switch (State)
	{
	case EGP_FoWState::Visible:
		return FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
	case EGP_FoWState::Explored:
		return FLinearColor(0.025f, 0.025f, 0.035f, GetObscurationForState(State));
	default:
		return FLinearColor(0.0f, 0.0f, 0.0f, GetObscurationForState(State));
	}
}

bool UGP_FoWWorldPresentationSubsystem::RequiresConservativeFullObscuration(
	const UGP_LocalFoWComponent* Mirror)
{
	return Mirror == nullptr || !Mirror->IsReady();
}

bool UGP_FoWWorldPresentationSubsystem::ShouldAddConservativeFeather(
	EGP_FoWState Current,
	EGP_FoWState Neighbor)
{
	return GetObscurationForState(Neighbor) > GetObscurationForState(Current);
}

float UGP_FoWWorldPresentationSubsystem::GetConservativeFeatherBoundaryAlpha(
	EGP_FoWState Current,
	EGP_FoWState MoreObscuredNeighbor)
{
	const float CurrentObscuration = GetObscurationForState(Current);
	const float NeighborObscuration = GetObscurationForState(MoreObscuredNeighbor);
	if (NeighborObscuration <= CurrentObscuration
		|| CurrentObscuration >= 1.0f - KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		(NeighborObscuration - CurrentObscuration) / (1.0f - CurrentObscuration),
		0.0f,
		1.0f);
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

void UGP_FoWWorldPresentationSubsystem::RecordOverlayStats(
	int32 SampledCells,
	int32 OverlayRuns,
	int32 FeatherQuads,
	int32 DrawBatches,
	const FIntPoint& MinCell,
	const FIntPoint& MaxCell,
	uint64 ConsumedSerial)
{
	LastSampledCellCount = SampledCells;
	LastOverlayRunCount = OverlayRuns;
	LastFeatherQuadCount = FeatherQuads;
	LastDrawBatchCount = DrawBatches;
	LastSampledMinCell = MinCell;
	LastSampledMaxCell = MaxCell;
	LastConsumedRenderSerial = ConsumedSerial;
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
	const FIntPoint Dimensions =
		Mirror != nullptr ? Mirror->GetGridDimensions() : FIntPoint::ZeroValue;
	const FVector2D Origin =
		Mirror != nullptr ? Mirror->GetGridOriginWorldXY() : FVector2D::ZeroVector;
	const float CellSize = Mirror != nullptr ? Mirror->GetCellSizeCm() : 0.0f;
	const UGP_LocalFoWUnitPresentationSubsystem* UnitPresentation =
		GetWorld() != nullptr
			? GetWorld()->GetSubsystem<UGP_LocalFoWUnitPresentationSubsystem>()
			: nullptr;

	UE_LOG(LogGPFoWWorldPresentation, Display,
		TEXT("GP FoW VisualDump: World=%s Active=%s Enabled=%s Ready=%s LocalTeam=%d MirrorRevision=%lld Method=ViewportLocalProjectedSlateRuns Origin=%s Dims=%s CellSize=%.1f MaxSampledCells=%d SampledCells=%d Runs=%d FeatherQuads=%d SmoothingCellFraction=%.2f Batches=%d RegisteredUnitPresentations=%d UnitEvaluationInterval=%.2f RegionMin=%s RegionMax=%s Dirty=%s LastUpdateRevision=%lld ConsumedSerial=%llu RenderSerial=%llu"),
		*GetNameSafe(GetWorld()),
		IsRendererActive() ? TEXT("true") : TEXT("false"),
		bVisualizationEnabled ? TEXT("true") : TEXT("false"),
		Mirror != nullptr && Mirror->IsReady() ? TEXT("true") : TEXT("false"),
		Mirror != nullptr ? Mirror->GetLocalTeamId() : -1,
		Mirror != nullptr ? Mirror->GetRevision() : -1,
		*Origin.ToString(),
		*Dimensions.ToString(),
		CellSize,
		GetMaximumSampledCells(),
		LastSampledCellCount,
		LastOverlayRunCount,
		LastFeatherQuadCount,
		GetSmoothingWidthCellFraction(),
		LastDrawBatchCount,
		UnitPresentation != nullptr ? UnitPresentation->GetRegisteredUnitCount() : 0,
		UGP_LocalFoWUnitPresentationSubsystem::GetEvaluationIntervalSeconds(),
		*LastSampledMinCell.ToString(),
		*LastSampledMaxCell.ToString(),
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
