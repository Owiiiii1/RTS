// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceNode.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPUnitCommandComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPNavDump, Log, All);

namespace GPNavDumpPrivate
{
	static void ProjectOne(
		UNavigationSystemV1* NavSys,
		const TCHAR* Label,
		const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			UE_LOG(LogGPNavDump, Log, TEXT("gp.Nav.Dump Project %s: actor=none"), Label);
			return;
		}

		const FVector Location = Actor->GetActorLocation();
		const ANavigationData* DefaultNav = NavSys != nullptr
			? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)
			: nullptr;
		if (NavSys == nullptr || DefaultNav == nullptr)
		{
			UE_LOG(LogGPNavDump, Log,
				TEXT("gp.Nav.Dump Project %s: %s Loc=%s Result=NO_NAVDATA"),
				Label,
				*GetNameSafe(Actor),
				*Location.ToCompactString());
			return;
		}

		FNavLocation Projected;
		const bool bProjected = NavSys->ProjectPointToNavigation(
			Location,
			Projected,
			FVector(250.0f, 250.0f, 400.0f));
		UE_LOG(LogGPNavDump, Log,
			TEXT("gp.Nav.Dump Project %s: %s Loc=%s Result=%s Projected=%s"),
			Label,
			*GetNameSafe(Actor),
			*Location.ToCompactString(),
			bProjected ? TEXT("PROJECTED") : TEXT("NAVDATA_EXISTS_BUT_PROJECTION_FAILED"),
			bProjected ? *Projected.Location.ToCompactString() : TEXT("none"));
	}

	static void RunNavDump(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPNavDump, Warning, TEXT("gp.Nav.Dump: no world"));
			return;
		}

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const ANavigationData* DefaultNav =
			NavSys != nullptr ? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;

		int32 RecastCount = 0;
		int32 NavDataCount = 0;
		for (TActorIterator<ANavigationData> It(World); It; ++It)
		{
			if (!IsValid(*It))
			{
				continue;
			}
			++NavDataCount;
			if (Cast<ARecastNavMesh>(*It) != nullptr)
			{
				++RecastCount;
			}
		}

		int32 BoundsCount = 0;
		for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
		{
			if (!IsValid(*It))
			{
				continue;
			}
			++BoundsCount;
			const FBox Bounds = It->GetComponentsBoundingBox(true);
			UE_LOG(LogGPNavDump, Log,
				TEXT("gp.Nav.Dump Bounds[%d]: %s Origin=%s Extent=%s"),
				BoundsCount - 1,
				*GetNameSafe(*It),
				*It->GetActorLocation().ToCompactString(),
				*Bounds.GetExtent().ToCompactString());
		}

		const bool bAsync = NavSys != nullptr && NavSys->IsNavigationBuildingLocked(ENavigationBuildLock::AsyncLoadLock);
		const bool bInitial = NavSys != nullptr && NavSys->IsNavigationBuildingLocked(ENavigationBuildLock::InitialLock);
		const bool bNoEditor = NavSys != nullptr && NavSys->IsNavigationBuildingLocked(ENavigationBuildLock::NoUpdateInEditor);
		const bool bNoPIE = NavSys != nullptr && NavSys->IsNavigationBuildingLocked(ENavigationBuildLock::NoUpdateInPIE);
		const bool bCustom = NavSys != nullptr && NavSys->IsNavigationBuildingLocked(ENavigationBuildLock::Custom);

		UE_LOG(LogGPNavDump, Log,
			TEXT("gp.Nav.Dump NavSys=%s DefaultNav=%s Class=%s RecastCount=%d NavDataCount=%d BoundsCount=%d LockedAsyncLoad=%s LockedInitial=%s LockedNoEditor=%s LockedNoPIE=%s LockedCustom=%s AsyncLoadIs0x20=%s"),
			NavSys != nullptr ? TEXT("true") : TEXT("false"),
			*GetNameSafe(DefaultNav),
			DefaultNav != nullptr ? *DefaultNav->GetClass()->GetName() : TEXT("none"),
			RecastCount,
			NavDataCount,
			BoundsCount,
			bAsync ? TEXT("true") : TEXT("false"),
			bInitial ? TEXT("true") : TEXT("false"),
			bNoEditor ? TEXT("true") : TEXT("false"),
			bNoPIE ? TEXT("true") : TEXT("false"),
			bCustom ? TEXT("true") : TEXT("false"),
			TEXT("true"));

		AGP_Worker* Worker = nullptr;
		AGP_MainBase* Base = nullptr;
		AGP_ResourceNode* Node = nullptr;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Worker = *It;
				break;
			}
		}
		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Base = *It;
				break;
			}
		}
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Node = *It;
				break;
			}
		}

		ProjectOne(NavSys, TEXT("Worker"), Worker);
		ProjectOne(NavSys, TEXT("MainBase"), Base);
		ProjectOne(NavSys, TEXT("ResourceNode"), Node);
	}

	static const TCHAR* MineExecName(EGP_MineExecutionState State)
	{
		switch (State)
		{
		case EGP_MineExecutionState::Idle: return TEXT("Idle");
		case EGP_MineExecutionState::Approaching: return TEXT("Approaching");
		case EGP_MineExecutionState::Active: return TEXT("Active");
		case EGP_MineExecutionState::WaitingForResource: return TEXT("WaitingForResource");
		default: return TEXT("Unknown");
		}
	}

	static const TCHAR* HaulExecName(EGP_HaulExecutionState State)
	{
		switch (State)
		{
		case EGP_HaulExecutionState::Idle: return TEXT("Idle");
		case EGP_HaulExecutionState::ReturningToBase: return TEXT("ReturningToBase");
		case EGP_HaulExecutionState::DroppingOff: return TEXT("DroppingOff");
		case EGP_HaulExecutionState::WaitingForDropOff: return TEXT("WaitingForDropOff");
		case EGP_HaulExecutionState::ReturningToDeposit: return TEXT("ReturningToDeposit");
		case EGP_HaulExecutionState::Failed: return TEXT("Failed");
		default: return TEXT("Unknown");
		}
	}

	static const TCHAR* MiningStateName(EGP_MiningState State)
	{
		switch (State)
		{
		case EGP_MiningState::Idle: return TEXT("Idle");
		case EGP_MiningState::WaitingForSlot: return TEXT("WaitingForSlot");
		case EGP_MiningState::Mining: return TEXT("Mining");
		case EGP_MiningState::CargoFull: return TEXT("CargoFull");
		case EGP_MiningState::DepositDepleted: return TEXT("DepositDepleted");
		case EGP_MiningState::OutOfRange: return TEXT("OutOfRange");
		case EGP_MiningState::Invalid: return TEXT("Invalid");
		default: return TEXT("Unknown");
		}
	}

	static void RunWorkerDump(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPNavDump, Warning, TEXT("gp.Worker.DumpState: no world"));
			return;
		}

		int32 Count = 0;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			AGP_Worker* Worker = *It;
			if (!IsValid(Worker))
			{
				continue;
			}
			++Count;
			UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
			UGP_MovementComponent* Movement = Worker->GetUnitMovementComponent();
			UGP_MiningComponent* Mining = Worker->GetMiningComponent();
			UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
			const FGP_StoredUnitCommand* Held = Cmd != nullptr ? Cmd->GetHeldCommand() : nullptr;
			UE_LOG(LogGPNavDump, Log,
				TEXT("gp.Worker.DumpState %s HeldTag=%s HeldSerial=%u MineState=%s MineSerial=%u HaulState=%s HaulSerial=%u Mining=%s Cargo=%.1f/%.1f Moving=%s MoveSerial=%u Dest=%s PathMode=%s UsedNav=%s"),
				*GetNameSafe(Worker),
				Held != nullptr ? *Held->CommandTag.ToString() : TEXT("none"),
				Held != nullptr ? Held->CommandSerial : 0u,
				Cmd != nullptr ? MineExecName(Cmd->GetMineExecutionState()) : TEXT("none"),
				Cmd != nullptr ? Cmd->GetActiveMineSerial() : 0u,
				Cmd != nullptr ? HaulExecName(Cmd->GetHaulExecutionState()) : TEXT("none"),
				Cmd != nullptr ? Cmd->GetActiveHaulSerial() : 0u,
				Mining != nullptr ? MiningStateName(Mining->GetMiningState()) : TEXT("none"),
				Cargo != nullptr ? Cargo->GetCurrentCargoAmount() : -1.0f,
				Cargo != nullptr ? Cargo->GetCargoCapacity() : -1.0f,
				Movement != nullptr && Movement->IsMoving() ? TEXT("true") : TEXT("false"),
				Movement != nullptr ? Movement->GetActiveMoveSerial() : 0u,
				Movement != nullptr ? *Movement->GetMoveDestination().ToCompactString() : TEXT("none"),
				Movement != nullptr ? *Movement->DebugGetLastPathMode() : TEXT("none"),
				Movement != nullptr && Movement->IsActivePathFromNavigation() ? TEXT("true") : TEXT("false"));
		}

		UE_LOG(LogGPNavDump, Log, TEXT("gp.Worker.DumpState Workers=%d"), Count);
	}

	static FAutoConsoleCommandWithWorldAndArgs GNavDump(
		TEXT("gp.Nav.Dump"),
		TEXT("Non-shipping: navigation system, Recast, bounds, and projection at Worker/Base/Node."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunNavDump));

	static FAutoConsoleCommandWithWorldAndArgs GWorkerDump(
		TEXT("gp.Worker.DumpState"),
		TEXT("Non-shipping: held command, mine/haul, cargo, and last movement path mode."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunWorkerDump));
}

#endif
