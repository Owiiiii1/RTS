// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPResourceLoopDiagnostics.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "GameFramework/PlayerStart.h"
#include "NavigationSystem.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceNode.h"
#include "Resources/GPStorageComponent.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPWorker.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPResourceLoopDiag, Log, All);

namespace GPResourceLoopDiagnostics
{
	static constexpr float InteractionRangeCm = 200.0f;
	static constexpr float DropOffRangeCm = 400.0f;
	static constexpr float AcceptanceRadiusCm = 50.0f;
	static constexpr float ApproachSafetyMarginCm = 25.0f;
	static constexpr float DefaultNodeSeparationCm = 1800.0f;
	static constexpr float WorkerNearNodeCm = 120.0f;

	FName MakeTeamScenarioTag(int32 TeamId)
	{
		return FName(*FString::Printf(TEXT("GP_DiagScenario_T%d"), TeamId));
	}

	bool ActorHasOwnerTag(const AActor* Actor, FName OwnerTag)
	{
		return IsValid(Actor) && OwnerTag != NAME_None && Actor->Tags.Contains(OwnerTag);
	}

	static void ApplyScenarioTags(AActor* Actor, int32 TeamId, FName OwnerTag)
	{
		if (!IsValid(Actor))
		{
			return;
		}
		Actor->Tags.AddUnique(TagScenario);
		Actor->Tags.AddUnique(MakeTeamScenarioTag(TeamId));
		if (OwnerTag != NAME_None)
		{
			Actor->Tags.AddUnique(OwnerTag);
		}
		if (OwnerTag != NAME_None && OwnerTag != GPContractTestCoordinator::OwnerTagOperator)
		{
			Actor->Tags.AddUnique(TagOwnedByContract);
		}
	}

	static bool HasScenarioTeamTag(const AActor* Actor, int32 TeamId)
	{
		return IsValid(Actor) && Actor->Tags.Contains(MakeTeamScenarioTag(TeamId));
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

	bool IsNavPointProjected(UWorld* World, const FVector& Location, FVector* OutProjected, float ExtentXY, float ExtentZ)
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
		const bool bOk = NavSys->ProjectPointToNavigation(
			Location,
			Projected,
			FVector(ExtentXY, ExtentXY, ExtentZ));
		if (bOk && OutProjected != nullptr)
		{
			*OutProjected = Projected.Location;
		}
		return bOk;
	}

	bool IsNavReachable(UWorld* World, const FVector& From, const FVector& To, FString* OutFailReason)
	{
		if (!IsValid(World))
		{
			if (OutFailReason)
			{
				*OutFailReason = TEXT("WorldInvalid");
			}
			return false;
		}
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSys == nullptr)
		{
			if (OutFailReason)
			{
				*OutFailReason = TEXT("NavSystemMissing");
			}
			return false;
		}

		FVector ProjectedFrom = From;
		FVector ProjectedTo = To;
		if (!IsNavPointProjected(World, From, &ProjectedFrom))
		{
			if (OutFailReason)
			{
				*OutFailReason = TEXT("FromNotProjected");
			}
			return false;
		}
		if (!IsNavPointProjected(World, To, &ProjectedTo))
		{
			if (OutFailReason)
			{
				*OutFailReason = TEXT("ToNotProjected");
			}
			return false;
		}

		const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
		if (NavData == nullptr)
		{
			if (OutFailReason)
			{
				*OutFailReason = TEXT("NavDataMissing");
			}
			return false;
		}

		FPathFindingQuery Query(NavSys, *NavData, ProjectedFrom, ProjectedTo);
		const bool bOk = NavSys->TestPathSync(Query, EPathFindingMode::Regular);
		if (!bOk && OutFailReason)
		{
			*OutFailReason = TEXT("TestPathSyncFailed");
		}
		return bOk;
	}

	static FVector MakeApproachPointAround(const FVector& Center, const FVector& AwayFrom, float DesiredDistance)
	{
		FVector Dir = AwayFrom - Center;
		Dir.Z = 0.0f;
		if (!Dir.Normalize())
		{
			Dir = FVector::ForwardVector;
		}
		return Center + Dir * DesiredDistance;
	}

	static bool FindNavigableAnchor(UWorld* World, FVector& OutAnchor, FString& OutSource)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSys == nullptr)
		{
			OutSource = TEXT("NavSystemMissing");
			return false;
		}

		auto TryProject = [&](const FVector& Candidate, const TCHAR* SourceLabel) -> bool
		{
			FVector Projected;
			if (IsNavPointProjected(World, Candidate, &Projected, 1200.0f, 1200.0f))
			{
				OutAnchor = Projected;
				OutSource = SourceLabel;
				return true;
			}
			return false;
		};

		// A) Authored (non-diagnostic) ResourceNode already on/near NavMesh.
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			AGP_ResourceNode* Node = *It;
			if (!IsValid(Node) || Node->Tags.Contains(TagScenario))
			{
				continue;
			}
			if (TryProject(Node->GetActorLocation(), TEXT("AuthoredResourceNode")))
			{
				return true;
			}
		}

		// A2) Existing mobile unit / worker not owned by diagnostic scenario.
		for (TActorIterator<AGP_MobileUnit> It(World); It; ++It)
		{
			AGP_MobileUnit* Unit = *It;
			if (!IsValid(Unit) || Unit->Tags.Contains(TagScenario))
			{
				continue;
			}
			if (TryProject(Unit->GetActorLocation(), TEXT("MobileUnit")))
			{
				return true;
			}
		}

		// B/C) PlayerStart and PrototypeArena-centered candidates.
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			if (IsValid(*It) && TryProject((*It)->GetActorLocation(), TEXT("PlayerStart")))
			{
				return true;
			}
		}

		static const FVector ArenaCandidates[] = {
			FVector(0.0f, 0.0f, 100.0f),
			FVector(0.0f, -500.0f, 100.0f),
			FVector(500.0f, 0.0f, 100.0f),
			FVector(-500.0f, 0.0f, 100.0f),
			FVector(0.0f, 500.0f, 100.0f),
			FVector(0.0f, -1500.0f, 100.0f),
			FVector(1000.0f, 0.0f, 100.0f),
			FVector(-1000.0f, 0.0f, 100.0f),
		};
		for (const FVector& Candidate : ArenaCandidates)
		{
			if (TryProject(Candidate, TEXT("ArenaCandidate")))
			{
				return true;
			}
		}

		FNavLocation RandomNav;
		if (NavSys->GetRandomPoint(RandomNav))
		{
			OutAnchor = RandomNav.Location;
			OutSource = TEXT("NavRandomPoint");
			return true;
		}

		OutSource = TEXT("NoNavigableAnchor");
		return false;
	}

	struct FResolvedLayout
	{
		FVector Anchor = FVector::ZeroVector;
		FVector BaseLoc = FVector::ZeroVector;
		FVector NodeLoc = FVector::ZeroVector;
		FVector WorkerLoc = FVector::ZeroVector;
		FVector NodeApproach = FVector::ZeroVector;
		FVector BaseDropOff = FVector::ZeroVector;
		bool bWorkerProjected = false;
		bool bNodeApproachProjected = false;
		bool bBaseDropOffProjected = false;
		bool bNavWorkerToNode = false;
		bool bNavNodeToBase = false;
		bool bNavBaseToNode = false;
		FString PathFailureReason;
		bool bOk = false;
	};

	static bool TryResolveLayoutAroundAnchor(UWorld* World, const FVector& Anchor, FResolvedLayout& OutLayout)
	{
		const float MineApproachDist = FMath::Max(
			50.0f,
			InteractionRangeCm - AcceptanceRadiusCm - ApproachSafetyMarginCm); // 125
		const float DropOffApproachDist = FMath::Max(
			100.0f,
			DropOffRangeCm - AcceptanceRadiusCm - ApproachSafetyMarginCm); // 325

		static const FVector Directions[] = {
			FVector(1.0f, 0.0f, 0.0f),
			FVector(-1.0f, 0.0f, 0.0f),
			FVector(0.0f, 1.0f, 0.0f),
			FVector(0.0f, -1.0f, 0.0f),
			FVector(0.7071f, 0.7071f, 0.0f),
			FVector(-0.7071f, 0.7071f, 0.0f),
		};
		static const float Separations[] = { DefaultNodeSeparationCm, 1500.0f, 2200.0f, 1200.0f };

		for (const float Separation : Separations)
		{
			for (const FVector& Dir : Directions)
			{
				FResolvedLayout Candidate;
				Candidate.Anchor = Anchor;

				FVector BaseProj;
				if (!IsNavPointProjected(World, Anchor, &BaseProj, 1000.0f, 1000.0f))
				{
					continue;
				}
				Candidate.BaseLoc = BaseProj;

				const FVector RawNode = BaseProj + Dir * Separation;
				FVector NodeProj;
				if (!IsNavPointProjected(World, RawNode, &NodeProj, 1000.0f, 1000.0f))
				{
					continue;
				}
				Candidate.NodeLoc = NodeProj;

				const FVector AwayFromBase = (NodeProj - BaseProj).GetSafeNormal2D();
				const FVector WorkerRaw = NodeProj + AwayFromBase * WorkerNearNodeCm;
				FVector WorkerProj;
				if (!IsNavPointProjected(World, WorkerRaw, &WorkerProj, 800.0f, 800.0f))
				{
					continue;
				}
				Candidate.WorkerLoc = WorkerProj;
				Candidate.bWorkerProjected = true;

				const FVector NodeApproachRaw = MakeApproachPointAround(NodeProj, WorkerProj, MineApproachDist);
				FVector NodeApproachProj;
				if (!IsNavPointProjected(World, NodeApproachRaw, &NodeApproachProj, 800.0f, 800.0f))
				{
					Candidate.PathFailureReason = TEXT("NodeApproachNotProjected");
					continue;
				}
				// Keep approach inside InteractionRange of node origin.
				if (FVector::Dist(NodeApproachProj, NodeProj) > InteractionRangeCm - 5.0f)
				{
					const FVector PullDir = (NodeApproachProj - NodeProj).GetSafeNormal();
					NodeApproachProj = NodeProj + PullDir * (MineApproachDist);
					if (!IsNavPointProjected(World, NodeApproachProj, &NodeApproachProj, 600.0f, 600.0f))
					{
						continue;
					}
				}
				Candidate.NodeApproach = NodeApproachProj;
				Candidate.bNodeApproachProjected = true;

				const FVector BaseDropRaw = MakeApproachPointAround(BaseProj, NodeProj, DropOffApproachDist);
				FVector BaseDropProj;
				if (!IsNavPointProjected(World, BaseDropRaw, &BaseDropProj, 800.0f, 800.0f))
				{
					Candidate.PathFailureReason = TEXT("BaseDropOffNotProjected");
					continue;
				}
				if (FVector::Dist(BaseDropProj, BaseProj) > DropOffRangeCm - 5.0f)
				{
					const FVector PullDir = (BaseDropProj - BaseProj).GetSafeNormal();
					BaseDropProj = BaseProj + PullDir * DropOffApproachDist;
					if (!IsNavPointProjected(World, BaseDropProj, &BaseDropProj, 600.0f, 600.0f))
					{
						continue;
					}
				}
				Candidate.BaseDropOff = BaseDropProj;
				Candidate.bBaseDropOffProjected = true;

				FString Fail;
				Candidate.bNavWorkerToNode = IsNavReachable(World, Candidate.WorkerLoc, Candidate.NodeApproach, &Fail);
				if (!Candidate.bNavWorkerToNode)
				{
					Candidate.PathFailureReason = FString::Printf(TEXT("WorkerToNode:%s"), *Fail);
					continue;
				}
				Candidate.bNavNodeToBase = IsNavReachable(World, Candidate.NodeApproach, Candidate.BaseDropOff, &Fail);
				if (!Candidate.bNavNodeToBase)
				{
					Candidate.PathFailureReason = FString::Printf(TEXT("NodeToBase:%s"), *Fail);
					continue;
				}
				Candidate.bNavBaseToNode = IsNavReachable(World, Candidate.BaseDropOff, Candidate.NodeApproach, &Fail);
				if (!Candidate.bNavBaseToNode)
				{
					Candidate.PathFailureReason = FString::Printf(TEXT("BaseToNode:%s"), *Fail);
					continue;
				}

				Candidate.bOk = true;
				OutLayout = Candidate;
				return true;
			}
		}

		OutLayout.PathFailureReason = TEXT("NoReachableDiagnosticLayout");
		return false;
	}

	void CleanupScenarioByOwnerTag(UWorld* World, FName OwnerTag)
	{
		if (!IsValid(World) || OwnerTag == NAME_None)
		{
			return;
		}

		TArray<AActor*> ToDestroy;
		auto Consider = [&](AActor* Actor)
		{
			if (ActorHasOwnerTag(Actor, OwnerTag))
			{
				ToDestroy.AddUnique(Actor);
			}
		};

		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			Consider(*It);
		}
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			Consider(*It);
		}
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			Consider(*It);
		}

		for (AActor* Actor : ToDestroy)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
		}
	}

	void CleanupOperatorScenarioForTeam(UWorld* World, int32 TeamId)
	{
		if (!IsValid(World) || TeamId < 1)
		{
			return;
		}

		TArray<AActor*> ToDestroy;
		auto Consider = [&](AActor* Actor)
		{
			if (!IsValid(Actor) || !HasScenarioTeamTag(Actor, TeamId))
			{
				return;
			}
			// Exact operator ownership only — never contract / other runners.
			if (!ActorHasOwnerTag(Actor, GPContractTestCoordinator::OwnerTagOperator))
			{
				return;
			}
			ToDestroy.AddUnique(Actor);
		};

		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			Consider(*It);
		}
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			Consider(*It);
		}
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			Consider(*It);
		}

		for (AActor* Actor : ToDestroy)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
		}
	}

	AGP_MainBase* SpawnMainBaseDeferred(UWorld* World, const FVector& Location, int32 TeamId, FName OwnerTag)
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
		ApplyScenarioTags(Base, TeamId, OwnerTag);
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

	AGP_Worker* SpawnWorkerDeferred(UWorld* World, const FVector& Location, int32 TeamId, FName OwnerTag)
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
		ApplyScenarioTags(Worker, TeamId, OwnerTag);
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

	AGP_ResourceNode* SpawnResourceNodeTransient(UWorld* World, const FVector& Location, FName OwnerTag)
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
		ApplyScenarioTags(Node, 0, OwnerTag);
		if (!Node->GetActorLocation().Equals(Location, 1.0f))
		{
			Node->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		}
		return Node;
	}

	static AGP_MainBase* FindTaggedMainBaseForTeam(UWorld* World, int32 TeamId)
	{
		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			if (IsValid(*It) && HasScenarioTeamTag(*It, TeamId))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static AGP_Worker* FindTaggedWorkerForTeam(UWorld* World, int32 TeamId)
	{
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (IsValid(*It) && HasScenarioTeamTag(*It, TeamId))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static AGP_ResourceNode* FindTaggedScenarioNode(UWorld* World, int32 TeamId)
	{
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			AGP_ResourceNode* Node = *It;
			if (!IsValid(Node) || !Node->Tags.Contains(TagScenario))
			{
				continue;
			}
			if (HasScenarioTeamTag(Node, TeamId) || Node->Tags.Contains(TagScenario))
			{
				if (Node->GetCurrentAmount() > 0 && !Node->IsDepleted())
				{
					return Node;
				}
			}
		}
		return nullptr;
	}

	int32 FindFreePlayableTeamId(UWorld* World)
	{
		if (!IsValid(World))
		{
			return INDEX_NONE;
		}
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (GS == nullptr)
		{
			return INDEX_NONE;
		}
		GS->PruneInvalidMainBaseRegistrations();
		for (int32 Candidate = 1; Candidate <= 8; ++Candidate)
		{
			if (GS->FindMainBaseForTeam(Candidate) == nullptr)
			{
				return Candidate;
			}
		}
		return INDEX_NONE;
	}

	FGP_DiagnosticScenarioActors SpawnDiagnosticScenario(UWorld* World, int32 TeamId, FName OwnerTag)
	{
		FGP_DiagnosticScenarioActors Result;
		Result.TeamId = TeamId < 1 ? 1 : TeamId;
		TeamId = Result.TeamId;
		Result.OwnerTag = OwnerTag.IsNone() ? GPContractTestCoordinator::OwnerTagOperator : OwnerTag;

		if (!IsValid(World) || World->GetNetMode() == NM_Client)
		{
			Result.Error = TEXT("MissingWorldOrClient");
			return Result;
		}

		AGP_GameState* GSEarly = World->GetGameState<AGP_GameState>();
		if (GSEarly != nullptr)
		{
			GSEarly->PruneInvalidMainBaseRegistrations();
		}

		const bool bOperatorOwned = (Result.OwnerTag == GPContractTestCoordinator::OwnerTagOperator);
		if (!bOperatorOwned)
		{
			// Select free team BEFORE any cleanup — never destroy other owners' Team1 actors.
			if (GSEarly != nullptr && GSEarly->FindMainBaseForTeam(TeamId) != nullptr)
			{
				const int32 OccupiedTeam = TeamId;
				const int32 FreeTeam = FindFreePlayableTeamId(World);
				if (FreeTeam == INDEX_NONE)
				{
					Result.Error = TEXT("BlockedByOccupiedPlayableTeams");
					UE_LOG(LogGPResourceLoopDiag, Error,
						TEXT("GP Resource.SpawnDiagnosticScenario: Ok=false Reason=BlockedByOccupiedPlayableTeams RequestedTeam=%d"),
						OccupiedTeam);
					return Result;
				}
				UE_LOG(LogGPResourceLoopDiag, Log,
					TEXT("GP Resource.SpawnDiagnosticScenario: Contract team remapped Requested=%d OccupiedOperatorTeam=%d -> Free=%d OwnerTag=%s"),
					OccupiedTeam,
					OccupiedTeam,
					FreeTeam,
					*Result.OwnerTag.ToString());
				TeamId = FreeTeam;
				Result.TeamId = TeamId;
			}

			// Only destroy leftovers owned by THIS OwnerTag (never Team-wide contract cleanup).
			CleanupScenarioByOwnerTag(World, Result.OwnerTag);
		}
		else
		{
			// Operator re-spawn: replace prior operator diagnostic for this team only.
			CleanupOperatorScenarioForTeam(World, TeamId);
			if (GSEarly != nullptr && GSEarly->FindMainBaseForTeam(TeamId) != nullptr)
			{
				Result.Error = TEXT("TeamMainBaseOccupied");
				UE_LOG(LogGPResourceLoopDiag, Error,
					TEXT("GP Resource.SpawnDiagnosticScenario: Ok=false Reason=TeamMainBaseOccupied TeamId=%d Existing=%s"),
					TeamId,
					*GetNameSafe(GSEarly->FindMainBaseForTeam(TeamId)));
				return Result;
			}
		}

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		Result.bNavSystemPresent = NavSys != nullptr;
		if (!Result.bNavSystemPresent)
		{
			Result.Error = TEXT("NoReachableDiagnosticLayout");
			Result.PathFailureReason = TEXT("NavSystemMissing");
			UE_LOG(LogGPResourceLoopDiag, Error,
				TEXT("GP Resource.SpawnDiagnosticScenario: Ok=false Reason=NoReachableDiagnosticLayout Detail=NavSystemMissing CreatedBase=false CreatedWorker=false CreatedNode=false"));
			return Result;
		}

		FVector Anchor = FVector::ZeroVector;
		FString AnchorSource;
		if (!FindNavigableAnchor(World, Anchor, AnchorSource))
		{
			Result.Error = TEXT("NoReachableDiagnosticLayout");
			Result.PathFailureReason = AnchorSource;
			UE_LOG(LogGPResourceLoopDiag, Error,
				TEXT("GP Resource.SpawnDiagnosticScenario: Ok=false Reason=NoReachableDiagnosticLayout Detail=%s CreatedBase=false CreatedWorker=false CreatedNode=false"),
				*AnchorSource);
			return Result;
		}
		Result.bAnchorProjected = true;
		Result.AnchorLocation = Anchor;

		FResolvedLayout Layout;
		if (!TryResolveLayoutAroundAnchor(World, Anchor, Layout))
		{
			Result.Error = TEXT("NoReachableDiagnosticLayout");
			Result.PathFailureReason = Layout.PathFailureReason.IsEmpty()
				? TEXT("NoReachableDiagnosticLayout")
				: Layout.PathFailureReason;
			UE_LOG(LogGPResourceLoopDiag, Error,
				TEXT("GP Resource.SpawnDiagnosticScenario: Ok=false Reason=NoReachableDiagnosticLayout Detail=%s Anchor=%s Source=%s CreatedBase=false CreatedWorker=false CreatedNode=false"),
				*Result.PathFailureReason,
				*Anchor.ToCompactString(),
				*AnchorSource);
			return Result;
		}

		Result.MainBaseLocation = Layout.BaseLoc;
		Result.ResourceNodeLocation = Layout.NodeLoc;
		Result.WorkerLocation = Layout.WorkerLoc;
		Result.NodeApproachLocation = Layout.NodeApproach;
		Result.BaseDropOffLocation = Layout.BaseDropOff;
		Result.bWorkerProjected = Layout.bWorkerProjected;
		Result.bNodeApproachProjected = Layout.bNodeApproachProjected;
		Result.bBaseDropOffProjected = Layout.bBaseDropOffProjected;
		Result.bNavWorkerToNode = Layout.bNavWorkerToNode;
		Result.bNavNodeToBase = Layout.bNavNodeToBase;
		Result.bNavBaseToNode = Layout.bNavBaseToNode;

		Result.MainBase = SpawnMainBaseDeferred(World, Layout.BaseLoc, TeamId, Result.OwnerTag);
		Result.bCreatedMainBase = IsValid(Result.MainBase);
		Result.ResourceNode = SpawnResourceNodeTransient(World, Layout.NodeLoc, Result.OwnerTag);
		if (IsValid(Result.ResourceNode))
		{
			Result.ResourceNode->Tags.AddUnique(MakeTeamScenarioTag(TeamId));
			Result.bCreatedResourceNode = true;
		}
		Result.Worker = SpawnWorkerDeferred(World, Layout.WorkerLoc, TeamId, Result.OwnerTag);
		Result.bCreatedWorker = IsValid(Result.Worker);

		auto FailAtomic = [&](const TCHAR* Reason)
		{
			Result.bOk = false;
			Result.Error = Reason;
			Result.bReadyForHaulingTest = false;
			DestroyDiagnosticScenarioActors(Result.MainBase, Result.Worker, Result.ResourceNode);
			Result.MainBase = nullptr;
			Result.Worker = nullptr;
			Result.ResourceNode = nullptr;
			Result.bCreatedMainBase = false;
			Result.bCreatedWorker = false;
			Result.bCreatedResourceNode = false;
			UE_LOG(LogGPResourceLoopDiag, Error,
				TEXT("GP Resource.SpawnDiagnosticScenario: Ok=false Reason=%s CreatedBase=false CreatedWorker=false CreatedNode=false (atomic cleanup)"),
				Reason);
		};

		if (!IsValid(Result.MainBase) || Result.MainBase->GetTeamId() != TeamId)
		{
			FailAtomic(TEXT("MainBaseSpawnOrTeamFailed"));
			return Result;
		}
		if (!IsValid(Result.ResourceNode) || Result.ResourceNode->GetCurrentAmount() <= 0)
		{
			FailAtomic(TEXT("ResourceNodeInvalid"));
			return Result;
		}
		if (!IsValid(Result.Worker) || Result.Worker->GetTeamId() != TeamId)
		{
			FailAtomic(TEXT("WorkerSpawnOrTeamFailed"));
			return Result;
		}

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (GS == nullptr || !Result.MainBase->Tags.Contains(TagScenario))
		{
			FailAtomic(TEXT("MainBaseRegistryResolveFailed"));
			return Result;
		}
		Result.MainBase->RefreshMainBaseRegistration();
		AGP_MainBase* Resolved = GS->FindMainBaseForTeam(TeamId);
		if (Resolved != Result.MainBase
			|| GS->CountRegisteredMainBasesForTeam(TeamId) != 1
			|| !GS->IsMainBaseRegistryUniqueForTeam(TeamId))
		{
			FailAtomic(TEXT("MainBaseRegistryResolveFailed"));
			return Result;
		}

		// Re-validate approach paths after actors exist (endpoints remain approach points, not actor origins).
		FString Fail;
		Result.bNavWorkerToNode = IsNavReachable(World, Result.Worker->GetActorLocation(), Result.NodeApproachLocation, &Fail);
		if (!Result.bNavWorkerToNode)
		{
			Result.PathFailureReason = FString::Printf(TEXT("PostSpawnWorkerToNode:%s"), *Fail);
			FailAtomic(TEXT("NoReachableDiagnosticLayout"));
			return Result;
		}
		Result.bNavNodeToBase = IsNavReachable(World, Result.NodeApproachLocation, Result.BaseDropOffLocation, &Fail);
		if (!Result.bNavNodeToBase)
		{
			Result.PathFailureReason = FString::Printf(TEXT("PostSpawnNodeToBase:%s"), *Fail);
			FailAtomic(TEXT("NoReachableDiagnosticLayout"));
			return Result;
		}
		Result.bNavBaseToNode = IsNavReachable(World, Result.BaseDropOffLocation, Result.NodeApproachLocation, &Fail);
		if (!Result.bNavBaseToNode)
		{
			Result.PathFailureReason = FString::Printf(TEXT("PostSpawnBaseToNode:%s"), *Fail);
			FailAtomic(TEXT("NoReachableDiagnosticLayout"));
			return Result;
		}

		Result.bReadyForHaulingTest =
			Result.bNavSystemPresent
			&& Result.bAnchorProjected
			&& Result.bWorkerProjected
			&& Result.bNodeApproachProjected
			&& Result.bBaseDropOffProjected
			&& Result.bNavWorkerToNode
			&& Result.bNavNodeToBase
			&& Result.bNavBaseToNode;
		Result.bOk = Result.bReadyForHaulingTest;
		if (!Result.bOk)
		{
			FailAtomic(TEXT("NoReachableDiagnosticLayout"));
			return Result;
		}

		UGP_StorageComponent* Storage = Result.MainBase->GetStorageComponent();
		UGP_CargoComponent* Cargo = Result.Worker->GetCargoComponent();
		UE_LOG(LogGPResourceLoopDiag, Log,
			TEXT("GP Resource.SpawnDiagnosticScenario: Ok=true TeamId=%d MainBase=%s MainBaseTeam=%d Worker=%s WorkerTeam=%d Node=%s NodeAmount=%d Stored=%.1f/%.1f Cargo=%.1f/%.1f Anchor=%s AnchorSource=%s WorkerLoc=%s NodeLoc=%s BaseLoc=%s NodeApproach=%s BaseDropOff=%s NavSystemPresent=true AnchorProjected=true WorkerProjected=%s NodeApproachProjected=%s BaseDropOffProjected=%s NavWorkerToNode=true NavNodeToBase=true NavBaseToNode=true ReadyForHaulingTest=true CreatedBase=%s CreatedWorker=%s CreatedNode=%s SuggestedCommand=gp.Worker.CommandMine %s %s"),
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
			*Result.AnchorLocation.ToCompactString(),
			*AnchorSource,
			*Result.Worker->GetActorLocation().ToCompactString(),
			*Result.ResourceNode->GetActorLocation().ToCompactString(),
			*Result.MainBase->GetActorLocation().ToCompactString(),
			*Result.NodeApproachLocation.ToCompactString(),
			*Result.BaseDropOffLocation.ToCompactString(),
			Result.bWorkerProjected ? TEXT("true") : TEXT("false"),
			Result.bNodeApproachProjected ? TEXT("true") : TEXT("false"),
			Result.bBaseDropOffProjected ? TEXT("true") : TEXT("false"),
			Result.bCreatedMainBase ? TEXT("true") : TEXT("false"),
			Result.bCreatedWorker ? TEXT("true") : TEXT("false"),
			Result.bCreatedResourceNode ? TEXT("true") : TEXT("false"),
			*Result.Worker->GetName(),
			*Result.ResourceNode->GetName());

		return Result;
	}

	static void ComputeApproachPointsForActors(
		UWorld* World,
		AGP_Worker* Worker,
		AGP_ResourceNode* Node,
		AGP_MainBase* Base,
		FVector& OutNodeApproach,
		FVector& OutBaseDropOff,
		bool& bNodeApproachOk,
		bool& bBaseDropOffOk)
	{
		bNodeApproachOk = false;
		bBaseDropOffOk = false;
		OutNodeApproach = FVector::ZeroVector;
		OutBaseDropOff = FVector::ZeroVector;
		if (!IsValid(World) || !IsValid(Node) || !IsValid(Base))
		{
			return;
		}

		const float MineApproachDist = InteractionRangeCm - AcceptanceRadiusCm - ApproachSafetyMarginCm;
		const float DropOffApproachDist = DropOffRangeCm - AcceptanceRadiusCm - ApproachSafetyMarginCm;
		const FVector WorkerLoc = IsValid(Worker) ? Worker->GetActorLocation() : Node->GetActorLocation() + FVector(WorkerNearNodeCm, 0.0f, 0.0f);

		const FVector NodeApproachRaw = MakeApproachPointAround(Node->GetActorLocation(), WorkerLoc, MineApproachDist);
		bNodeApproachOk = IsNavPointProjected(World, NodeApproachRaw, &OutNodeApproach, 800.0f, 800.0f);

		const FVector BaseDropRaw = MakeApproachPointAround(Base->GetActorLocation(), Node->GetActorLocation(), DropOffApproachDist);
		bBaseDropOffOk = IsNavPointProjected(World, BaseDropRaw, &OutBaseDropOff, 800.0f, 800.0f);
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
		V.bNavSystemPresent = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) != nullptr;
		if (!V.bNavSystemPresent)
		{
			++V.Errors;
			V.PathFailureReason = TEXT("NavSystemMissing");
		}

		AGP_Worker* Worker = WorkerHint;
		if (!IsValid(Worker) || !HasScenarioTeamTag(Worker, TeamId))
		{
			Worker = FindTaggedWorkerForTeam(World, TeamId);
		}
		if (!IsValid(Worker))
		{
			// Fallback: any team worker (operator may have non-tagged).
			for (TActorIterator<AGP_Worker> It(World); It; ++It)
			{
				if (IsValid(*It) && (*It)->GetTeamId() == TeamId)
				{
					Worker = *It;
					break;
				}
			}
		}

		AGP_MainBase* Base = FindTaggedMainBaseForTeam(World, TeamId);
		if (!IsValid(Base))
		{
			if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
			{
				Base = GS->FindMainBaseForTeam(TeamId);
			}
		}

		AGP_ResourceNode* Node = FindTaggedScenarioNode(World, TeamId);
		if (!IsValid(Node))
		{
			for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
			{
				if (IsValid(*It) && (*It)->GetCurrentAmount() > 0 && !(*It)->IsDepleted())
				{
					Node = *It;
					break;
				}
			}
		}

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (GS != nullptr)
		{
			GS->PruneInvalidMainBaseRegistrations();
		}
		AGP_MainBase* Registered = GS != nullptr ? GS->FindMainBaseForTeam(TeamId) : nullptr;
		V.MainBaseCountForWorkerTeam = GS != nullptr ? GS->CountRegisteredMainBasesForTeam(TeamId) : 0;
		V.bRegistryUniqueForTeam = GS != nullptr && GS->IsMainBaseRegistryUniqueForTeam(TeamId) && V.MainBaseCountForWorkerTeam == 1;
		V.bResolvedMainBaseMatchesListedBase = IsValid(Registered) && IsValid(Base) && Registered == Base;
		V.bMainBaseRegisteredForTeam = V.bResolvedMainBaseMatchesListedBase && V.bRegistryUniqueForTeam;
		V.bWorkerHasMainBase = IsValid(Worker) && IsValid(Registered) && Registered->GetTeamId() == Worker->GetTeamId();
		V.bWorkerHasResourceNode = IsValid(Worker) && IsValid(Node);
		V.bWorkerAndBaseSameTeam = IsValid(Worker) && IsValid(Base) && Worker->GetTeamId() == Base->GetTeamId() && Worker->GetTeamId() == TeamId;

		FString MineFail;
		V.bNodeMineable = IsValid(Node) && Node->CanAcceptMineCommand(true, &MineFail) && Node->GetCurrentAmount() > 0;

		FVector NodeApproach;
		FVector BaseDropOff;
		ComputeApproachPointsForActors(World, Worker, Node, Base, NodeApproach, BaseDropOff, V.bNodeApproachProjected, V.bBaseDropOffProjected);

		FVector WorkerProj;
		V.bWorkerProjected = IsValid(Worker) && IsNavPointProjected(World, Worker->GetActorLocation(), &WorkerProj);

		FString Fail;
		if (V.bWorkerProjected && V.bNodeApproachProjected)
		{
			V.bNavReachableWorkerToNode = IsNavReachable(World, WorkerProj, NodeApproach, &Fail);
			if (!V.bNavReachableWorkerToNode)
			{
				V.PathFailureReason = FString::Printf(TEXT("WorkerToNode:%s"), *Fail);
				++V.Errors;
			}
		}
		else
		{
			++V.Errors;
		}

		if (V.bNodeApproachProjected && V.bBaseDropOffProjected)
		{
			V.bNavReachableNodeToBase = IsNavReachable(World, NodeApproach, BaseDropOff, &Fail);
			if (!V.bNavReachableNodeToBase)
			{
				V.PathFailureReason = FString::Printf(TEXT("NodeToBase:%s"), *Fail);
				++V.Errors;
			}
			V.bNavReachableBaseToNode = IsNavReachable(World, BaseDropOff, NodeApproach, &Fail);
			if (!V.bNavReachableBaseToNode)
			{
				V.PathFailureReason = FString::Printf(TEXT("BaseToNode:%s"), *Fail);
				++V.Errors;
			}
		}
		else
		{
			++V.Errors;
		}

		if (!V.bPlayableTeamValid || !V.bWorkerHasMainBase || !V.bWorkerHasResourceNode
			|| !V.bMainBaseRegisteredForTeam || !V.bWorkerAndBaseSameTeam || !V.bNodeMineable
			|| V.MainBaseCountForWorkerTeam != 1 || !V.bRegistryUniqueForTeam
			|| !V.bResolvedMainBaseMatchesListedBase)
		{
			++V.Errors;
		}

		V.bReadyForHaulingTest =
			V.bPlayableTeamValid
			&& V.bWorkerHasMainBase
			&& V.bWorkerHasResourceNode
			&& V.bMainBaseRegisteredForTeam
			&& V.bWorkerAndBaseSameTeam
			&& V.bNodeMineable
			&& V.MainBaseCountForWorkerTeam == 1
			&& V.bRegistryUniqueForTeam
			&& V.bResolvedMainBaseMatchesListedBase
			&& V.bNavSystemPresent
			&& V.bWorkerProjected
			&& V.bNodeApproachProjected
			&& V.bBaseDropOffProjected
			&& V.bNavReachableWorkerToNode
			&& V.bNavReachableNodeToBase
			&& V.bNavReachableBaseToNode
			&& V.Errors == 0;

		if (IsValid(Worker) && IsValid(Node))
		{
			V.SuggestedCommand = FString::Printf(
				TEXT("gp.Worker.CommandMine %s %s"),
				*Worker->GetName(),
				*Node->GetName());
		}

		return V;
	}

	void DestroyDiagnosticScenarioActors(AGP_MainBase* MainBase, AGP_Worker* Worker, AGP_ResourceNode* Node)
	{
		if (IsValid(Worker))
		{
			Worker->Destroy();
		}
		if (IsValid(Node))
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
