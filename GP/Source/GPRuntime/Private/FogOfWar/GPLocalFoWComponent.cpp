// Copyright Epic Games, Inc. All Rights Reserved.

#include "FogOfWar/GPLocalFoWComponent.h"

#include "Engine/World.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "Game/GPGameState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPLocalFogOfWar, Log, All);

namespace GPLocalFogOfWarPrivate
{
	static bool IsFiniteLocation(const FVector& Location)
	{
		return !Location.ContainsNaN()
			&& FMath::IsFinite(Location.X)
			&& FMath::IsFinite(Location.Y)
			&& FMath::IsFinite(Location.Z);
	}

	static const TCHAR* StateToString(EGP_FoWState State)
	{
		switch (State)
		{
		case EGP_FoWState::Visible:
			return TEXT("Visible");
		case EGP_FoWState::Explored:
			return TEXT("Explored");
		default:
			return TEXT("Unexplored");
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

UGP_LocalFoWComponent::UGP_LocalFoWComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UGP_LocalFoWComponent::IsMetadataValid(const FGP_FoWPresentationUpdate& Update) const
{
	if (Update.TeamId < 1
		|| Update.Revision < 0
		|| !FMath::IsFinite(Update.CellSizeCm)
		|| Update.CellSizeCm <= KINDA_SMALL_NUMBER
		|| Update.GridDimensions.X <= 0
		|| Update.GridDimensions.Y <= 0)
	{
		return false;
	}

	const int64 NumCells64 =
		static_cast<int64>(Update.GridDimensions.X) * static_cast<int64>(Update.GridDimensions.Y);
	return NumCells64 > 0 && NumCells64 <= MAX_int32
		&& FMath::IsFinite(Update.GridOriginWorldXY.X)
		&& FMath::IsFinite(Update.GridOriginWorldXY.Y);
}

bool UGP_LocalFoWComponent::AreRangesValid(
	const TArray<FGP_FoWCellRange>& Ranges,
	int32 NumCells) const
{
	for (const FGP_FoWCellRange& Range : Ranges)
	{
		if (Range.StartIndex < 0 || Range.NumCells <= 0)
		{
			return false;
		}
		const int64 EndExclusive = static_cast<int64>(Range.StartIndex) + Range.NumCells;
		if (EndExclusive > NumCells)
		{
			return false;
		}
	}
	return true;
}

void UGP_LocalFoWComponent::ApplyRanges(
	TBitArray<>& Bits,
	const TArray<FGP_FoWCellRange>& Ranges)
{
	for (const FGP_FoWCellRange& Range : Ranges)
	{
		for (int32 Index = Range.StartIndex; Index < Range.StartIndex + Range.NumCells; ++Index)
		{
			Bits[Index] = true;
		}
	}
}

bool UGP_LocalFoWComponent::ApplyServerUpdate(const FGP_FoWPresentationUpdate& Update)
{
	if (!IsMetadataValid(Update))
	{
		return false;
	}

	const int32 NumCells = Update.GridDimensions.X * Update.GridDimensions.Y;
	if (!AreRangesValid(Update.ExploredRanges, NumCells)
		|| !AreRangesValid(Update.VisibleRanges, NumCells))
	{
		return false;
	}

	if (Update.bInitialSnapshot
		&& bReady
		&& Update.TeamId == LocalTeamId
		&& Update.Revision <= Revision)
	{
		return false;
	}

	if (!Update.bInitialSnapshot)
	{
		if (!bReady
			|| Update.TeamId != LocalTeamId
			|| Update.Revision <= Revision
			|| Update.GridDimensions != GridDimensions
			|| !Update.GridOriginWorldXY.Equals(GridOriginWorldXY)
			|| !FMath::IsNearlyEqual(Update.CellSizeCm, CellSizeCm))
		{
			return false;
		}
	}

	if (Update.bInitialSnapshot)
	{
		ResetPresentation();
		LocalTeamId = Update.TeamId;
		GridOriginWorldXY = Update.GridOriginWorldXY;
		GridDimensions = Update.GridDimensions;
		CellSizeCm = Update.CellSizeCm;
		Explored.Init(false, NumCells);
		Visible.Init(false, NumCells);
	}

	ApplyRanges(Explored, Update.ExploredRanges);
	Visible.Init(false, NumCells);
	ApplyRanges(Visible, Update.VisibleRanges);

	for (TConstSetBitIterator<> It(Visible); It; ++It)
	{
		Explored[It.GetIndex()] = true;
	}

	Revision = Update.Revision;
	bReady = true;
	OnLocalFoWUpdated.Broadcast(this);
	return true;
}

void UGP_LocalFoWComponent::ResetPresentation()
{
	const bool bHadState = bReady || LocalTeamId >= 1 || Explored.Num() > 0 || Visible.Num() > 0;
	bReady = false;
	LocalTeamId = -1;
	Revision = 0;
	GridOriginWorldXY = FVector2D::ZeroVector;
	GridDimensions = FIntPoint::ZeroValue;
	CellSizeCm = 0.0f;
	Explored.Reset();
	Visible.Reset();
	if (bHadState)
	{
		OnLocalFoWUpdated.Broadcast(this);
	}
}

int32 UGP_LocalFoWComponent::WorldLocationToIndex(const FVector& WorldLocation) const
{
	if (!bReady
		|| !GPLocalFogOfWarPrivate::IsFiniteLocation(WorldLocation)
		|| CellSizeCm <= KINDA_SMALL_NUMBER)
	{
		return INDEX_NONE;
	}

	const int32 X = FMath::FloorToInt((WorldLocation.X - GridOriginWorldXY.X) / CellSizeCm);
	const int32 Y = FMath::FloorToInt((WorldLocation.Y - GridOriginWorldXY.Y) / CellSizeCm);
	if (X < 0 || Y < 0 || X >= GridDimensions.X || Y >= GridDimensions.Y)
	{
		return INDEX_NONE;
	}
	return Y * GridDimensions.X + X;
}

EGP_FoWState UGP_LocalFoWComponent::GetStateAtWorldLocation(const FVector& WorldLocation) const
{
	const int32 Index = WorldLocationToIndex(WorldLocation);
	if (Index == INDEX_NONE || Index >= Explored.Num() || Index >= Visible.Num())
	{
		return EGP_FoWState::Unexplored;
	}
	if (Visible[Index])
	{
		return EGP_FoWState::Visible;
	}
	return Explored[Index] ? EGP_FoWState::Explored : EGP_FoWState::Unexplored;
}

bool UGP_LocalFoWComponent::IsExplored(const FVector& WorldLocation) const
{
	return GetStateAtWorldLocation(WorldLocation) != EGP_FoWState::Unexplored;
}

bool UGP_LocalFoWComponent::IsVisible(const FVector& WorldLocation) const
{
	return GetStateAtWorldLocation(WorldLocation) == EGP_FoWState::Visible;
}

bool UGP_LocalFoWComponent::AllowsLocalPlacementPreview(const FVector& WorldLocation) const
{
	return bReady && IsVisible(WorldLocation);
}

#if !UE_BUILD_SHIPPING

void UGP_LocalFoWComponent::DebugDumpToLog() const
{
	const UWorld* World = GetWorld();
	const AGP_GameState* GameState =
		World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	const UGP_FogOfWarComponent* AuthorityFoW =
		GameState != nullptr ? GameState->GetFogOfWarComponent() : nullptr;
	const float Interval =
		AuthorityFoW != nullptr ? AuthorityFoW->GetUpdateIntervalSeconds() : 0.0f;
	UE_LOG(LogGPLocalFogOfWar, Display,
		TEXT("GP LocalFoW Dump: Renderer=PerCellBlurredQuadRenderer PostProcessActive=false MaskProjectionActive=false World=%s NetMode=%s Owner=%s Ready=%s LocalTeam=%d Revision=%lld CellSize=%.1f Dims=%dx%d Interval=%.2f Origin=%s Explored=%d Visible=%d"),
		*GetNameSafe(World),
		World != nullptr ? GPLocalFogOfWarPrivate::NetModeToString(World->GetNetMode()) : TEXT("None"),
		*GetNameSafe(GetOwner()),
		bReady ? TEXT("true") : TEXT("false"),
		LocalTeamId,
		Revision,
		CellSizeCm,
		GridDimensions.X,
		GridDimensions.Y,
		Interval,
		*GridOriginWorldXY.ToString(),
		DebugGetExploredCellCount(),
		DebugGetVisibleCellCount());
}

namespace GPLocalFogOfWarPrivate
{
	static UGP_LocalFoWComponent* ResolveLocalComponent(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		AGP_PlayerController* PC = Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		return PC != nullptr && PC->IsLocalController() ? PC->GetLocalFogOfWarComponent() : nullptr;
	}

	static void LocalDump(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (UGP_LocalFoWComponent* LocalFoW = ResolveLocalComponent(World))
		{
			LocalFoW->DebugDumpToLog();
			return;
		}
		UE_LOG(LogGPLocalFogOfWar, Warning,
			TEXT("gp.FoW.LocalDump: no local GP PlayerController/FoW mirror in World=%s"),
			*GetNameSafe(World));
	}

	static void LocalQueryState(const TArray<FString>& Args, UWorld* World)
	{
		UGP_LocalFoWComponent* LocalFoW = ResolveLocalComponent(World);
		if (LocalFoW == nullptr || Args.Num() < 2)
		{
			UE_LOG(LogGPLocalFogOfWar, Warning,
				TEXT("gp.FoW.LocalQueryState usage: gp.FoW.LocalQueryState <WorldX> <WorldY>"));
			return;
		}

		const FVector Location(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), 0.0f);
		const EGP_FoWState State = LocalFoW->GetStateAtWorldLocation(Location);
		UE_LOG(LogGPLocalFogOfWar, Display,
			TEXT("GP LocalFoW Query: World=%s NetMode=%s LocalTeam=%d Revision=%lld Ready=%s Location=%s State=%s"),
			*GetNameSafe(World),
			World != nullptr ? NetModeToString(World->GetNetMode()) : TEXT("None"),
			LocalFoW->GetLocalTeamId(),
			LocalFoW->GetRevision(),
			LocalFoW->IsReady() ? TEXT("true") : TEXT("false"),
			*Location.ToCompactString(),
			StateToString(State));
	}

	static FAutoConsoleCommandWithWorldAndArgs GLocalDumpCommand(
		TEXT("gp.FoW.LocalDump"),
		TEXT("Dump the owning client's trusted local FoW presentation mirror."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LocalDump));

	static FAutoConsoleCommandWithWorldAndArgs GLocalQueryCommand(
		TEXT("gp.FoW.LocalQueryState"),
		TEXT("Query local presentation FoW: gp.FoW.LocalQueryState <WorldX> <WorldY>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LocalQueryState));
}

#endif
