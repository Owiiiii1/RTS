// Copyright Epic Games, Inc. All Rights Reserved.

#include "FogOfWar/GPFogOfWarComponent.h"

#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerState.h"
#include "Units/GPUnitBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFogOfWar, Log, All);

namespace GPFogOfWarPrivate
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
}

UGP_FogOfWarComponent::UGP_FogOfWarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.TickInterval = UpdateIntervalSeconds;
	SetIsReplicatedByDefault(false);
}

void UGP_FogOfWarComponent::BeginPlay()
{
	Super::BeginPlay();

	CellSizeCm = FMath::Max(50.0f, CellSizeCm);
	GridDimensions.X = FMath::Max(1, GridDimensions.X);
	GridDimensions.Y = FMath::Max(1, GridDimensions.Y);
	UpdateIntervalSeconds = FMath::Max(0.05f, UpdateIntervalSeconds);
	PrimaryComponentTick.TickInterval = UpdateIntervalSeconds;
	SetComponentTickEnabled(HasAuthoritativeOwner());

	if (HasAuthoritativeOwner())
	{
		RecomputeVisibilityNow();
	}
}

void UGP_FogOfWarComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RecomputeVisibilityNow();
}

bool UGP_FogOfWarComponent::HasAuthoritativeOwner() const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	return OwnerActor != nullptr
		&& OwnerActor->HasAuthority()
		&& World != nullptr
		&& World->GetNetMode() != NM_Client;
}

int32 UGP_FogOfWarComponent::CellToIndex(const FIntPoint& Cell) const
{
	return IsCellInBounds(Cell) ? Cell.Y * GridDimensions.X + Cell.X : INDEX_NONE;
}

bool UGP_FogOfWarComponent::IsCellInBounds(const FIntPoint& Cell) const
{
	return Cell.X >= 0 && Cell.Y >= 0
		&& Cell.X < GridDimensions.X && Cell.Y < GridDimensions.Y;
}

bool UGP_FogOfWarComponent::WorldToCell(const FVector& WorldLocation, FIntPoint& OutCell) const
{
	OutCell = FIntPoint::ZeroValue;
	if (!GPFogOfWarPrivate::IsFiniteLocation(WorldLocation) || CellSizeCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutCell.X = FMath::FloorToInt((WorldLocation.X - GridOriginWorldXY.X) / CellSizeCm);
	OutCell.Y = FMath::FloorToInt((WorldLocation.Y - GridOriginWorldXY.Y) / CellSizeCm);
	return IsCellInBounds(OutCell);
}

FVector UGP_FogOfWarComponent::GetCellCenterWorld(const FIntPoint& Cell, float Z) const
{
	return FVector(
		GridOriginWorldXY.X + (static_cast<float>(Cell.X) + 0.5f) * CellSizeCm,
		GridOriginWorldXY.Y + (static_cast<float>(Cell.Y) + 0.5f) * CellSizeCm,
		Z);
}

UGP_FogOfWarComponent::FTeamGrid& UGP_FogOfWarComponent::FindOrAddTeamGrid(int32 TeamId)
{
	FTeamGrid& Grid = TeamGrids.FindOrAdd(TeamId);
	const int32 NumCells = GridDimensions.X * GridDimensions.Y;
	if (Grid.Explored.Num() != NumCells)
	{
		Grid.Explored.Init(false, NumCells);
	}
	if (Grid.Visible.Num() != NumCells)
	{
		Grid.Visible.Init(false, NumCells);
	}
	return Grid;
}

const UGP_FogOfWarComponent::FTeamGrid* UGP_FogOfWarComponent::FindTeamGrid(int32 TeamId) const
{
	return TeamGrids.Find(TeamId);
}

EGP_FoWState UGP_FogOfWarComponent::GetStateForTeamAtWorldLocation(
	int32 TeamId,
	const FVector& WorldLocation) const
{
	if (!HasAuthoritativeOwner() || TeamId < 1)
	{
		return EGP_FoWState::Unexplored;
	}

	FIntPoint Cell;
	if (!WorldToCell(WorldLocation, Cell))
	{
		return EGP_FoWState::Unexplored;
	}

	const FTeamGrid* Grid = FindTeamGrid(TeamId);
	const int32 Index = CellToIndex(Cell);
	if (Grid == nullptr || Index == INDEX_NONE)
	{
		return EGP_FoWState::Unexplored;
	}
	if (Grid->Visible[Index])
	{
		return EGP_FoWState::Visible;
	}
	return Grid->Explored[Index] ? EGP_FoWState::Explored : EGP_FoWState::Unexplored;
}

bool UGP_FogOfWarComponent::IsExploredByTeam(int32 TeamId, const FVector& WorldLocation) const
{
	return GetStateForTeamAtWorldLocation(TeamId, WorldLocation) != EGP_FoWState::Unexplored;
}

bool UGP_FogOfWarComponent::IsVisibleToTeam(int32 TeamId, const FVector& WorldLocation) const
{
	return GetStateForTeamAtWorldLocation(TeamId, WorldLocation) == EGP_FoWState::Visible;
}

bool UGP_FogOfWarComponent::IsCellVisibleToTeam(int32 TeamId, const FIntPoint& Cell) const
{
	if (!HasAuthoritativeOwner() || TeamId < 1)
	{
		return false;
	}
	const FTeamGrid* Grid = FindTeamGrid(TeamId);
	const int32 Index = CellToIndex(Cell);
	return Grid != nullptr && Index != INDEX_NONE && Grid->Visible[Index];
}

bool UGP_FogOfWarComponent::RegisterSightSource(AGP_UnitBase* Source)
{
	if (!HasAuthoritativeOwner()
		|| !IsValid(Source)
		|| !Source->HasAuthority()
		|| Source->GetWorld() != GetWorld())
	{
		return false;
	}

	PruneSightSources();
	SightSources.AddUnique(Source);
	RecomputeVisibilityNow();
	return true;
}

void UGP_FogOfWarComponent::UnregisterSightSource(AGP_UnitBase* Source)
{
	if (!HasAuthoritativeOwner() || Source == nullptr)
	{
		return;
	}

	SightSources.RemoveAllSwap(
		[Source](const TWeakObjectPtr<AGP_UnitBase>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Source;
		},
		EAllowShrinking::No);
	RecomputeVisibilityNow();
}

void UGP_FogOfWarComponent::PruneSightSources()
{
	SightSources.RemoveAllSwap(
		[this](const TWeakObjectPtr<AGP_UnitBase>& Entry)
		{
			const AGP_UnitBase* Source = Entry.Get();
			return !IsValid(Source)
				|| Source->GetWorld() != GetWorld()
				|| Source->IsActorBeingDestroyed();
		},
		EAllowShrinking::No);
}

void UGP_FogOfWarComponent::MarkVisibleCircle(
	FTeamGrid& TeamGrid,
	const FVector& CenterWorld,
	float RadiusCm)
{
	FIntPoint CenterCell;
	if (!WorldToCell(CenterWorld, CenterCell))
	{
		return;
	}

	const float SafeRadius = FMath::Max(0.0f, RadiusCm);
	const int32 RadiusCells = FMath::CeilToInt(SafeRadius / CellSizeCm);
	const float RadiusSq = SafeRadius * SafeRadius;

	for (int32 Y = CenterCell.Y - RadiusCells; Y <= CenterCell.Y + RadiusCells; ++Y)
	{
		for (int32 X = CenterCell.X - RadiusCells; X <= CenterCell.X + RadiusCells; ++X)
		{
			const FIntPoint Cell(X, Y);
			const int32 Index = CellToIndex(Cell);
			if (Index == INDEX_NONE)
			{
				continue;
			}

			const FVector CellCenter = GetCellCenterWorld(Cell, CenterWorld.Z);
			if (FVector::DistSquared2D(CellCenter, CenterWorld) > RadiusSq + KINDA_SMALL_NUMBER)
			{
				continue;
			}

			TeamGrid.Visible[Index] = true;
			TeamGrid.Explored[Index] = true;
		}
	}
}

void UGP_FogOfWarComponent::RecomputeVisibilityNow()
{
	if (!HasAuthoritativeOwner())
	{
		return;
	}

	PruneSightSources();

	if (const AGameStateBase* GameState = Cast<AGameStateBase>(GetOwner()))
	{
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			const AGP_PlayerState* GPPlayerState = Cast<AGP_PlayerState>(PlayerState);
			if (GPPlayerState != nullptr && GPPlayerState->GetTeamId() >= 1)
			{
				FindOrAddTeamGrid(GPPlayerState->GetTeamId());
			}
		}
	}

	for (const TWeakObjectPtr<AGP_UnitBase>& Entry : SightSources)
	{
		const AGP_UnitBase* Source = Entry.Get();
		if (IsValid(Source) && Source->GetTeamId() >= 1)
		{
			FindOrAddTeamGrid(Source->GetTeamId());
		}
	}

	for (TPair<int32, FTeamGrid>& Pair : TeamGrids)
	{
		Pair.Value.Visible.Init(false, Pair.Value.Visible.Num());
	}

	for (const TWeakObjectPtr<AGP_UnitBase>& Entry : SightSources)
	{
		const AGP_UnitBase* Source = Entry.Get();
		if (!IsValid(Source)
			|| Source->IsDead()
			|| Source->GetTeamId() < 1
			|| !Source->GrantsFogOfWarVision())
		{
			continue;
		}

		const float RadiusCm = Source->GetFogOfWarSightRadiusCm();
		if (!FMath::IsFinite(RadiusCm) || RadiusCm <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		MarkVisibleCircle(
			FindOrAddTeamGrid(Source->GetTeamId()),
			Source->GetActorLocation(),
			RadiusCm);
	}
}

#if !UE_BUILD_SHIPPING

int32 UGP_FogOfWarComponent::DebugGetRegisteredSightSourceCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AGP_UnitBase>& Entry : SightSources)
	{
		if (Entry.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

int32 UGP_FogOfWarComponent::DebugGetVisibleCellCountForTeam(int32 TeamId) const
{
	const FTeamGrid* Grid = FindTeamGrid(TeamId);
	return Grid != nullptr ? Grid->Visible.CountSetBits() : 0;
}

int32 UGP_FogOfWarComponent::DebugGetExploredCellCountForTeam(int32 TeamId) const
{
	const FTeamGrid* Grid = FindTeamGrid(TeamId);
	return Grid != nullptr ? Grid->Explored.CountSetBits() : 0;
}

void UGP_FogOfWarComponent::DebugDumpToLog() const
{
	UE_LOG(LogGPFogOfWar, Display,
		TEXT("GP FoW Dump: Sources=%d CellSize=%.1f Origin=%s Dims=%s Interval=%.2f"),
		DebugGetRegisteredSightSourceCount(),
		GetCellSizeCm(),
		*GetGridOriginWorldXY().ToString(),
		*GetGridDimensions().ToString(),
		GetUpdateIntervalSeconds());

	for (const TWeakObjectPtr<AGP_UnitBase>& Entry : SightSources)
	{
		const AGP_UnitBase* Source = Entry.Get();
		if (!IsValid(Source))
		{
			continue;
		}
		UE_LOG(LogGPFogOfWar, Display,
			TEXT("GP FoW Source: Actor=%s Team=%d Location=%s Radius=%.1f Grants=%s Dead=%s"),
			*GetNameSafe(Source),
			Source->GetTeamId(),
			*Source->GetActorLocation().ToCompactString(),
			Source->GetFogOfWarSightRadiusCm(),
			Source->GrantsFogOfWarVision() ? TEXT("true") : TEXT("false"),
			Source->IsDead() ? TEXT("true") : TEXT("false"));
	}
}

void UGP_FogOfWarComponent::DebugResetAllState()
{
	if (HasAuthoritativeOwner())
	{
		TeamGrids.Reset();
		RecomputeVisibilityNow();
	}
}

namespace GPFogOfWarPrivate
{
	static UGP_FogOfWarComponent* ResolveComponent(UWorld* World)
	{
		AGP_GameState* GameState = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
		return GameState != nullptr ? GameState->GetFogOfWarComponent() : nullptr;
	}

	static void QueryState(const TArray<FString>& Args, UWorld* World)
	{
		UGP_FogOfWarComponent* FoW = ResolveComponent(World);
		if (FoW == nullptr || Args.Num() < 3)
		{
			UE_LOG(LogGPFogOfWar, Warning,
				TEXT("gp.FoW.QueryState usage: gp.FoW.QueryState <TeamId> <WorldX> <WorldY>"));
			return;
		}

		const int32 TeamId = FCString::Atoi(*Args[0]);
		const FVector Location(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), 0.0f);
		const EGP_FoWState State = FoW->GetStateForTeamAtWorldLocation(TeamId, Location);
		UE_LOG(LogGPFogOfWar, Display,
			TEXT("GP FoW Query: Team=%d Location=%s State=%s"),
			TeamId,
			*Location.ToCompactString(),
			StateToString(State));
	}

	static void DumpState(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		UGP_FogOfWarComponent* FoW = ResolveComponent(World);
		if (FoW == nullptr)
		{
			UE_LOG(LogGPFogOfWar, Warning, TEXT("gp.FoW.DebugDump: missing authoritative FoW component"));
			return;
		}

		FoW->DebugDumpToLog();
	}

	static FAutoConsoleCommandWithWorldAndArgs GQueryStateCommand(
		TEXT("gp.FoW.QueryState"),
		TEXT("Query authoritative FoW: gp.FoW.QueryState <TeamId> <WorldX> <WorldY>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&QueryState));

	static FAutoConsoleCommandWithWorldAndArgs GDumpStateCommand(
		TEXT("gp.FoW.DebugDump"),
		TEXT("Dump authoritative FoW grid and sight-source summary."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DumpState));
}

#endif
