// Copyright Epic Games, Inc. All Rights Reserved.

#include "Command/GPCommandComponent.h"

#include "Command/GPCommandRequest.h"
#include "Command/GPUnitCommand.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPUnitBase.h"

DEFINE_LOG_CATEGORY(LogGPCommandServer);

namespace GPCommandPrivate
{
	static constexpr int32 MaxIssuingUnits = 24;
	static constexpr double MaxAbsCoordinate = 10000000.0;

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
			// Canonical path: UnitBase CapabilityTags + GP.Resource.Node.
			CommandTag = GPTags.Command_Mine;
			RequestTargetActor = TargetActor;
		}
		else if (TargetTeamId >= 1 && TargetTeamId == LocalTeamId)
		{
			CommandTag = GPTags.Command_Move;
			RequestTargetActor = nullptr;
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
		// Non-UnitBase: no canonical Resource.Node accessor outside UnitBase CapabilityTags.
		// Mine mapping deferred for non-UnitBase actors. Deterministic Move fallback.
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
	const bool bIsMine = CommandTag == GPTags.Command_Mine;
	if (!bIsMove && !bIsAttack && !bIsMine)
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

	if (bIsMove)
	{
		NormalizedTargetActor = nullptr;
		NormalizedLocation = ClientRequest.TargetLocation;
		if (!GPCommandPrivate::IsCommandLocationSane(NormalizedLocation))
		{
			return Fail(EGP_CommandRejectReason::InvalidTargetLocation);
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
	else // Mine
	{
		AGP_UnitBase* ResourceUnit = Cast<AGP_UnitBase>(ClientRequest.TargetActor.Get());
		if (!IsValid(ResourceUnit) || !ResourceUnit->HasCapabilityTag(GPTags.Resource_Node))
		{
			return Fail(EGP_CommandRejectReason::InvalidResourceTarget);
		}

		NormalizedTargetActor = ResourceUnit;
		NormalizedLocation = ResourceUnit->GetActorLocation();
		if (!GPCommandPrivate::IsCommandLocationSane(NormalizedLocation))
		{
			return Fail(EGP_CommandRejectReason::InvalidTargetLocation);
		}
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

	int32 DeliveredUnits = 0;

	for (const TObjectPtr<AGP_UnitBase>& UnitPtr : ValidatedRequest.IssuingUnits)
	{
		AGP_UnitBase* Unit = UnitPtr.Get();
		if (!IsValid(Unit) || !Unit->HasAuthority())
		{
			continue;
		}

		Unit->ReceiveCommand(UnitCommand);
		++DeliveredUnits;
	}

	return DeliveredUnits;
}
