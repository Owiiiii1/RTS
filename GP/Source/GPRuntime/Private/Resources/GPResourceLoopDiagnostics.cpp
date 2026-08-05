// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPResourceLoopDiagnostics.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "NavigationSystem.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceNode.h"
#include "Resources/GPStorageComponent.h"
#include "Units/GPWorker.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPResourceLoopDiag, Log, All);

namespace GPResourceLoopDiagnostics
{
	static constexpr float DiagnosticOriginX = -45000.0f;
	static constexpr float DiagnosticOriginY = 0.0f;
	static constexpr float DiagnosticOriginZ = 100.0f;
	static constexpr float DiagnosticNodeOffsetX = 2000.0f;
	static constexpr float DiagnosticWorkerNearNode = 150.0f;

	FVector GetDiagnosticMainBaseLocation()
	{
		return FVector(DiagnosticOriginX, DiagnosticOriginY, DiagnosticOriginZ);
	}

	FVector GetDiagnosticResourceNodeLocation()
	{
		return FVector(DiagnosticOriginX + DiagnosticNodeOffsetX, DiagnosticOriginY, DiagnosticOriginZ);
	}

	FVector GetDiagnosticWorkerLocation()
	{
		return GetDiagnosticResourceNodeLocation() + FVector(DiagnosticWorkerNearNode, 0.0f, 0.0f);
	}

	static void MakeTransientUniqueName(AActor* Actor, const TCHAR* Prefix, int32 TeamId)
	{
		if (!IsValid(Actor))
		{
			return;
		}
		const FName Unique = MakeUniqueObjectName(
			Actor->GetOuter(),
			Actor->GetClass(),
			*FString::Printf(TEXT("%s_T%d"), Prefix, TeamId));
		Actor->Rename(*Unique.ToString(), nullptr, REN_DoNotDirty | REN_NonTransactional);
	}

	bool IsNavPointProjected(UWorld* World, const FVector& Location, FVector* OutProjected)
	{
		if (!IsValid(World))
		{
			return false;
		}
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSys == nullptr)
		{
			return false;
		}
		FNavLocation Projected;
		const bool bOk = NavSys->ProjectPointToNavigation(Location, Projected, FVector(500.0f, 500.0f, 500.0f));
		if (bOk && OutProjected != nullptr)
		{
			*OutProjected = Projected.Location;
		}
		return bOk;
	}

	bool IsNavReachable(UWorld* World, const FVector& From, const FVector& To)
	{
		if (!IsValid(World))
		{
			return false;
		}
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSys == nullptr)
		{
			return false;
		}

		FVector ProjectedFrom = From;
		FVector ProjectedTo = To;
		if (!IsNavPointProjected(World, From, &ProjectedFrom) || !IsNavPointProjected(World, To, &ProjectedTo))
		{
			return false;
		}

		const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		if (NavData == nullptr)
		{
			return false;
		}

		FPathFindingQuery Query(NavSys, *NavData, ProjectedFrom, ProjectedTo);
		return NavSys->TestPathSync(Query, EPathFindingMode::Regular);
	}

	AGP_MainBase* SpawnMainBaseDeferred(UWorld* World, const FVector& Location, int32 TeamId)
	{
		if (!IsValid(World) || TeamId < 1)
		{
			return nullptr;
		}

		const FTransform Xform(FRotator::ZeroRotator, Location);
		AGP_MainBase* Base = World->SpawnActorDeferred<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			Xform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!IsValid(Base))
		{
			return nullptr;
		}

		Base->SetFlags(RF_Transient);
		MakeTransientUniqueName(Base, TEXT("GP_DiagMainBase"), TeamId);
		// Assign TeamId before FinishSpawning so BeginPlay can register with a playable team.
		Base->SetTeamId(TeamId);
		Base->FinishSpawning(Xform);
		if (Base->GetTeamId() != TeamId)
		{
			Base->SetTeamId(TeamId);
		}
		else
		{
			Base->RefreshMainBaseRegistration();
		}
		if (!Base->GetActorLocation().Equals(Location, 1.0f))
		{
			Base->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		}
		return Base;
	}

	AGP_Worker* SpawnWorkerDeferred(UWorld* World, const FVector& Location, int32 TeamId)
	{
		if (!IsValid(World) || TeamId < 1)
		{
			return nullptr;
		}

		const FTransform Xform(FRotator::ZeroRotator, Location);
		AGP_Worker* Worker = World->SpawnActorDeferred<AGP_Worker>(
			AGP_Worker::StaticClass(),
			Xform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!IsValid(Worker))
		{
			return nullptr;
		}

		Worker->SetFlags(RF_Transient);
		MakeTransientUniqueName(Worker, TEXT("GP_DiagWorker"), TeamId);
		Worker->SetTeamId(TeamId);
		Worker->FinishSpawning(Xform);
		if (Worker->GetTeamId() != TeamId)
		{
			Worker->SetTeamId(TeamId);
		}
		if (!Worker->GetActorLocation().Equals(Location, 1.0f))
		{
			Worker->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		}
		return Worker;
	}

	AGP_ResourceNode* SpawnResourceNodeTransient(UWorld* World, const FVector& Location)
	{
		if (!IsValid(World))
		{
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_ResourceNode* Node = World->SpawnActor<AGP_ResourceNode>(
			AGP_ResourceNode::StaticClass(),
			Location,
			FRotator::ZeroRotator,
			Params);
		if (!IsValid(Node))
		{
			return nullptr;
		}

		Node->SetResourceDefinitionSoft(TSoftObjectPtr<UGP_ResourceDefinition>(
			FSoftObjectPath(UGP_ResourceDefinition::DefaultFerroniteAssetPath())));
		Node->ResolveResourceDefinition(true);
		MakeTransientUniqueName(Node, TEXT("GP_DiagResourceNode"), 0);
		if (!Node->GetActorLocation().Equals(Location, 1.0f))
		{
			Node->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		}
		return Node;
	}

	static AGP_MainBase* FindExistingMainBaseForTeam(UWorld* World, int32 TeamId)
	{
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			if (AGP_MainBase* Registered = GS->FindMainBaseForTeam(TeamId))
			{
				return Registered;
			}
		}
		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->GetTeamId() == TeamId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static AGP_Worker* FindExistingWorkerForTeam(UWorld* World, int32 TeamId)
	{
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->GetTeamId() == TeamId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static AGP_ResourceNode* FindMineableNodeNear(UWorld* World, const FVector& Preferred)
	{
		AGP_ResourceNode* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			AGP_ResourceNode* Node = *It;
			if (!IsValid(Node) || Node->IsDepleted())
			{
				continue;
			}
			FString Fail;
			if (!Node->CanAcceptMineCommand(true, &Fail))
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(Node->GetActorLocation(), Preferred);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Node;
			}
		}
		return Best;
	}

	FGP_DiagnosticScenarioActors SpawnDiagnosticScenario(UWorld* World, int32 TeamId)
	{
		FGP_DiagnosticScenarioActors Result;
		Result.TeamId = TeamId < 1 ? 1 : TeamId;
		TeamId = Result.TeamId;

		if (!IsValid(World) || World->GetNetMode() == NM_Client)
		{
			Result.Error = TEXT("MissingWorldOrClient");
			return Result;
		}

		const FVector BaseLoc = GetDiagnosticMainBaseLocation();
		const FVector NodeLoc = GetDiagnosticResourceNodeLocation();
		const FVector WorkerLoc = GetDiagnosticWorkerLocation();

		Result.MainBase = FindExistingMainBaseForTeam(World, TeamId);
		if (!IsValid(Result.MainBase))
		{
			Result.MainBase = SpawnMainBaseDeferred(World, BaseLoc, TeamId);
			Result.bCreatedMainBase = IsValid(Result.MainBase);
		}
		else if (Result.MainBase->GetTeamId() != TeamId)
		{
			Result.MainBase->SetTeamId(TeamId);
		}
		else
		{
			Result.MainBase->RefreshMainBaseRegistration();
		}

		if (!IsValid(Result.MainBase) || Result.MainBase->GetTeamId() != TeamId)
		{
			Result.Error = TEXT("MainBaseSpawnOrTeamFailed");
			return Result;
		}

		// Deterministic: always use a transient diagnostic node at the known layout.
		// Do not depend on authored map ResourceNodes.
		{
			AGP_ResourceNode* ExistingDiagNode = nullptr;
			for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
			{
				AGP_ResourceNode* Candidate = *It;
				if (!IsValid(Candidate) || !Candidate->HasAnyFlags(RF_Transient))
				{
					continue;
				}
				if (FVector::DistSquared(Candidate->GetActorLocation(), NodeLoc) <= FMath::Square(400.0f)
					&& !Candidate->IsDepleted()
					&& Candidate->GetCurrentAmount() > 0)
				{
					ExistingDiagNode = Candidate;
					break;
				}
			}
			if (IsValid(ExistingDiagNode))
			{
				Result.ResourceNode = ExistingDiagNode;
			}
			else
			{
				Result.ResourceNode = SpawnResourceNodeTransient(World, NodeLoc);
				Result.bCreatedResourceNode = IsValid(Result.ResourceNode);
			}
		}

		if (!IsValid(Result.ResourceNode)
			|| Result.ResourceNode->GetCurrentAmount() <= 0
			|| Result.ResourceNode->IsDepleted())
		{
			Result.Error = TEXT("ResourceNodeInvalid");
			return Result;
		}

		Result.Worker = FindExistingWorkerForTeam(World, TeamId);
		if (!IsValid(Result.Worker))
		{
			Result.Worker = SpawnWorkerDeferred(World, WorkerLoc, TeamId);
			Result.bCreatedWorker = IsValid(Result.Worker);
		}
		else
		{
			if (Result.Worker->GetTeamId() != TeamId)
			{
				Result.Worker->SetTeamId(TeamId);
			}
			Result.Worker->SetActorLocation(WorkerLoc, false, nullptr, ETeleportType::TeleportPhysics);
		}

		if (!IsValid(Result.Worker) || Result.Worker->GetTeamId() != TeamId)
		{
			Result.Error = TEXT("WorkerSpawnOrTeamFailed");
			return Result;
		}

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		AGP_MainBase* Resolved = GS != nullptr ? GS->FindMainBaseForTeam(TeamId) : nullptr;
		if (Resolved != Result.MainBase)
		{
			Result.MainBase->RefreshMainBaseRegistration();
			Resolved = GS != nullptr ? GS->FindMainBaseForTeam(TeamId) : nullptr;
		}
		if (Resolved != Result.MainBase)
		{
			Result.Error = TEXT("MainBaseRegistryResolveFailed");
			return Result;
		}

		Result.bNavWorkerToNode = IsNavReachable(World, Result.Worker->GetActorLocation(), Result.ResourceNode->GetActorLocation());
		Result.bNavNodeToBase = IsNavReachable(World, Result.ResourceNode->GetActorLocation(), Result.MainBase->GetActorLocation());
		Result.bOk = true;

		UGP_StorageComponent* Storage = Result.MainBase->GetStorageComponent();
		UGP_CargoComponent* Cargo = Result.Worker->GetCargoComponent();
		UE_LOG(LogGPResourceLoopDiag, Log,
			TEXT("GP Resource.SpawnDiagnosticScenario: Ok=true TeamId=%d MainBase=%s MainBaseTeam=%d Worker=%s WorkerTeam=%d Node=%s NodeAmount=%d Stored=%.1f/%.1f Cargo=%.1f/%.1f WorkerLoc=%s NodeLoc=%s BaseLoc=%s NavWorkerToNode=%s NavNodeToBase=%s CreatedBase=%s CreatedWorker=%s CreatedNode=%s"),
			TeamId,
			*GetNameSafe(Result.MainBase),
			Result.MainBase->GetTeamId(),
			*GetNameSafe(Result.Worker),
			Result.Worker->GetTeamId(),
			*GetNameSafe(Result.ResourceNode),
			Result.ResourceNode->GetCurrentAmount(),
			IsValid(Storage) ? Storage->GetTotalStored() : -1.0f,
			IsValid(Storage) ? Storage->GetTotalCapacity() : -1.0f,
			IsValid(Cargo) ? Cargo->GetCurrentCargoAmount() : -1.0f,
			IsValid(Cargo) ? Cargo->GetCargoCapacity() : -1.0f,
			*Result.Worker->GetActorLocation().ToCompactString(),
			*Result.ResourceNode->GetActorLocation().ToCompactString(),
			*Result.MainBase->GetActorLocation().ToCompactString(),
			Result.bNavWorkerToNode ? TEXT("true") : TEXT("false"),
			Result.bNavNodeToBase ? TEXT("true") : TEXT("false"),
			Result.bCreatedMainBase ? TEXT("true") : TEXT("false"),
			Result.bCreatedWorker ? TEXT("true") : TEXT("false"),
			Result.bCreatedResourceNode ? TEXT("true") : TEXT("false"));

		return Result;
	}

	FGP_ScenarioValidation ValidateHaulingScenario(UWorld* World, int32 TeamId, AGP_Worker* WorkerHint)
	{
		FGP_ScenarioValidation V;
		if (!IsValid(World))
		{
			++V.Errors;
			return V;
		}

		TeamId = TeamId < 1 ? 1 : TeamId;
		V.bPlayableTeamValid = TeamId >= 1;

		AGP_Worker* Worker = WorkerHint;
		if (!IsValid(Worker))
		{
			Worker = FindExistingWorkerForTeam(World, TeamId);
		}
		AGP_MainBase* Base = FindExistingMainBaseForTeam(World, TeamId);

		AGP_ResourceNode* Node = nullptr;
		const FVector PreferredNode = GetDiagnosticResourceNodeLocation();
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			AGP_ResourceNode* Candidate = *It;
			if (!IsValid(Candidate) || !Candidate->HasAnyFlags(RF_Transient))
			{
				continue;
			}
			if (FVector::DistSquared(Candidate->GetActorLocation(), PreferredNode) <= FMath::Square(500.0f)
				&& Candidate->GetCurrentAmount() > 0
				&& !Candidate->IsDepleted())
			{
				Node = Candidate;
				break;
			}
		}
		if (!IsValid(Node))
		{
			Node = FindMineableNodeNear(World, PreferredNode);
		}

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		AGP_MainBase* Registered = GS != nullptr ? GS->FindMainBaseForTeam(TeamId) : nullptr;
		V.bMainBaseRegisteredForTeam = IsValid(Registered) && Registered == Base && IsValid(Base);

		V.bWorkerHasMainBase = IsValid(Worker) && IsValid(Registered) && Registered->GetTeamId() == Worker->GetTeamId();
		V.bWorkerHasResourceNode = IsValid(Worker) && IsValid(Node);
		V.bWorkerAndBaseSameTeam = IsValid(Worker) && IsValid(Base) && Worker->GetTeamId() == Base->GetTeamId() && Worker->GetTeamId() == TeamId;

		FString Fail;
		V.bNodeMineable = IsValid(Node) && Node->CanAcceptMineCommand(true, &Fail) && Node->GetCurrentAmount() > 0
			&& Node->GetMaxConcurrentMiners() >= 1;

		if (IsValid(Worker) && IsValid(Node))
		{
			V.bNavReachableWorkerToNode = IsNavReachable(World, Worker->GetActorLocation(), Node->GetActorLocation());
			if (!V.bNavReachableWorkerToNode)
			{
				++V.Warnings;
			}
		}
		else
		{
			++V.Errors;
		}

		if (IsValid(Node) && IsValid(Base))
		{
			V.bNavReachableNodeToBase = IsNavReachable(World, Node->GetActorLocation(), Base->GetActorLocation());
			if (!V.bNavReachableNodeToBase)
			{
				++V.Warnings;
			}
		}
		else
		{
			++V.Errors;
		}

		if (!V.bPlayableTeamValid || !V.bWorkerHasMainBase || !V.bWorkerHasResourceNode
			|| !V.bMainBaseRegisteredForTeam || !V.bWorkerAndBaseSameTeam || !V.bNodeMineable)
		{
			++V.Errors;
		}

		// Nav missing is warning-only so PIE without rebuilt paths can still functional-test haul via forced cycles,
		// but ReadyForHaulingTest requires projected path when NavSys exists.
		const bool bNavSysPresent = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) != nullptr;
		const bool bNavOk = !bNavSysPresent
			|| (V.bNavReachableWorkerToNode && V.bNavReachableNodeToBase);

		V.bReadyForHaulingTest =
			V.bPlayableTeamValid
			&& V.bWorkerHasMainBase
			&& V.bWorkerHasResourceNode
			&& V.bMainBaseRegisteredForTeam
			&& V.bWorkerAndBaseSameTeam
			&& V.bNodeMineable
			&& bNavOk
			&& V.Errors == 0;

		return V;
	}

	void DestroyDiagnosticScenarioActors(AGP_MainBase* MainBase, AGP_Worker* Worker, AGP_ResourceNode* Node)
	{
		if (IsValid(Worker))
		{
			Worker->Destroy();
		}
		if (IsValid(Node) && Node->HasAnyFlags(RF_Transient))
		{
			Node->Destroy();
		}
		if (IsValid(MainBase))
		{
			MainBase->Destroy();
		}
	}
}

#endif // !UE_BUILD_SHIPPING
