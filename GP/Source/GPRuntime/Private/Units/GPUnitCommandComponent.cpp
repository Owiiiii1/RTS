// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitCommandComponent.h"

#include "Command/GPUnitCommand.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPUnitBase.h"

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include <limits>
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGPUnitCommandState, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogGPUnitCommandExecution, Log, All);

namespace GPUnitCommandStatePrivate
{
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

	static const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None:
			return TEXT("None");
		case ROLE_SimulatedProxy:
			return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy:
			return TEXT("AutonomousProxy");
		case ROLE_Authority:
			return TEXT("Authority");
		default:
			return TEXT("Unknown");
		}
	}

	static ENetMode GetOwnerNetMode(const AActor* Owner)
	{
		if (Owner == nullptr)
		{
			return NM_MAX;
		}

		const UWorld* World = Owner->GetWorld();
		return World != nullptr ? World->GetNetMode() : NM_MAX;
	}

	static const TCHAR* MovementResultToString(EGP_MovementResult Result)
	{
		switch (Result)
		{
		case EGP_MovementResult::Reached:
			return TEXT("Reached");
		case EGP_MovementResult::Cancelled:
			return TEXT("Cancelled");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* MovementResultReasonToString(EGP_MovementResultReason Reason)
	{
		switch (Reason)
		{
		case EGP_MovementResultReason::None:
			return TEXT("None");
		case EGP_MovementResultReason::Superseded:
			return TEXT("Superseded");
		case EGP_MovementResultReason::CommandReplaced:
			return TEXT("CommandReplaced");
		case EGP_MovementResultReason::Manual:
			return TEXT("Manual");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* RejectReasonToString(EGP_MovementRejectReason Reason)
	{
		switch (Reason)
		{
		case EGP_MovementRejectReason::None:
			return TEXT("None");
		case EGP_MovementRejectReason::MissingOwner:
			return TEXT("MissingOwner");
		case EGP_MovementRejectReason::NoAuthority:
			return TEXT("NoAuthority");
		case EGP_MovementRejectReason::InvalidSerial:
			return TEXT("InvalidSerial");
		case EGP_MovementRejectReason::InvalidDestination:
			return TEXT("InvalidDestination");
		case EGP_MovementRejectReason::InvalidMoveSpeed:
			return TEXT("InvalidMoveSpeed");
		case EGP_MovementRejectReason::InvalidAcceptanceRadius:
			return TEXT("InvalidAcceptanceRadius");
		default:
			return TEXT("Unknown");
		}
	}
}

const TCHAR* UGP_UnitCommandComponent::AttackStateToString(EGP_AttackExecutionState State)
{
	switch (State)
	{
	case EGP_AttackExecutionState::Idle:
		return TEXT("Idle");
	case EGP_AttackExecutionState::Approaching:
		return TEXT("Approaching");
	case EGP_AttackExecutionState::Ready:
		return TEXT("Ready");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* UGP_UnitCommandComponent::AttackTerminalResultToString(EGP_AttackTerminalResult Result)
{
	switch (Result)
	{
	case EGP_AttackTerminalResult::Cancelled:
		return TEXT("Cancelled");
	case EGP_AttackTerminalResult::Failed:
		return TEXT("Failed");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* UGP_UnitCommandComponent::AttackTerminalReasonToString(EGP_AttackTerminalReason Reason)
{
	switch (Reason)
	{
	case EGP_AttackTerminalReason::CommandReplaced:
		return TEXT("CommandReplaced");
	case EGP_AttackTerminalReason::InvalidTarget:
		return TEXT("InvalidTarget");
	case EGP_AttackTerminalReason::TargetDestroyed:
		return TEXT("TargetDestroyed");
	case EGP_AttackTerminalReason::MovementRejected:
		return TEXT("MovementRejected");
	case EGP_AttackTerminalReason::MovementCancelled:
		return TEXT("MovementCancelled");
	case EGP_AttackTerminalReason::EndPlay:
		return TEXT("EndPlay");
	default:
		return TEXT("Unknown");
	}
}

UGP_UnitCommandComponent::UGP_UnitCommandComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetComponentTickEnabled(false);
	SetIsReplicatedByDefault(false);
}

void UGP_UnitCommandComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	AGP_MobileUnit* MobileUnit = Cast<AGP_MobileUnit>(Owner);
	if (MobileUnit == nullptr)
	{
		return;
	}

	UGP_MovementComponent* Movement = MobileUnit->GetUnitMovementComponent();
	if (Movement == nullptr)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MovementUnavailable: Unit=%s Serial=0 Destination=none Reason=MissingComponent Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			GPUnitCommandStatePrivate::RoleToString(Owner->GetLocalRole()),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
		return;
	}

	BoundMovementComponent = Movement;
	MovementResultHandle = Movement->OnMovementResult().AddUObject(
		this,
		&UGP_UnitCommandComponent::HandleMovementResult);
}

void UGP_UnitCommandComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AttackState != EGP_AttackExecutionState::Idle || ActiveAttackSerial != 0)
	{
		const AActor* Owner = GetOwner();
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution AttackCancelled: Unit=%s AttackSerial=%u Target=%s Reason=EndPlay PreviousState=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ActiveAttackSerial,
			*GetNameSafe(AttackTarget.Get()),
			AttackStateToString(AttackState),
			GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
	}

	ResetAttackExecutor();

	if (BoundMovementComponent.IsValid() && MovementResultHandle.IsValid())
	{
		BoundMovementComponent->OnMovementResult().Remove(MovementResultHandle);
	}
	MovementResultHandle.Reset();
	BoundMovementComponent.Reset();

	if (HeldCommand.IsSet())
	{
		const AActor* Owner = GetOwner();
		const FGP_StoredUnitCommand& Cleared = HeldCommand.GetValue();
		const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
		const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

		UE_LOG(LogGPUnitCommandState, Log,
			TEXT("GP UnitCommandState HeldCleared: Unit=%s Serial=%u Tag=%s Reason=EndPlay Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			Cleared.CommandSerial,
			*Cleared.CommandTag.ToString(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));

		ClearHeldCommand();
	}

	Super::EndPlay(EndPlayReason);
}

void UGP_UnitCommandComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		SetAttackTickEnabled(false);
		return;
	}

	if (AttackState == EGP_AttackExecutionState::Idle)
	{
		SetAttackTickEnabled(false);
		return;
	}

	EvaluateAttack();
}

void UGP_UnitCommandComponent::SetAttackTickEnabled(bool bEnabled)
{
	SetComponentTickEnabled(bEnabled);
}

bool UGP_UnitCommandComponent::IsAttackConfigValid() const
{
	return FMath::IsFinite(AttackRange) && AttackRange >= 0.0f
		&& FMath::IsFinite(AttackReissueDistance) && AttackReissueDistance >= 0.0f
		&& FMath::IsFinite(AttackReissueInterval) && AttackReissueInterval >= 0.0f;
}

bool UGP_UnitCommandComponent::TryComputeAttackDistance2D(
	const AActor* Owner,
	const AActor* Target,
	float& OutDistance) const
{
	OutDistance = -1.0f;
	if (Owner == nullptr || !IsValid(Target))
	{
		return false;
	}

	OutDistance = FVector::Dist2D(Owner->GetActorLocation(), Target->GetActorLocation());
	return true;
}

FVector UGP_UnitCommandComponent::MakeApproachDestination(const AActor* Owner, const AActor* Target) const
{
	const FVector OwnerLocation = Owner != nullptr ? Owner->GetActorLocation() : FVector::ZeroVector;
	const FVector TargetLocation = Target != nullptr ? Target->GetActorLocation() : FVector::ZeroVector;
	return FVector(TargetLocation.X, TargetLocation.Y, OwnerLocation.Z);
}

UGP_MovementComponent* UGP_UnitCommandComponent::ResolveMovementComponent() const
{
	if (BoundMovementComponent.IsValid())
	{
		return BoundMovementComponent.Get();
	}

	const AGP_MobileUnit* MobileUnit = Cast<AGP_MobileUnit>(GetOwner());
	return MobileUnit != nullptr ? MobileUnit->GetUnitMovementComponent() : nullptr;
}

bool UGP_UnitCommandComponent::HasExactActiveHeldAttack() const
{
	if (!HeldCommand.IsSet() || ActiveAttackSerial == 0)
	{
		return false;
	}

	const FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
	return Held.CommandTag == FGPGameplayTags::Get().Command_Attack
		&& Held.CommandSerial == ActiveAttackSerial;
}

bool UGP_UnitCommandComponent::ValidateAttackTarget(
	AActor* Candidate,
	AGP_UnitBase*& OutTarget,
	EGP_AttackTerminalReason& OutReason) const
{
	OutTarget = nullptr;
	OutReason = EGP_AttackTerminalReason::InvalidTarget;

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (OwnerUnit == nullptr)
	{
		return false;
	}

	if (!IsValid(Candidate))
	{
		OutReason = EGP_AttackTerminalReason::TargetDestroyed;
		return false;
	}

	if (Candidate == Owner)
	{
		OutReason = EGP_AttackTerminalReason::InvalidTarget;
		return false;
	}

	AGP_UnitBase* TargetUnit = Cast<AGP_UnitBase>(Candidate);
	if (TargetUnit == nullptr)
	{
		OutReason = EGP_AttackTerminalReason::InvalidTarget;
		return false;
	}

	if (!IsValid(TargetUnit) || TargetUnit->IsActorBeingDestroyed())
	{
		OutReason = EGP_AttackTerminalReason::TargetDestroyed;
		return false;
	}

	if (TargetUnit->GetWorld() != Owner->GetWorld())
	{
		OutReason = EGP_AttackTerminalReason::InvalidTarget;
		return false;
	}

	const FVector TargetLocation = TargetUnit->GetActorLocation();
	if (TargetLocation.ContainsNaN()
		|| !FMath::IsFinite(TargetLocation.X)
		|| !FMath::IsFinite(TargetLocation.Y)
		|| !FMath::IsFinite(TargetLocation.Z))
	{
		OutReason = EGP_AttackTerminalReason::InvalidTarget;
		return false;
	}

	const int32 OwnerTeamId = OwnerUnit->GetTeamId();
	if (OwnerTeamId < 1)
	{
		OutReason = EGP_AttackTerminalReason::InvalidTarget;
		return false;
	}

	const int32 TargetTeamId = TargetUnit->GetTeamId();
	// TeamId 0 (neutral) and -1 (unassigned) are allowed — matches server ValidateAndNormalizeCommand.
	if (TargetTeamId == OwnerTeamId)
	{
		OutReason = EGP_AttackTerminalReason::InvalidTarget;
		return false;
	}

	OutTarget = TargetUnit;
	OutReason = EGP_AttackTerminalReason::InvalidTarget;
	return true;
}

void UGP_UnitCommandComponent::ResetAttackExecutor()
{
	AttackState = EGP_AttackExecutionState::Idle;
	ActiveAttackSerial = 0;
	AttackTarget.Reset();
	LastApproachDestination = FVector::ZeroVector;
	LastApproachIssueTime = -1.0;
	bExpectRangeEntryStop = false;
	bExpectAttackCleanupStopResult = false;
	PendingAttackCleanupMovementSerial = 0;
	SetAttackTickEnabled(false);
}

void UGP_UnitCommandComponent::ResetAttackExecutorForReplacement(
	const TOptional<FGP_StoredUnitCommand>& PreviousCommand)
{
	if (AttackState == EGP_AttackExecutionState::Idle && ActiveAttackSerial == 0)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	const uint32 OldSerial = ActiveAttackSerial;
	const EGP_AttackExecutionState OldState = AttackState;
	AGP_UnitBase* OldTarget = AttackTarget.Get();

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackCancelled: Unit=%s AttackSerial=%u Target=%s Reason=CommandReplaced PreviousState=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		OldSerial,
		*GetNameSafe(OldTarget),
		AttackStateToString(OldState),
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));

	(void)PreviousCommand;
	ResetAttackExecutor();
}

bool UGP_UnitCommandComponent::StartAttackExecutor()
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	if (Owner == nullptr || !Owner->HasAuthority() || !HeldCommand.IsSet())
	{
		return HeldCommand.IsSet();
	}

	const FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
	const FGameplayTag AttackTag = FGPGameplayTags::Get().Command_Attack;
	if (!(Held.CommandTag == AttackTag))
	{
		return true;
	}

	if (!IsAttackConfigValid())
	{
		const uint32 RejectedSerial = Held.CommandSerial;
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution AttackRejected: Unit=%s AttackSerial=%u Target=%s Reason=InvalidTarget Detail=InvalidAttackConfig Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			RejectedSerial,
			*GetNameSafe(Held.TargetActor.Get()),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));

		if (HeldCommand.IsSet()
			&& HeldCommand.GetValue().CommandTag == AttackTag
			&& HeldCommand.GetValue().CommandSerial == RejectedSerial)
		{
			ClearHeldCommand();
		}
		ResetAttackExecutor();
		return false;
	}

	const uint32 AttackSerial = Held.CommandSerial;
	ActiveAttackSerial = AttackSerial;

	AGP_UnitBase* ValidTarget = nullptr;
	EGP_AttackTerminalReason FailReason = EGP_AttackTerminalReason::InvalidTarget;
	if (!ValidateAttackTarget(Held.TargetActor.Get(), ValidTarget, FailReason))
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution AttackRejected: Unit=%s AttackSerial=%u Target=%s Reason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			AttackSerial,
			*GetNameSafe(Held.TargetActor.Get()),
			AttackTerminalReasonToString(FailReason),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));

		if (HeldCommand.IsSet()
			&& HeldCommand.GetValue().CommandTag == AttackTag
			&& HeldCommand.GetValue().CommandSerial == AttackSerial)
		{
			ClearHeldCommand();
		}
		ResetAttackExecutor();
		return false;
	}

	AttackTarget = ValidTarget;
	SetAttackTickEnabled(true);

	float Distance = -1.0f;
	const bool bDistanceAvailable = TryComputeAttackDistance2D(Owner, ValidTarget, Distance);
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackAccepted: Unit=%s AttackSerial=%u Target=%s Distance=%.1f DistanceAvailable=%s AttackRange=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		AttackSerial,
		*GetNameSafe(ValidTarget),
		Distance,
		bDistanceAvailable ? TEXT("true") : TEXT("false"),
		AttackRange,
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	if (bDistanceAvailable && Distance <= AttackRange)
	{
		EnterAttackReady();
	}
	else
	{
		EnterAttackApproaching();
	}

	return HasExactActiveHeldAttack();
}

void UGP_UnitCommandComponent::EnterAttackApproaching()
{
	AActor* Owner = GetOwner();
	const EGP_AttackExecutionState PreviousState = AttackState;
	AttackState = EGP_AttackExecutionState::Approaching;
	SetAttackTickEnabled(true);

	float Distance = -1.0f;
	const bool bDistanceAvailable = TryComputeAttackDistance2D(Owner, AttackTarget.Get(), Distance);
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackStateChanged: Unit=%s AttackSerial=%u Target=%s PreviousState=%s NewState=Approaching Distance=%.1f DistanceAvailable=%s AttackRange=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveAttackSerial,
		*GetNameSafe(AttackTarget.Get()),
		AttackStateToString(PreviousState),
		Distance,
		bDistanceAvailable ? TEXT("true") : TEXT("false"),
		AttackRange,
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));

	RequestOrRefreshAttackApproach(true);
}

void UGP_UnitCommandComponent::EnterAttackReady()
{
	if (!HasExactActiveHeldAttack())
	{
		return;
	}

	AActor* Owner = GetOwner();
	const EGP_AttackExecutionState PreviousState = AttackState;
	AttackState = EGP_AttackExecutionState::Ready;
	bExpectRangeEntryStop = false;
	SetAttackTickEnabled(true);

	float Distance = -1.0f;
	const bool bDistanceAvailable = TryComputeAttackDistance2D(Owner, AttackTarget.Get(), Distance);
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackStateChanged: Unit=%s AttackSerial=%u Target=%s PreviousState=%s NewState=Ready Distance=%.1f DistanceAvailable=%s AttackRange=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveAttackSerial,
		*GetNameSafe(AttackTarget.Get()),
		AttackStateToString(PreviousState),
		Distance,
		bDistanceAvailable ? TEXT("true") : TEXT("false"),
		AttackRange,
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackReady: Unit=%s AttackSerial=%u Target=%s Distance=%.1f DistanceAvailable=%s AttackRange=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveAttackSerial,
		*GetNameSafe(AttackTarget.Get()),
		Distance,
		bDistanceAvailable ? TEXT("true") : TEXT("false"),
		AttackRange,
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
}

void UGP_UnitCommandComponent::RequestOrRefreshAttackApproach(bool bForceIssue)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || !HasExactActiveHeldAttack())
	{
		return;
	}

	AGP_UnitBase* Target = nullptr;
	EGP_AttackTerminalReason FailReason = EGP_AttackTerminalReason::InvalidTarget;
	if (!ValidateAttackTarget(AttackTarget.Get(), Target, FailReason))
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution AttackTargetInvalidated: Unit=%s AttackSerial=%u Target=%s Reason=%s Distance=-1.0 DistanceAvailable=false Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ActiveAttackSerial,
			*GetNameSafe(AttackTarget.Get()),
			AttackTerminalReasonToString(FailReason),
			GPUnitCommandStatePrivate::RoleToString(Owner->GetLocalRole()),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
		FinishAttack(EGP_AttackTerminalResult::Failed, FailReason);
		return;
	}

	AttackTarget = Target;

	float Distance = -1.0f;
	if (!TryComputeAttackDistance2D(Owner, Target, Distance))
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
		return;
	}

	if (Distance <= AttackRange)
	{
		UGP_MovementComponent* Movement = ResolveMovementComponent();
		if (Movement != nullptr && Movement->IsMoving()
			&& Movement->GetActiveMoveSerial() == ActiveAttackSerial)
		{
			bExpectRangeEntryStop = true;
			Movement->StopMove(EGP_MovementStopReason::Manual);
			return;
		}

		EnterAttackReady();
		return;
	}

	const UWorld* World = Owner->GetWorld();
	const double Now = World != nullptr ? World->GetTimeSeconds() : 0.0;
	const FVector Destination = MakeApproachDestination(Owner, Target);

	if (!bForceIssue)
	{
		const float DestDelta = FVector::Dist2D(Destination, LastApproachDestination);
		const bool bIntervalOk = LastApproachIssueTime < 0.0
			|| (Now - LastApproachIssueTime) >= static_cast<double>(AttackReissueInterval);
		const bool bDistanceOk = DestDelta >= AttackReissueDistance;
		if (!bIntervalOk || !bDistanceOk)
		{
			return;
		}
	}

	UGP_MovementComponent* Movement = ResolveMovementComponent();
	if (Movement == nullptr)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution AttackApproachRejected: Unit=%s AttackSerial=%u RejectReason=MissingComponent Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ActiveAttackSerial,
			GPUnitCommandStatePrivate::RoleToString(Owner->GetLocalRole()),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::MovementRejected);
		return;
	}

	const FGP_MovementRequestOutcome Outcome = Movement->RequestMove(Destination, ActiveAttackSerial);
	if (!Outcome.IsAccepted())
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution AttackApproachRejected: Unit=%s AttackSerial=%u Destination=%s RejectReason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ActiveAttackSerial,
			*Destination.ToCompactString(),
			GPUnitCommandStatePrivate::RejectReasonToString(Outcome.RejectReason),
			GPUnitCommandStatePrivate::RoleToString(Owner->GetLocalRole()),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::MovementRejected);
		return;
	}

	LastApproachDestination = Destination;
	LastApproachIssueTime = Now;
	AttackState = EGP_AttackExecutionState::Approaching;
	SetAttackTickEnabled(true);

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackApproachRequested: Unit=%s AttackSerial=%u MovementSerial=%u Destination=%s Target=%s Distance=%.1f AttackRange=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveAttackSerial,
		ActiveAttackSerial,
		*Destination.ToCompactString(),
		*GetNameSafe(Target),
		Distance,
		AttackRange,
		GPUnitCommandStatePrivate::RoleToString(Owner->GetLocalRole()),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
}

void UGP_UnitCommandComponent::EvaluateAttack()
{
	if (bFinishingAttack || AttackState == EGP_AttackExecutionState::Idle)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || !HasExactActiveHeldAttack())
	{
		if (AttackState != EGP_AttackExecutionState::Idle)
		{
			ResetAttackExecutor();
		}
		return;
	}

	AGP_UnitBase* Target = nullptr;
	EGP_AttackTerminalReason FailReason = EGP_AttackTerminalReason::InvalidTarget;
	if (!ValidateAttackTarget(AttackTarget.Get(), Target, FailReason))
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution AttackTargetInvalidated: Unit=%s AttackSerial=%u Target=%s Reason=%s Distance=-1.0 DistanceAvailable=false Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ActiveAttackSerial,
			*GetNameSafe(AttackTarget.Get()),
			AttackTerminalReasonToString(FailReason),
			GPUnitCommandStatePrivate::RoleToString(Owner->GetLocalRole()),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
		FinishAttack(EGP_AttackTerminalResult::Failed, FailReason);
		return;
	}

	AttackTarget = Target;
	float Distance = -1.0f;
	if (!TryComputeAttackDistance2D(Owner, Target, Distance))
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
		return;
	}

	if (AttackState == EGP_AttackExecutionState::Ready)
	{
		if (Distance > AttackRange)
		{
			EnterAttackApproaching();
		}
		return;
	}

	if (AttackState == EGP_AttackExecutionState::Approaching)
	{
		if (Distance <= AttackRange)
		{
			UGP_MovementComponent* Movement = ResolveMovementComponent();
			if (Movement != nullptr && Movement->IsMoving()
				&& Movement->GetActiveMoveSerial() == ActiveAttackSerial)
			{
				bExpectRangeEntryStop = true;
				Movement->StopMove(EGP_MovementStopReason::Manual);
				return;
			}

			EnterAttackReady();
			return;
		}

		RequestOrRefreshAttackApproach(false);
	}
}

void UGP_UnitCommandComponent::FinishAttack(
	EGP_AttackTerminalResult Result,
	EGP_AttackTerminalReason Reason)
{
	if (bFinishingAttack)
	{
		return;
	}

	bFinishingAttack = true;

	AActor* Owner = GetOwner();
	const uint32 FinishedSerial = ActiveAttackSerial;
	const EGP_AttackExecutionState PreviousState = AttackState;
	AGP_UnitBase* FinishedTarget = AttackTarget.Get();
	float Distance = -1.0f;
	const bool bDistanceAvailable = TryComputeAttackDistance2D(Owner, FinishedTarget, Distance);
	const FGameplayTag AttackTag = FGPGameplayTags::Get().Command_Attack;

	UGP_MovementComponent* Movement = ResolveMovementComponent();
	const bool bNeedCleanupStop = Movement != nullptr
		&& Movement->IsMoving()
		&& FinishedSerial != 0
		&& Movement->GetActiveMoveSerial() == FinishedSerial;

	// Range-entry must not consume terminal cleanup StopMove.
	bExpectRangeEntryStop = false;

	if (bNeedCleanupStop)
	{
		bExpectAttackCleanupStopResult = true;
		PendingAttackCleanupMovementSerial = FinishedSerial;
	}

	AttackState = EGP_AttackExecutionState::Idle;
	ActiveAttackSerial = 0;
	AttackTarget.Reset();
	LastApproachDestination = FVector::ZeroVector;
	LastApproachIssueTime = -1.0;
	SetAttackTickEnabled(false);

	if (bNeedCleanupStop)
	{
		Movement->StopMove(EGP_MovementStopReason::Manual);
	}

	// Defensive clear if StopMove did not broadcast (e.g. already idle).
	bExpectAttackCleanupStopResult = false;
	PendingAttackCleanupMovementSerial = 0;

	if (HeldCommand.IsSet())
	{
		const FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
		if (Held.CommandTag == AttackTag && Held.CommandSerial == FinishedSerial)
		{
			ClearHeldCommand();
		}
	}

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackFinished: Unit=%s AttackSerial=%u Target=%s Result=%s Reason=%s PreviousState=%s Distance=%.1f DistanceAvailable=%s AttackRange=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		FinishedSerial,
		*GetNameSafe(FinishedTarget),
		AttackTerminalResultToString(Result),
		AttackTerminalReasonToString(Reason),
		AttackStateToString(PreviousState),
		Distance,
		bDistanceAvailable ? TEXT("true") : TEXT("false"),
		AttackRange,
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));

	bFinishingAttack = false;
}

bool UGP_UnitCommandComponent::TryConsumeAttackMovementResult(
	uint32 Serial,
	EGP_MovementResult Result,
	EGP_MovementResultReason Reason)
{
	AActor* Owner = GetOwner();

	// Terminal cleanup StopMove(Manual) from FinishAttack — consume before any Held Move fallback.
	if (bExpectAttackCleanupStopResult
		&& Serial == PendingAttackCleanupMovementSerial
		&& PendingAttackCleanupMovementSerial != 0
		&& Result == EGP_MovementResult::Cancelled
		&& Reason == EGP_MovementResultReason::Manual)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution AttackApproachResultIgnored: Unit=%s AttackSerial=%u ResultSerial=%u IgnoreReason=TerminalCleanupStop MovementResult=%s MovementReason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			PendingAttackCleanupMovementSerial,
			Serial,
			GPUnitCommandStatePrivate::MovementResultToString(Result),
			GPUnitCommandStatePrivate::MovementResultReasonToString(Reason),
			GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));

		bExpectAttackCleanupStopResult = false;
		PendingAttackCleanupMovementSerial = 0;
		return true;
	}

	if (bFinishingAttack
		|| AttackState == EGP_AttackExecutionState::Idle
		|| ActiveAttackSerial == 0
		|| Serial != ActiveAttackSerial
		|| !HasExactActiveHeldAttack())
	{
		return false;
	}

	// Approach movement results are only meaningful while Approaching (or during range-entry stop).
	if (AttackState != EGP_AttackExecutionState::Approaching && !bExpectRangeEntryStop)
	{
		return false;
	}

	UGP_MovementComponent* Movement = ResolveMovementComponent();
	float Distance = -1.0f;
	const bool bDistanceAvailable = TryComputeAttackDistance2D(Owner, AttackTarget.Get(), Distance);

	auto LogApproachResult = [&](const TCHAR* Note)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution AttackApproachResult: Unit=%s AttackSerial=%u MovementSerial=%u MovementResult=%s MovementReason=%s Distance=%.1f DistanceAvailable=%s Note=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ActiveAttackSerial,
			Serial,
			GPUnitCommandStatePrivate::MovementResultToString(Result),
			GPUnitCommandStatePrivate::MovementResultReasonToString(Reason),
			Distance,
			bDistanceAvailable ? TEXT("true") : TEXT("false"),
			Note,
			GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
	};

	if (Result == EGP_MovementResult::Reached && Reason == EGP_MovementResultReason::None)
	{
		LogApproachResult(TEXT("Reached"));

		AGP_UnitBase* Target = nullptr;
		EGP_AttackTerminalReason FailReason = EGP_AttackTerminalReason::InvalidTarget;
		if (!ValidateAttackTarget(AttackTarget.Get(), Target, FailReason))
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution AttackTargetInvalidated: Unit=%s AttackSerial=%u Target=%s Reason=%s Distance=-1.0 DistanceAvailable=false Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				ActiveAttackSerial,
				*GetNameSafe(AttackTarget.Get()),
				AttackTerminalReasonToString(FailReason),
				GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
				GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
			FinishAttack(EGP_AttackTerminalResult::Failed, FailReason);
			return true;
		}

		AttackTarget = Target;
		float Dist = -1.0f;
		if (!TryComputeAttackDistance2D(Owner, Target, Dist))
		{
			FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
			return true;
		}

		if (Dist <= AttackRange)
		{
			EnterAttackReady();
		}
		else
		{
			RequestOrRefreshAttackApproach(true);
		}
		return true;
	}

	if (Result == EGP_MovementResult::Cancelled && Reason == EGP_MovementResultReason::Superseded)
	{
		if (Movement != nullptr
			&& Movement->IsMoving()
			&& Movement->GetActiveMoveSerial() == ActiveAttackSerial)
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution AttackApproachResultIgnored: Unit=%s AttackSerial=%u ResultSerial=%u IgnoreReason=SelfSupersede MovementResult=%s MovementReason=%s Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				ActiveAttackSerial,
				Serial,
				GPUnitCommandStatePrivate::MovementResultToString(Result),
				GPUnitCommandStatePrivate::MovementResultReasonToString(Reason),
				GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
				GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
			return true;
		}

		LogApproachResult(TEXT("SupersededExternal"));
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::MovementCancelled);
		return true;
	}

	if (Result == EGP_MovementResult::Cancelled && Reason == EGP_MovementResultReason::Manual)
	{
		if (bExpectRangeEntryStop)
		{
			bExpectRangeEntryStop = false;
			LogApproachResult(TEXT("RangeEntryStop"));

			AGP_UnitBase* Target = nullptr;
			EGP_AttackTerminalReason FailReason = EGP_AttackTerminalReason::InvalidTarget;
			if (!ValidateAttackTarget(AttackTarget.Get(), Target, FailReason))
			{
				FinishAttack(EGP_AttackTerminalResult::Failed, FailReason);
				return true;
			}

			AttackTarget = Target;
			float Dist = -1.0f;
			if (!TryComputeAttackDistance2D(Owner, Target, Dist))
			{
				FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
				return true;
			}

			if (Dist <= AttackRange)
			{
				EnterAttackReady();
			}
			else
			{
				RequestOrRefreshAttackApproach(true);
			}
			return true;
		}

		LogApproachResult(TEXT("ManualCancel"));
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::MovementCancelled);
		return true;
	}

	if (Result == EGP_MovementResult::Cancelled && Reason == EGP_MovementResultReason::CommandReplaced)
	{
		LogApproachResult(TEXT("CommandReplaced"));
		FinishAttack(EGP_AttackTerminalResult::Cancelled, EGP_AttackTerminalReason::CommandReplaced);
		return true;
	}

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackApproachResultIgnored: Unit=%s AttackSerial=%u ResultSerial=%u IgnoreReason=UnsupportedCombination MovementResult=%s MovementReason=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveAttackSerial,
		Serial,
		GPUnitCommandStatePrivate::MovementResultToString(Result),
		GPUnitCommandStatePrivate::MovementResultReasonToString(Reason),
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
	return true;
}

void UGP_UnitCommandComponent::HandleMovementResult(
	uint32 Serial,
	EGP_MovementResult Result,
	EGP_MovementResultReason Reason)
{
	if (TryConsumeAttackMovementResult(Serial, Result, Reason))
	{
		return;
	}

	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	auto LogIgnored = [&](const TCHAR* IgnoreReason, uint32 HeldSerial, const FString& HeldTag, bool bWarning)
	{
		if (bWarning)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution MovementResultIgnored: Unit=%s ResultSerial=%u HeldSerial=%u HeldTag=%s Result=%s ResultReason=%s IgnoreReason=%s Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				Serial,
				HeldSerial,
				*HeldTag,
				GPUnitCommandStatePrivate::MovementResultToString(Result),
				GPUnitCommandStatePrivate::MovementResultReasonToString(Reason),
				IgnoreReason,
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
		}
		else
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution MovementResultIgnored: Unit=%s ResultSerial=%u HeldSerial=%u HeldTag=%s Result=%s ResultReason=%s IgnoreReason=%s Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				Serial,
				HeldSerial,
				*HeldTag,
				GPUnitCommandStatePrivate::MovementResultToString(Result),
				GPUnitCommandStatePrivate::MovementResultReasonToString(Reason),
				IgnoreReason,
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
		}
	};

	if (Owner == nullptr || !Owner->HasAuthority())
	{
		LogIgnored(TEXT("NoAuthority"), 0, FString(TEXT("none")), true);
		return;
	}

	if (Result != EGP_MovementResult::Reached && Result != EGP_MovementResult::Cancelled)
	{
		const uint32 HeldSerial = HeldCommand.IsSet() ? HeldCommand.GetValue().CommandSerial : 0;
		const FString HeldTag = HeldCommand.IsSet()
			? HeldCommand.GetValue().CommandTag.ToString()
			: FString(TEXT("none"));
		LogIgnored(TEXT("UnsupportedResult"), HeldSerial, HeldTag, true);
		return;
	}

	if (!HeldCommand.IsSet())
	{
		LogIgnored(TEXT("NoHeldCommand"), 0, FString(TEXT("none")), false);
		return;
	}

	const FGP_StoredUnitCommand& CurrentHeld = HeldCommand.GetValue();
	const FGameplayTag MoveTag = FGPGameplayTags::Get().Command_Move;
	if (!(CurrentHeld.CommandTag == MoveTag))
	{
		LogIgnored(TEXT("HeldTagNotMove"), CurrentHeld.CommandSerial, CurrentHeld.CommandTag.ToString(), false);
		return;
	}

	if (CurrentHeld.CommandSerial != Serial)
	{
		LogIgnored(TEXT("SerialMismatch"), CurrentHeld.CommandSerial, CurrentHeld.CommandTag.ToString(), false);
		return;
	}

	const uint32 ClearedSerial = CurrentHeld.CommandSerial;
	const FGameplayTag ClearedTag = CurrentHeld.CommandTag;
	HeldCommand.Reset();

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution HeldMoveFinished: Unit=%s Serial=%u Tag=%s Result=%s Reason=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ClearedSerial,
		*ClearedTag.ToString(),
		GPUnitCommandStatePrivate::MovementResultToString(Result),
		GPUnitCommandStatePrivate::MovementResultReasonToString(Reason),
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));
}

void UGP_UnitCommandComponent::HandleCommand(const FGP_UnitCommand& Command)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	if (Owner == nullptr || !Owner->HasAuthority())
	{
		UE_LOG(LogGPUnitCommandState, Log,
			TEXT("GP UnitCommandState RejectedAuthority: Unit=%s Tag=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			*Command.CommandTag.ToString(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return;
	}

	if (Command.bQueue)
	{
		const TCHAR* HeldSerialText = TEXT("none");
		FString HeldSerialStorage;
		if (HeldCommand.IsSet())
		{
			HeldSerialStorage = FString::Printf(TEXT("%u"), HeldCommand.GetValue().CommandSerial);
			HeldSerialText = *HeldSerialStorage;
		}

		UE_LOG(LogGPUnitCommandState, Log,
			TEXT("GP UnitCommandState QueueDeferred: Unit=%s Tag=%s HeldSerial=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			*Command.CommandTag.ToString(),
			HeldSerialText,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return;
	}

	const TOptional<FGP_StoredUnitCommand> PreviousCommand = HeldCommand;
	const bool bHadHeldCommand = PreviousCommand.IsSet();

	FGP_StoredUnitCommand Stored;
	Stored.CommandTag = Command.CommandTag;
	Stored.TargetLocation = Command.TargetLocation;
	Stored.TargetActor = Command.TargetActor;
	Stored.bQueue = Command.bQueue;
	Stored.CommandSerial = AllocateCommandSerial();

	HeldCommand = Stored;

	ResetAttackExecutorForReplacement(PreviousCommand);

	const bool bHeldRemainsAfterSync = SynchronizeMovementWithHeldCommand(PreviousCommand);
	if (!bHeldRemainsAfterSync || !HeldCommand.IsSet())
	{
		return;
	}

	const FGameplayTag AttackTag = FGPGameplayTags::Get().Command_Attack;
	if (HeldCommand.GetValue().CommandTag == AttackTag)
	{
		if (!StartAttackExecutor() || !HeldCommand.IsSet())
		{
			return;
		}
	}

	if (bHadHeldCommand)
	{
		UE_LOG(LogGPUnitCommandState, Log,
			TEXT("GP UnitCommandState HeldReplaced: Unit=%s PreviousSerial=%u NewSerial=%u PreviousTag=%s NewTag=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			PreviousCommand.GetValue().CommandSerial,
			HeldCommand.GetValue().CommandSerial,
			*PreviousCommand.GetValue().CommandTag.ToString(),
			*HeldCommand.GetValue().CommandTag.ToString(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
	}
	else
	{
		UE_LOG(LogGPUnitCommandState, Log,
			TEXT("GP UnitCommandState HeldAccepted: Unit=%s Serial=%u Tag=%s TargetActor=%s Loc=%s Queue=false Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HeldCommand.GetValue().CommandSerial,
			*HeldCommand.GetValue().CommandTag.ToString(),
			*GetNameSafe(HeldCommand.GetValue().TargetActor.Get()),
			*HeldCommand.GetValue().TargetLocation.ToCompactString(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
	}
}

bool UGP_UnitCommandComponent::SynchronizeMovementWithHeldCommand(
	const TOptional<FGP_StoredUnitCommand>& PreviousCommand)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return HeldCommand.IsSet();
	}

	if (!HeldCommand.IsSet())
	{
		return false;
	}

	const FGP_StoredUnitCommand& CurrentHeld = HeldCommand.GetValue();
	const FGameplayTag MoveTag = FGPGameplayTags::Get().Command_Move;
	const bool bCurrentIsMove = CurrentHeld.CommandTag == MoveTag;

	const uint32 PreviousSerial = PreviousCommand.IsSet() ? PreviousCommand.GetValue().CommandSerial : 0;
	const FString PreviousTagString = PreviousCommand.IsSet()
		? PreviousCommand.GetValue().CommandTag.ToString()
		: FString(TEXT("none"));

	AGP_MobileUnit* MobileUnit = Cast<AGP_MobileUnit>(Owner);
	if (MobileUnit == nullptr)
	{
		if (bCurrentIsMove)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution MovementUnavailable: Unit=%s Serial=%u Destination=%s Reason=NonMobileOwner Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				CurrentHeld.CommandSerial,
				*CurrentHeld.TargetLocation.ToCompactString(),
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
		}
		return HeldCommand.IsSet();
	}

	UGP_MovementComponent* Movement = MobileUnit->GetUnitMovementComponent();
	if (Movement == nullptr)
	{
		if (bCurrentIsMove)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution MovementUnavailable: Unit=%s Serial=%u Destination=%s Reason=MissingComponent Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				CurrentHeld.CommandSerial,
				*CurrentHeld.TargetLocation.ToCompactString(),
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
		}
		return HeldCommand.IsSet();
	}

	if (bCurrentIsMove)
	{
		const uint32 RequestedSerial = CurrentHeld.CommandSerial;
		const FGameplayTag RequestedTag = CurrentHeld.CommandTag;
		const FVector RequestedDestination = CurrentHeld.TargetLocation;

		const FGP_MovementRequestOutcome Outcome =
			Movement->RequestMove(RequestedDestination, RequestedSerial);

		if (Outcome.IsAccepted())
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution MoveExecutionRequested: Unit=%s Serial=%u Destination=%s PreviousSerial=%u PreviousTag=%s Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				RequestedSerial,
				*RequestedDestination.ToCompactString(),
				PreviousSerial,
				*PreviousTagString,
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
			return true;
		}

		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MoveExecutionRejected: Unit=%s Serial=%u Destination=%s RejectReason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			RequestedSerial,
			*RequestedDestination.ToCompactString(),
			GPUnitCommandStatePrivate::RejectReasonToString(Outcome.RejectReason),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));

		if (HeldCommand.IsSet())
		{
			const FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
			if (Held.CommandTag == MoveTag && Held.CommandSerial == RequestedSerial)
			{
				HeldCommand.Reset();

				UE_LOG(LogGPUnitCommandState, Log,
					TEXT("GP UnitCommandState HeldMoveRejectedCleared: Unit=%s Serial=%u Tag=%s RejectReason=%s Destination=%s Role=%s NetMode=%s"),
					*GetNameSafe(Owner),
					RequestedSerial,
					*RequestedTag.ToString(),
					GPUnitCommandStatePrivate::RejectReasonToString(Outcome.RejectReason),
					*RequestedDestination.ToCompactString(),
					GPUnitCommandStatePrivate::RoleToString(Role),
					GPUnitCommandStatePrivate::NetModeToString(NetMode));
			}
		}

		return false;
	}

	if (Movement->IsMoving())
	{
		const uint32 PreviousMoveSerial = Movement->GetActiveMoveSerial();
		Movement->StopMove(EGP_MovementStopReason::CommandReplaced);

		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution MovementCancelledByCommand: Unit=%s PreviousMoveSerial=%u NewCommandSerial=%u NewCommandTag=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			PreviousMoveSerial,
			CurrentHeld.CommandSerial,
			*CurrentHeld.CommandTag.ToString(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
	}

	return HeldCommand.IsSet();
}

bool UGP_UnitCommandComponent::HasHeldCommand() const
{
	return HeldCommand.IsSet();
}

const FGP_StoredUnitCommand* UGP_UnitCommandComponent::GetHeldCommand() const
{
	return HeldCommand.IsSet() ? &HeldCommand.GetValue() : nullptr;
}

EGP_AttackExecutionState UGP_UnitCommandComponent::GetAttackExecutionState() const
{
	return AttackState;
}

uint32 UGP_UnitCommandComponent::GetActiveAttackSerial() const
{
	return ActiveAttackSerial;
}

AGP_UnitBase* UGP_UnitCommandComponent::GetAttackTarget() const
{
	return AttackTarget.Get();
}

float UGP_UnitCommandComponent::GetAttackRange() const
{
	return AttackRange;
}

bool UGP_UnitCommandComponent::IsAttackActive() const
{
	return AttackState != EGP_AttackExecutionState::Idle && ActiveAttackSerial != 0;
}

void UGP_UnitCommandComponent::ClearHeldCommand()
{
	HeldCommand.Reset();
}

void UGP_UnitCommandComponent::NotifyOwnerDied()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner->GetLocalRole();
	const uint32 ClearedSerial = HeldCommand.IsSet() ? HeldCommand.GetValue().CommandSerial : 0;
	const FString ClearedTag = HeldCommand.IsSet()
		? HeldCommand.GetValue().CommandTag.ToString()
		: FString(TEXT("none"));
	const EGP_AttackExecutionState PreviousAttackState = AttackState;
	const uint32 PreviousAttackSerial = ActiveAttackSerial;

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitDeathCommandShutdown: Unit=%s HeldSerial=%u HeldTag=%s AttackSerial=%u AttackState=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ClearedSerial,
		*ClearedTag,
		PreviousAttackSerial,
		AttackStateToString(PreviousAttackState),
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	SetAttackTickEnabled(false);
	ResetAttackExecutor();

	if (UGP_MovementComponent* Movement = ResolveMovementComponent())
	{
		Movement->StopMove(EGP_MovementStopReason::OwnerDied);
	}

	if (HeldCommand.IsSet())
	{
		UE_LOG(LogGPUnitCommandState, Log,
			TEXT("GP UnitCommandState HeldCleared: Unit=%s Serial=%u Tag=%s Reason=OwnerDied Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HeldCommand.GetValue().CommandSerial,
			*HeldCommand.GetValue().CommandTag.ToString(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		ClearHeldCommand();
	}
}

uint32 UGP_UnitCommandComponent::AllocateCommandSerial()
{
	const uint32 Allocated = NextCommandSerial;
	++NextCommandSerial;
	if (NextCommandSerial == 0)
	{
		NextCommandSerial = 1;
	}
	return Allocated;
}

#if !UE_BUILD_SHIPPING
namespace GPUnitCommandConsolePrivate
{
	static const TCHAR* AttackStateLabel(EGP_AttackExecutionState State)
	{
		switch (State)
		{
		case EGP_AttackExecutionState::Idle:
			return TEXT("Idle");
		case EGP_AttackExecutionState::Approaching:
			return TEXT("Approaching");
		case EGP_AttackExecutionState::Ready:
			return TEXT("Ready");
		default:
			return TEXT("Unknown");
		}
	}

	static AGP_MobileUnit* FindFirstAuthorityMobileUnit(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (TActorIterator<AGP_MobileUnit> It(World); It; ++It)
		{
			AGP_MobileUnit* MobileUnit = *It;
			if (MobileUnit != nullptr && MobileUnit->HasAuthority())
			{
				return MobileUnit;
			}
		}

		return nullptr;
	}

	static AGP_MobileUnit* FindFirstAuthorityAttackingMobileUnit(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (TActorIterator<AGP_MobileUnit> It(World); It; ++It)
		{
			AGP_MobileUnit* MobileUnit = *It;
			if (MobileUnit == nullptr || !MobileUnit->HasAuthority())
			{
				continue;
			}

			UGP_UnitCommandComponent* Command = MobileUnit->GetUnitCommandComponent();
			if (Command != nullptr && Command->IsAttackActive())
			{
				return MobileUnit;
			}
		}

		return nullptr;
	}

	static void UnitCommandTestRejectedMove(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.UnitCommand.TestRejectedMove missing world"));
			return;
		}

		AGP_MobileUnit* MobileUnit = FindFirstAuthorityMobileUnit(World);
		if (MobileUnit == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: no authority AGP_MobileUnit found"));
			return;
		}

		UGP_UnitCommandComponent* CommandComponent = MobileUnit->GetUnitCommandComponent();
		if (CommandComponent == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: Unit=%s missing UnitCommandComponent"),
				*MobileUnit->GetName());
			return;
		}

		const float InvalidCoord = std::numeric_limits<float>::quiet_NaN();

		FGP_UnitCommand Command;
		Command.CommandTag = FGPGameplayTags::Get().Command_Move;
		Command.TargetLocation = FVector(InvalidCoord, InvalidCoord, InvalidCoord);
		Command.TargetActor = nullptr;
		Command.bQueue = false;

		const bool bHadHeldBefore = CommandComponent->HasHeldCommand();
		CommandComponent->HandleCommand(Command);
		const bool bHasHeldAfter = CommandComponent->HasHeldCommand();

		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution Console: gp.UnitCommand.TestRejectedMove Unit=%s HadHeldBefore=%s HasHeldAfter=%s (exercises Held-before-RequestMove InvalidDestination reject)"),
			*MobileUnit->GetName(),
			bHadHeldBefore ? TEXT("true") : TEXT("false"),
			bHasHeldAfter ? TEXT("true") : TEXT("false"));
	}

	static void AttackInspect(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.Inspect missing world"));
			return;
		}

		const TCHAR* Selection = TEXT("ActiveAttack");
		AGP_MobileUnit* MobileUnit = FindFirstAuthorityAttackingMobileUnit(World);
		if (MobileUnit == nullptr)
		{
			Selection = TEXT("FallbackFirstAuthority");
			MobileUnit = FindFirstAuthorityMobileUnit(World);
		}

		if (MobileUnit == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.Inspect no authority AGP_MobileUnit found"));
			return;
		}

		UGP_UnitCommandComponent* Command = MobileUnit->GetUnitCommandComponent();
		UGP_MovementComponent* Movement = MobileUnit->GetUnitMovementComponent();
		if (Command == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.Inspect Unit=%s missing UnitCommandComponent"),
				*MobileUnit->GetName());
			return;
		}

		const FGP_StoredUnitCommand* Held = Command->GetHeldCommand();
		const uint32 HeldSerial = Held != nullptr ? Held->CommandSerial : 0;
		const FString HeldTag = Held != nullptr ? Held->CommandTag.ToString() : FString(TEXT("none"));
		AGP_UnitBase* Target = Command->GetAttackTarget();
		const float Distance = (Target != nullptr)
			? FVector::Dist2D(MobileUnit->GetActorLocation(), Target->GetActorLocation())
			: -1.0f;

		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution Console: gp.Attack.Inspect Unit=%s Selection=%s HeldSerial=%u HeldTag=%s AttackState=%s ActiveAttackSerial=%u Target=%s Distance=%.1f AttackRange=%.1f IsMoving=%s MovementSerial=%u Role=%s NetMode=%s"),
			*MobileUnit->GetName(),
			Selection,
			HeldSerial,
			*HeldTag,
			AttackStateLabel(Command->GetAttackExecutionState()),
			Command->GetActiveAttackSerial(),
			*GetNameSafe(Target),
			Distance,
			Command->GetAttackRange(),
			(Movement != nullptr && Movement->IsMoving()) ? TEXT("true") : TEXT("false"),
			Movement != nullptr ? Movement->GetActiveMoveSerial() : 0u,
			GPUnitCommandStatePrivate::RoleToString(MobileUnit->GetLocalRole()),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(MobileUnit)));
	}

	static void AttackDestroyTarget(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.DestroyTarget missing world"));
			return;
		}

		AGP_MobileUnit* MobileUnit = FindFirstAuthorityAttackingMobileUnit(World);
		if (MobileUnit == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.DestroyTarget no active Attack authority unit"));
			return;
		}

		UGP_UnitCommandComponent* Command = MobileUnit->GetUnitCommandComponent();
		AGP_UnitBase* Target = Command != nullptr ? Command->GetAttackTarget() : nullptr;
		if (Target == nullptr || !IsValid(Target))
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.DestroyTarget Unit=%s has no valid AttackTarget"),
				*MobileUnit->GetName());
			return;
		}

		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution Console: gp.Attack.DestroyTarget Attacker=%s Target=%s AttackSerial=%u"),
			*MobileUnit->GetName(),
			*Target->GetName(),
			Command->GetActiveAttackSerial());

		Target->Destroy();
	}

	static void AttackMoveTarget(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.MoveTarget missing world"));
			return;
		}

		if (Args.Num() < 2)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: usage gp.Attack.MoveTarget X Y"));
			return;
		}

		AGP_MobileUnit* MobileUnit = FindFirstAuthorityAttackingMobileUnit(World);
		if (MobileUnit == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.MoveTarget no active Attack authority unit"));
			return;
		}

		UGP_UnitCommandComponent* Command = MobileUnit->GetUnitCommandComponent();
		AGP_UnitBase* Target = Command != nullptr ? Command->GetAttackTarget() : nullptr;
		if (Target == nullptr || !IsValid(Target))
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.MoveTarget no valid AttackTarget"));
			return;
		}

		const float X = FCString::Atof(*Args[0]);
		const float Y = FCString::Atof(*Args[1]);
		const FVector Current = Target->GetActorLocation();
		const FVector NewLocation(X, Y, Current.Z);
		Target->SetActorLocation(NewLocation, false);

		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution Console: gp.Attack.MoveTarget Attacker=%s Target=%s NewLocation=%s (non-shipping Ready/Approaching validation)"),
			*MobileUnit->GetName(),
			*Target->GetName(),
			*NewLocation.ToCompactString());
	}

	static void AttackTestInvalid(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.TestInvalid missing world"));
			return;
		}

		if (Args.Num() < 1)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: usage gp.Attack.TestInvalid <Self|Friendly|Null>"));
			return;
		}

		AGP_MobileUnit* MobileUnit = FindFirstAuthorityMobileUnit(World);
		if (MobileUnit == nullptr)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: gp.Attack.TestInvalid no authority AGP_MobileUnit"));
			return;
		}

		UGP_UnitCommandComponent* Command = MobileUnit->GetUnitCommandComponent();
		if (Command == nullptr)
		{
			return;
		}

		FGP_UnitCommand UnitCommand;
		UnitCommand.CommandTag = FGPGameplayTags::Get().Command_Attack;
		UnitCommand.bQueue = false;
		UnitCommand.TargetLocation = MobileUnit->GetActorLocation();

		const FString Mode = Args[0];
		if (Mode.Equals(TEXT("Self"), ESearchCase::IgnoreCase))
		{
			UnitCommand.TargetActor = MobileUnit;
		}
		else if (Mode.Equals(TEXT("Null"), ESearchCase::IgnoreCase))
		{
			UnitCommand.TargetActor = nullptr;
		}
		else if (Mode.Equals(TEXT("Friendly"), ESearchCase::IgnoreCase))
		{
			AGP_UnitBase* Friendly = nullptr;
			for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
			{
				AGP_UnitBase* Candidate = *It;
				if (Candidate != nullptr
					&& Candidate->HasAuthority()
					&& Candidate != MobileUnit
					&& Candidate->GetTeamId() == MobileUnit->GetTeamId()
					&& Candidate->GetTeamId() >= 1)
				{
					Friendly = Candidate;
					break;
				}
			}

			if (Friendly == nullptr)
			{
				UE_LOG(LogGPUnitCommandExecution, Warning,
					TEXT("GP UnitCommandExecution Console: gp.Attack.TestInvalid Friendly — no same-team unit found"));
				return;
			}
			UnitCommand.TargetActor = Friendly;
		}
		else
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution Console: usage gp.Attack.TestInvalid <Self|Friendly|Null>"));
			return;
		}

		const bool bHadHeldBefore = Command->HasHeldCommand();
		Command->HandleCommand(UnitCommand);
		const bool bHasHeldAfter = Command->HasHeldCommand();
		const bool bAttackActive = Command->IsAttackActive();

		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution Console: gp.Attack.TestInvalid Mode=%s Unit=%s HadHeldBefore=%s HasHeldAfter=%s AttackActive=%s"),
			*Mode,
			*MobileUnit->GetName(),
			bHadHeldBefore ? TEXT("true") : TEXT("false"),
			bHasHeldAfter ? TEXT("true") : TEXT("false"),
			bAttackActive ? TEXT("true") : TEXT("false"));
	}

	static FAutoConsoleCommandWithWorldAndArgs GUnitCommandTestRejectedMoveCommand(
		TEXT("gp.UnitCommand.TestRejectedMove"),
		TEXT("GP-S23 non-shipping: HandleCommand Move with non-finite destination to validate phantom-Held clear."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UnitCommandTestRejectedMove));

	static FAutoConsoleCommandWithWorldAndArgs GAttackInspectCommand(
		TEXT("gp.Attack.Inspect"),
		TEXT("GP-S24 non-shipping: dump Attack executor state (active Attack preferred)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AttackInspect));

	static FAutoConsoleCommandWithWorldAndArgs GAttackDestroyTargetCommand(
		TEXT("gp.Attack.DestroyTarget"),
		TEXT("GP-S24 non-shipping: Destroy() current Attack target; Tick must detect TargetDestroyed."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AttackDestroyTarget));

	static FAutoConsoleCommandWithWorldAndArgs GAttackMoveTargetCommand(
		TEXT("gp.Attack.MoveTarget"),
		TEXT("GP-S24 non-shipping: teleport Attack target to X Y (Z preserved) for Ready↔Approaching tests."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AttackMoveTarget));

	static FAutoConsoleCommandWithWorldAndArgs GAttackTestInvalidCommand(
		TEXT("gp.Attack.TestInvalid"),
		TEXT("GP-S24 non-shipping: HandleCommand Attack with Self|Friendly|Null to validate accept-time reject."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AttackTestInvalid));
}
#endif
