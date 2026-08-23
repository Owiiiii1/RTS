// Copyright Epic Games, Inc. All Rights Reserved.

#include "Command/GPCommandComponent.h"

#include "Command/GPCommandRequest.h"
#include "Command/GPUnitCommand.h"
#include "NavigationSystem.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Buildings/GPMainBase.h"
#include "Resources/GPResourceNode.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPUnitBase.h"
#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#endif

DEFINE_LOG_CATEGORY(LogGPCommandServer);

namespace GPCommandPrivate
{
	static constexpr int32 MaxIssuingUnits = 24;
	static constexpr double MaxAbsCoordinate = 10000000.0;
	static constexpr float GroupSlotSpacingCm = 110.0f;
	static constexpr float GroupSlotNavExtentXY = 250.0f;
	static constexpr float GroupSlotNavExtentZ = 400.0f;

	static bool IsCommandLocationSane(const FVector& Location)
	{
		if (Location.ContainsNaN())
		{
			return false;
		}

		if (!FMath::IsFinite(Location.X)
			|| !FMath::IsFinite(Location.Y)
			|| !FMath::IsFinite(Location.Z))
		{
			return false;
		}

		if (FMath::Abs(Location.X) > MaxAbsCoordinate
			|| FMath::Abs(Location.Y) > MaxAbsCoordinate
			|| FMath::Abs(Location.Z) > MaxAbsCoordinate)
		{
			return false;
		}

		return true;
	}

	/** Deterministic compact grid around Center (1 unit → exact Center). */
	static void BuildGroupDestinationSlots(const FVector& Center, int32 Count, TArray<FVector>& OutSlots)
	{
		OutSlots.Reset();
		if (Count <= 0)
		{
			return;
		}
		if (Count == 1)
		{
			OutSlots.Add(Center);
			return;
		}

		const int32 Cols = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Count))));
		const int32 Rows = FMath::Max(1, FMath::CeilToInt(static_cast<float>(Count) / static_cast<float>(Cols)));
		const float OriginX = -0.5f * static_cast<float>(Cols - 1) * GroupSlotSpacingCm;
		const float OriginY = -0.5f * static_cast<float>(Rows - 1) * GroupSlotSpacingCm;
		OutSlots.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 Row = Index / Cols;
			const int32 Col = Index % Cols;
			OutSlots.Add(FVector(
				Center.X + OriginX + static_cast<float>(Col) * GroupSlotSpacingCm,
				Center.Y + OriginY + static_cast<float>(Row) * GroupSlotSpacingCm,
				Center.Z));
		}
	}

	static FVector ProjectGroupSlotOrFallback(
		UWorld* World,
		const FVector& Desired,
		const FVector& Center,
		int32 SlotIndex)
	{
		if (World == nullptr || !IsCommandLocationSane(Desired))
		{
			return Center + FVector(static_cast<float>(SlotIndex) * 10.0f, 0.0f, 0.0f);
		}

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSys == nullptr || NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) == nullptr)
		{
			return Desired;
		}

		const FVector Extent(GroupSlotNavExtentXY, GroupSlotNavExtentXY, GroupSlotNavExtentZ);
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Desired, Projected, Extent))
		{
			return FVector(Projected.Location.X, Projected.Location.Y, Desired.Z);
		}

		// Nearby ring fallback so failed slots do not all collapse to Center.
		static const FVector2D RingOffsets[] = {
			FVector2D(GroupSlotSpacingCm, 0.0f),
			FVector2D(-GroupSlotSpacingCm, 0.0f),
			FVector2D(0.0f, GroupSlotSpacingCm),
			FVector2D(0.0f, -GroupSlotSpacingCm),
			FVector2D(GroupSlotSpacingCm, GroupSlotSpacingCm),
			FVector2D(-GroupSlotSpacingCm, GroupSlotSpacingCm),
			FVector2D(GroupSlotSpacingCm, -GroupSlotSpacingCm),
			FVector2D(-GroupSlotSpacingCm, -GroupSlotSpacingCm)
		};
		for (const FVector2D& Offset : RingOffsets)
		{
			const FVector Candidate = Desired + FVector(Offset.X, Offset.Y, 0.0f);
			if (NavSys->ProjectPointToNavigation(Candidate, Projected, Extent))
			{
				return FVector(Projected.Location.X, Projected.Location.Y, Desired.Z);
			}
		}

		// Keep deterministic grid offsets when off-nav — never collapse all units to Center.
		return Desired;
	}
}

UGP_CommandComponent::UGP_CommandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UGP_CommandComponent::BuildSmartCommand(
	AActor* TargetActor,
	const FVector& TargetLocation,
	bool bQueue,
	FGP_CommandRequest& OutRequest) const
{
	OutRequest = FGP_CommandRequest{};

	const AGP_PlayerController* PlayerController = Cast<AGP_PlayerController>(GetOwner());
	if (PlayerController == nullptr)
	{
		return false;
	}

	const UGP_SelectionComponent* SelectionComponent = PlayerController->GetSelectionComponent();
	if (SelectionComponent == nullptr)
	{
		return false;
	}

	const AGP_PlayerState* GPPlayerState = PlayerController->GetPlayerState<AGP_PlayerState>();
	if (GPPlayerState == nullptr)
	{
		return false;
	}

	// Local playable TeamId: -1 unassigned, 0 neutral, 1+ playable (same as selection policy).
	const int32 LocalTeamId = GPPlayerState->GetTeamId();
	if (LocalTeamId < 1)
	{
		return false;
	}

	const TArray<TWeakObjectPtr<AGP_UnitBase>>& SelectedUnits = SelectionComponent->GetSelectedUnits();

	TArray<TObjectPtr<AGP_UnitBase>> NormalizedUnits;
	NormalizedUnits.Reserve(FMath::Min(SelectedUnits.Num(), GPCommandPrivate::MaxIssuingUnits));

	TSet<const AGP_UnitBase*> SeenUnits;
	SeenUnits.Reserve(NormalizedUnits.Max());

	for (const TWeakObjectPtr<AGP_UnitBase>& WeakUnit : SelectedUnits)
	{
		AGP_UnitBase* Unit = WeakUnit.Get();
		if (Unit == nullptr)
		{
			continue;
		}

		if (SeenUnits.Contains(Unit))
		{
			continue;
		}
		SeenUnits.Add(Unit);

		NormalizedUnits.Add(Unit);
		if (NormalizedUnits.Num() >= GPCommandPrivate::MaxIssuingUnits)
		{
			break;
		}
	}

	if (NormalizedUnits.Num() == 0)
	{
		return false;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	FGameplayTag CommandTag;
	AActor* RequestTargetActor = nullptr;

	if (TargetActor == nullptr)
	{
		CommandTag = GPTags.Command_Move;
	}
	else if (const AGP_ResourceNode* ResourceNode = Cast<AGP_ResourceNode>(TargetActor))
	{
		// Canonical Ferronite deposit: AGP_ResourceNode (AActor), not UnitBase.
		if (ResourceNode->HasResourceCapabilityTag(GPTags.Resource_Node))
		{
			CommandTag = GPTags.Command_Mine;
			RequestTargetActor = TargetActor;
		}
		else
		{
			CommandTag = GPTags.Command_Move;
			RequestTargetActor = nullptr;
		}
	}
	else if (const AGP_UnitBase* TargetUnit = Cast<AGP_UnitBase>(TargetActor))
	{
		const int32 TargetTeamId = TargetUnit->GetTeamId();

		if (TargetTeamId >= 1 && TargetTeamId != LocalTeamId)
		{
			// Enemy (assigned, different team). Speculative — server must validate attackability.
			CommandTag = GPTags.Command_Attack;
			RequestTargetActor = TargetActor;
		}
		else if (TargetUnit->HasCapabilityTag(GPTags.Resource_Node))
		{
			// Legacy/hybrid UnitBase path if a unit ever carries Resource.Node.
			CommandTag = GPTags.Command_Mine;
			RequestTargetActor = TargetActor;
		}
		else if (TargetTeamId >= 1 && TargetTeamId == LocalTeamId)
		{
			CommandTag = GPTags.Command_Move;
			RequestTargetActor = Cast<AGP_MainBase>(TargetActor) != nullptr ? TargetActor : nullptr;
		}
		else
		{
			// Neutral (0) or unassigned (-1): speculative Attack. Server must validate.
			CommandTag = GPTags.Command_Attack;
			RequestTargetActor = TargetActor;
		}
	}
	else
	{
		CommandTag = GPTags.Command_Move;
		RequestTargetActor = nullptr;
	}

	if (!CommandTag.IsValid()
		|| NormalizedUnits.Num() < 1
		|| NormalizedUnits.Num() > GPCommandPrivate::MaxIssuingUnits)
	{
		OutRequest = FGP_CommandRequest{};
		return false;
	}

	OutRequest.CommandTag = CommandTag;
	OutRequest.IssuingUnits = MoveTemp(NormalizedUnits);
	OutRequest.TargetLocation = TargetLocation;
	OutRequest.TargetActor = RequestTargetActor;
	OutRequest.bQueue = bQueue;
	return true;
}

bool UGP_CommandComponent::ValidateAndNormalizeCommand(
	const FGP_CommandRequest& ClientRequest,
	FGP_CommandRequest& OutValidatedRequest,
	EGP_CommandRejectReason& OutRejectReason) const
{
	OutValidatedRequest = FGP_CommandRequest{};
	OutRejectReason = EGP_CommandRejectReason::None;

	auto Fail = [&OutValidatedRequest, &OutRejectReason](EGP_CommandRejectReason Reason) -> bool
	{
		OutValidatedRequest = FGP_CommandRequest{};
		OutRejectReason = Reason;
		return false;
	};

	const AGP_PlayerController* RequestingController = Cast<AGP_PlayerController>(GetOwner());
	if (RequestingController == nullptr)
	{
		return Fail(EGP_CommandRejectReason::InvalidController);
	}

	const AGP_PlayerState* GPPlayerState = RequestingController->GetPlayerState<AGP_PlayerState>();
	if (GPPlayerState == nullptr)
	{
		return Fail(EGP_CommandRejectReason::InvalidPlayerState);
	}

	const int32 RequestingTeamId = GPPlayerState->GetTeamId();
	if (RequestingTeamId < 1)
	{
		return Fail(EGP_CommandRejectReason::InvalidRequestingTeam);
	}

	if (!ClientRequest.CommandTag.IsValid())
	{
		return Fail(EGP_CommandRejectReason::InvalidCommandTag);
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const FGameplayTag& CommandTag = ClientRequest.CommandTag;
	const bool bIsMove = CommandTag == GPTags.Command_Move;
	const bool bIsAttack = CommandTag == GPTags.Command_Attack;
	const bool bIsAttackMove = CommandTag == GPTags.Command_AttackMove;
	const bool bIsMine = CommandTag == GPTags.Command_Mine;
	const bool bIsStop = CommandTag == GPTags.Command_Stop;
	if (!bIsMove && !bIsAttack && !bIsAttackMove && !bIsMine && !bIsStop)
	{
		return Fail(EGP_CommandRejectReason::UnsupportedCommandTag);
	}

	const int32 ReceivedUnits = ClientRequest.IssuingUnits.Num();
	int32 InvalidCount = 0;
	int32 DuplicateCount = 0;
	int32 UnauthorizedCount = 0;
	int32 CappedCount = 0;

	TArray<TObjectPtr<AGP_UnitBase>> AcceptedUnits;
	AcceptedUnits.Reserve(FMath::Min(ReceivedUnits, GPCommandPrivate::MaxIssuingUnits));

	TSet<const AGP_UnitBase*> SeenUnits;
	SeenUnits.Reserve(AcceptedUnits.Max());

	for (const TObjectPtr<AGP_UnitBase>& UnitPtr : ClientRequest.IssuingUnits)
	{
		AGP_UnitBase* Unit = UnitPtr.Get();
		if (!IsValid(Unit))
		{
			++InvalidCount;
			continue;
		}

		if (SeenUnits.Contains(Unit))
		{
			++DuplicateCount;
			continue;
		}

		if (Unit->GetTeamId() != RequestingTeamId)
		{
			++UnauthorizedCount;
			continue;
		}

		if (AcceptedUnits.Num() >= GPCommandPrivate::MaxIssuingUnits)
		{
			++CappedCount;
			continue;
		}

		SeenUnits.Add(Unit);
		AcceptedUnits.Add(Unit);
	}

	if (UnauthorizedCount > 0)
	{
		UE_LOG(LogGPCommandServer, Warning,
			TEXT("GP CommandServer UnauthorizedUnits: PC=%s ReceivedUnits=%d UnauthorizedUnits=%d"),
			*GetNameSafe(RequestingController),
			ReceivedUnits,
			UnauthorizedCount);
	}

	(void)InvalidCount;
	(void)DuplicateCount;
	(void)CappedCount;

	if (AcceptedUnits.Num() == 0)
	{
		return Fail(EGP_CommandRejectReason::NoCommandableUnits);
	}

	AActor* NormalizedTargetActor = nullptr;
	FVector NormalizedLocation = FVector::ZeroVector;

	if (bIsMove || bIsAttackMove)
	{
		NormalizedTargetActor = nullptr;
		NormalizedLocation = ClientRequest.TargetLocation;
		if (bIsMove)
		{
			if (AGP_MainBase* MainBase = Cast<AGP_MainBase>(ClientRequest.TargetActor.Get()))
			{
				if (IsValid(MainBase) && MainBase->GetTeamId() == RequestingTeamId)
				{
					NormalizedTargetActor = MainBase;
					NormalizedLocation = MainBase->GetActorLocation();
				}
			}
		}
		if (!GPCommandPrivate::IsCommandLocationSane(NormalizedLocation))
		{
			return Fail(EGP_CommandRejectReason::InvalidTargetLocation);
		}

		// GP-S32A: AttackMove is combat-capable movable units only (SalvageWalker MVP).
		if (bIsAttackMove)
		{
			TArray<TObjectPtr<AGP_UnitBase>> CombatUnits;
			CombatUnits.Reserve(AcceptedUnits.Num());
			for (const TObjectPtr<AGP_UnitBase>& UnitPtr : AcceptedUnits)
			{
				AGP_UnitBase* Unit = UnitPtr.Get();
				if (IsValid(Unit)
					&& GPTags.Unit_Type_SalvageWalker.IsValid()
					&& Unit->HasCapabilityTag(GPTags.Unit_Type_SalvageWalker))
				{
					CombatUnits.Add(UnitPtr);
				}
			}

			if (CombatUnits.Num() == 0)
			{
				return Fail(EGP_CommandRejectReason::UnsupportedUnit);
			}

			AcceptedUnits = MoveTemp(CombatUnits);
		}
	}
	else if (bIsAttack)
	{
		AGP_UnitBase* TargetUnit = Cast<AGP_UnitBase>(ClientRequest.TargetActor.Get());
		if (!IsValid(TargetUnit))
		{
			return Fail(EGP_CommandRejectReason::InvalidTarget);
		}

		const int32 TargetTeamId = TargetUnit->GetTeamId();
		if (TargetTeamId == RequestingTeamId)
		{
			return Fail(EGP_CommandRejectReason::FriendlyAttackTarget);
		}

		// TeamId 0 / -1 = neutral candidate; other playable teams = enemy.
		NormalizedTargetActor = TargetUnit;
		NormalizedLocation = TargetUnit->GetActorLocation();
		if (!GPCommandPrivate::IsCommandLocationSane(NormalizedLocation))
		{
			return Fail(EGP_CommandRejectReason::InvalidTargetLocation);
		}
	}
	else if (bIsStop)
	{
		NormalizedTargetActor = nullptr;
		NormalizedLocation = FVector::ZeroVector;
	}
	else // Mine
	{
		AActor* TargetActor = ClientRequest.TargetActor.Get();
		if (!IsValid(TargetActor) || TargetActor->IsActorBeingDestroyed())
		{
			return Fail(EGP_CommandRejectReason::InvalidResourceTarget);
		}

		const UWorld* OwnerWorld = GetWorld();
		if (OwnerWorld == nullptr || TargetActor->GetWorld() != OwnerWorld)
		{
			return Fail(EGP_CommandRejectReason::InvalidResourceTarget);
		}

		AGP_ResourceNode* ResourceNode = Cast<AGP_ResourceNode>(TargetActor);
		if (ResourceNode != nullptr)
		{
			FString MineFail;
			if (!ResourceNode->CanAcceptMineCommand(true, &MineFail))
			{
				UE_LOG(LogGPCommandServer, Verbose,
					TEXT("GP CommandServer Mine rejected ResourceNode: Target=%s Reason=%s"),
					*GetNameSafe(ResourceNode),
					*MineFail);
				return Fail(EGP_CommandRejectReason::InvalidResourceTarget);
			}

			NormalizedTargetActor = ResourceNode;
			NormalizedLocation = ResourceNode->GetActorLocation();
		}
		else if (AGP_UnitBase* ResourceUnit = Cast<AGP_UnitBase>(TargetActor))
		{
			// Legacy UnitBase resource-capable target (not the Ferronite deposit path).
			if (!ResourceUnit->HasCapabilityTag(GPTags.Resource_Node))
			{
				return Fail(EGP_CommandRejectReason::InvalidResourceTarget);
			}

			NormalizedTargetActor = ResourceUnit;
			NormalizedLocation = ResourceUnit->GetActorLocation();
		}
		else
		{
			return Fail(EGP_CommandRejectReason::InvalidResourceTarget);
		}

		if (!GPCommandPrivate::IsCommandLocationSane(NormalizedLocation))
		{
			return Fail(EGP_CommandRejectReason::InvalidTargetLocation);
		}

		// Mine execution is Worker-only (GP-S27). Drop non-Worker issuers.
		TArray<TObjectPtr<AGP_UnitBase>> WorkerUnits;
		WorkerUnits.Reserve(AcceptedUnits.Num());
		for (const TObjectPtr<AGP_UnitBase>& UnitPtr : AcceptedUnits)
		{
			if (Cast<AGP_Worker>(UnitPtr.Get()) != nullptr)
			{
				WorkerUnits.Add(UnitPtr);
			}
		}

		if (WorkerUnits.Num() == 0)
		{
			return Fail(EGP_CommandRejectReason::UnsupportedUnit);
		}

		AcceptedUnits = MoveTemp(WorkerUnits);
	}

	if (AcceptedUnits.Num() < 1 || AcceptedUnits.Num() > GPCommandPrivate::MaxIssuingUnits || !CommandTag.IsValid())
	{
		return Fail(EGP_CommandRejectReason::NoCommandableUnits);
	}

	OutValidatedRequest.CommandTag = CommandTag;
	OutValidatedRequest.IssuingUnits = MoveTemp(AcceptedUnits);
	OutValidatedRequest.TargetLocation = NormalizedLocation;
	OutValidatedRequest.TargetActor = NormalizedTargetActor;
	OutValidatedRequest.bQueue = ClientRequest.bQueue;
	OutRejectReason = EGP_CommandRejectReason::None;
	return true;
}

int32 UGP_CommandComponent::DispatchValidatedCommand(const FGP_CommandRequest& ValidatedRequest) const
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		UE_LOG(LogGPCommandServer, Warning,
			TEXT("GP CommandDispatch: skipped — owner missing authority (Owner=%s)"),
			*GetNameSafe(OwnerActor));
		return 0;
	}

	FGP_UnitCommand UnitCommand;
	UnitCommand.CommandTag = ValidatedRequest.CommandTag;
	UnitCommand.TargetLocation = ValidatedRequest.TargetLocation;
	UnitCommand.TargetActor = ValidatedRequest.TargetActor.Get();
	UnitCommand.bQueue = ValidatedRequest.bQueue;

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const bool bFriendlyMainBaseMove =
		ValidatedRequest.CommandTag == GPTags.Command_Move
		&& Cast<AGP_MainBase>(ValidatedRequest.TargetActor.Get()) != nullptr;
	const bool bSpreadDestination =
		(ValidatedRequest.CommandTag == GPTags.Command_Move
			|| ValidatedRequest.CommandTag == GPTags.Command_AttackMove)
		&& !bFriendlyMainBaseMove;

	TArray<FVector> SpreadSlots;
	if (bSpreadDestination)
	{
		GPCommandPrivate::BuildGroupDestinationSlots(
			ValidatedRequest.TargetLocation,
			ValidatedRequest.IssuingUnits.Num(),
			SpreadSlots);
	}

	int32 DeliveredUnits = 0;
	int32 SlotIndex = 0;

	for (const TObjectPtr<AGP_UnitBase>& UnitPtr : ValidatedRequest.IssuingUnits)
	{
		AGP_UnitBase* Unit = UnitPtr.Get();
		if (!IsValid(Unit) || !Unit->HasAuthority())
		{
			++SlotIndex;
			continue;
		}

		if (bSpreadDestination && SpreadSlots.IsValidIndex(SlotIndex))
		{
			UnitCommand.TargetLocation = GPCommandPrivate::ProjectGroupSlotOrFallback(
				Unit->GetWorld(),
				SpreadSlots[SlotIndex],
				ValidatedRequest.TargetLocation,
				SlotIndex);
		}
		else
		{
			UnitCommand.TargetLocation = ValidatedRequest.TargetLocation;
		}

		Unit->ReceiveCommand(UnitCommand);
		++DeliveredUnits;
		++SlotIndex;
	}

	return DeliveredUnits;
}

#if !UE_BUILD_SHIPPING
namespace GPCommandMineDebug
{
	static const TCHAR* EvaluateMineTargetAcceptance(AActor* TargetActor, const UWorld* World, FString& OutDetail)
	{
		const FGPGameplayTags& GPTags = FGPGameplayTags::Get();

		if (!IsValid(TargetActor) || TargetActor->IsActorBeingDestroyed())
		{
			OutDetail = TEXT("InvalidOrNullTarget");
			return TEXT("REJECT");
		}

		if (World == nullptr || TargetActor->GetWorld() != World)
		{
			OutDetail = TEXT("InvalidWorld");
			return TEXT("REJECT");
		}

		if (AGP_ResourceNode* ResourceNode = Cast<AGP_ResourceNode>(TargetActor))
		{
			FString MineFail;
			if (!ResourceNode->CanAcceptMineCommand(true, &MineFail))
			{
				OutDetail = FString::Printf(TEXT("ResourceNodeFail=%s"), *MineFail);
				return TEXT("REJECT");
			}

			OutDetail = TEXT("ResourceNodeAccepted");
			return TEXT("ACCEPT");
		}

		if (AGP_UnitBase* ResourceUnit = Cast<AGP_UnitBase>(TargetActor))
		{
			if (!ResourceUnit->HasCapabilityTag(GPTags.Resource_Node))
			{
				OutDetail = TEXT("UnitWithoutResourceNodeCapability");
				return TEXT("REJECT");
			}

			OutDetail = TEXT("UnitBaseResourceNodeLegacyAccepted");
			return TEXT("ACCEPT");
		}

		OutDetail = TEXT("ActorWithoutResourceContract");
		return TEXT("REJECT");
	}

	static void InspectMineTarget(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPCommandServer, Warning, TEXT("GP Command.InspectMineTarget: missing world"));
			return;
		}

		FString Detail;
		const TCHAR* NullResult = EvaluateMineTargetAcceptance(nullptr, World, Detail);
		UE_LOG(LogGPCommandServer, Log,
			TEXT("GP Command.InspectMineTarget: Case=NullTarget Result=%s Detail=%s"),
			NullResult,
			*Detail);

		AGP_ResourceNode* Node = nullptr;
		AGP_ResourceNode* DepletedNode = nullptr;
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			AGP_ResourceNode* Candidate = *It;
			if (!IsValid(Candidate))
			{
				continue;
			}
			if (Node == nullptr)
			{
				Node = Candidate;
			}
			if (Candidate->IsDepleted() && DepletedNode == nullptr)
			{
				DepletedNode = Candidate;
			}
		}

		if (Node != nullptr)
		{
			const TCHAR* NodeResult = EvaluateMineTargetAcceptance(Node, World, Detail);
			UE_LOG(LogGPCommandServer, Log,
				TEXT("GP Command.InspectMineTarget: Case=ResourceNode Actor=%s Current=%d Depleted=%s Result=%s Detail=%s"),
				*Node->GetName(),
				Node->GetCurrentAmount(),
				Node->IsDepleted() ? TEXT("true") : TEXT("false"),
				NodeResult,
				*Detail);
		}
		else
		{
			UE_LOG(LogGPCommandServer, Log,
				TEXT("GP Command.InspectMineTarget: Case=ResourceNode Result=SKIP Detail=NoResourceNodeInWorld"));
		}

		if (DepletedNode != nullptr)
		{
			const TCHAR* DepletedResult = EvaluateMineTargetAcceptance(DepletedNode, World, Detail);
			UE_LOG(LogGPCommandServer, Log,
				TEXT("GP Command.InspectMineTarget: Case=DepletedResourceNode Actor=%s Result=%s Detail=%s"),
				*DepletedNode->GetName(),
				DepletedResult,
				*Detail);
		}
		else
		{
			UE_LOG(LogGPCommandServer, Log,
				TEXT("GP Command.InspectMineTarget: Case=DepletedResourceNode Result=SKIP Detail=NoDepletedNode (use gp.ResourceNode.Consume to deplete)"));
		}

		AGP_UnitBase* OrdinaryUnit = nullptr;
		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (!IsValid(Unit))
			{
				continue;
			}
			if (!Unit->HasCapabilityTag(FGPGameplayTags::Get().Resource_Node))
			{
				OrdinaryUnit = Unit;
				break;
			}
		}

		if (OrdinaryUnit != nullptr)
		{
			const TCHAR* UnitResult = EvaluateMineTargetAcceptance(OrdinaryUnit, World, Detail);
			UE_LOG(LogGPCommandServer, Log,
				TEXT("GP Command.InspectMineTarget: Case=OrdinaryUnit Actor=%s Result=%s Detail=%s"),
				*OrdinaryUnit->GetName(),
				UnitResult,
				*Detail);
		}
		else
		{
			UE_LOG(LogGPCommandServer, Log,
				TEXT("GP Command.InspectMineTarget: Case=OrdinaryUnit Result=SKIP Detail=NoOrdinaryUnitInWorld"));
		}

		AActor* PlainActor = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor))
			{
				continue;
			}
			if (Cast<AGP_ResourceNode>(Actor) != nullptr || Cast<AGP_UnitBase>(Actor) != nullptr)
			{
				continue;
			}
			PlainActor = Actor;
			break;
		}

		if (PlainActor != nullptr)
		{
			const TCHAR* PlainResult = EvaluateMineTargetAcceptance(PlainActor, World, Detail);
			UE_LOG(LogGPCommandServer, Log,
				TEXT("GP Command.InspectMineTarget: Case=ActorWithoutResourceContract Actor=%s Class=%s Result=%s Detail=%s"),
				*PlainActor->GetName(),
				*GetNameSafe(PlainActor->GetClass()),
				PlainResult,
				*Detail);
		}
		else
		{
			UE_LOG(LogGPCommandServer, Log,
				TEXT("GP Command.InspectMineTarget: Case=ActorWithoutResourceContract Result=SKIP Detail=NoPlainActorInWorld"));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs GInspectMineTargetCommand(
		TEXT("gp.Command.InspectMineTarget"),
		TEXT("Non-shipping: log Mine target accept/reject cases (null, ResourceNode, depleted, ordinary unit, plain actor)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&InspectMineTarget));
}
#endif
