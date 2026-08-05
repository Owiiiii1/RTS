// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitCommandComponent.h"

#include "AttributeSets/GPUnitAttributeSet.h"
#include "Combat/GPCombatPresentationComponent.h"
#include "Combat/GPDamageApplication.h"
#include "Command/GPUnitCommand.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceNode.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPUnitBase.h"
#include "Units/GPWorker.h"

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
	case EGP_AttackTerminalReason::TargetDied:
		return TEXT("TargetDied");
	case EGP_AttackTerminalReason::MovementRejected:
		return TEXT("MovementRejected");
	case EGP_AttackTerminalReason::MovementCancelled:
		return TEXT("MovementCancelled");
	case EGP_AttackTerminalReason::RangeUnreachable:
		return TEXT("RangeUnreachable");
	case EGP_AttackTerminalReason::EndPlay:
		return TEXT("EndPlay");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* UGP_UnitCommandComponent::AttackRangeSourceToString(EGP_AttackRangeSource Source)
{
	switch (Source)
	{
	case EGP_AttackRangeSource::GAS:
		return TEXT("GAS");
	case EGP_AttackRangeSource::FallbackComponent:
		return TEXT("FallbackComponent");
	case EGP_AttackRangeSource::Invalid:
		return TEXT("Invalid");
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
	ResetMineExecutor();

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
	float EffectiveRange = 0.0f;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::Invalid;
	if (!TryResolveEffectiveAttackRange(EffectiveRange, RangeSource))
	{
		return false;
	}

	return FMath::IsFinite(AttackReissueDistance) && AttackReissueDistance >= 0.0f
		&& FMath::IsFinite(AttackReissueInterval) && AttackReissueInterval >= 0.0f;
}

bool UGP_UnitCommandComponent::TryResolveEffectiveAttackRange(
	float& OutRange,
	EGP_AttackRangeSource& OutSource) const
{
	OutRange = 0.0f;
	OutSource = EGP_AttackRangeSource::Invalid;

	if (const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(GetOwner()))
	{
		if (const UGP_UnitAttributeSet* Attrs = OwnerUnit->GetUnitAttributeSet())
		{
			const float GasRange = Attrs->GetAttackRange();
			if (FMath::IsFinite(GasRange) && GasRange > 0.0f)
			{
				OutRange = GasRange;
				OutSource = EGP_AttackRangeSource::GAS;
				return true;
			}
		}
	}

	if (FMath::IsFinite(AttackRange) && AttackRange > 0.0f)
	{
		OutRange = AttackRange;
		OutSource = EGP_AttackRangeSource::FallbackComponent;
		return true;
	}

	return false;
}

float UGP_UnitCommandComponent::ResolveSanitizedAttackCooldown(bool bLogSanitize) const
{
	float RawCooldown = -1.0f;
	if (const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(GetOwner()))
	{
		if (const UGP_UnitAttributeSet* Attrs = OwnerUnit->GetUnitAttributeSet())
		{
			RawCooldown = Attrs->GetAttackCooldown();
		}
	}

	if (FMath::IsFinite(RawCooldown) && RawCooldown > 0.0f)
	{
		return RawCooldown;
	}

	constexpr float MinCooldown = 0.05f;
	if (bLogSanitize)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP AttackCooldownSanitized: Unit=%s RawCooldown=%.4f UsedCooldown=%.2f Serial=%u"),
			*GetNameSafe(GetOwner()),
			RawCooldown,
			MinCooldown,
			ActiveAttackSerial);
	}

	return MinCooldown;
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

	if (TargetUnit->IsDead())
	{
		OutReason = EGP_AttackTerminalReason::TargetDied;
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

void UGP_UnitCommandComponent::ClearAttackCadenceState()
{
	NextAttackHitTime = -1.0;
	bHasAttemptedFirstHit = false;
	bAttackHitInProgress = false;
}

void UGP_UnitCommandComponent::ClearApproachProgressState()
{
	bHasReachedOutOfRangeSample = false;
	LastReachedOutOfRangeDistance = -1.0f;
	LastReachedOutOfRangeLocation = FVector::ZeroVector;
	ConsecutiveNoProgressApproachCount = 0;
}

bool UGP_UnitCommandComponent::HandleReachedStillOutOfRange(
	AActor* Owner,
	AGP_UnitBase* Target,
	float Distance,
	float EffectiveRange,
	EGP_AttackRangeSource RangeSource)
{
	if (Owner == nullptr || Target == nullptr)
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
		return true;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector Destination = MakeApproachDestination(Owner, Target);

	constexpr float LocationEpsilon = 5.0f;
	constexpr float DistanceImproveEpsilon = 5.0f;
	constexpr float DestinationEpsilon = 5.0f;
	constexpr int32 MaxNoProgressResults = 2;

	bool bNoProgress = false;
	if (bHasReachedOutOfRangeSample)
	{
		const float LocationDelta = FVector::Dist2D(OwnerLocation, LastReachedOutOfRangeLocation);
		const float DistanceImproved = LastReachedOutOfRangeDistance - Distance;
		const float DestinationDelta = FVector::Dist2D(Destination, LastApproachDestination);

		if (LocationDelta < LocationEpsilon
			&& DistanceImproved < DistanceImproveEpsilon
			&& DestinationDelta < DestinationEpsilon)
		{
			bNoProgress = true;
		}
	}

	if (bNoProgress)
	{
		++ConsecutiveNoProgressApproachCount;
	}
	else
	{
		ConsecutiveNoProgressApproachCount = 0;
	}

	LastReachedOutOfRangeDistance = Distance;
	LastReachedOutOfRangeLocation = OwnerLocation;
	bHasReachedOutOfRangeSample = true;

	if (ConsecutiveNoProgressApproachCount >= MaxNoProgressResults)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP AttackApproachUnreachable: Unit=%s Target=%s Serial=%u Distance=%.1f AttackRange=%.1f RangeSource=%s OwnerLocation=%s Destination=%s NoProgressCount=%d"),
			*GetNameSafe(Owner),
			*GetNameSafe(Target),
			ActiveAttackSerial,
			Distance,
			EffectiveRange,
			AttackRangeSourceToString(RangeSource),
			*OwnerLocation.ToCompactString(),
			*Destination.ToCompactString(),
			ConsecutiveNoProgressApproachCount);

		FinishAttack(
			EGP_AttackTerminalResult::Failed,
			EGP_AttackTerminalReason::RangeUnreachable);
		return true;
	}

	RequestOrRefreshAttackApproach(true);
	return false;
}

void UGP_UnitCommandComponent::UnbindAttackTargetDeath()
{
	if (TargetDiedHandle.IsValid())
	{
		if (AGP_UnitBase* BoundTarget = BoundDeathTarget.Get())
		{
			BoundTarget->OnUnitDied().Remove(TargetDiedHandle);
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP AttackTargetDeathUnbound: Unit=%s Target=%s AttackSerial=%u"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(BoundTarget),
				ActiveAttackSerial);
		}
		TargetDiedHandle.Reset();
	}

	BoundDeathTarget.Reset();
}

void UGP_UnitCommandComponent::BindAttackTargetDeath(AGP_UnitBase* Target)
{
	UnbindAttackTargetDeath();

	if (Target == nullptr)
	{
		return;
	}

	BoundDeathTarget = Target;
	TargetDiedHandle = Target->OnUnitDied().AddUObject(
		this,
		&UGP_UnitCommandComponent::HandleAttackTargetDied);

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP AttackTargetDeathBound: Unit=%s Target=%s AttackSerial=%u"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Target),
		ActiveAttackSerial);
}

void UGP_UnitCommandComponent::HandleAttackTargetDied(AGP_UnitBase* DeadUnit)
{
	if (bFinishingAttack)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (OwnerUnit != nullptr && OwnerUnit->IsDead())
	{
		return;
	}

	if (DeadUnit == nullptr
		|| DeadUnit != AttackTarget.Get()
		|| DeadUnit != BoundDeathTarget.Get()
		|| ActiveAttackSerial == 0
		|| !HasExactActiveHeldAttack())
	{
		return;
	}

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP AttackTargetDiedCallback: Unit=%s Target=%s AttackSerial=%u State=%s"),
		*GetNameSafe(Owner),
		*GetNameSafe(DeadUnit),
		ActiveAttackSerial,
		AttackStateToString(AttackState));

	FinishAttack(
		EGP_AttackTerminalResult::Failed,
		EGP_AttackTerminalReason::TargetDied);
}

void UGP_UnitCommandComponent::ResetAttackExecutor()
{
	UnbindAttackTargetDeath();
	ClearAttackCadenceState();
	ClearApproachProgressState();

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

const TCHAR* UGP_UnitCommandComponent::MineStateToString(EGP_MineExecutionState State)
{
	switch (State)
	{
	case EGP_MineExecutionState::Idle: return TEXT("Idle");
	case EGP_MineExecutionState::Approaching: return TEXT("Approaching");
	case EGP_MineExecutionState::Active: return TEXT("Active");
	default: return TEXT("Unknown");
	}
}

EGP_MineExecutionState UGP_UnitCommandComponent::GetMineExecutionState() const
{
	return MineState;
}

uint32 UGP_UnitCommandComponent::GetActiveMineSerial() const
{
	return ActiveMineSerial;
}

AGP_ResourceNode* UGP_UnitCommandComponent::GetMineTarget() const
{
	return MineTarget.Get();
}

bool UGP_UnitCommandComponent::IsMineApproachActive() const
{
	return MineState == EGP_MineExecutionState::Approaching && ActiveMineSerial != 0;
}

FVector UGP_UnitCommandComponent::GetMineApproachDestination() const
{
	return MineApproachDestination;
}

float UGP_UnitCommandComponent::GetMineApproachDesiredNodeDistance() const
{
	return MineApproachDesiredNodeDistance;
}

float UGP_UnitCommandComponent::GetMineApproachSafetyMarginCm() const
{
	return WorkerMineApproachSafetyMarginCm;
}

int32 UGP_UnitCommandComponent::GetMineApproachAttempt() const
{
	return MineApproachAttempt;
}

float UGP_UnitCommandComponent::GetMinePredictedWorstCaseDistance() const
{
	return MinePredictedWorstCaseDistance;
}

float UGP_UnitCommandComponent::GetMineLastArrivalDistance() const
{
	return MineLastArrivalDistance;
}

float UGP_UnitCommandComponent::GetMineLastArrivalRangeError() const
{
	return MineLastArrivalRangeError;
}

#if !UE_BUILD_SHIPPING
void UGP_UnitCommandComponent::DebugForceNextMineArrivalOutOfRangeOnce()
{
	bDebugForceNextMineArrivalOutOfRange = true;
}
#endif

bool UGP_UnitCommandComponent::TryMakeMineApproachDestination(
	const AActor* Owner,
	const AGP_ResourceNode* Node,
	float InteractionRangeCm,
	float AcceptanceRadius,
	float ExtraInwardMarginCm,
	FVector& OutDestination,
	float& OutDesiredHorizontalDistance,
	float& OutPredictedWorstCaseDistance) const
{
	OutDestination = FVector::ZeroVector;
	OutDesiredHorizontalDistance = -1.0f;
	OutPredictedWorstCaseDistance = -1.0f;

	if (Owner == nullptr || Node == nullptr
		|| !FMath::IsFinite(InteractionRangeCm) || InteractionRangeCm <= 0.0f
		|| !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 0.0f)
	{
		return false;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector NodeLocation = Node->GetActorLocation();
	const float DeltaZ = OwnerLocation.Z - NodeLocation.Z;
	const float AbsDeltaZ = FMath::Abs(DeltaZ);

	// Movement completes in a 2D acceptance circle; mining validates 3D distance.
	// Require: sqrt((D_h + Acc)^2 + DeltaZ^2) < Range
	const float RangeSq = FMath::Square(InteractionRangeCm);
	const float DeltaZSq = FMath::Square(AbsDeltaZ);
	if (DeltaZSq >= RangeSq)
	{
		return false;
	}

	const float MaxHorizontalBudget = FMath::Sqrt(RangeSq - DeltaZSq);
	const float TotalInward = AcceptanceRadius + WorkerMineApproachSafetyMarginCm + FMath::Max(0.0f, ExtraInwardMarginCm);
	if (MaxHorizontalBudget <= TotalInward + 1.0f)
	{
		return false;
	}

	const float DesiredHorizontal = MaxHorizontalBudget - TotalInward;
	FVector Away = OwnerLocation - NodeLocation;
	Away.Z = 0.0f;
	if (!Away.Normalize())
	{
		Away = FVector::ForwardVector;
	}

	const FVector DestinationXY = NodeLocation + Away * DesiredHorizontal;
	OutDestination = FVector(DestinationXY.X, DestinationXY.Y, OwnerLocation.Z);
	OutDesiredHorizontalDistance = DesiredHorizontal;
	OutPredictedWorstCaseDistance = FMath::Sqrt(FMath::Square(DesiredHorizontal + AcceptanceRadius) + DeltaZSq);
	return OutPredictedWorstCaseDistance < InteractionRangeCm;
}

bool UGP_UnitCommandComponent::RequestMineApproachMove(
	AActor* Owner,
	AGP_ResourceNode* Node,
	uint32 MineSerial,
	float ExtraInwardMarginCm,
	const TCHAR* LogLabel)
{
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;
	UGP_MovementComponent* Movement = ResolveMovementComponent();
	AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	UGP_MiningComponent* Mining = Worker != nullptr ? Worker->GetMiningComponent() : nullptr;
	if (Movement == nullptr || !IsValid(Node) || !IsValid(Mining))
	{
		return false;
	}

	const float InteractionRange = ResolveMineInteractionRangeCm(Mining, Node);
	const float AcceptanceRadius = Movement->AcceptanceRadius;
	FVector Destination = FVector::ZeroVector;
	float DesiredHorizontal = -1.0f;
	float PredictedWorst = -1.0f;
	if (!TryMakeMineApproachDestination(
		Owner,
		Node,
		InteractionRange,
		AcceptanceRadius,
		ExtraInwardMarginCm,
		Destination,
		DesiredHorizontal,
		PredictedWorst))
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineApproachGeometryFailed: Unit=%s MineSerial=%u Target=%s Label=%s Range=%.1f Acc=%.1f Safety=%.1f ExtraInward=%.1f Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			MineSerial,
			*GetNameSafe(Node),
			LogLabel,
			InteractionRange,
			AcceptanceRadius,
			WorkerMineApproachSafetyMarginCm,
			ExtraInwardMarginCm,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return false;
	}

	const FGP_MovementRequestOutcome Outcome = Movement->RequestMove(Destination, MineSerial);
	if (!Outcome.IsAccepted())
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineApproachMoveRejected: Unit=%s MineSerial=%u Label=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			MineSerial,
			LogLabel,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return false;
	}

	MineApproachDestination = Destination;
	MineApproachDesiredNodeDistance = DesiredHorizontal;
	MinePredictedWorstCaseDistance = PredictedWorst;
	MineState = EGP_MineExecutionState::Approaching;
	ActiveMineSerial = MineSerial;
	MineTarget = Node;

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution MineApproachRequested: Unit=%s MineSerial=%u Target=%s Label=%s Attempt=%d Destination=%s DesiredHoriz=%.1f PredictedWorst=%.1f Range=%.1f Acc=%.1f Safety=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		MineSerial,
		*GetNameSafe(Node),
		LogLabel,
		MineApproachAttempt,
		*Destination.ToCompactString(),
		DesiredHorizontal,
		PredictedWorst,
		InteractionRange,
		AcceptanceRadius,
		WorkerMineApproachSafetyMarginCm,
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));
	return true;
}

void UGP_UnitCommandComponent::UnbindMiningStateEvents()
{
	if (BoundMiningComponent.IsValid())
	{
		BoundMiningComponent->OnMiningStateChanged.RemoveDynamic(
			this,
			&UGP_UnitCommandComponent::HandleMiningStateChanged);
	}
	BoundMiningComponent.Reset();
}

void UGP_UnitCommandComponent::BindMiningStateEvents(UGP_MiningComponent* Mining)
{
	UnbindMiningStateEvents();
	if (!IsValid(Mining))
	{
		return;
	}

	BoundMiningComponent = Mining;
	Mining->OnMiningStateChanged.AddDynamic(this, &UGP_UnitCommandComponent::HandleMiningStateChanged);
}

float UGP_UnitCommandComponent::ResolveMineInteractionRangeCm(
	const UGP_MiningComponent* Mining,
	const AGP_ResourceNode* Node) const
{
	if (IsValid(Mining) && Mining->GetInteractionRangeCm() > 0.0f)
	{
		return Mining->GetInteractionRangeCm();
	}

	if (IsValid(Node))
	{
		if (const UGP_ResourceDefinition* Definition = Node->ResolveResourceDefinition(true))
		{
			if (Definition->InteractionRangeCm > 0.0f)
			{
				return Definition->InteractionRangeCm;
			}
		}
	}

	return 200.0f;
}

void UGP_UnitCommandComponent::ResetMineExecutor()
{
	if (bFinishingMine)
	{
		return;
	}

	TGuardValue<bool> Guard(bFinishingMine, true);
	UnbindMiningStateEvents();

	if (AGP_Worker* Worker = Cast<AGP_Worker>(GetOwner()))
	{
		if (UGP_MiningComponent* Mining = Worker->GetMiningComponent())
		{
			if (IsValid(Mining)
				&& (Mining->IsMining() || Mining->IsWaitingForSlot()
					|| Mining->GetMiningState() == EGP_MiningState::Mining
					|| Mining->GetMiningState() == EGP_MiningState::WaitingForSlot))
			{
				Mining->StopMining(EGP_MiningStopReason::ManualStop);
			}
		}
	}

	MineState = EGP_MineExecutionState::Idle;
	ActiveMineSerial = 0;
	MineTarget.Reset();
	MineApproachDestination = FVector::ZeroVector;
	MineApproachDesiredNodeDistance = -1.0f;
	MinePredictedWorstCaseDistance = -1.0f;
	MineLastArrivalDistance = -1.0f;
	MineLastArrivalRangeError = -1.0f;
	MineApproachAttempt = 0;
#if !UE_BUILD_SHIPPING
	bDebugForceNextMineArrivalOutOfRange = false;
#endif
}

void UGP_UnitCommandComponent::ResetMineExecutorForReplacement(
	const TOptional<FGP_StoredUnitCommand>& PreviousCommand)
{
	if (MineState == EGP_MineExecutionState::Idle && ActiveMineSerial == 0)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution MineCancelled: Unit=%s MineSerial=%u Target=%s Reason=CommandReplaced PreviousState=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveMineSerial,
		*GetNameSafe(MineTarget.Get()),
		MineStateToString(MineState),
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));

	(void)PreviousCommand;
	ResetMineExecutor();
}

bool UGP_UnitCommandComponent::TryAcceptIdempotentMineCommand(const FGP_UnitCommand& Command) const
{
	const FGameplayTag MineTag = FGPGameplayTags::Get().Command_Mine;
	if (!(Command.CommandTag == MineTag))
	{
		return false;
	}

	AGP_ResourceNode* Node = Cast<AGP_ResourceNode>(Command.TargetActor);
	if (!IsValid(Node))
	{
		return false;
	}

	if (HeldCommand.IsSet()
		&& HeldCommand.GetValue().CommandTag == MineTag
		&& HeldCommand.GetValue().TargetActor.Get() == Node
		&& ActiveMineSerial != 0
		&& MineState != EGP_MineExecutionState::Idle)
	{
		return true;
	}

	const AGP_Worker* Worker = Cast<AGP_Worker>(GetOwner());
	if (Worker == nullptr)
	{
		return false;
	}

	const UGP_MiningComponent* Mining = Worker->GetMiningComponent();
	if (!IsValid(Mining))
	{
		return false;
	}

	return Mining->GetCurrentResourceNode() == Node
		&& (Mining->IsMining() || Mining->IsWaitingForSlot());
}

bool UGP_UnitCommandComponent::TryRejectMineCommandBeforeAccept(const FGP_UnitCommand& Command) const
{
	const FGameplayTag MineTag = FGPGameplayTags::Get().Command_Mine;
	if (!(Command.CommandTag == MineTag))
	{
		return false;
	}

	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	const AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	if (Worker == nullptr)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineRejected: Unit=%s Reason=UnsupportedUnit Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return true;
	}

	UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
	UGP_MiningComponent* Mining = Worker->GetMiningComponent();
	if (!IsValid(Cargo) || !IsValid(Mining))
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineRejected: Unit=%s Reason=MissingCargoOrMining Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return true;
	}

	if (Cargo->IsFull())
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineRejected: Unit=%s Reason=CargoFull Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return true;
	}

	AGP_ResourceNode* Node = Cast<AGP_ResourceNode>(Command.TargetActor);
	if (!IsValid(Node) || Node->IsActorBeingDestroyed())
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineRejected: Unit=%s Reason=InvalidTarget Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return true;
	}

	FString MineFail;
	if (!Node->CanAcceptMineCommand(true, &MineFail))
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineRejected: Unit=%s Target=%s Reason=InvalidOrDepleted Detail=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			*GetNameSafe(Node),
			*MineFail,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return true;
	}

	return false;
}

void UGP_UnitCommandComponent::BeginMiningAtHeldTarget(uint32 MineSerial)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;
	AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	if (Worker == nullptr || !HeldCommand.IsSet())
	{
		ResetMineExecutor();
		return;
	}

	const FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
	if (!(Held.CommandTag == FGPGameplayTags::Get().Command_Mine) || Held.CommandSerial != MineSerial)
	{
		return;
	}

	AGP_ResourceNode* Node = Cast<AGP_ResourceNode>(Held.TargetActor.Get());
	UGP_MiningComponent* Mining = Worker->GetMiningComponent();
	UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
	if (!IsValid(Node) || !IsValid(Mining) || !IsValid(Cargo) || Cargo->IsFull() || Node->IsDepleted())
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineArrivalRejected: Unit=%s MineSerial=%u Target=%s Reason=InvalidArrivalPrereq Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			MineSerial,
			*GetNameSafe(Node),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		ClearHeldCommand();
		ResetMineExecutor();
		return;
	}

	const float InteractionRange = ResolveMineInteractionRangeCm(Mining, Node);
	float Distance = FVector::Dist(Owner->GetActorLocation(), Node->GetActorLocation());
#if !UE_BUILD_SHIPPING
	if (bDebugForceNextMineArrivalOutOfRange)
	{
		bDebugForceNextMineArrivalOutOfRange = false;
		Distance = InteractionRange + 5.0f;
	}
#endif
	MineLastArrivalDistance = Distance;
	MineLastArrivalRangeError = Distance - InteractionRange;

	if (Distance > InteractionRange)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineArrivalOutOfRange: Unit=%s MineSerial=%u Target=%s Distance=%.1f Range=%.1f Attempt=%d Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			MineSerial,
			*GetNameSafe(Node),
			Distance,
			InteractionRange,
			MineApproachAttempt,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));

		// One-shot corrective approach (deeper inward); no slot/timer until success.
		if (MineApproachAttempt < 1)
		{
			++MineApproachAttempt;
			if (RequestMineApproachMove(Owner, Node, MineSerial, WorkerMineApproachSafetyMarginCm, TEXT("Corrective")))
			{
				return;
			}
		}

		ClearHeldCommand();
		ResetMineExecutor();
		return;
	}

	MineTarget = Node;
	MineState = EGP_MineExecutionState::Active;
	ActiveMineSerial = MineSerial;
	BindMiningStateEvents(Mining);

	const EGP_BeginMiningResult BeginResult = Mining->BeginMining(Node);
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution MineBegin: Unit=%s MineSerial=%u Target=%s Result=%d Distance=%.1f Range=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		MineSerial,
		*GetNameSafe(Node),
		static_cast<int32>(BeginResult),
		Distance,
		InteractionRange,
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	if (BeginResult != EGP_BeginMiningResult::Started
		&& BeginResult != EGP_BeginMiningResult::WaitingForSlot
		&& BeginResult != EGP_BeginMiningResult::AlreadyMiningTarget)
	{
		ClearHeldCommand();
		ResetMineExecutor();
	}
}

bool UGP_UnitCommandComponent::StartMineExecutor()
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	if (Owner == nullptr || !Owner->HasAuthority() || !HeldCommand.IsSet())
	{
		return HeldCommand.IsSet();
	}

	const FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
	const FGameplayTag MineTag = FGPGameplayTags::Get().Command_Mine;
	if (!(Held.CommandTag == MineTag))
	{
		return true;
	}

	AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	UGP_MiningComponent* Mining = Worker != nullptr ? Worker->GetMiningComponent() : nullptr;
	UGP_CargoComponent* Cargo = Worker != nullptr ? Worker->GetCargoComponent() : nullptr;
	AGP_ResourceNode* Node = Cast<AGP_ResourceNode>(Held.TargetActor.Get());
	if (Worker == nullptr || !IsValid(Mining) || !IsValid(Cargo) || !IsValid(Node))
	{
		ClearHeldCommand();
		ResetMineExecutor();
		return false;
	}

	const uint32 MineSerial = Held.CommandSerial;
	ActiveMineSerial = MineSerial;
	MineTarget = Node;
	MineApproachAttempt = 0;

	const float InteractionRange = ResolveMineInteractionRangeCm(Mining, Node);
	const float Distance = FVector::Dist(Owner->GetActorLocation(), Node->GetActorLocation());
	UGP_MovementComponent* Movement = ResolveMovementComponent();

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution MineAccepted: Unit=%s MineSerial=%u Target=%s Distance=%.1f Range=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		MineSerial,
		*GetNameSafe(Node),
		Distance,
		InteractionRange,
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	if (Distance <= InteractionRange)
	{
		if (Movement != nullptr && Movement->IsMoving())
		{
			Movement->StopMove(EGP_MovementStopReason::CommandReplaced);
		}
		BeginMiningAtHeldTarget(MineSerial);
		return HeldCommand.IsSet() && ActiveMineSerial == MineSerial;
	}

	if (Movement == nullptr)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineRejected: Unit=%s MineSerial=%u Reason=MovementUnavailable Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			MineSerial,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		ClearHeldCommand();
		ResetMineExecutor();
		return false;
	}

	if (!RequestMineApproachMove(Owner, Node, MineSerial, 0.0f, TEXT("Primary")))
	{
		ClearHeldCommand();
		ResetMineExecutor();
		return false;
	}

	return true;
}

bool UGP_UnitCommandComponent::TryConsumeMineMovementResult(
	uint32 Serial,
	EGP_MovementResult Result,
	EGP_MovementResultReason Reason)
{
	if (MineState == EGP_MineExecutionState::Idle || ActiveMineSerial == 0)
	{
		return false;
	}

	if (Serial != ActiveMineSerial)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	if (Result == EGP_MovementResult::Cancelled)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution MineApproachCancelled: Unit=%s MineSerial=%u MovementReason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			Serial,
			GPUnitCommandStatePrivate::MovementResultReasonToString(Reason),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));

		if (MineState == EGP_MineExecutionState::Approaching)
		{
			if (HeldCommand.IsSet()
				&& HeldCommand.GetValue().CommandTag == FGPGameplayTags::Get().Command_Mine
				&& HeldCommand.GetValue().CommandSerial == Serial)
			{
				ClearHeldCommand();
			}
			ResetMineExecutor();
		}
		return true;
	}

	if (Result != EGP_MovementResult::Reached)
	{
		return true;
	}

	if (MineState != EGP_MineExecutionState::Approaching)
	{
		return true;
	}

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution MineApproachReached: Unit=%s MineSerial=%u Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		Serial,
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	BeginMiningAtHeldTarget(Serial);
	return true;
}

void UGP_UnitCommandComponent::HandleMiningStateChanged(
	EGP_MiningState PreviousState,
	EGP_MiningState NewState,
	EGP_MiningStopReason Reason)
{
	(void)PreviousState;
	if (bFinishingMine || ActiveMineSerial == 0)
	{
		return;
	}

	const bool bTerminal =
		NewState == EGP_MiningState::CargoFull
		|| NewState == EGP_MiningState::DepositDepleted
		|| NewState == EGP_MiningState::OutOfRange
		|| NewState == EGP_MiningState::Invalid
		|| NewState == EGP_MiningState::Idle;

	if (!bTerminal)
	{
		return;
	}

	AActor* Owner = GetOwner();
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution MineTerminal: Unit=%s MineSerial=%u NewState=%d Reason=%d"),
		*GetNameSafe(Owner),
		ActiveMineSerial,
		static_cast<int32>(NewState),
		static_cast<int32>(Reason));

	const uint32 Serial = ActiveMineSerial;
	TGuardValue<bool> Guard(bFinishingMine, true);
	UnbindMiningStateEvents();
	MineState = EGP_MineExecutionState::Idle;
	ActiveMineSerial = 0;
	MineTarget.Reset();

	if (HeldCommand.IsSet()
		&& HeldCommand.GetValue().CommandTag == FGPGameplayTags::Get().Command_Mine
		&& HeldCommand.GetValue().CommandSerial == Serial)
	{
		ClearHeldCommand();
	}
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

	ClearAttackCadenceState();
	ClearApproachProgressState();
	AttackTarget = ValidTarget;
	BindAttackTargetDeath(ValidTarget);
	SetAttackTickEnabled(true);

	float EffectiveRange = 0.0f;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::Invalid;
	if (!TryResolveEffectiveAttackRange(EffectiveRange, RangeSource))
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution AttackRejected: Unit=%s AttackSerial=%u Target=%s Reason=InvalidTarget Detail=InvalidEffectiveRange Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			AttackSerial,
			*GetNameSafe(ValidTarget),
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

	float Distance = -1.0f;
	const bool bDistanceAvailable = TryComputeAttackDistance2D(Owner, ValidTarget, Distance);
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackAccepted: Unit=%s AttackSerial=%u Target=%s Distance=%.1f DistanceAvailable=%s AttackRange=%.1f RangeSource=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		AttackSerial,
		*GetNameSafe(ValidTarget),
		Distance,
		bDistanceAvailable ? TEXT("true") : TEXT("false"),
		EffectiveRange,
		AttackRangeSourceToString(RangeSource),
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	if (bDistanceAvailable && Distance <= EffectiveRange)
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

	float EffectiveRange = AttackRange;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::FallbackComponent;
	TryResolveEffectiveAttackRange(EffectiveRange, RangeSource);

	float Distance = -1.0f;
	const bool bDistanceAvailable = TryComputeAttackDistance2D(Owner, AttackTarget.Get(), Distance);
	const float AttackExitRange = EffectiveRange + AttackReadyExitTolerance;
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackStateChanged: Unit=%s AttackSerial=%u Target=%s PreviousState=%s NewState=Approaching Distance=%.1f DistanceAvailable=%s AttackRange=%.1f AttackExitRange=%.1f ExitTolerance=%.1f RangeSource=%s NextHitTime=%.3f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveAttackSerial,
		*GetNameSafe(AttackTarget.Get()),
		AttackStateToString(PreviousState),
		Distance,
		bDistanceAvailable ? TEXT("true") : TEXT("false"),
		EffectiveRange,
		AttackExitRange,
		AttackReadyExitTolerance,
		AttackRangeSourceToString(RangeSource),
		NextAttackHitTime,
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
	ClearApproachProgressState();
	SetAttackTickEnabled(true);

	float EffectiveRange = AttackRange;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::FallbackComponent;
	TryResolveEffectiveAttackRange(EffectiveRange, RangeSource);

	float Distance = -1.0f;
	const bool bDistanceAvailable = TryComputeAttackDistance2D(Owner, AttackTarget.Get(), Distance);
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackStateChanged: Unit=%s AttackSerial=%u Target=%s PreviousState=%s NewState=Ready Distance=%.1f DistanceAvailable=%s AttackRange=%.1f RangeSource=%s NextHitTime=%.3f FirstHitAttempted=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveAttackSerial,
		*GetNameSafe(AttackTarget.Get()),
		AttackStateToString(PreviousState),
		Distance,
		bDistanceAvailable ? TEXT("true") : TEXT("false"),
		EffectiveRange,
		AttackRangeSourceToString(RangeSource),
		NextAttackHitTime,
		bHasAttemptedFirstHit ? TEXT("true") : TEXT("false"),
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackReady: Unit=%s AttackSerial=%u Target=%s Distance=%.1f DistanceAvailable=%s AttackRange=%.1f RangeSource=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveAttackSerial,
		*GetNameSafe(AttackTarget.Get()),
		Distance,
		bDistanceAvailable ? TEXT("true") : TEXT("false"),
		EffectiveRange,
		AttackRangeSourceToString(RangeSource),
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));

	ProcessReadyCadence();
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

	float EffectiveRange = 0.0f;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::Invalid;
	if (!TryResolveEffectiveAttackRange(EffectiveRange, RangeSource))
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
		return;
	}

	float Distance = -1.0f;
	if (!TryComputeAttackDistance2D(Owner, Target, Distance))
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
		return;
	}

	if (Distance <= EffectiveRange)
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

	const float PreviousDestDelta = FVector::Dist2D(Destination, LastApproachDestination);
	if (PreviousDestDelta >= AttackReissueDistance)
	{
		// Meaningful new destination (moving target) — allow fresh approach progress.
		ClearApproachProgressState();
	}

	LastApproachDestination = Destination;
	LastApproachIssueTime = Now;
	AttackState = EGP_AttackExecutionState::Approaching;
	SetAttackTickEnabled(true);

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackApproachRequested: Unit=%s AttackSerial=%u MovementSerial=%u Destination=%s Target=%s Distance=%.1f AttackRange=%.1f RangeSource=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveAttackSerial,
		ActiveAttackSerial,
		*Destination.ToCompactString(),
		*GetNameSafe(Target),
		Distance,
		EffectiveRange,
		AttackRangeSourceToString(RangeSource),
		GPUnitCommandStatePrivate::RoleToString(Owner->GetLocalRole()),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
}

void UGP_UnitCommandComponent::EvaluateAttack()
{
	if (bFinishingAttack || AttackState == EGP_AttackExecutionState::Idle || bAttackHitInProgress)
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

	const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (OwnerUnit != nullptr && OwnerUnit->IsDead())
	{
		ResetAttackExecutor();
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

	float EffectiveRange = 0.0f;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::Invalid;
	if (!TryResolveEffectiveAttackRange(EffectiveRange, RangeSource))
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
		return;
	}

	float Distance = -1.0f;
	if (!TryComputeAttackDistance2D(Owner, Target, Distance))
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
		return;
	}

	if (AttackState == EGP_AttackExecutionState::Ready)
	{
		const float AttackExitRange = EffectiveRange + AttackReadyExitTolerance;
		if (Distance > AttackExitRange)
		{
			EnterAttackApproaching();
			return;
		}

		ProcessReadyCadence();
		return;
	}

	if (AttackState == EGP_AttackExecutionState::Approaching)
	{
		if (Distance <= EffectiveRange)
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

void UGP_UnitCommandComponent::ProcessReadyCadence()
{
	if (bFinishingAttack
		|| bAttackHitInProgress
		|| AttackState != EGP_AttackExecutionState::Ready
		|| !HasExactActiveHeldAttack())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	const UWorld* World = Owner->GetWorld();
	const double Now = World != nullptr ? World->GetTimeSeconds() : 0.0;

	if (!bHasAttemptedFirstHit)
	{
		AttemptAttackHit();
		return;
	}

	if (NextAttackHitTime >= 0.0 && Now >= NextAttackHitTime)
	{
		AttemptAttackHit();
	}
}

void UGP_UnitCommandComponent::AttemptAttackHit()
{
	if (bFinishingAttack || bAttackHitInProgress)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (OwnerUnit == nullptr || OwnerUnit->IsDead())
	{
		return;
	}

	if (!HasExactActiveHeldAttack() || AttackState != EGP_AttackExecutionState::Ready || ActiveAttackSerial == 0)
	{
		return;
	}

	const uint32 HitSerial = ActiveAttackSerial;
	TWeakObjectPtr<AGP_UnitBase> HitTarget = AttackTarget;

	AGP_UnitBase* Target = nullptr;
	EGP_AttackTerminalReason FailReason = EGP_AttackTerminalReason::InvalidTarget;
	if (!ValidateAttackTarget(AttackTarget.Get(), Target, FailReason))
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, FailReason);
		return;
	}

	float EffectiveRange = 0.0f;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::Invalid;
	if (!TryResolveEffectiveAttackRange(EffectiveRange, RangeSource))
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
		return;
	}

	float Distance = -1.0f;
	if (!TryComputeAttackDistance2D(Owner, Target, Distance))
	{
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
		return;
	}

	const float AttackExitRange = EffectiveRange + AttackReadyExitTolerance;
	if (Distance > AttackExitRange)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP AttackHitRejected: Unit=%s Target=%s Serial=%u Reason=OutOfRange Distance=%.1f AttackRange=%.1f AttackExitRange=%.1f ExitTolerance=%.1f"),
			*GetNameSafe(Owner),
			*GetNameSafe(Target),
			HitSerial,
			Distance,
			EffectiveRange,
			AttackExitRange,
			AttackReadyExitTolerance);
		EnterAttackApproaching();
		return;
	}

	if (Distance > EffectiveRange)
	{
		// Hysteresis band: stay Ready, preserve NextHitTime / FirstHitAttempted; no damage until re-entry.
		return;
	}

	const UWorld* World = Owner->GetWorld();
	const double Now = World != nullptr ? World->GetTimeSeconds() : 0.0;
	const float PendingCooldown = ResolveSanitizedAttackCooldown(false);

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP AttackHitAttempt: Unit=%s Target=%s Serial=%u State=Ready Time=%.3f Cooldown=%.3f Range=%.1f RangeSource=%s Distance=%.1f"),
		*OwnerUnit->GetName(),
		*Target->GetName(),
		HitSerial,
		Now,
		PendingCooldown,
		EffectiveRange,
		AttackRangeSourceToString(RangeSource),
		Distance);

	bAttackHitInProgress = true;
	bHasAttemptedFirstHit = true;

	FGP_DamageApplicationResult DamageResult;
	const bool bApplied = Target->ApplyDamageFromUnit(OwnerUnit, DamageResult);

	bAttackHitInProgress = false;

	// Snapshot before reading mutable Attack state — sync TargetDied may FinishAttack during Apply.
	const uint32 PresentationAttackSerial = HitSerial;
	AGP_UnitBase* const PresentationTarget = Target;
	const float PresentationAppliedDamage = DamageResult.FinalDamage;
	const bool bPresentationBlocked = bApplied && PresentationAppliedDamage <= 0.0f;
	const bool bPresentationTargetDied =
		!IsValid(PresentationTarget) || PresentationTarget->IsDead();
	const float PresentationWorldTime =
		World != nullptr ? static_cast<float>(World->GetTimeSeconds()) : static_cast<float>(Now);

	if (bApplied)
	{
		if (UGP_CombatPresentationComponent* Presentation = OwnerUnit->GetCombatPresentationComponent())
		{
			Presentation->AuthorityEmitAttackHitPresentation(
				PresentationAttackSerial,
				PresentationTarget,
				EGP_CombatPresentationEventType::MeleeImpact,
				PresentationWorldTime,
				PresentationAppliedDamage,
				bPresentationBlocked,
				bPresentationTargetDied);
		}
	}

	if (bFinishingAttack
		|| ActiveAttackSerial != HitSerial
		|| AttackTarget != HitTarget
		|| !HasExactActiveHeldAttack()
		|| AttackState != EGP_AttackExecutionState::Ready)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP AttackHitApplied: Unit=%s Target=%s Serial=%u Applied=%s AppliedDamage=%.2f TargetHealthBefore=%.2f TargetHealthAfter=%.2f TargetDead=%s NextHitTime=none Note=AttackEndedDuringApply"),
			*GetNameSafe(Owner),
			*GetNameSafe(HitTarget.Get()),
			HitSerial,
			bApplied ? TEXT("true") : TEXT("false"),
			DamageResult.FinalDamage,
			DamageResult.HealthBefore,
			DamageResult.HealthAfter,
			(HitTarget.IsValid() && HitTarget->IsDead()) ? TEXT("true") : TEXT("false"));
		return;
	}

	if (OwnerUnit->IsDead())
	{
		return;
	}

	AGP_UnitBase* PostTarget = AttackTarget.Get();
	if (PostTarget == nullptr || !IsValid(PostTarget) || PostTarget->IsDead())
	{
		FinishAttack(
			EGP_AttackTerminalResult::Failed,
			EGP_AttackTerminalReason::TargetDied);
		return;
	}

	const float UsedCooldown = ResolveSanitizedAttackCooldown(true);
	const double ScheduleNow = World != nullptr ? World->GetTimeSeconds() : Now;
	NextAttackHitTime = ScheduleNow + static_cast<double>(UsedCooldown);

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP AttackHitApplied: Unit=%s Target=%s Serial=%u Applied=%s AppliedDamage=%.2f TargetHealthBefore=%.2f TargetHealthAfter=%.2f TargetDead=%s NextHitTime=%.3f Cooldown=%.3f"),
		*OwnerUnit->GetName(),
		*PostTarget->GetName(),
		HitSerial,
		bApplied ? TEXT("true") : TEXT("false"),
		DamageResult.FinalDamage,
		DamageResult.HealthBefore,
		DamageResult.HealthAfter,
		PostTarget->IsDead() ? TEXT("true") : TEXT("false"),
		NextAttackHitTime,
		UsedCooldown);

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP AttackCooldownScheduled: Unit=%s Serial=%u NextHitTime=%.3f Cooldown=%.3f"),
		*OwnerUnit->GetName(),
		HitSerial,
		NextAttackHitTime,
		UsedCooldown);
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
	const float FinishedRange = GetAttackRange();
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

	UnbindAttackTargetDeath();
	ClearAttackCadenceState();
	ClearApproachProgressState();

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
		FinishedRange,
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

		float EffectiveRange = 0.0f;
		EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::Invalid;
		if (!TryResolveEffectiveAttackRange(EffectiveRange, RangeSource))
		{
			FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
			return true;
		}

		if (Dist > EffectiveRange)
		{
			HandleReachedStillOutOfRange(Owner, Target, Dist, EffectiveRange, RangeSource);
		}
		else
		{
			EnterAttackReady();
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

			float EffectiveRange = 0.0f;
			EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::Invalid;
			if (!TryResolveEffectiveAttackRange(EffectiveRange, RangeSource))
			{
				FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::InvalidTarget);
				return true;
			}

			if (Dist > EffectiveRange)
			{
				HandleReachedStillOutOfRange(Owner, Target, Dist, EffectiveRange, RangeSource);
			}
			else
			{
				EnterAttackReady();
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

	if (TryConsumeMineMovementResult(Serial, Result, Reason))
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

	if (TryAcceptIdempotentMineCommand(Command))
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution MineIdempotentAccepted: Unit=%s Target=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			*GetNameSafe(Command.TargetActor),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return;
	}

	if (TryRejectMineCommandBeforeAccept(Command))
	{
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
	ResetMineExecutorForReplacement(PreviousCommand);

	const bool bHeldRemainsAfterSync = SynchronizeMovementWithHeldCommand(PreviousCommand);
	if (!bHeldRemainsAfterSync || !HeldCommand.IsSet())
	{
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (HeldCommand.GetValue().CommandTag == GPTags.Command_Attack)
	{
		if (!StartAttackExecutor() || !HeldCommand.IsSet())
		{
			return;
		}
	}
	else if (HeldCommand.GetValue().CommandTag == GPTags.Command_Mine)
	{
		if (!StartMineExecutor() || !HeldCommand.IsSet())
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
	float EffectiveRange = AttackRange;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::FallbackComponent;
	if (TryResolveEffectiveAttackRange(EffectiveRange, RangeSource))
	{
		return EffectiveRange;
	}
	return AttackRange;
}

EGP_AttackRangeSource UGP_UnitCommandComponent::GetAttackRangeSource() const
{
	float EffectiveRange = 0.0f;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::Invalid;
	TryResolveEffectiveAttackRange(EffectiveRange, RangeSource);
	return RangeSource;
}

const TCHAR* UGP_UnitCommandComponent::GetAttackRangeSourceLabel() const
{
	return AttackRangeSourceToString(GetAttackRangeSource());
}

double UGP_UnitCommandComponent::GetNextAttackHitTime() const
{
	return NextAttackHitTime;
}

bool UGP_UnitCommandComponent::HasAttemptedFirstAttackHit() const
{
	return bHasAttemptedFirstHit;
}

bool UGP_UnitCommandComponent::IsAttackTargetDeathBound() const
{
	return TargetDiedHandle.IsValid() && BoundDeathTarget.IsValid();
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
	ResetMineExecutor();

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
		const UWorld* InspectWorld = MobileUnit->GetWorld();
		const double Now = InspectWorld != nullptr ? InspectWorld->GetTimeSeconds() : 0.0;
		const double NextHit = Command->GetNextAttackHitTime();
		const double TimeUntilNext = (NextHit >= 0.0) ? FMath::Max(0.0, NextHit - Now) : -1.0;
		const UGP_UnitAttributeSet* Attrs = MobileUnit->GetUnitAttributeSet();
		const float Cooldown = Attrs != nullptr ? Attrs->GetAttackCooldown() : -1.0f;

		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution Console: gp.Attack.Inspect Unit=%s Selection=%s HeldSerial=%u HeldTag=%s AttackState=%s ActiveAttackSerial=%u Target=%s TargetDead=%s Distance=%.1f EffectiveRange=%.1f RangeSource=%s AttackCooldown=%.3f FirstHitAttempted=%s NextHitTime=%.3f Now=%.3f TimeUntilNextHit=%.3f TargetDeathBound=%s IsMoving=%s MovementSerial=%u Role=%s NetMode=%s"),
			*MobileUnit->GetName(),
			Selection,
			HeldSerial,
			*HeldTag,
			AttackStateLabel(Command->GetAttackExecutionState()),
			Command->GetActiveAttackSerial(),
			*GetNameSafe(Target),
			(Target != nullptr && Target->IsDead()) ? TEXT("true") : TEXT("false"),
			Distance,
			Command->GetAttackRange(),
			Command->GetAttackRangeSourceLabel(),
			Cooldown,
			Command->HasAttemptedFirstAttackHit() ? TEXT("true") : TEXT("false"),
			NextHit,
			Now,
			TimeUntilNext,
			Command->IsAttackTargetDeathBound() ? TEXT("true") : TEXT("false"),
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
