// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitCommandComponent.h"

#include "AttributeSets/GPUnitAttributeSet.h"
#include "Buildings/GPBuildingBase.h"
#include "Buildings/GPMainBase.h"
#include "Combat/GPCombatPresentationComponent.h"
#include "Combat/GPCombatLOS.h"
#include "Combat/GPDamageApplication.h"
#include "Command/GPUnitCommand.h"
#include "Components/BoxComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "GameFramework/Actor.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceApproach.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceNode.h"
#include "Resources/GPStorageComponent.h"
#include "Settings/GPResourceGameplaySettings.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPUnitBase.h"
#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING
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
		case EGP_MovementResult::Failed:
			return TEXT("Failed");
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
		case EGP_MovementResultReason::PathNotFound:
			return TEXT("PathNotFound");
		case EGP_MovementResultReason::PathInvalid:
			return TEXT("PathInvalid");
		case EGP_MovementResultReason::DestinationOffNav:
			return TEXT("DestinationOffNav");
		case EGP_MovementResultReason::Blocked:
			return TEXT("Blocked");
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
		case EGP_MovementRejectReason::PathNotFound:
			return TEXT("PathNotFound");
		case EGP_MovementRejectReason::DestinationOffNav:
			return TEXT("DestinationOffNav");
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

	StartCombatAutoAcquireTimer();

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
	CancelRetaliation(TEXT("EndPlay"), true);
	StopCombatAutoAcquireTimer();
	LastAutoAcquireCandidate.Reset();

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
	// ResetMineExecutor → ResetHaulExecutor clears drop-off subscriptions/timers.
	// Explicit haul cleanup remains safe if mine reset short-circuits in future.

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

	// Ready/firing: authority yaw toward Attack target. Approaching uses movement orientation.
	if (AttackState == EGP_AttackExecutionState::Ready)
	{
		UpdateAttackFacingTowardTarget(DeltaTime);
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
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (Held.CommandSerial != ActiveAttackSerial)
	{
		return false;
	}

	// Explicit Attack or AttackMove engagement share the Attack FSM serial ownership.
	return Held.CommandTag == GPTags.Command_Attack
		|| Held.CommandTag == GPTags.Command_AttackMove;
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

bool UGP_UnitCommandComponent::IsCombatCapableForAutoAcquire(const AGP_UnitBase* Unit) const
{
	if (Unit == nullptr)
	{
		return false;
	}
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPTags.Unit_Type_SalvageWalker.IsValid()
		&& Unit->HasCapabilityTag(GPTags.Unit_Type_SalvageWalker))
	{
		return true;
	}
	return GPTags.Building_Type_DefensiveTurret.IsValid()
		&& Unit->HasCapabilityTag(GPTags.Building_Type_DefensiveTurret);
}

EGP_AutoAcquireMode UGP_UnitCommandComponent::ResolveIdleAutoAcquireMode(const AGP_UnitBase* OwnerUnit) const
{
	if (OwnerUnit == nullptr)
	{
		return EGP_AutoAcquireMode::LegacyUnitIdle;
	}
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPTags.Building_Type_DefensiveTurret.IsValid()
		&& OwnerUnit->HasCapabilityTag(GPTags.Building_Type_DefensiveTurret))
	{
		return EGP_AutoAcquireMode::DefensiveTurretIdle;
	}
	return EGP_AutoAcquireMode::LegacyUnitIdle;
}

bool UGP_UnitCommandComponent::IsEligibleAutoAcquireTarget(
	const AGP_UnitBase* OwnerUnit,
	const AGP_UnitBase* Candidate,
	EGP_AutoAcquireMode Mode) const
{
	if (OwnerUnit == nullptr || !IsValid(Candidate) || Candidate == OwnerUnit)
	{
		return false;
	}

	if (Candidate->IsA(AGP_BuildingBase::StaticClass()))
	{
		// Buildings are valid only for Defensive Turret idle AutoAcquire.
		// Legacy Salvage Walker idle + AttackMove keep the S30R/S32A unit-only surface.
		return Mode == EGP_AutoAcquireMode::DefensiveTurretIdle;
	}

	return true;
}

bool UGP_UnitCommandComponent::IsEligibleForCombatAutoAcquire() const
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (OwnerUnit == nullptr || !OwnerUnit->IsUnitDefinitionReady()
		|| OwnerUnit->IsDead() || !IsCombatCapableForAutoAcquire(OwnerUnit))
	{
		return false;
	}

	if (IsAttackActive() || bRetaliationActive)
	{
		return false;
	}

	if (MineState != EGP_MineExecutionState::Idle || ActiveMineSerial != 0)
	{
		return false;
	}
	if (HaulState != EGP_HaulExecutionState::Idle || ActiveHaulSerial != 0)
	{
		return false;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (HeldCommand.IsSet())
	{
		const FGameplayTag& HeldTag = HeldCommand.GetValue().CommandTag;
		if (HeldTag == GPTags.Command_Move
			|| HeldTag == GPTags.Command_Attack
			|| HeldTag == GPTags.Command_AttackMove
			|| HeldTag == GPTags.Command_Mine)
		{
			return false;
		}
	}

	if (const UGP_MovementComponent* Movement = ResolveMovementComponent())
	{
		if (Movement->IsMoving())
		{
			return false;
		}
	}

	return true;
}

bool UGP_UnitCommandComponent::IsEligibleForAttackMoveAcquire() const
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (OwnerUnit == nullptr || !OwnerUnit->IsUnitDefinitionReady() || OwnerUnit->IsDead())
	{
		return false;
	}

	const FGPGameplayTags& AttackMoveTags = FGPGameplayTags::Get();
	if (!AttackMoveTags.Unit_Type_SalvageWalker.IsValid()
		|| !OwnerUnit->HasCapabilityTag(AttackMoveTags.Unit_Type_SalvageWalker))
	{
		return false;
	}

	if (IsAttackActive())
	{
		return false;
	}

	if (MineState != EGP_MineExecutionState::Idle || ActiveMineSerial != 0)
	{
		return false;
	}
	if (HaulState != EGP_HaulExecutionState::Idle || ActiveHaulSerial != 0)
	{
		return false;
	}

	if (!HeldCommand.IsSet())
	{
		return false;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	return HeldCommand.GetValue().CommandTag == GPTags.Command_AttackMove;
}

bool UGP_UnitCommandComponent::IsAttackMoveActive() const
{
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	return HeldCommand.IsSet()
		&& HeldCommand.GetValue().CommandTag == GPTags.Command_AttackMove;
}

bool UGP_UnitCommandComponent::IsAttackMoveEngaging() const
{
	return IsAttackMoveActive() && IsAttackActive();
}

FVector UGP_UnitCommandComponent::GetAttackMoveDestination() const
{
	if (!IsAttackMoveActive())
	{
		return FVector::ZeroVector;
	}
	return HeldCommand.GetValue().TargetLocation;
}

bool UGP_UnitCommandComponent::IsHeldAttackMove(uint32 Serial) const
{
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	return HeldCommand.IsSet()
		&& HeldCommand.GetValue().CommandTag == GPTags.Command_AttackMove
		&& HeldCommand.GetValue().CommandSerial == Serial;
}

void UGP_UnitCommandComponent::StartCombatAutoAcquireTimer()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (World == nullptr || Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (OwnerUnit == nullptr || !OwnerUnit->IsUnitDefinitionReady()
		|| !IsCombatCapableForAutoAcquire(OwnerUnit))
	{
		return;
	}

	StopCombatAutoAcquireTimer();
	const float Interval = FMath::Max(0.05f, AutoAcquireScanIntervalSeconds);
	World->GetTimerManager().SetTimer(
		AutoAcquireTimerHandle,
		this,
		&UGP_UnitCommandComponent::OnCombatAutoAcquireScan,
		Interval,
		true);
}

void UGP_UnitCommandComponent::RefreshCombatAutoAcquireTimer()
{
	StartCombatAutoAcquireTimer();
}

void UGP_UnitCommandComponent::StopCombatAutoAcquireTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoAcquireTimerHandle);
	}
	AutoAcquireTimerHandle.Invalidate();
}

AGP_UnitBase* UGP_UnitCommandComponent::FindNearestAutoAcquireTarget(
	float MaxRangeCm,
	EGP_AutoAcquireMode Mode) const
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (World == nullptr || OwnerUnit == nullptr || MaxRangeCm <= 0.0f)
	{
		return nullptr;
	}

	AGP_UnitBase* Best = nullptr;
	float BestDistSq = MaxRangeCm * MaxRangeCm;
	FName BestName = NAME_None;
	const FVector OwnerLoc = OwnerUnit->GetActorLocation();

	for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
	{
		AGP_UnitBase* Candidate = *It;
		if (!IsEligibleAutoAcquireTarget(OwnerUnit, Candidate, Mode))
		{
			continue;
		}

		AGP_UnitBase* ValidTarget = nullptr;
		EGP_AttackTerminalReason Reason = EGP_AttackTerminalReason::InvalidTarget;
		if (!ValidateAttackTarget(Candidate, ValidTarget, Reason) || ValidTarget == nullptr)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(OwnerLoc, ValidTarget->GetActorLocation());
		if (DistSq > BestDistSq + KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FName CandidateName = ValidTarget->GetFName();
		if (Best != nullptr && FMath::IsNearlyEqual(DistSq, BestDistSq, KINDA_SMALL_NUMBER))
		{
			// Deterministic tie-break: prefer lexicographically smaller name.
			if (CandidateName.ToString() >= BestName.ToString())
			{
				continue;
			}
		}

		Best = ValidTarget;
		BestDistSq = DistSq;
		BestName = CandidateName;
	}

	return Best;
}

void UGP_UnitCommandComponent::TryIssueAutoAcquireAttack(AGP_UnitBase* Target)
{
	if (!IsValid(Target) || !IsEligibleForCombatAutoAcquire())
	{
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	FGP_UnitCommand AttackCommand;
	AttackCommand.CommandTag = GPTags.Command_Attack;
	AttackCommand.TargetActor = Target;
	AttackCommand.TargetLocation = Target->GetActorLocation();
	AttackCommand.bQueue = false;

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AutoAcquire: Unit=%s Target=%s Range=%.1f"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Target),
		GetAttackRange());

	HandleCommand(AttackCommand);
}

void UGP_UnitCommandComponent::TryIssueAttackMoveAcquire(AGP_UnitBase* Target)
{
	if (!IsValid(Target) || !IsEligibleForAttackMoveAcquire())
	{
		return;
	}

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackMoveAcquire: Unit=%s Target=%s Sight=%.1f Dest=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Target),
		GetEffectiveAutoAcquireRange(),
		*GetAttackMoveDestination().ToCompactString());

	StartAttackMoveEngagement(Target);
}

bool UGP_UnitCommandComponent::StartAttackMoveEngagement(AGP_UnitBase* Target)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	if (Owner == nullptr || !Owner->HasAuthority() || !IsAttackMoveActive())
	{
		return false;
	}

	const FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
	const uint32 AttackSerial = Held.CommandSerial;

	if (!IsAttackConfigValid())
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution AttackMoveEngageRejected: Unit=%s Serial=%u Target=%s Reason=InvalidAttackConfig Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			AttackSerial,
			*GetNameSafe(Target),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return false;
	}

	AGP_UnitBase* ValidTarget = nullptr;
	EGP_AttackTerminalReason FailReason = EGP_AttackTerminalReason::InvalidTarget;
	if (!ValidateAttackTarget(Target, ValidTarget, FailReason) || ValidTarget == nullptr)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution AttackMoveEngageRejected: Unit=%s Serial=%u Target=%s Reason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			AttackSerial,
			*GetNameSafe(Target),
			AttackTerminalReasonToString(FailReason),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return false;
	}

	float EffectiveRange = 0.0f;
	EGP_AttackRangeSource RangeSource = EGP_AttackRangeSource::Invalid;
	if (!TryResolveEffectiveAttackRange(EffectiveRange, RangeSource))
	{
		return false;
	}

	// Preserve Held AttackMove destination; run Attack FSM under the same serial.
	ClearAttackCadenceState();
	ClearApproachProgressState();
	ActiveAttackSerial = AttackSerial;
	AttackTarget = ValidTarget;
	BindAttackTargetDeath(ValidTarget);
	SetAttackTickEnabled(true);

	float Distance = -1.0f;
	const bool bDistanceAvailable = TryComputeAttackDistance2D(Owner, ValidTarget, Distance);
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackMoveEngageAccepted: Unit=%s Serial=%u Target=%s Distance=%.1f AttackRange=%.1f Dest=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		AttackSerial,
		*GetNameSafe(ValidTarget),
		Distance,
		EffectiveRange,
		*Held.TargetLocation.ToCompactString(),
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

	return IsAttackActive();
}

bool UGP_UnitCommandComponent::ResumeAttackMoveTravelAfterEngagement()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || !IsAttackMoveActive())
	{
		return false;
	}

	const FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
	AGP_MobileUnit* MobileUnit = Cast<AGP_MobileUnit>(Owner);
	UGP_MovementComponent* Movement =
		MobileUnit != nullptr ? MobileUnit->GetUnitMovementComponent() : ResolveMovementComponent();
	if (Movement == nullptr)
	{
		return false;
	}

	const FGP_MovementRequestOutcome Outcome =
		Movement->RequestMove(Held.TargetLocation, Held.CommandSerial);
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution AttackMoveResume: Unit=%s Serial=%u Destination=%s Accepted=%s"),
		*GetNameSafe(Owner),
		Held.CommandSerial,
		*Held.TargetLocation.ToCompactString(),
		Outcome.IsAccepted() ? TEXT("true") : TEXT("false"));
	return Outcome.IsAccepted();
}

float UGP_UnitCommandComponent::GetEffectiveAutoAcquireRange() const
{
	const float EffectiveAttackRange = GetAttackRange();
	const float Sight = FMath::Max(0.0f, AutoAcquireSightRangeCm);
	// Predictable semantics: sight never shrinks acquire below fire range.
	return FMath::Max(Sight, EffectiveAttackRange);
}

void UGP_UnitCommandComponent::UpdateAttackFacingTowardTarget(float DeltaTime)
{
	if (DeltaTime <= 0.0f || AttackFacingRotationSpeedDegreesPerSecond <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	AActor* Owner = GetOwner();
	AGP_UnitBase* Target = AttackTarget.Get();
	if (Owner == nullptr || !Owner->HasAuthority() || !IsValid(Target) || Target->IsDead())
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - Owner->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (!ToTarget.Normalize())
	{
		return;
	}

	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
	const FRotator CurrentRotation = Owner->GetActorRotation();
	const FRotator TargetRotation(0.0f, TargetYaw, 0.0f);
	const FRotator NewRotation = FMath::RInterpConstantTo(
		CurrentRotation,
		TargetRotation,
		DeltaTime,
		AttackFacingRotationSpeedDegreesPerSecond);
	Owner->SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
}

void UGP_UnitCommandComponent::OnCombatAutoAcquireScan()
{
	LastAutoAcquireCandidate.Reset();

	const float Range = GetEffectiveAutoAcquireRange();
	if (Range <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// GP-S32A: AttackMove travelling may acquire; pure Move stays suppressed via Idle eligibility.
	if (IsEligibleForAttackMoveAcquire())
	{
		AGP_UnitBase* Target = FindNearestAutoAcquireTarget(Range, EGP_AutoAcquireMode::AttackMove);
		LastAutoAcquireCandidate = Target;
		if (Target != nullptr)
		{
			TryIssueAttackMoveAcquire(Target);
		}
		return;
	}

	if (!IsEligibleForCombatAutoAcquire())
	{
		return;
	}

	const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(GetOwner());
	AGP_UnitBase* Target = FindNearestAutoAcquireTarget(Range, ResolveIdleAutoAcquireMode(OwnerUnit));
	LastAutoAcquireCandidate = Target;
	if (Target != nullptr)
	{
		TryIssueAutoAcquireAttack(Target);
	}
}

void UGP_UnitCommandComponent::ClearAttackCadenceState()
{
	NextAttackHitTime = -1.0;
	bHasAttemptedFirstHit = false;
	bAttackHitInProgress = false;
	bAttackLOSBlocked = false;
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
	case EGP_MineExecutionState::WaitingForResource: return TEXT("WaitingForResource");
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

float UGP_UnitCommandComponent::ResolveApproachSafetyMarginCm() const
{
	if (const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get())
	{
		return Settings->ResourceApproachSafetyMarginCm;
	}
	return 25.0f;
}

float UGP_UnitCommandComponent::GetMineApproachSafetyMarginCm() const
{
	return ResolveApproachSafetyMarginCm();
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

void UGP_UnitCommandComponent::DebugForceNextHaulArrivalOutOfRangeOnce()
{
	bDebugForceNextHaulArrivalOutOfRange = true;
}

void UGP_UnitCommandComponent::DebugForceNextHaulApproachRejectOnce()
{
	bDebugForceNextHaulApproachReject = true;
}

void UGP_UnitCommandComponent::DebugResetFifoWatchdogCounters()
{
	DebugMineBeginCallsThisTransition = 0;
	DebugReassignmentAttemptsThisTransition = 0;
	DebugSameTargetRetargetAttempts = 0;
	bLoggedSameTargetRetargetSkip = false;
}
#endif

const TCHAR* UGP_UnitCommandComponent::HaulStateToString(EGP_HaulExecutionState State)
{
	switch (State)
	{
	case EGP_HaulExecutionState::Idle: return TEXT("Idle");
	case EGP_HaulExecutionState::ReturningToBase: return TEXT("ReturningToBase");
	case EGP_HaulExecutionState::DroppingOff: return TEXT("DroppingOff");
	case EGP_HaulExecutionState::ReturningToDeposit: return TEXT("ReturningToDeposit");
	case EGP_HaulExecutionState::WaitingForDropOff: return TEXT("WaitingForDropOff");
	case EGP_HaulExecutionState::Failed: return TEXT("Failed");
	default: return TEXT("Unknown");
	}
}

EGP_HaulExecutionState UGP_UnitCommandComponent::GetHaulExecutionState() const
{
	return HaulState;
}

uint32 UGP_UnitCommandComponent::GetActiveHaulSerial() const
{
	return ActiveHaulSerial;
}

AGP_ResourceNode* UGP_UnitCommandComponent::GetLastHaulDeposit() const
{
	return LastHaulDeposit.Get();
}

AGP_MainBase* UGP_UnitCommandComponent::GetHaulMainBase() const
{
	return HaulMainBase.Get();
}

bool UGP_UnitCommandComponent::IsHaulActive() const
{
	// WaitingForDropOff keeps chain identity but is not an active travel/drop-off leg.
	return ActiveHaulSerial != 0
		&& (HaulState == EGP_HaulExecutionState::ReturningToBase
			|| HaulState == EGP_HaulExecutionState::DroppingOff
			|| HaulState == EGP_HaulExecutionState::ReturningToDeposit);
}

bool UGP_UnitCommandComponent::ShouldReturnToDepositAfterHaul() const
{
	return bShouldReturnToDepositAfterHaul;
}

float UGP_UnitCommandComponent::GetLastHaulAcceptedAmount() const
{
	return LastHaulAcceptedAmount;
}

float UGP_UnitCommandComponent::GetLastHaulRejectedAmount() const
{
	return LastHaulRejectedAmount;
}

float UGP_UnitCommandComponent::GetLastHaulThreatDelta() const
{
	return LastHaulThreatDelta;
}

FVector UGP_UnitCommandComponent::GetHaulApproachDestination() const
{
	return HaulApproachDestination;
}

float UGP_UnitCommandComponent::GetHaulApproachDesiredDistance() const
{
	return HaulApproachDesiredDistance;
}

int32 UGP_UnitCommandComponent::GetHaulApproachAttempt() const
{
	return HaulApproachAttempt;
}

float UGP_UnitCommandComponent::GetHaulLastArrivalDistance() const
{
	return HaulLastArrivalDistance;
}

float UGP_UnitCommandComponent::GetHaulDropOffRangeCm() const
{
	return HaulDropOffRangeCm;
}

bool UGP_UnitCommandComponent::IsActiveHaulChainForDeposit(const AGP_ResourceNode* Node) const
{
	if (!IsValid(Node) || ActiveHaulSerial == 0 || !IsHaulActive())
	{
		return false;
	}
	return LastHaulDeposit.Get() == Node;
}

bool UGP_UnitCommandComponent::TryMakeRangeApproachDestination(
	const AActor* Owner,
	const AActor* Target,
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

	if (Owner == nullptr || Target == nullptr
		|| !FMath::IsFinite(InteractionRangeCm) || InteractionRangeCm <= 0.0f
		|| !FMath::IsFinite(AcceptanceRadius) || AcceptanceRadius < 0.0f)
	{
		return false;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const EGP_RangeApproachDistanceMode DistanceMode = Cast<AGP_MainBase>(Target) != nullptr
		? EGP_RangeApproachDistanceMode::GroundPlane2D
		: EGP_RangeApproachDistanceMode::ThreeDimensional;

	const float DeltaZ = OwnerLocation.Z - TargetLocation.Z;
	const float AbsDeltaZ = FMath::Abs(DeltaZ);
	const float DeltaZSq = FMath::Square(AbsDeltaZ);
	if (DistanceMode == EGP_RangeApproachDistanceMode::ThreeDimensional)
	{
		// Movement completes in a 2D acceptance circle; ResourceNode interaction validates 3D distance.
		const float RangeSq = FMath::Square(InteractionRangeCm);
		if (DeltaZSq >= RangeSq)
		{
			return false;
		}
	}

	const float CollisionHalfXY = ResolveTargetApproachClearanceHalfXY(Target);

	float DesiredHorizontal = -1.0f;
	if (!GPResourceApproach::TryComputeDesiredHorizontalDistance(
			OwnerLocation,
			TargetLocation,
			InteractionRangeCm,
			AcceptanceRadius,
			ResolveApproachSafetyMarginCm() + FMath::Max(0.0f, ExtraInwardMarginCm),
			CollisionHalfXY,
			DesiredHorizontal,
			DistanceMode))
	{
		return false;
	}

	FVector Away = OwnerLocation - TargetLocation;
	Away.Z = 0.0f;
	if (!GPResourceApproach::TryMakeApproachPoint(
			OwnerLocation,
			TargetLocation,
			Away.IsNearlyZero() ? FVector::ForwardVector : Away,
			DesiredHorizontal,
			InteractionRangeCm,
			OutDestination,
			DistanceMode))
	{
		return false;
	}

	OutDesiredHorizontalDistance = DesiredHorizontal;
	if (DistanceMode == EGP_RangeApproachDistanceMode::GroundPlane2D)
	{
		OutPredictedWorstCaseDistance = DesiredHorizontal + AcceptanceRadius;
	}
	else
	{
		OutPredictedWorstCaseDistance = FMath::Sqrt(
			FMath::Square(DesiredHorizontal + AcceptanceRadius) + DeltaZSq);
	}
	return OutPredictedWorstCaseDistance < InteractionRangeCm;
}

float UGP_UnitCommandComponent::ResolveTargetApproachClearanceHalfXY(const AActor* Target) const
{
	if (!IsValid(Target))
	{
		return 0.0f;
	}

	if (const AGP_BuildingBase* Building = Cast<AGP_BuildingBase>(Target))
	{
		if (const UBoxComponent* NavBox = Building->GetNavigationObstacle())
		{
			// World bounds half-extent captures BP rotation / non-uniform scale.
			const FVector BoundExtent = NavBox->Bounds.BoxExtent;
			return FMath::Max(BoundExtent.X, BoundExtent.Y);
		}
	}

	if (const AGP_ResourceNode* Node = Cast<AGP_ResourceNode>(Target))
	{
		if (const UBoxComponent* Box = Node->GetCollisionBox())
		{
			const FVector Extent = Box->GetScaledBoxExtent();
			return FMath::Max(Extent.X, Extent.Y);
		}
	}

	return 0.0f;
}

bool UGP_UnitCommandComponent::TryFindReachableRangeApproachDestination(
	const AActor* Owner,
	const AActor* Target,
	float InteractionRangeCm,
	float AcceptanceRadius,
	float ExtraInwardMarginCm,
	uint32 LogSerial,
	FVector& OutDestination,
	float& OutDesiredHorizontalDistance,
	float& OutPredictedWorstCaseDistance,
	float* OutPathLengthCm,
	int32* OutCandidateIndex) const
{
	OutDestination = FVector::ZeroVector;
	OutDesiredHorizontalDistance = -1.0f;
	OutPredictedWorstCaseDistance = -1.0f;
	if (OutPathLengthCm != nullptr)
	{
		*OutPathLengthCm = -1.0f;
	}
	if (OutCandidateIndex != nullptr)
	{
		*OutCandidateIndex = -1;
	}
#if !UE_BUILD_SHIPPING
	DebugLastApproachCandidateIndex = -1;
	DebugLastApproachCandidateCount = 0;
#endif

	if (!IsValid(Owner) || !IsValid(Target))
	{
		return false;
	}

	UWorld* World = Owner->GetWorld();
	const float ClearanceHalfXY = ResolveTargetApproachClearanceHalfXY(Target);
	const float Safety =
		ResolveApproachSafetyMarginCm() + FMath::Max(0.0f, ExtraInwardMarginCm);
	const bool bMainBaseTarget = Cast<AGP_MainBase>(Target) != nullptr;
	const EGP_RangeApproachDistanceMode DistanceMode = bMainBaseTarget
		? EGP_RangeApproachDistanceMode::GroundPlane2D
		: EGP_RangeApproachDistanceMode::ThreeDimensional;

	GPResourceApproach::FRangeApproachParams Params;
	Params.PathStart = Owner->GetActorLocation();
	Params.InteractionRangeCm = InteractionRangeCm;
	Params.AcceptanceRadiusCm = AcceptanceRadius;
	Params.SafetyMarginCm = Safety;
	Params.MaxPathLengthCm = 12000.0f;
	Params.DirectionCount = 8;
	Params.PathfindingActor = const_cast<AActor*>(Owner);
	Params.DistanceMode = DistanceMode;
	// Index 0 remains the direct radial toward Owner; distinct Worker positions yield distinct radials.
	Params.StartAngleBiasDegrees = 0.0f;
#if !UE_BUILD_SHIPPING
	Params.DebugSkipCandidateMask = DebugApproachSkipCandidateMask;
#endif

	const FVector TargetLocation = Target->GetActorLocation();
	const bool bHaulLog = bMainBaseTarget;

	const GPResourceApproach::FRangeApproachResult Eval = GPResourceApproach::EvaluateRangeApproachPath(
		World,
		TargetLocation,
		ClearanceHalfXY,
		Params,
		[&](const GPResourceApproach::FRangeApproachCandidateInfo& Info)
		{
			if (!bHaulLog)
			{
				return;
			}
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution HaulApproachCandidate: Unit=%s Serial=%u CandidateIndex=%d Candidate=%s Projected=%s WithinRange=%s PathResult=%s PathLength=%.1f Skipped=%s DistanceMode=%s"),
				*GetNameSafe(Owner),
				LogSerial,
				Info.Index,
				*Info.RawCandidate.ToCompactString(),
				Info.bProjected ? *Info.Projected.ToCompactString() : TEXT("None"),
				Info.bWithinRange ? TEXT("true") : TEXT("false"),
				Info.bPathOk ? TEXT("Ok") : (Info.bSkipped ? TEXT("Skipped") : TEXT("Fail")),
				Info.PathLengthCm,
				Info.bSkipped ? TEXT("true") : TEXT("false"),
				GPResourceApproach::DistanceModeToString(DistanceMode));
		});

#if !UE_BUILD_SHIPPING
	DebugLastApproachCandidateCount = Eval.CandidateCount;
#endif

	if (!Eval.bReachable)
	{
		if (Eval.RejectReason == EGP_ResourceCandidateRejectReason::ApproachGeometryFailed
			&& ClearanceHalfXY + Safety + AcceptanceRadius >= InteractionRangeCm - 1.0f)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution HaulApproachConfigFailure: Unit=%s Serial=%u Target=%s ClearanceHalfXY=%.1f DropOffRange=%.1f Safety=%.1f Acc=%.1f DistanceMode=%s — NavigationObstacle nearly fills interaction range"),
				*GetNameSafe(Owner),
				LogSerial,
				*GetNameSafe(Target),
				ClearanceHalfXY,
				InteractionRangeCm,
				Safety,
				AcceptanceRadius,
				GPResourceApproach::DistanceModeToString(DistanceMode));
		}

		if (bHaulLog)
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution HaulApproachNoReachableCandidate: Unit=%s Serial=%u MainBase=%s CandidateCount=%d Reason=%s DistanceMode=%s WorkerLocation=%s MainBaseLocation=%s DeltaZ=%.1f Distance2D=%.1f ClearanceHalfXY=%.1f DropOffRange=%.1f Acceptance=%.1f Safety=%.1f DesiredHorizontal=%.1f MaxHorizontalBudget=%.1f"),
				*GetNameSafe(Owner),
				LogSerial,
				*GetNameSafe(Target),
				Eval.CandidateCount,
				GPResourceApproach::RejectReasonToString(Eval.RejectReason),
				GPResourceApproach::DistanceModeToString(DistanceMode),
				*Params.PathStart.ToCompactString(),
				*TargetLocation.ToCompactString(),
				Eval.DeltaZCm,
				Eval.Distance2DCm,
				ClearanceHalfXY,
				InteractionRangeCm,
				AcceptanceRadius,
				Safety,
				Eval.DesiredHorizontalCm,
				Eval.MaxHorizontalBudgetCm);
		}

		// No nav: fall back to legacy single radial (may still Reject at RequestMove).
		// ApproachGeometryFailed must NOT fall back — that was the operator PathRejected loop.
		if (Eval.RejectReason == EGP_ResourceCandidateRejectReason::NoNavSystem
			|| Eval.RejectReason == EGP_ResourceCandidateRejectReason::PathStartProjectionFailed)
		{
			return TryMakeRangeApproachDestination(
				Owner,
				Target,
				InteractionRangeCm,
				AcceptanceRadius,
				ExtraInwardMarginCm,
				OutDestination,
				OutDesiredHorizontalDistance,
				OutPredictedWorstCaseDistance);
		}
		return false;
	}

	OutDestination = Eval.BestApproachLocation;
	OutDesiredHorizontalDistance = Eval.DesiredHorizontalCm;
	if (DistanceMode == EGP_RangeApproachDistanceMode::GroundPlane2D)
	{
		OutPredictedWorstCaseDistance = Eval.DesiredHorizontalCm + AcceptanceRadius;
	}
	else
	{
		const float DeltaZ = Owner->GetActorLocation().Z - TargetLocation.Z;
		OutPredictedWorstCaseDistance = FMath::Sqrt(
			FMath::Square(Eval.DesiredHorizontalCm + AcceptanceRadius) + FMath::Square(DeltaZ));
	}
	if (OutPathLengthCm != nullptr)
	{
		*OutPathLengthCm = Eval.PathLengthCm;
	}
	if (OutCandidateIndex != nullptr)
	{
		*OutCandidateIndex = Eval.BestCandidateIndex;
	}
#if !UE_BUILD_SHIPPING
	DebugLastApproachCandidateIndex = Eval.BestCandidateIndex;
#endif

	if (bHaulLog)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution HaulApproachSelected: Unit=%s Serial=%u CandidateIndex=%d Destination=%s PathLength=%.1f MainBase=%s DropOffRange=%.1f ClearanceHalfXY=%.1f DistanceMode=%s DeltaZ=%.1f Distance2D=%.1f DesiredHorizontal=%.1f CandidateCount=%d"),
			*GetNameSafe(Owner),
			LogSerial,
			Eval.BestCandidateIndex,
			*OutDestination.ToCompactString(),
			Eval.PathLengthCm,
			*GetNameSafe(Target),
			InteractionRangeCm,
			ClearanceHalfXY,
			GPResourceApproach::DistanceModeToString(DistanceMode),
			Eval.DeltaZCm,
			Eval.Distance2DCm,
			Eval.DesiredHorizontalCm,
			Eval.CandidateCount);
	}

	return OutPredictedWorstCaseDistance < InteractionRangeCm;
}

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
	return TryFindReachableRangeApproachDestination(
		Owner,
		Node,
		InteractionRangeCm,
		AcceptanceRadius,
		ExtraInwardMarginCm,
		0u,
		OutDestination,
		OutDesiredHorizontalDistance,
		OutPredictedWorstCaseDistance);
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
			ResolveApproachSafetyMarginCm(),
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
	LastMineDepositForHaul = Node;

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
		ResolveApproachSafetyMarginCm(),
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

void UGP_UnitCommandComponent::ResetHaulExecutor()
{
	if (bFinishingHaul)
	{
		return;
	}

	TGuardValue<bool> Guard(bFinishingHaul, true);
	ClearDropOffSubscriptionsAndTimer();
	bDropOffResumeScheduled = false;
	PendingDropOffResumeSerial = 0;
	PendingDropOffResumeDeposit.Reset();
	bPendingDropOffResumeReturnToDeposit = false;
	HaulState = EGP_HaulExecutionState::Idle;
	ActiveHaulSerial = 0;
	LastHaulDeposit.Reset();
	HaulMainBase.Reset();
	bShouldReturnToDepositAfterHaul = false;
	HaulApproachDestination = FVector::ZeroVector;
	HaulApproachDesiredDistance = -1.0f;
	HaulPredictedWorstCaseDistance = -1.0f;
	HaulLastArrivalDistance = -1.0f;
	HaulLastArrivalRangeError = -1.0f;
	HaulApproachAttempt = 0;
	HaulDropOffRangeCm = 400.0f;
	LastDropOffWaitReason = NAME_None;
	LastDropOffRetryLogReason = NAME_None;
#if !UE_BUILD_SHIPPING
	bDebugForceNextHaulArrivalOutOfRange = false;
	bDebugForceNextHaulApproachReject = false;
	DebugDropOffWakeCount = 0;
#endif
}

void UGP_UnitCommandComponent::ResetHaulExecutorForReplacement(
	const TOptional<FGP_StoredUnitCommand>& PreviousCommand)
{
	if (HaulState == EGP_HaulExecutionState::Idle && ActiveHaulSerial == 0)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution HaulCancelled: Unit=%s HaulSerial=%u Deposit=%s MainBase=%s Reason=CommandReplaced PreviousState=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveHaulSerial,
		*GetNameSafe(LastHaulDeposit.Get()),
		*GetNameSafe(HaulMainBase.Get()),
		HaulStateToString(HaulState),
		GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
		GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));

	(void)PreviousCommand;
	ResetHaulExecutor();
}

void UGP_UnitCommandComponent::ResetMineExecutor()
{
	if (bFinishingMine)
	{
		return;
	}

	TGuardValue<bool> Guard(bFinishingMine, true);
	UnbindMiningStateEvents();
	UnbindResourceRegistryWake();

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
	LastMineDepositForHaul.Reset();
	MineApproachDestination = FVector::ZeroVector;
	MineApproachDesiredNodeDistance = -1.0f;
	MinePredictedWorstCaseDistance = -1.0f;
	MineLastArrivalDistance = -1.0f;
	MineLastArrivalRangeError = -1.0f;
	MineApproachAttempt = 0;
	ClearMineSearchAnchor();
#if !UE_BUILD_SHIPPING
	bDebugForceNextMineArrivalOutOfRange = false;
#endif

	ResetHaulExecutor();
}

void UGP_UnitCommandComponent::ResetMineExecutorForReplacement(
	const TOptional<FGP_StoredUnitCommand>& PreviousCommand)
{
	const bool bMineIdle = MineState == EGP_MineExecutionState::Idle && ActiveMineSerial == 0;
	const bool bHaulIdle = HaulState == EGP_HaulExecutionState::Idle && ActiveHaulSerial == 0;
	if (bMineIdle && bHaulIdle)
	{
		return;
	}

	const AActor* Owner = GetOwner();
	if (!bMineIdle)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution MineCancelled: Unit=%s MineSerial=%u Target=%s Reason=CommandReplaced PreviousState=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ActiveMineSerial,
			*GetNameSafe(MineTarget.Get()),
			MineStateToString(MineState),
			GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
	}
	if (!bHaulIdle)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution HaulCancelled: Unit=%s HaulSerial=%u Deposit=%s MainBase=%s Reason=CommandReplaced PreviousState=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ActiveHaulSerial,
			*GetNameSafe(LastHaulDeposit.Get()),
			*GetNameSafe(HaulMainBase.Get()),
			HaulStateToString(HaulState),
			GPUnitCommandStatePrivate::RoleToString(Owner != nullptr ? Owner->GetLocalRole() : ROLE_None),
			GPUnitCommandStatePrivate::NetModeToString(GPUnitCommandStatePrivate::GetOwnerNetMode(Owner)));
	}

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

	// Same deposit while hauling: accept, do not restart a second route.
	if (IsActiveHaulChainForDeposit(Node)
		&& HeldCommand.IsSet()
		&& HeldCommand.GetValue().CommandTag == MineTag
		&& HeldCommand.GetValue().TargetActor.Get() == Node)
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

	if (Cargo->IsFull() && !IsActiveHaulChainForDeposit(Node))
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution MineRejected: Unit=%s Reason=CargoFull Role=%s NetMode=%s"),
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

	// Iterative/return-based control — nested re-entry must not restart the chain.
	if (bBeginMiningAtHeldTargetInProgress)
	{
		return;
	}
	TGuardValue<bool> ReentryGuard(bBeginMiningAtHeldTargetInProgress, true);
#if !UE_BUILD_SHIPPING
	DebugResetFifoWatchdogCounters();
#endif

	// Post-haul return-to-deposit ends when mining execution begins.
	if (HaulState == EGP_HaulExecutionState::ReturningToDeposit)
	{
		HaulState = EGP_HaulExecutionState::Idle;
		ActiveHaulSerial = 0;
		bShouldReturnToDepositAfterHaul = false;
		HaulMainBase.Reset();
	}

	const FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
	if (!(Held.CommandTag == FGPGameplayTags::Get().Command_Mine) || Held.CommandSerial != MineSerial)
	{
		return;
	}

	AGP_ResourceNode* Node = Cast<AGP_ResourceNode>(Held.TargetActor.Get());
	UGP_MiningComponent* Mining = Worker->GetMiningComponent();
	UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
	if (!IsValid(Mining) || !IsValid(Cargo))
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
	if (Cargo->IsFull())
	{
		// Arrival with full cargo must haul — never clear Held and idle.
		ActiveMineSerial = MineSerial;
		MineTarget = IsValid(Node) ? Node : MineTarget;
		StartHaulReturnToBase(
			MineSerial,
			IsValid(Node) ? Node : MineTarget.Get(),
			IsValid(Node)
				&& !Node->IsDepleted()
				&& !Node->HasCompletedDepletionTransition()
				&& !Node->IsDestroyPending());
		return;
	}
	if (!IsValid(Node) || Node->IsDepleted() || Node->HasCompletedDepletionTransition() || Node->IsDestroyPending())
	{
		// Partial cargo must haul before PostDepletion reassignment / WaitingForResource.
		if (Cargo->GetCurrentCargoAmount() > KINDA_SMALL_NUMBER)
		{
			ActiveMineSerial = MineSerial;
			StartHaulReturnToBase(MineSerial, Node, false);
			return;
		}
#if !UE_BUILD_SHIPPING
		++DebugReassignmentAttemptsThisTransition;
#endif
		if (TryAutoReassignMine(MineSerial, Node, true, FName(TEXT("PostDepletion"))))
		{
			return;
		}
		EnterWaitingForResource(MineSerial);
		return;
	}

	// Already registered on this node — stable Mining / WaitingForSlot; wait for occupancy events.
	if (Mining->GetCurrentResourceNode() == Node
		&& (Mining->IsWaitingForSlot() || Mining->IsMining()
			|| Mining->GetMiningState() == EGP_MiningState::WaitingForSlot
			|| Mining->GetMiningState() == EGP_MiningState::Mining))
	{
		MineTarget = Node;
		LastMineDepositForHaul = Node;
		MineState = EGP_MineExecutionState::Active;
		ActiveMineSerial = MineSerial;
		BindMiningStateEvents(Mining);
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
			if (RequestMineApproachMove(Owner, Node, MineSerial, ResolveApproachSafetyMarginCm(), TEXT("Corrective")))
			{
				return;
			}
		}

		ClearHeldCommand();
		ResetMineExecutor();
		return;
	}

	MineTarget = Node;
	LastMineDepositForHaul = Node;
	MineState = EGP_MineExecutionState::Active;
	ActiveMineSerial = MineSerial;
	// Successful re-entry into mining ends the haul leg of the chain.
	if (HaulState == EGP_HaulExecutionState::ReturningToDeposit
		|| HaulState == EGP_HaulExecutionState::DroppingOff)
	{
		HaulState = EGP_HaulExecutionState::Idle;
		ActiveHaulSerial = 0;
		HaulMainBase.Reset();
		bShouldReturnToDepositAfterHaul = false;
	}

	const EGP_MiningState MiningStateBefore = Mining->GetMiningState();

	// Stop prior node occupancy while unbound so BeginMining's internal ManualStop→Idle
	// cannot clear ActiveMineSerial via HandleMiningStateChanged.
	if (IsValid(Mining)
		&& (Mining->IsMining() || Mining->IsWaitingForSlot())
		&& Mining->GetCurrentResourceNode() != Node)
	{
		UnbindMiningStateEvents();
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
	}

	BindMiningStateEvents(Mining);

	// Alternative free-slot search BEFORE entering FIFO on a full target (preserves 5th→NodeB).
	const bool bTargetFull = Node->GetActiveMinerCount() >= Node->GetMaxConcurrentMiners();
	if (bTargetFull)
	{
#if !UE_BUILD_SHIPPING
		++DebugReassignmentAttemptsThisTransition;
#endif
		if (AGP_ResourceNode* FreeAlt = FindAutoResourceCandidate(
				Worker, Node, false, FName(TEXT("SlotFullAlternative"))))
		{
			if (FreeAlt != Node
				&& FreeAlt->GetActiveMinerCount() < FreeAlt->GetMaxConcurrentMiners())
			{
				if (TryRetargetMineToNode(FreeAlt, MineSerial, true))
				{
					return;
				}
			}
		}
		// Retarget may have mutated Held/MineTarget; never BeginMining a stale local Node pointer.
		if (HeldCommand.IsSet())
		{
			if (AGP_ResourceNode* HeldNode = Cast<AGP_ResourceNode>(HeldCommand.GetValue().TargetActor.Get()))
			{
				Node = HeldNode;
			}
		}
		// No free alternative — fall through to BeginMining → WaitingForSlot (stable FIFO).
	}

	const EGP_BeginMiningResult BeginResult = Mining->BeginMining(Node);
#if !UE_BUILD_SHIPPING
	++DebugMineBeginCallsThisTransition;
#endif
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution BeginMiningAtHeldTarget: Unit=%s MineSerial=%u HeldTarget=%s MineTarget=%s BoundMining=%s MiningStateBefore=%d MiningStateAfter=%d BeginResult=%d Distance=%.1f Range=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		MineSerial,
		*GetNameSafe(Node),
		*GetNameSafe(MineTarget.Get()),
		BoundMiningComponent.IsValid() ? TEXT("true") : TEXT("false"),
		static_cast<int32>(MiningStateBefore),
		static_cast<int32>(Mining->GetMiningState()),
		static_cast<int32>(BeginResult),
		Distance,
		InteractionRange,
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	if (BeginResult == EGP_BeginMiningResult::WaitingForSlot
		|| BeginResult == EGP_BeginMiningResult::AlreadyMiningTarget
		|| BeginResult == EGP_BeginMiningResult::Started)
	{
		// Reaffirm ownership after BeginMining remine Idle / retarget paths.
		MineTarget = Node;
		LastMineDepositForHaul = Node;
		MineState = EGP_MineExecutionState::Active;
		ActiveMineSerial = MineSerial;
		BindMiningStateEvents(Mining);
		if (BeginResult == EGP_BeginMiningResult::WaitingForSlot)
		{
			const int32 WaitIndex = Node->FindWaitingMinerIndex(Owner);
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution MineWaitingForSlot: Unit=%s MineSerial=%u Node=%s Position=%d Active=%d Waiting=%d Max=%d"),
				*GetNameSafe(Owner),
				MineSerial,
				*GetNameSafe(Node),
				WaitIndex >= 0 ? WaitIndex + 1 : -1,
				Node->GetActiveMinerCount(),
				Node->GetWaitingMinerCount(),
				Node->GetMaxConcurrentMiners());
		}
		return;
	}

	if (BeginResult == EGP_BeginMiningResult::RejectedDepleted
		|| BeginResult == EGP_BeginMiningResult::RejectedInvalidNode)
	{
		if (Cargo->GetCurrentCargoAmount() > KINDA_SMALL_NUMBER)
		{
			StartHaulReturnToBase(MineSerial, Node, false);
			return;
		}
#if !UE_BUILD_SHIPPING
		++DebugReassignmentAttemptsThisTransition;
#endif
		if (TryAutoReassignMine(MineSerial, Node, true, FName(TEXT("PostDepletion"))))
		{
			return;
		}
		EnterWaitingForResource(MineSerial);
		return;
	}

	if (BeginResult == EGP_BeginMiningResult::RejectedCargoFull)
	{
		StartHaulReturnToBase(
			MineSerial,
			Node,
			IsValid(Node)
				&& !Node->IsDepleted()
				&& !Node->HasCompletedDepletionTransition()
				&& !Node->IsDestroyPending());
		return;
	}

	if (BeginResult != EGP_BeginMiningResult::Started)
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
	LastMineDepositForHaul = Node;
	MineApproachAttempt = 0;
	// Persistent cluster SearchCenter for the whole Mine/haul/reassignment intent.
	SetMineSearchAnchorFromNode(Node);

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
	UGP_MovementComponent* Movement = ResolveMovementComponent();

	// Same pattern as Attack: RequestMineApproachMove replace must not tear down the Mine chain.
	if (Result == EGP_MovementResult::Cancelled && Reason == EGP_MovementResultReason::Superseded)
	{
		if (Movement != nullptr
			&& Movement->IsMoving()
			&& Movement->GetActiveMoveSerial() == ActiveMineSerial)
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution MineApproachResultIgnored: Unit=%s MineSerial=%u IgnoreReason=SelfSupersede Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				Serial,
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
			return true;
		}
	}

	if (Result == EGP_MovementResult::Cancelled || Result == EGP_MovementResult::Failed)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution MineApproachCancelled: Unit=%s MineSerial=%u MovementResult=%s MovementReason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			Serial,
			GPUnitCommandStatePrivate::MovementResultToString(Result),
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

void UGP_UnitCommandComponent::StartHaulReturnToBase(
	uint32 ChainSerial,
	AGP_ResourceNode* Deposit,
	bool bReturnToDeposit)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;
	AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	if (Worker == nullptr || ChainSerial == 0)
	{
		FinishHaulChain(true);
		return;
	}

	ActiveMineSerial = ChainSerial;
	ActiveHaulSerial = ChainSerial;
	LastHaulDeposit = Deposit;
	bShouldReturnToDepositAfterHaul = bReturnToDeposit;
	HaulApproachAttempt = 0;
	LastHaulAcceptedAmount = 0.0f;
	LastHaulRejectedAmount = 0.0f;
	LastHaulThreatDelta = 0.0f;

	UWorld* World = Owner->GetWorld();
	AGP_GameState* GameState = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	AGP_MainBase* MainBase = GameState != nullptr
		? GameState->FindMainBaseForTeam(Worker->GetTeamId())
		: nullptr;
	UGP_StorageComponent* Storage = IsValid(MainBase) ? MainBase->GetStorageComponent() : nullptr;
	if (!IsValid(MainBase) || !IsValid(Storage))
	{
		if (WorkerHasHaulCargo())
		{
			EnterWaitingForDropOff(FName(TEXT("MissingMainBase")));
			return;
		}

		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution HaulFailed: Unit=%s HaulSerial=%u Reason=MissingMainBaseOrStorage TeamId=%d Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ChainSerial,
			Worker->GetTeamId(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		HaulState = EGP_HaulExecutionState::Failed;
		FinishHaulChain(true);
		return;
	}

	HaulMainBase = MainBase;
	HaulDropOffRangeCm = MainBase->GetDropOffRangeCm();

	const float Distance = MainBase->ComputeDropOffDistance2D(Owner->GetActorLocation());
	const float DeltaZ = Owner->GetActorLocation().Z - MainBase->GetActorLocation().Z;
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution HaulReturnToBase: Unit=%s HaulSerial=%u Deposit=%s MainBase=%s Distance2D=%.1f DeltaZ=%.1f DropOffRange=%.1f DistanceMode=GroundPlane2D ReturnToDeposit=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ChainSerial,
		*GetNameSafe(Deposit),
		*GetNameSafe(MainBase),
		Distance,
		DeltaZ,
		HaulDropOffRangeCm,
		bReturnToDeposit ? TEXT("true") : TEXT("false"),
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	if (MainBase->IsWithinDropOffRange2D(Owner->GetActorLocation()))
	{
		if (UGP_MovementComponent* Movement = ResolveMovementComponent())
		{
			if (Movement->IsMoving())
			{
				Movement->StopMove(EGP_MovementStopReason::CommandReplaced);
			}
		}
		UnbindDropOffWaitingRegisterWake();
		ClearDropOffRetryTimer();
		HaulState = EGP_HaulExecutionState::DroppingOff;
		HaulMainBase = MainBase;
		BindActiveHaulMainBaseUnregister();
		BeginDropOffAtMainBase(ChainSerial);
		return;
	}

	if (!RequestHaulApproachMove(Owner, MainBase, ChainSerial, 0.0f, TEXT("Primary")))
	{
		if (WorkerHasHaulCargo())
		{
			EnterWaitingForDropOff(FName(TEXT("PathRejected")));
			return;
		}
		HaulState = EGP_HaulExecutionState::Failed;
		FinishHaulChain(true);
	}
}

bool UGP_UnitCommandComponent::RequestHaulApproachMove(
	AActor* Owner,
	AGP_MainBase* MainBase,
	uint32 HaulSerial,
	float ExtraInwardMarginCm,
	const TCHAR* LogLabel)
{
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;
#if !UE_BUILD_SHIPPING
	if (bDebugForceNextHaulApproachReject)
	{
		bDebugForceNextHaulApproachReject = false;
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution HaulApproachForcedReject: Unit=%s HaulSerial=%u Label=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HaulSerial,
			LogLabel,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return false;
	}
#endif
	UGP_MovementComponent* Movement = ResolveMovementComponent();
	if (Movement == nullptr || !IsValid(MainBase))
	{
		return false;
	}

	const float DropOffRange = MainBase->GetDropOffRangeCm();
	const float AcceptanceRadius = Movement->AcceptanceRadius;
	FVector Destination = FVector::ZeroVector;
	float DesiredHorizontal = -1.0f;
	float PredictedWorst = -1.0f;
	float PathLength = -1.0f;
	int32 CandidateIndex = -1;
	if (!TryFindReachableRangeApproachDestination(
		Owner,
		MainBase,
		DropOffRange,
		AcceptanceRadius,
		ExtraInwardMarginCm,
		HaulSerial,
		Destination,
		DesiredHorizontal,
		PredictedWorst,
		&PathLength,
		&CandidateIndex))
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution HaulApproachGeometryFailed: Unit=%s HaulSerial=%u MainBase=%s Label=%s Range=%.1f Acc=%.1f Safety=%.1f ExtraInward=%.1f Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HaulSerial,
			*GetNameSafe(MainBase),
			LogLabel,
			DropOffRange,
			AcceptanceRadius,
			ResolveApproachSafetyMarginCm(),
			ExtraInwardMarginCm,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return false;
	}

	const FGP_MovementRequestOutcome Outcome = Movement->RequestMove(Destination, HaulSerial);
	if (!Outcome.IsAccepted())
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution HaulApproachMoveRejected: Unit=%s HaulSerial=%u Label=%s CandidateIndex=%d Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HaulSerial,
			LogLabel,
			CandidateIndex,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		return false;
	}

	HaulApproachDestination = Destination;
	HaulApproachDesiredDistance = DesiredHorizontal;
	HaulPredictedWorstCaseDistance = PredictedWorst;
	HaulDropOffRangeCm = DropOffRange;
	HaulState = EGP_HaulExecutionState::ReturningToBase;
	ActiveHaulSerial = HaulSerial;
	ActiveMineSerial = HaulSerial;
	HaulMainBase = MainBase;
	UnbindDropOffWaitingRegisterWake();
	ClearDropOffRetryTimer();
	BindActiveHaulMainBaseUnregister();

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution HaulApproachRequested: Unit=%s HaulSerial=%u MainBase=%s Label=%s Attempt=%d CandidateIndex=%d Destination=%s PathLength=%.1f DesiredHoriz=%.1f PredictedWorst=%.1f Range=%.1f Acc=%.1f Safety=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		HaulSerial,
		*GetNameSafe(MainBase),
		LogLabel,
		HaulApproachAttempt,
		CandidateIndex,
		*Destination.ToCompactString(),
		PathLength,
		DesiredHorizontal,
		PredictedWorst,
		DropOffRange,
		AcceptanceRadius,
		ResolveApproachSafetyMarginCm(),
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));
	return true;
}

void UGP_UnitCommandComponent::BeginDropOffAtMainBase(uint32 HaulSerial)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;
	AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	if (Worker == nullptr || HaulSerial == 0 || ActiveHaulSerial != HaulSerial)
	{
		return;
	}

	HaulState = EGP_HaulExecutionState::DroppingOff;
	BindActiveHaulMainBaseUnregister();

	AGP_MainBase* MainBase = HaulMainBase.Get();
	UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
	UGP_StorageComponent* Storage = IsValid(MainBase) ? MainBase->GetStorageComponent() : nullptr;
	if (!IsValid(MainBase) || !IsValid(Storage) || !IsValid(Cargo))
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution HaulDropOffFailed: Unit=%s HaulSerial=%u Reason=MissingActors Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HaulSerial,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		if (WorkerHasHaulCargo())
		{
			EnterWaitingForDropOff(FName(TEXT("InvalidBeforeDropOff")));
			return;
		}
		HaulState = EGP_HaulExecutionState::Failed;
		FinishHaulChain(true);
		return;
	}

	if (Worker->GetTeamId() != MainBase->GetTeamId() || Worker->GetTeamId() < 1)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution HaulDropOffFailed: Unit=%s HaulSerial=%u Reason=TeamMismatch WorkerTeam=%d BaseTeam=%d Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HaulSerial,
			Worker->GetTeamId(),
			MainBase->GetTeamId(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		HaulState = EGP_HaulExecutionState::Failed;
		FinishHaulChain(true);
		return;
	}

	float Distance = MainBase->ComputeDropOffDistance2D(Owner->GetActorLocation());
#if !UE_BUILD_SHIPPING
	if (bDebugForceNextHaulArrivalOutOfRange)
	{
		bDebugForceNextHaulArrivalOutOfRange = false;
		Distance = MainBase->GetDropOffRangeCm() + 5.0f;
	}
#endif
	HaulLastArrivalDistance = Distance;
	HaulLastArrivalRangeError = Distance - MainBase->GetDropOffRangeCm();
	HaulDropOffRangeCm = MainBase->GetDropOffRangeCm();

	if (Distance > HaulDropOffRangeCm)
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution HaulArrivalOutOfRange: Unit=%s HaulSerial=%u MainBase=%s Distance2D=%.1f Range=%.1f DeltaZ=%.1f DistanceMode=GroundPlane2D Attempt=%d Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HaulSerial,
			*GetNameSafe(MainBase),
			Distance,
			HaulDropOffRangeCm,
			Owner->GetActorLocation().Z - MainBase->GetActorLocation().Z,
			HaulApproachAttempt,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));

		if (HaulApproachAttempt < 1)
		{
			++HaulApproachAttempt;
			if (RequestHaulApproachMove(Owner, MainBase, HaulSerial, ResolveApproachSafetyMarginCm(), TEXT("Corrective")))
			{
				return;
			}
		}

		if (WorkerHasHaulCargo())
		{
			EnterWaitingForDropOff(FName(TEXT("PathRejected")));
			return;
		}
		HaulState = EGP_HaulExecutionState::Failed;
		FinishHaulChain(true);
		return;
	}

	const float CargoAmount = Cargo->GetCurrentCargoAmount();
	if (!(CargoAmount > KINDA_SMALL_NUMBER))
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution HaulDropOffEmptyCargo: Unit=%s HaulSerial=%u Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HaulSerial,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		ClearDropOffSubscriptionsAndTimer();
		FinishHaulChain(true);
		return;
	}

	const FGP_StorageAddResult AddResult = Storage->AddPlanetaryFerronite(CargoAmount);
	LastHaulAcceptedAmount = AddResult.Accepted;
	LastHaulRejectedAmount = AddResult.Rejected;

	if (AddResult.Accepted > KINDA_SMALL_NUMBER)
	{
		const float Removed = Cargo->RemoveCargo(AddResult.Accepted);
		if (!FMath::IsNearlyEqual(Removed, AddResult.Accepted, 0.01f))
		{
			UE_LOG(LogGPUnitCommandExecution, Error,
				TEXT("GP UnitCommandExecution HaulInvariantFailure: Unit=%s HaulSerial=%u Accepted=%.3f Removed=%.3f — rolling back storage"),
				*GetNameSafe(Owner),
				HaulSerial,
				AddResult.Accepted,
				Removed);
			Storage->RemovePlanetaryFerronite(AddResult.Accepted);
			HaulState = EGP_HaulExecutionState::Failed;
			FinishHaulChain(true);
			return;
		}

		const float ThreatPerUnit = Storage->GetThreatPerStoredUnit();
		LastHaulThreatDelta = AddResult.Accepted * ThreatPerUnit;
		if (UWorld* World = Owner->GetWorld())
		{
			if (AGP_GameState* GameState = World->GetGameState<AGP_GameState>())
			{
				GameState->AddFerroniteThreatValueForTeam(Worker->GetTeamId(), LastHaulThreatDelta);
			}
		}
	}

	const float RemainingCargo = Cargo->GetCurrentCargoAmount();
	if (RemainingCargo > KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution HaulDropOffWaitingForSpace: Unit=%s HaulSerial=%u Accepted=%.3f Rejected=%.3f RemainingCargo=%.3f StorageFull=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			HaulSerial,
			LastHaulAcceptedAmount,
			LastHaulRejectedAmount,
			RemainingCargo,
			AddResult.bStorageFullAfter ? TEXT("true") : TEXT("false"),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		EnterWaitingForDropOff(FName(TEXT("StorageFull")));
		return;
	}

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution HaulDropOffComplete: Unit=%s HaulSerial=%u Accepted=%.3f Rejected=%.3f ThreatDelta=%.3f ThreatPerUnit=%.3f ReturnToDeposit=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		HaulSerial,
		LastHaulAcceptedAmount,
		LastHaulRejectedAmount,
		LastHaulThreatDelta,
		Storage->GetThreatPerStoredUnit(),
		bShouldReturnToDepositAfterHaul ? TEXT("true") : TEXT("false"),
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	ClearDropOffSubscriptionsAndTimer();
	ContinueMineAfterSuccessfulHaul(HaulSerial);
}

void UGP_UnitCommandComponent::ContinueMineAfterSuccessfulHaul(uint32 ChainSerial)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	// Cargo-first invariant: never resume mining while haul cargo remains.
	if (WorkerHasHaulCargo())
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution HaulContinueMineBlockedByCargo: Unit=%s HaulSerial=%u Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ChainSerial,
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
		EnterWaitingForDropOff(FName(TEXT("StorageFull")));
		return;
	}

	const bool bHeldMineSameSerial =
		HeldCommand.IsSet()
		&& HeldCommand.GetValue().CommandTag == FGPGameplayTags::Get().Command_Mine
		&& HeldCommand.GetValue().CommandSerial == ChainSerial;

	AGP_ResourceNode* Deposit = LastHaulDeposit.Get();
	const bool bDepositOk =
		IsValid(Deposit)
		&& !Deposit->IsDepleted()
		&& !Deposit->HasCompletedDepletionTransition()
		&& !Deposit->IsDestroyPending()
		&& !Deposit->IsActorBeingDestroyed();

	if (bShouldReturnToDepositAfterHaul && bDepositOk && bHeldMineSameSerial)
	{
		HaulState = EGP_HaulExecutionState::ReturningToDeposit;
		HaulMainBase.Reset();
		HaulApproachAttempt = 0;
		ActiveHaulSerial = ChainSerial;
		ActiveMineSerial = ChainSerial;
		MineTarget = Deposit;
		LastMineDepositForHaul = Deposit;
		MineApproachAttempt = 0;

		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution HaulReturnToDeposit: Unit=%s ChainSerial=%u Deposit=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ChainSerial,
			*GetNameSafe(Deposit),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));

		AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
		UGP_MiningComponent* Mining = Worker != nullptr ? Worker->GetMiningComponent() : nullptr;
		const float InteractionRange = ResolveMineInteractionRangeCm(Mining, Deposit);
		const float Distance = FVector::Dist(Owner->GetActorLocation(), Deposit->GetActorLocation());
		if (Distance <= InteractionRange)
		{
			// Haul phase ends when mining resumes; keep haul diagnostics fields.
			HaulState = EGP_HaulExecutionState::Idle;
			ActiveHaulSerial = 0;
			bShouldReturnToDepositAfterHaul = false;
			BeginMiningAtHeldTarget(ChainSerial);
			return;
		}

		if (!RequestMineApproachMove(Owner, Deposit, ChainSerial, 0.0f, TEXT("PostHaul")))
		{
			FinishHaulChain(true);
		}
		return;
	}

	// Post-haul: previous deposit gone — search alternative before clearing Mine intent.
	if (bHeldMineSameSerial)
	{
		HaulState = EGP_HaulExecutionState::Idle;
		ActiveHaulSerial = 0;
		bShouldReturnToDepositAfterHaul = false;
		HaulMainBase.Reset();
		ActiveMineSerial = ChainSerial;
		if (TryAutoReassignMine(ChainSerial, Deposit, true, FName(TEXT("PostDropOff"))))
		{
			return;
		}
		EnterWaitingForResource(ChainSerial);
		return;
	}

	FinishHaulChain(true);
}

void UGP_UnitCommandComponent::FinishHaulChain(bool bClearHeld)
{
	if (bFinishingHaul)
	{
		return;
	}

	TGuardValue<bool> Guard(bFinishingHaul, true);
	AActor* Owner = GetOwner();
	const uint32 Serial = ActiveHaulSerial != 0 ? ActiveHaulSerial : ActiveMineSerial;
	const EGP_HaulExecutionState Previous = HaulState;

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution HaulFinished: Unit=%s HaulSerial=%u PreviousState=%s ClearHeld=%s Accepted=%.3f Rejected=%.3f ThreatDelta=%.3f"),
		*GetNameSafe(Owner),
		Serial,
		HaulStateToString(Previous),
		bClearHeld ? TEXT("true") : TEXT("false"),
		LastHaulAcceptedAmount,
		LastHaulRejectedAmount,
		LastHaulThreatDelta);

	ClearDropOffSubscriptionsAndTimer();
	HaulState = EGP_HaulExecutionState::Idle;
	ActiveHaulSerial = 0;
	LastHaulDeposit.Reset();
	HaulMainBase.Reset();
	bShouldReturnToDepositAfterHaul = false;
	HaulApproachDestination = FVector::ZeroVector;
	HaulApproachDesiredDistance = -1.0f;
	HaulPredictedWorstCaseDistance = -1.0f;
	HaulLastArrivalDistance = -1.0f;
	HaulLastArrivalRangeError = -1.0f;
	HaulApproachAttempt = 0;

	if (bClearHeld
		&& Serial != 0
		&& HeldCommand.IsSet()
		&& HeldCommand.GetValue().CommandTag == FGPGameplayTags::Get().Command_Mine
		&& HeldCommand.GetValue().CommandSerial == Serial)
	{
		ClearHeldCommand();
	}

	// Clear mine chain identity without recursing into ResetHaulExecutor.
	UnbindMiningStateEvents();
	MineState = EGP_MineExecutionState::Idle;
	ActiveMineSerial = 0;
	MineTarget.Reset();
	MineApproachAttempt = 0;
}

bool UGP_UnitCommandComponent::TryConsumeHaulMovementResult(
	uint32 Serial,
	EGP_MovementResult Result,
	EGP_MovementResultReason Reason)
{
	if (ActiveHaulSerial == 0 || Serial != ActiveHaulSerial)
	{
		return false;
	}

	if (HaulState != EGP_HaulExecutionState::ReturningToBase
		&& HaulState != EGP_HaulExecutionState::WaitingForDropOff)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	if (Result == EGP_MovementResult::Cancelled || Result == EGP_MovementResult::Failed)
	{
		UGP_MovementComponent* Movement = ResolveMovementComponent();
		if (Result == EGP_MovementResult::Cancelled
			&& Reason == EGP_MovementResultReason::Superseded
			&& Movement != nullptr
			&& Movement->IsMoving()
			&& Movement->GetActiveMoveSerial() == ActiveHaulSerial)
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution HaulApproachResultIgnored: Unit=%s HaulSerial=%u IgnoreReason=SelfSupersede Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				Serial,
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
			return true;
		}

		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution HaulApproachCancelled: Unit=%s HaulSerial=%u MovementResult=%s MovementReason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			Serial,
			GPUnitCommandStatePrivate::MovementResultToString(Result),
			GPUnitCommandStatePrivate::MovementResultReasonToString(Reason),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));

		if (Reason == EGP_MovementResultReason::CommandReplaced)
		{
			// Command replacement already resets via ResetMineExecutorForReplacement.
			return true;
		}

		// Manual/Failed with cargo → wait (destroyed target / path loss). Without cargo → clear.
		if (WorkerHasHaulCargo())
		{
			EnterWaitingForDropOff(FName(TEXT("MoveFailed")));
		}
		else
		{
			HaulState = EGP_HaulExecutionState::Failed;
			FinishHaulChain(true);
		}
		return true;
	}

	if (Result != EGP_MovementResult::Reached)
	{
		return true;
	}

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution HaulApproachReached: Unit=%s HaulSerial=%u Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		Serial,
		GPUnitCommandStatePrivate::RoleToString(Role),
		GPUnitCommandStatePrivate::NetModeToString(NetMode));

	BeginDropOffAtMainBase(Serial);
	return true;
}

bool UGP_UnitCommandComponent::WorkerHasHaulCargo() const
{
	const AGP_Worker* Worker = Cast<AGP_Worker>(GetOwner());
	const UGP_CargoComponent* Cargo = Worker != nullptr ? Worker->GetCargoComponent() : nullptr;
	return IsValid(Cargo) && Cargo->GetCurrentCargoAmount() > KINDA_SMALL_NUMBER;
}

bool UGP_UnitCommandComponent::TeamMainBaseHasStorageRoom() const
{
	const AGP_Worker* Worker = Cast<AGP_Worker>(GetOwner());
	if (!IsValid(Worker))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	const AGP_MainBase* Base = GS != nullptr ? GS->FindMainBaseForTeam(Worker->GetTeamId()) : nullptr;
	const UGP_StorageComponent* Storage = IsValid(Base) ? Base->GetStorageComponent() : nullptr;
	return IsValid(Storage) && Storage->GetTotalRemaining() > KINDA_SMALL_NUMBER;
}

void UGP_UnitCommandComponent::ClearDropOffSubscriptionsAndTimer()
{
	UnbindActiveHaulMainBaseUnregister();
	UnbindDropOffWaitingRegisterWake();
	UnbindDropOffWaitingStorageWake();
	ClearDropOffRetryTimer();
	bDropOffResumeScheduled = false;
	PendingDropOffResumeSerial = 0;
	PendingDropOffResumeDeposit.Reset();
	bPendingDropOffResumeReturnToDeposit = false;
}

void UGP_UnitCommandComponent::BindActiveHaulMainBaseUnregister()
{
	if (bMainBaseUnregisteredHaulBound)
	{
		return;
	}

	UWorld* World = GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	if (GS == nullptr)
	{
		return;
	}

	MainBaseUnregisteredHaulHandle = GS->OnMainBaseUnregistered.AddUObject(
		this, &UGP_UnitCommandComponent::HandleMainBaseUnregisteredActiveHaul);
	bMainBaseUnregisteredHaulBound = true;
}

void UGP_UnitCommandComponent::UnbindActiveHaulMainBaseUnregister()
{
	if (!bMainBaseUnregisteredHaulBound)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			GS->OnMainBaseUnregistered.Remove(MainBaseUnregisteredHaulHandle);
		}
	}
	MainBaseUnregisteredHaulHandle.Reset();
	bMainBaseUnregisteredHaulBound = false;
}

void UGP_UnitCommandComponent::BindDropOffWaitingRegisterWake()
{
	if (bMainBaseRegisteredDropOffBound)
	{
		return;
	}

	UWorld* World = GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	if (GS == nullptr)
	{
		return;
	}

	MainBaseRegisteredDropOffHandle = GS->OnMainBaseRegistered.AddUObject(
		this, &UGP_UnitCommandComponent::HandleMainBaseRegisteredDropOffWake);
	bMainBaseRegisteredDropOffBound = true;
}

void UGP_UnitCommandComponent::UnbindDropOffWaitingRegisterWake()
{
	if (!bMainBaseRegisteredDropOffBound)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			GS->OnMainBaseRegistered.Remove(MainBaseRegisteredDropOffHandle);
		}
	}
	MainBaseRegisteredDropOffHandle.Reset();
	bMainBaseRegisteredDropOffBound = false;
}

void UGP_UnitCommandComponent::BindDropOffWaitingStorageWake()
{
	const AGP_Worker* Worker = Cast<AGP_Worker>(GetOwner());
	if (!IsValid(Worker))
	{
		return;
	}

	UWorld* World = GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	AGP_MainBase* Base = GS != nullptr ? GS->FindMainBaseForTeam(Worker->GetTeamId()) : nullptr;
	UGP_StorageComponent* Storage = IsValid(Base) ? Base->GetStorageComponent() : nullptr;
	if (!IsValid(Storage))
	{
		return;
	}

	if (bDropOffStorageWakeBound && BoundDropOffWaitStorage.Get() == Storage)
	{
		return;
	}

	UnbindDropOffWaitingStorageWake();
	Storage->OnStorageChanged.AddDynamic(this, &UGP_UnitCommandComponent::HandleDropOffWaitingStorageChanged);
	BoundDropOffWaitStorage = Storage;
	bDropOffStorageWakeBound = true;
}

void UGP_UnitCommandComponent::UnbindDropOffWaitingStorageWake()
{
	if (!bDropOffStorageWakeBound)
	{
		BoundDropOffWaitStorage.Reset();
		return;
	}

	if (UGP_StorageComponent* Storage = BoundDropOffWaitStorage.Get())
	{
		Storage->OnStorageChanged.RemoveDynamic(
			this, &UGP_UnitCommandComponent::HandleDropOffWaitingStorageChanged);
	}
	BoundDropOffWaitStorage.Reset();
	bDropOffStorageWakeBound = false;
}

void UGP_UnitCommandComponent::ClearDropOffRetryTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DropOffRetryTimerHandle);
	}
}

void UGP_UnitCommandComponent::ArmDropOffRetryTimer()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	float RetrySeconds = 3.0f;
	if (const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get())
	{
		RetrySeconds = FMath::Max(0.1f, Settings->DropOffRetrySeconds);
	}

	World->GetTimerManager().ClearTimer(DropOffRetryTimerHandle);
	World->GetTimerManager().SetTimer(
		DropOffRetryTimerHandle,
		this,
		&UGP_UnitCommandComponent::HandleDropOffSafetyRetry,
		RetrySeconds,
		false);
}

void UGP_UnitCommandComponent::EnterWaitingForDropOff(FName Reason)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || ActiveHaulSerial == 0)
	{
		return;
	}

	if (!WorkerHasHaulCargo())
	{
		HaulState = EGP_HaulExecutionState::Failed;
		FinishHaulChain(true);
		return;
	}

	if (bEnteringDropOffWait)
	{
		return;
	}

	TGuardValue<bool> EnterGuard(bEnteringDropOffWait, true);

	if (UGP_MovementComponent* Movement = ResolveMovementComponent())
	{
		if (Movement->IsMoving())
		{
			Movement->StopMove(EGP_MovementStopReason::CommandReplaced);
		}
	}

	UnbindActiveHaulMainBaseUnregister();
	HaulMainBase.Reset();
	HaulState = EGP_HaulExecutionState::WaitingForDropOff;
	ActiveMineSerial = ActiveHaulSerial;

	const AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	const float CargoAmount = (Worker != nullptr && IsValid(Worker->GetCargoComponent()))
		? Worker->GetCargoComponent()->GetCurrentCargoAmount()
		: 0.0f;
	const int32 TeamId = Worker != nullptr ? Worker->GetTeamId() : 0;
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP DropOffWait Enter: Unit=%s HaulSerial=%u Reason=%s Cargo=%.1f Team=%d"),
		*GetNameSafe(Owner),
		ActiveHaulSerial,
		*Reason.ToString(),
		CargoAmount,
		TeamId);

	LastDropOffWaitReason = Reason;
	if (LastDropOffRetryLogReason != Reason)
	{
		LastDropOffRetryLogReason = NAME_None;
	}

	BindDropOffWaitingRegisterWake();
	BindDropOffWaitingStorageWake();
	ArmDropOffRetryTimer();
}

void UGP_UnitCommandComponent::HandleMainBaseUnregisteredActiveHaul(AGP_MainBase* MainBase)
{
	if (HaulState != EGP_HaulExecutionState::ReturningToBase
		&& HaulState != EGP_HaulExecutionState::DroppingOff)
	{
		return;
	}

	if (MainBase == nullptr || HaulMainBase.Get() != MainBase)
	{
		return;
	}

	EnterWaitingForDropOff(FName(TEXT("MainBaseDestroyed")));
}

void UGP_UnitCommandComponent::HandleMainBaseRegisteredDropOffWake(AGP_MainBase* MainBase)
{
	if (HaulState != EGP_HaulExecutionState::WaitingForDropOff || ActiveHaulSerial == 0)
	{
		return;
	}

	AGP_Worker* Worker = Cast<AGP_Worker>(GetOwner());
	if (!IsValid(Worker) || !IsValid(MainBase)
		|| MainBase->GetTeamId() != Worker->GetTeamId()
		|| MainBase->GetTeamId() < 1
		|| !IsValid(MainBase->GetStorageComponent())
		|| !WorkerHasHaulCargo())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	++DebugDropOffWakeCount;
#endif
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP DropOffWait Wake: Unit=%s HaulSerial=%u Reason=MainBaseRegistered Base=%s"),
		*GetNameSafe(GetOwner()),
		ActiveHaulSerial,
		*GetNameSafe(MainBase));

	TryResumeHaulFromDropOffWait(FName(TEXT("MainBaseRegistered")));
}

void UGP_UnitCommandComponent::HandleDropOffWaitingStorageChanged(
	float PreviousTotalStored,
	float NewTotalStored,
	float TotalCapacity)
{
	(void)PreviousTotalStored;
	if (HaulState != EGP_HaulExecutionState::WaitingForDropOff || ActiveHaulSerial == 0)
	{
		return;
	}

	if (!WorkerHasHaulCargo())
	{
		return;
	}

	const float Remaining = TotalCapacity - NewTotalStored;
	if (!(Remaining > KINDA_SMALL_NUMBER))
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	++DebugDropOffWakeCount;
#endif
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP DropOffWait Wake: Unit=%s HaulSerial=%u Reason=StorageSpaceAvailable Remaining=%.1f"),
		*GetNameSafe(GetOwner()),
		ActiveHaulSerial,
		Remaining);

	TryResumeHaulFromDropOffWait(FName(TEXT("StorageSpaceAvailable")));
}

void UGP_UnitCommandComponent::HandleDropOffSafetyRetry()
{
	if (HaulState != EGP_HaulExecutionState::WaitingForDropOff || ActiveHaulSerial == 0)
	{
		ClearDropOffRetryTimer();
		return;
	}

	AGP_Worker* Worker = Cast<AGP_Worker>(GetOwner());
	UWorld* World = GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	AGP_MainBase* Base = (GS != nullptr && Worker != nullptr)
		? GS->FindMainBaseForTeam(Worker->GetTeamId())
		: nullptr;

	if (!IsValid(Base) || !IsValid(Base->GetStorageComponent()) || !WorkerHasHaulCargo())
	{
		if (LastDropOffRetryLogReason != FName(TEXT("StillMissing")))
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP DropOffWait Retry: Unit=%s Result=StillMissing"),
				*GetNameSafe(GetOwner()));
			LastDropOffRetryLogReason = FName(TEXT("StillMissing"));
		}
		ArmDropOffRetryTimer();
		return;
	}

	if (LastDropOffWaitReason == FName(TEXT("StorageFull"))
		&& !(Base->GetStorageComponent()->GetTotalRemaining() > KINDA_SMALL_NUMBER))
	{
		if (LastDropOffRetryLogReason != FName(TEXT("StillFull")))
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP DropOffWait Retry: Unit=%s Result=StillFull Base=%s"),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Base));
			LastDropOffRetryLogReason = FName(TEXT("StillFull"));
		}
		BindDropOffWaitingStorageWake();
		ArmDropOffRetryTimer();
		return;
	}

	const FName RetryResult =
		(LastDropOffWaitReason == FName(TEXT("PathRejected"))
			|| LastDropOffWaitReason == FName(TEXT("MoveFailed"))
			|| LastDropOffRetryLogReason == FName(TEXT("AttemptResume")))
		? FName(TEXT("StillUnreachable"))
		: FName(TEXT("AttemptResume"));
	if (LastDropOffRetryLogReason != RetryResult)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP DropOffWait Retry: Unit=%s Result=%s Base=%s"),
			*GetNameSafe(GetOwner()),
			*RetryResult.ToString(),
			*GetNameSafe(Base));
		LastDropOffRetryLogReason = RetryResult;
	}
	TryResumeHaulFromDropOffWait(FName(TEXT("SafetyRetry")));
}

void UGP_UnitCommandComponent::TryResumeHaulFromDropOffWait(FName WakeReason)
{
	(void)WakeReason;
	if (bDropOffWakeInProgress || bDropOffResumeScheduled || ActiveHaulSerial == 0)
	{
		return;
	}

	if (HaulState != EGP_HaulExecutionState::WaitingForDropOff)
	{
		return;
	}

	// Capacity gate only for storage-full waits — do not block MainBase missing/unreachable recovery.
	if (LastDropOffWaitReason == FName(TEXT("StorageFull")) && !TeamMainBaseHasStorageRoom())
	{
		BindDropOffWaitingRegisterWake();
		BindDropOffWaitingStorageWake();
		ArmDropOffRetryTimer();
		return;
	}

	TGuardValue<bool> WakeGuard(bDropOffWakeInProgress, true);
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UnbindDropOffWaitingRegisterWake();
	UnbindDropOffWaitingStorageWake();
	ClearDropOffRetryTimer();

	PendingDropOffResumeSerial = ActiveHaulSerial;
	PendingDropOffResumeDeposit = LastHaulDeposit;
	bPendingDropOffResumeReturnToDeposit = bShouldReturnToDepositAfterHaul;
	bDropOffResumeScheduled = true;
	World->GetTimerManager().SetTimerForNextTick(
		this,
		&UGP_UnitCommandComponent::ExecuteScheduledDropOffHaulResume);
}

void UGP_UnitCommandComponent::ExecuteScheduledDropOffHaulResume()
{
	bDropOffResumeScheduled = false;
	const uint32 Serial = PendingDropOffResumeSerial;
	AGP_ResourceNode* Deposit = PendingDropOffResumeDeposit.Get();
	const bool bReturn = bPendingDropOffResumeReturnToDeposit;
	PendingDropOffResumeSerial = 0;
	PendingDropOffResumeDeposit.Reset();
	bPendingDropOffResumeReturnToDeposit = false;

	if (Serial == 0 || ActiveHaulSerial != Serial)
	{
		return;
	}

	if (HaulState != EGP_HaulExecutionState::WaitingForDropOff)
	{
		return;
	}

	StartHaulReturnToBase(Serial, Deposit, bReturn);
}

void UGP_UnitCommandComponent::SetMineSearchAnchorFromNode(const AGP_ResourceNode* Node)
{
	if (!IsValid(Node))
	{
		return;
	}
	MineSearchAnchorLocation = Node->GetActorLocation();
	bHasMineSearchAnchor = true;
}

void UGP_UnitCommandComponent::ClearMineSearchAnchor()
{
	MineSearchAnchorLocation = FVector::ZeroVector;
	bHasMineSearchAnchor = false;
}

AGP_ResourceNode* UGP_UnitCommandComponent::FindAutoResourceCandidate(
	AGP_Worker* Worker,
	AGP_ResourceNode* ExcludeNode,
	bool bRequireFreeSlot,
	FName SearchReason)
{
	if (!IsValid(Worker))
	{
		return nullptr;
	}

	UWorld* World = Worker->GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	if (GS == nullptr || !GS->HasAuthority())
	{
		return nullptr;
	}

	UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
	UGP_ResourceDefinition* CompatibleDef =
		IsValid(Cargo) ? Cargo->ResolveResourceDefinition(true) : nullptr;
	UGP_MiningComponent* Mining = Worker->GetMiningComponent();
	UGP_MovementComponent* Movement = ResolveMovementComponent();
	const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get();

	FGP_ResourceNodeSearchQuery Query;
	Query.SearchCenter = bHasMineSearchAnchor ? MineSearchAnchorLocation : Worker->GetActorLocation();
	Query.PathStart = Worker->GetActorLocation();
	Query.SearchRadiusCm = Worker->GetResourceSearchRadiusCm();
	Query.MaxPathLengthCm = Worker->GetMaxResourcePathLengthCm();
	Query.InteractionRangeCm = ResolveMineInteractionRangeCm(Mining, ExcludeNode);
	Query.AcceptanceRadiusCm = Movement != nullptr ? Movement->AcceptanceRadius : 50.0f;
	Query.ApproachSafetyMarginCm = ResolveApproachSafetyMarginCm();
	Query.ApproachDirectionCount = Settings != nullptr ? Settings->ResourceApproachDirectionCount : 8;
	Query.ExcludeNode = ExcludeNode;
	Query.CompatibleDefinition = CompatibleDef;
	Query.bRequireFreeSlot = bRequireFreeSlot;
	Query.PathfindingActor = Worker;
	Query.bPreferFreeSlot = true;

#if !UE_BUILD_SHIPPING
	const int32 RegistryCount = GS->GetRegisteredResourceNodeCount();
	const bool bWaitingWake = SearchReason == FName(TEXT("WaitingWake"));
	bool bLog = true;
	if (bWaitingWake && bHasLastWaitingNoCandidate
		&& LastWaitingNoCandidateRegistryCount == RegistryCount
		&& LastWaitingNoCandidateReason == SearchReason
		&& LastWaitingNoCandidateAnchor.Equals(Query.SearchCenter, 1.0f))
	{
		bLog = false;
	}
	Query.SearchReason = SearchReason;
	Query.bLogDiagnostics = bLog;
	if (bLog)
	{
		UE_LOG(LogGPUnitCommandExecution, Verbose,
			TEXT("GP ResourceReassignmentSearch: Worker=%s Reason=%s HasAnchor=%s Anchor=(%.0f,%.0f,%.0f) PathStart=(%.0f,%.0f,%.0f)"),
			*GetNameSafe(Worker),
			*SearchReason.ToString(),
			bHasMineSearchAnchor ? TEXT("true") : TEXT("false"),
			Query.SearchCenter.X, Query.SearchCenter.Y, Query.SearchCenter.Z,
			Query.PathStart.X, Query.PathStart.Y, Query.PathStart.Z);
	}
#else
	(void)SearchReason;
#endif

	AGP_ResourceNode* Best = GS->FindBestResourceCandidate(Query);

#if !UE_BUILD_SHIPPING
	if (Best == nullptr)
	{
		LastWaitingNoCandidateRegistryCount = RegistryCount;
		LastWaitingNoCandidateReason = SearchReason;
		LastWaitingNoCandidateAnchor = Query.SearchCenter;
		bHasLastWaitingNoCandidate = true;
	}
	else
	{
		bHasLastWaitingNoCandidate = false;
	}
#endif
	return Best;
}

bool UGP_UnitCommandComponent::TryRetargetMineToNode(
	AGP_ResourceNode* NewNode,
	uint32 MineSerial,
	bool bStartApproach)
{
	AActor* Owner = GetOwner();
	AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	if (!IsValid(Worker) || !IsValid(NewNode) || MineSerial == 0)
	{
		return false;
	}

	if (!HeldCommand.IsSet()
		|| HeldCommand.GetValue().CommandTag != FGPGameplayTags::Get().Command_Mine
		|| HeldCommand.GetValue().CommandSerial != MineSerial)
	{
		return false;
	}

	UGP_MiningComponent* Mining = Worker->GetMiningComponent();
	const AGP_ResourceNode* HeldTarget = Cast<AGP_ResourceNode>(HeldCommand.GetValue().TargetActor.Get());
	const AGP_ResourceNode* CurrentMine = MineTarget.Get();
	const AGP_ResourceNode* MiningNode = IsValid(Mining) ? Mining->GetCurrentResourceNode() : nullptr;
	const bool bSameTarget =
		NewNode == HeldTarget || NewNode == CurrentMine || NewNode == MiningNode;
	const bool bBusyOnTarget =
		IsValid(Mining)
		&& (Mining->IsMining() || Mining->IsWaitingForSlot()
			|| Mining->GetMiningState() == EGP_MiningState::WaitingForSlot
			|| Mining->GetMiningState() == EGP_MiningState::Mining)
		&& (MiningNode == NewNode || (MiningNode == nullptr && (HeldTarget == NewNode || CurrentMine == NewNode)));

	if (bSameTarget && bBusyOnTarget)
	{
#if !UE_BUILD_SHIPPING
		++DebugSameTargetRetargetAttempts;
		if (!bLoggedSameTargetRetargetSkip)
		{
			bLoggedSameTargetRetargetSkip = true;
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution MineRetargetSkippedSameTarget: Unit=%s MineSerial=%u Target=%s MiningState=%d"),
				*GetNameSafe(Owner),
				MineSerial,
				*GetNameSafe(NewNode),
				IsValid(Mining) ? static_cast<int32>(Mining->GetMiningState()) : -1);
		}
#endif
		return false;
	}

	UnbindResourceRegistryWake();
	UnbindMiningStateEvents();

	if (IsValid(Mining) && (Mining->IsMining() || Mining->IsWaitingForSlot()))
	{
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
	}

	const AGP_ResourceNode* OldTarget = HeldTarget;
	FGP_StoredUnitCommand& Held = HeldCommand.GetValue();
	Held.TargetActor = NewNode;
	MineTarget = NewNode;
	LastMineDepositForHaul = NewNode;
	ActiveMineSerial = MineSerial;
	MineApproachAttempt = 0;
#if !UE_BUILD_SHIPPING
	bLoggedSameTargetRetargetSkip = false;
#endif

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution MineRetarget: Unit=%s MineSerial=%u OldTarget=%s NewTarget=%s HeldTarget=%s MineTarget=%s"),
		*GetNameSafe(Owner),
		MineSerial,
		*GetNameSafe(OldTarget),
		*GetNameSafe(NewNode),
		*GetNameSafe(Cast<AGP_ResourceNode>(Held.TargetActor.Get())),
		*GetNameSafe(MineTarget.Get()));

	if (!bStartApproach)
	{
		MineState = EGP_MineExecutionState::Active;
		return true;
	}

	const float Distance = FVector::Dist(Owner->GetActorLocation(), NewNode->GetActorLocation());
	const float InteractionRange = ResolveMineInteractionRangeCm(Mining, NewNode);
	if (Distance <= InteractionRange)
	{
		// Nested BeginMiningAtHeldTarget is blocked by re-entry guard — call BeginMining directly.
		if (bBeginMiningAtHeldTargetInProgress && IsValid(Mining))
		{
			MineState = EGP_MineExecutionState::Active;
			LastMineDepositForHaul = NewNode;
			BindMiningStateEvents(Mining);
			const EGP_BeginMiningResult NestedResult = Mining->BeginMining(NewNode);
#if !UE_BUILD_SHIPPING
			++DebugMineBeginCallsThisTransition;
#endif
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution BeginMiningAtHeldTarget: Unit=%s MineSerial=%u HeldTarget=%s MineTarget=%s BoundMining=%s BeginResult=%d Label=RetargetNested"),
				*GetNameSafe(Owner),
				MineSerial,
				*GetNameSafe(NewNode),
				*GetNameSafe(MineTarget.Get()),
				BoundMiningComponent.IsValid() ? TEXT("true") : TEXT("false"),
				static_cast<int32>(NestedResult));
			if (NestedResult == EGP_BeginMiningResult::WaitingForSlot
				|| NestedResult == EGP_BeginMiningResult::AlreadyMiningTarget
				|| NestedResult == EGP_BeginMiningResult::Started)
			{
				MineTarget = NewNode;
				LastMineDepositForHaul = NewNode;
				ActiveMineSerial = MineSerial;
				BindMiningStateEvents(Mining);
				if (NestedResult == EGP_BeginMiningResult::WaitingForSlot
					|| NestedResult == EGP_BeginMiningResult::AlreadyMiningTarget)
				{
					const int32 WaitIndex = NewNode->FindWaitingMinerIndex(Owner);
					UE_LOG(LogGPUnitCommandExecution, Log,
						TEXT("GP UnitCommandExecution MineWaitingForSlot: Unit=%s MineSerial=%u Node=%s Position=%d Active=%d Waiting=%d Max=%d"),
						*GetNameSafe(Owner),
						MineSerial,
						*GetNameSafe(NewNode),
						WaitIndex >= 0 ? WaitIndex + 1 : -1,
						NewNode->GetActiveMinerCount(),
						NewNode->GetWaitingMinerCount(),
						NewNode->GetMaxConcurrentMiners());
				}
				return true;
			}
			return false;
		}
		BeginMiningAtHeldTarget(MineSerial);
		return HeldCommand.IsSet() && MineTarget.Get() == NewNode;
	}

	if (!RequestMineApproachMove(Owner, NewNode, MineSerial, 0.0f, TEXT("Reassign")))
	{
		return false;
	}
	return true;
}

bool UGP_UnitCommandComponent::TryAutoReassignMine(
	uint32 MineSerial,
	AGP_ResourceNode* PreferredOrFailedNode,
	bool bPreferFreeSlotFirst,
	FName SearchReason)
{
	AGP_Worker* Worker = Cast<AGP_Worker>(GetOwner());
	if (!IsValid(Worker) || MineSerial == 0)
	{
		return false;
	}

	UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
	if (IsValid(Cargo) && Cargo->IsFull())
	{
		// Full cargo must haul before any alternative mining.
		return false;
	}

	const bool bPreferredValid =
		IsValid(PreferredOrFailedNode)
		&& !PreferredOrFailedNode->IsDepleted()
		&& !PreferredOrFailedNode->HasCompletedDepletionTransition()
		&& !PreferredOrFailedNode->IsDestroyPending()
		&& PreferredOrFailedNode->CanAcceptMineCommand(true, nullptr);

	if (bPreferredValid)
	{
		const bool bPreferredHasFreeSlot =
			PreferredOrFailedNode->GetActiveMinerCount() < PreferredOrFailedNode->GetMaxConcurrentMiners();
		if (bPreferredHasFreeSlot)
		{
			return TryRetargetMineToNode(PreferredOrFailedNode, MineSerial, true);
		}

		if (bPreferFreeSlotFirst)
		{
			// Single pass: free nodes sort first. Never same-target retarget into FIFO churn.
			if (AGP_ResourceNode* FreeAlt = FindAutoResourceCandidate(
					Worker, PreferredOrFailedNode, false, SearchReason))
			{
				if (FreeAlt != PreferredOrFailedNode
					&& FreeAlt->GetActiveMinerCount() < FreeAlt->GetMaxConcurrentMiners())
				{
					return TryRetargetMineToNode(FreeAlt, MineSerial, true);
				}
			}
		}

		// No free alternative — caller enters/keeps FIFO via BeginMining; do not retarget same node.
		return false;
	}

	if (AGP_ResourceNode* AnyAlt = FindAutoResourceCandidate(
			Worker, PreferredOrFailedNode, false, SearchReason))
	{
		return TryRetargetMineToNode(AnyAlt, MineSerial, true);
	}

	return false;
}

bool UGP_UnitCommandComponent::TryHaulPartialCargoBeforeWaiting(
	uint32 MineSerial,
	AGP_ResourceNode* DepositHint)
{
	if (bRedirectingStrandedCargoHaul || MineSerial == 0)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	UGP_CargoComponent* Cargo = Worker != nullptr ? Worker->GetCargoComponent() : nullptr;
	if (!IsValid(Cargo) || Cargo->GetCurrentCargoAmount() <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

#if !UE_BUILD_SHIPPING
	UE_LOG(LogGPUnitCommandExecution, Error,
		TEXT("GP UnitCommandExecution WaitingForResourceInvariant: Unit=%s MineSerial=%u Cargo=%.1f — redirecting to haul"),
		*GetNameSafe(Owner),
		MineSerial,
		Cargo->GetCurrentCargoAmount());
#endif

	UWorld* World = Owner != nullptr ? Owner->GetWorld() : nullptr;
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	AGP_MainBase* MainBase = (GS != nullptr && Worker != nullptr)
		? GS->FindMainBaseForTeam(Worker->GetTeamId())
		: nullptr;
	if (!IsValid(MainBase))
	{
		// Documented unrecoverable MainBase failure — do not invent P3 recovery here.
		return false;
	}

	TGuardValue<bool> RedirectGuard(bRedirectingStrandedCargoHaul, true);
	AGP_ResourceNode* Deposit = IsValid(DepositHint) ? DepositHint : LastHaulDeposit.Get();
	if (!IsValid(Deposit) && HeldCommand.IsSet())
	{
		Deposit = Cast<AGP_ResourceNode>(HeldCommand.GetValue().TargetActor.Get());
	}
	ActiveMineSerial = MineSerial;
	StartHaulReturnToBase(MineSerial, Deposit, false);
	return true;
}

void UGP_UnitCommandComponent::EnterWaitingForResource(uint32 MineSerial)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || MineSerial == 0)
	{
		return;
	}

	AGP_ResourceNode* DepositHint = MineTarget.Get();
	if (TryHaulPartialCargoBeforeWaiting(MineSerial, DepositHint))
	{
		return;
	}

	UnbindMiningStateEvents();
	MineState = EGP_MineExecutionState::WaitingForResource;
	ActiveMineSerial = MineSerial;
	MineTarget.Reset();
	BindResourceRegistryWake();

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution WaitingForResource: Unit=%s MineSerial=%u"),
		*GetNameSafe(Owner),
		MineSerial);
}

void UGP_UnitCommandComponent::BindResourceRegistryWake()
{
	if (bResourceRegistryWakeBound)
	{
		return;
	}

	UWorld* World = GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	if (GS == nullptr)
	{
		return;
	}

	ResourceNodeRegisteredHandle = GS->OnResourceNodeRegistered.AddUObject(
		this, &UGP_UnitCommandComponent::HandleResourceNodeRegisteredWake);
	bResourceRegistryWakeBound = true;

	float RetrySeconds = 3.0f;
	if (const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get())
	{
		RetrySeconds = FMath::Max(0.1f, Settings->WaitingForResourceRetrySeconds);
	}
	World->GetTimerManager().ClearTimer(WaitingForResourceRetryTimerHandle);
	World->GetTimerManager().SetTimer(
		WaitingForResourceRetryTimerHandle,
		this,
		&UGP_UnitCommandComponent::HandleWaitingForResourceSafetyRetry,
		RetrySeconds,
		true);
}

void UGP_UnitCommandComponent::UnbindResourceRegistryWake()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WaitingForResourceRetryTimerHandle);
	}

	if (!bResourceRegistryWakeBound)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			GS->OnResourceNodeRegistered.Remove(ResourceNodeRegisteredHandle);
		}
	}
	ResourceNodeRegisteredHandle.Reset();
	bResourceRegistryWakeBound = false;
}

void UGP_UnitCommandComponent::HandleResourceNodeRegisteredWake(AGP_ResourceNode* Node)
{
	(void)Node;
	if (MineState != EGP_MineExecutionState::WaitingForResource || ActiveMineSerial == 0)
	{
		return;
	}

	const uint32 Serial = ActiveMineSerial;
	if (TryAutoReassignMine(Serial, nullptr, true, FName(TEXT("WaitingWake"))))
	{
		UnbindResourceRegistryWake();
	}
}

void UGP_UnitCommandComponent::HandleWaitingForResourceSafetyRetry()
{
	if (MineState != EGP_MineExecutionState::WaitingForResource || ActiveMineSerial == 0)
	{
		UnbindResourceRegistryWake();
		return;
	}

	const uint32 Serial = ActiveMineSerial;
	if (TryAutoReassignMine(Serial, nullptr, true, FName(TEXT("WaitingWake"))))
	{
		UnbindResourceRegistryWake();
	}
}

void UGP_UnitCommandComponent::NotifyMiningComponentTerminal(
	EGP_MiningState PreviousState,
	EGP_MiningState NewState,
	EGP_MiningStopReason Reason)
{
	// Multicast already invoked HandleMiningStateChanged when bound; that path starts haul / clears.
	// This direct call covers the unbound orphan case after reassignment remine Idle cleared binding.
	if (HaulState == EGP_HaulExecutionState::ReturningToBase
		|| HaulState == EGP_HaulExecutionState::DroppingOff
		|| HaulState == EGP_HaulExecutionState::WaitingForDropOff
		|| HaulState == EGP_HaulExecutionState::ReturningToDeposit)
	{
		return;
	}
	if (bFinishingMine)
	{
		return;
	}
	HandleMiningStateChanged(PreviousState, NewState, Reason);
}

void UGP_UnitCommandComponent::HandleMiningStateChanged(
	EGP_MiningState PreviousState,
	EGP_MiningState NewState,
	EGP_MiningStopReason Reason)
{
	(void)PreviousState;
	if (bFinishingMine)
	{
		return;
	}

	// Idempotent: CargoFull/partial terminals must not restart haul when already hauling.
	if ((NewState == EGP_MiningState::CargoFull
			|| NewState == EGP_MiningState::DepositDepleted)
		&& (HaulState == EGP_HaulExecutionState::ReturningToBase
			|| HaulState == EGP_HaulExecutionState::DroppingOff
			|| HaulState == EGP_HaulExecutionState::WaitingForDropOff
			|| HaulState == EGP_HaulExecutionState::ReturningToDeposit))
	{
		return;
	}

	AActor* Owner = GetOwner();
	AGP_Worker* Worker = Cast<AGP_Worker>(Owner);
	UGP_CargoComponent* Cargo = Worker != nullptr ? Worker->GetCargoComponent() : nullptr;
	UGP_MiningComponent* Mining = Worker != nullptr ? Worker->GetMiningComponent() : nullptr;
	const float CargoAmount = IsValid(Cargo) ? Cargo->GetCurrentCargoAmount() : 0.0f;

	// Recover chain identity if remine Idle cleared ActiveMineSerial before CargoFull.
	if (ActiveMineSerial == 0)
	{
		if (NewState == EGP_MiningState::CargoFull
			&& CargoAmount > KINDA_SMALL_NUMBER
			&& HeldCommand.IsSet()
			&& HeldCommand.GetValue().CommandTag == FGPGameplayTags::Get().Command_Mine)
		{
			ActiveMineSerial = HeldCommand.GetValue().CommandSerial;
			if (!MineTarget.IsValid())
			{
				MineTarget = Cast<AGP_ResourceNode>(HeldCommand.GetValue().TargetActor.Get());
			}
			if (!LastMineDepositForHaul.IsValid())
			{
				LastMineDepositForHaul = MineTarget;
			}
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution MineTerminalSerialRecovered: Unit=%s MineSerial=%u HeldTarget=%s"),
				*GetNameSafe(Owner),
				ActiveMineSerial,
				*GetNameSafe(MineTarget.Get()));
		}
		else
		{
			return;
		}
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

	// BeginMining may StopMining(ManualStop)→Idle while UnitCommand is still bound during
	// BeginMiningAtHeldTarget (including nested retarget BeginMining). Ignore that Idle only
	// while the remine guard is set — external StopMining(ManualStop) must still clear the chain.
	if (NewState == EGP_MiningState::Idle && bBeginMiningAtHeldTargetInProgress)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution MiningStateChangedIgnored: Unit=%s Serial=%u NewState=Idle Reason=%d IgnoreReason=BeginMiningRemine HeldPresent=%s BoundMiningMatches=%s"),
			*GetNameSafe(Owner),
			ActiveMineSerial,
			static_cast<int32>(Reason),
			HeldCommand.IsSet() ? TEXT("true") : TEXT("false"),
			BoundMiningComponent.Get() == Mining ? TEXT("true") : TEXT("false"));
		return;
	}

	AGP_ResourceNode* HeldTarget =
		HeldCommand.IsSet() ? Cast<AGP_ResourceNode>(HeldCommand.GetValue().TargetActor.Get()) : nullptr;
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution MiningStateChanged: Unit=%s Serial=%u Node=%s NewState=%d Reason=%d HeldPresent=%s HeldTarget=%s MineTarget=%s ActiveMineSerial=%u BoundMiningMatches=%s Cargo=%.1f"),
		*GetNameSafe(Owner),
		ActiveMineSerial,
		*GetNameSafe(MineTarget.IsValid() ? MineTarget.Get() : LastMineDepositForHaul.Get()),
		static_cast<int32>(NewState),
		static_cast<int32>(Reason),
		HeldCommand.IsSet() ? TEXT("true") : TEXT("false"),
		*GetNameSafe(HeldTarget),
		*GetNameSafe(MineTarget.Get()),
		ActiveMineSerial,
		BoundMiningComponent.Get() == Mining ? TEXT("true") : TEXT("false"),
		CargoAmount);

	const uint32 Serial = ActiveMineSerial;
	AGP_ResourceNode* DepositBeforeReset = MineTarget.Get();
	if (!IsValid(DepositBeforeReset))
	{
		DepositBeforeReset = LastMineDepositForHaul.Get();
	}
	if (!IsValid(DepositBeforeReset))
	{
		DepositBeforeReset = HeldTarget;
	}

	const bool bHaulCargoFull = NewState == EGP_MiningState::CargoFull;
	const bool bHaulDepletedPartial =
		NewState == EGP_MiningState::DepositDepleted && CargoAmount > KINDA_SMALL_NUMBER;
	// Destroyed/teardown deposit with leftover cargo: haul without return-to-deposit (same as depleted partial).
	const bool bHaulDestroyedPartial =
		Reason == EGP_MiningStopReason::TargetEndPlay && CargoAmount > KINDA_SMALL_NUMBER;

	const bool bNeedsReassignment =
		!bHaulCargoFull
		&& CargoAmount <= KINDA_SMALL_NUMBER
		&& (NewState == EGP_MiningState::DepositDepleted
			|| Reason == EGP_MiningStopReason::TargetEndPlay
			|| NewState == EGP_MiningState::Invalid);

	bool bStartHaul = false;
	bool bReturnToDeposit = false;
	bool bReassignAfter = false;
	{
		TGuardValue<bool> Guard(bFinishingMine, true);
		UnbindMiningStateEvents();
		MineState = EGP_MineExecutionState::Idle;
		// Keep ActiveMineSerial as haul/mine chain identity for haul terminals.
		MineTarget.Reset();

		if (bHaulCargoFull || bHaulDepletedPartial || bHaulDestroyedPartial)
		{
			bReturnToDeposit =
				bHaulCargoFull
				&& IsValid(DepositBeforeReset)
				&& !DepositBeforeReset->IsDepleted()
				&& !DepositBeforeReset->HasCompletedDepletionTransition()
				&& !DepositBeforeReset->IsDestroyPending()
				&& !DepositBeforeReset->IsActorBeingDestroyed()
				&& !DepositBeforeReset->IsClearingOccupancy();
			bStartHaul = true;
		}
		else if (bNeedsReassignment
			&& HeldCommand.IsSet()
			&& HeldCommand.GetValue().CommandTag == FGPGameplayTags::Get().Command_Mine
			&& HeldCommand.GetValue().CommandSerial == Serial)
		{
			bReassignAfter = true;
		}
		else
		{
			ActiveMineSerial = 0;
			if (HeldCommand.IsSet()
				&& HeldCommand.GetValue().CommandTag == FGPGameplayTags::Get().Command_Mine
				&& HeldCommand.GetValue().CommandSerial == Serial)
			{
				ClearHeldCommand();
			}
			ResetHaulExecutor();
			LastMineDepositForHaul.Reset();
		}
	}

	if (bStartHaul)
	{
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution CargoFullHaulDecision: Unit=%s MineSerial=%u Deposit=%s Cargo=%.1f StartHaul=true ReturnToDeposit=%s MainBaseWillResolve=true"),
			*GetNameSafe(Owner),
			Serial,
			*GetNameSafe(DepositBeforeReset),
			CargoAmount,
			bReturnToDeposit ? TEXT("true") : TEXT("false"));
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP UnitCommandExecution MineTerminalHaul: Unit=%s MineSerial=%u Deposit=%s Cargo=%.1f ReturnToDeposit=%s"),
			*GetNameSafe(Owner),
			Serial,
			*GetNameSafe(DepositBeforeReset),
			CargoAmount,
			bReturnToDeposit ? TEXT("true") : TEXT("false"));
		StartHaulReturnToBase(Serial, DepositBeforeReset, bReturnToDeposit);
		return;
	}

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution CargoFullHaulDecision: Unit=%s MineSerial=%u Deposit=%s Cargo=%.1f StartHaul=false ReassignAfter=%s NewState=%d"),
		*GetNameSafe(Owner),
		Serial,
		*GetNameSafe(DepositBeforeReset),
		CargoAmount,
		bReassignAfter ? TEXT("true") : TEXT("false"),
		static_cast<int32>(NewState));

	if (bReassignAfter)
	{
		ActiveMineSerial = Serial;
		if (!TryAutoReassignMine(Serial, DepositBeforeReset, true, FName(TEXT("PostDepletion"))))
		{
			EnterWaitingForResource(Serial);
		}
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

	UWorld* World = Owner->GetWorld();
	if (!GPCombatLOS::HasLineOfSight(World, OwnerUnit, Target))
	{
		if (!bAttackLOSBlocked)
		{
			bAttackLOSBlocked = true;
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP AttackLOSBlocked: Unit=%s Target=%s Serial=%u Distance=%.1f AttackRange=%.1f"),
				*GetNameSafe(Owner),
				*GetNameSafe(Target),
				HitSerial,
				Distance,
				EffectiveRange);
		}
		// Stay Ready; do not apply damage, presentation, or successful-hit cooldown.
		return;
	}

	if (bAttackLOSBlocked)
	{
		bAttackLOSBlocked = false;
		UE_LOG(LogGPUnitCommandExecution, Log,
			TEXT("GP AttackLOSRestored: Unit=%s Target=%s Serial=%u Distance=%.1f AttackRange=%.1f"),
			*GetNameSafe(Owner),
			*GetNameSafe(Target),
			HitSerial,
			Distance,
			EffectiveRange);
	}

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
		else if (Held.CommandTag == FGPGameplayTags::Get().Command_AttackMove
			&& Held.CommandSerial == FinishedSerial)
		{
			// Engagement ended under AttackMove ownership — resume original destination.
			ResumeAttackMoveTravelAfterEngagement();
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

	if (Result == EGP_MovementResult::Failed)
	{
		LogApproachResult(TEXT("MovementFailed"));
		FinishAttack(EGP_AttackTerminalResult::Failed, EGP_AttackTerminalReason::MovementCancelled);
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
	if (TryConsumeRetaliationMovementResult(Serial, Result, Reason))
	{
		return;
	}

	if (TryConsumeAttackMovementResult(Serial, Result, Reason))
	{
		return;
	}

	if (TryConsumeHaulMovementResult(Serial, Result, Reason))
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

	if (Result != EGP_MovementResult::Reached
		&& Result != EGP_MovementResult::Cancelled
		&& Result != EGP_MovementResult::Failed)
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
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const FGameplayTag MoveTag = GPTags.Command_Move;
	const FGameplayTag AttackMoveTag = GPTags.Command_AttackMove;
	const bool bHeldDestinationTravel =
		CurrentHeld.CommandTag == MoveTag || CurrentHeld.CommandTag == AttackMoveTag;
	if (!bHeldDestinationTravel)
	{
		LogIgnored(TEXT("HeldTagNotDestinationTravel"), CurrentHeld.CommandSerial, CurrentHeld.CommandTag.ToString(), false);
		return;
	}

	if (CurrentHeld.CommandSerial != Serial)
	{
		LogIgnored(TEXT("SerialMismatch"), CurrentHeld.CommandSerial, CurrentHeld.CommandTag.ToString(), false);
		return;
	}

	// While AttackMove is engaging, destination travel results are owned by Attack approach/cleanup.
	if (CurrentHeld.CommandTag == AttackMoveTag && IsAttackActive())
	{
		LogIgnored(TEXT("AttackMoveEngaging"), CurrentHeld.CommandSerial, CurrentHeld.CommandTag.ToString(), false);
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

	if (!Command.bQueue && !bIssuingRetaliationEngageCommand)
	{
		CancelRetaliation(TEXT("ManualCommand"), true);
	}

	if (Cast<AGP_BuildingBase>(Owner) != nullptr)
	{
		const FGPGameplayTags& BuildingCommandTags = FGPGameplayTags::Get();
		if (Command.CommandTag == BuildingCommandTags.Command_Move
			|| Command.CommandTag == BuildingCommandTags.Command_AttackMove)
		{
			UE_LOG(LogGPUnitCommandState, Log,
				TEXT("GP UnitCommandState RejectedStationary: Unit=%s Tag=%s Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				*Command.CommandTag.ToString(),
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
			return;
		}
	}

	const FGPGameplayTags& IncomingTags = FGPGameplayTags::Get();
	if (Command.CommandTag == IncomingTags.Command_Stop && !Command.bQueue)
	{
		const TOptional<FGP_StoredUnitCommand> PreviousCommand = HeldCommand;
		ResetAttackExecutorForReplacement(PreviousCommand);
		ResetMineExecutorForReplacement(PreviousCommand);
		if (UGP_MovementComponent* Movement = ResolveMovementComponent())
		{
			Movement->StopMove(EGP_MovementStopReason::Manual);
		}
		if (HeldCommand.IsSet())
		{
			UE_LOG(LogGPUnitCommandState, Log,
				TEXT("GP UnitCommandState HeldCleared: Unit=%s Serial=%u Tag=%s Reason=Stop Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				HeldCommand.GetValue().CommandSerial,
				*HeldCommand.GetValue().CommandTag.ToString(),
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
			ClearHeldCommand();
		}
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
	const FGameplayTag AttackMoveTag = FGPGameplayTags::Get().Command_AttackMove;
	const bool bCurrentIsMove = CurrentHeld.CommandTag == MoveTag;
	const bool bCurrentIsAttackMove = CurrentHeld.CommandTag == AttackMoveTag;
	const bool bCurrentRequestsDestinationTravel = bCurrentIsMove || bCurrentIsAttackMove;

	const uint32 PreviousSerial = PreviousCommand.IsSet() ? PreviousCommand.GetValue().CommandSerial : 0;
	const FString PreviousTagString = PreviousCommand.IsSet()
		? PreviousCommand.GetValue().CommandTag.ToString()
		: FString(TEXT("none"));

	AGP_MobileUnit* MobileUnit = Cast<AGP_MobileUnit>(Owner);
	if (MobileUnit == nullptr)
	{
		if (bCurrentRequestsDestinationTravel)
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
		if (bCurrentRequestsDestinationTravel)
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

	if (bCurrentRequestsDestinationTravel)
	{
		const uint32 RequestedSerial = CurrentHeld.CommandSerial;
		const FGameplayTag RequestedTag = CurrentHeld.CommandTag;
		const FVector RequestedDestination = CurrentHeld.TargetLocation;

		const FGP_MovementRequestOutcome Outcome =
			Movement->RequestMove(RequestedDestination, RequestedSerial);

		if (Outcome.IsAccepted())
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution MoveExecutionRequested: Unit=%s Serial=%u Tag=%s Destination=%s PreviousSerial=%u PreviousTag=%s Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				RequestedSerial,
				*RequestedTag.ToString(),
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
			if ((Held.CommandTag == MoveTag || Held.CommandTag == AttackMoveTag)
				&& Held.CommandSerial == RequestedSerial)
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

bool UGP_UnitCommandComponent::IsAttackLOSBlocked() const
{
	return bAttackLOSBlocked;
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
	CancelRetaliation(TEXT("OwnerDied"), false);
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

void UGP_UnitCommandComponent::NotifyHostileDamageReceived(AGP_UnitBase* SourceUnit)
{
	AActor* Owner = GetOwner();
	AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (Owner == nullptr || !Owner->HasAuthority() || OwnerUnit == nullptr || OwnerUnit->IsDead())
	{
		return;
	}

	if (OwnerUnit->GetRetaliationPursuitSeconds() <= 0.0f)
	{
		return;
	}

	if (!IsRetaliationAttackerValid(SourceUnit))
	{
		return;
	}

	if (bRetaliationActive)
	{
		StartOrRefreshRetaliation(SourceUnit);
		return;
	}

	if (!IsMobileCombatUnitForRetaliation(OwnerUnit) || HasCommandThatBlocksRetaliationStart())
	{
		return;
	}

	StartOrRefreshRetaliation(SourceUnit);
}

bool UGP_UnitCommandComponent::IsMobileCombatUnitForRetaliation(const AGP_UnitBase* Unit) const
{
	if (Unit == nullptr || Unit->IsA(AGP_BuildingBase::StaticClass()))
	{
		return false;
	}

	return IsCombatCapableForAutoAcquire(Unit);
}

bool UGP_UnitCommandComponent::HasCommandThatBlocksRetaliationStart() const
{
	if (IsAttackActive())
	{
		return true;
	}
	if (MineState != EGP_MineExecutionState::Idle || ActiveMineSerial != 0)
	{
		return true;
	}
	if (HaulState != EGP_HaulExecutionState::Idle || ActiveHaulSerial != 0)
	{
		return true;
	}
	if (!HeldCommand.IsSet())
	{
		return false;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const FGameplayTag& HeldTag = HeldCommand.GetValue().CommandTag;
	return HeldTag == GPTags.Command_Move
		|| HeldTag == GPTags.Command_Attack
		|| HeldTag == GPTags.Command_AttackMove
		|| HeldTag == GPTags.Command_Mine;
}

bool UGP_UnitCommandComponent::IsRetaliationAttackerValid(const AGP_UnitBase* Attacker) const
{
	const AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(GetOwner());
	if (OwnerUnit == nullptr || Attacker == nullptr || !IsValid(Attacker) || Attacker == OwnerUnit)
	{
		return false;
	}
	if (Attacker->IsDead())
	{
		return false;
	}

	AGP_UnitBase* ValidTarget = nullptr;
	EGP_AttackTerminalReason Reason = EGP_AttackTerminalReason::InvalidTarget;
	return ValidateAttackTarget(const_cast<AGP_UnitBase*>(Attacker), ValidTarget, Reason) && ValidTarget != nullptr;
}

bool UGP_UnitCommandComponent::CanEngageRetaliationTarget(AGP_UnitBase* Attacker) const
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return false;
	}

	AGP_UnitBase* ValidTarget = nullptr;
	EGP_AttackTerminalReason Reason = EGP_AttackTerminalReason::InvalidTarget;
	if (!ValidateAttackTarget(Attacker, ValidTarget, Reason) || ValidTarget == nullptr)
	{
		return false;
	}

	float Distance = -1.0f;
	if (!TryComputeAttackDistance2D(Owner, ValidTarget, Distance))
	{
		return false;
	}

	return Distance <= GetEffectiveAutoAcquireRange();
}

void UGP_UnitCommandComponent::StartOrRefreshRetaliation(AGP_UnitBase* Attacker)
{
	AActor* Owner = GetOwner();
	AGP_UnitBase* OwnerUnit = Cast<AGP_UnitBase>(Owner);
	if (OwnerUnit == nullptr || !IsRetaliationAttackerValid(Attacker))
	{
		return;
	}

	const bool bSameAttacker = RetaliationTarget.Get() == Attacker;
	RetaliationTarget = Attacker;
	bRetaliationActive = true;
	BindRetaliationAttackerDeath(Attacker);
	ArmRetaliationTimeout(OwnerUnit->GetRetaliationPursuitSeconds());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RetaliationEvaluateHandle,
			this,
			&UGP_UnitCommandComponent::OnRetaliationEvaluate,
			0.20f,
			true);
	}

	if (CanEngageRetaliationTarget(Attacker))
	{
		TryEngageRetaliationTarget(Attacker);
		return;
	}

	RequestRetaliationPursuitMove(Attacker, !bSameAttacker || RetaliationMovementSerial == 0);

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution RetaliationStart: Unit=%s Attacker=%s Same=%s Duration=%.2f"),
		*GetNameSafe(Owner),
		*GetNameSafe(Attacker),
		bSameAttacker ? TEXT("true") : TEXT("false"),
		OwnerUnit->GetRetaliationPursuitSeconds());
}

void UGP_UnitCommandComponent::CancelRetaliation(const TCHAR* Reason, bool bStopRetaliationMovement)
{
	if (!bRetaliationActive && RetaliationMovementSerial == 0 && !RetaliationTimeoutHandle.IsValid())
	{
		UnbindRetaliationAttackerDeath();
		ClearRetaliationTimers();
		RetaliationTarget.Reset();
		return;
	}

	AActor* Owner = GetOwner();
	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution RetaliationCancel: Unit=%s Attacker=%s Reason=%s"),
		*GetNameSafe(Owner),
		*GetNameSafe(RetaliationTarget.Get()),
		Reason);

	const uint32 SerialToStop = RetaliationMovementSerial;
	bRetaliationActive = false;
	RetaliationTarget.Reset();
	UnbindRetaliationAttackerDeath();
	ClearRetaliationTimers();
	RetaliationMovementSerial = 0;
	LastRetaliationDestination = FVector::ZeroVector;
	LastRetaliationIssueTime = -1.0;

	if (bStopRetaliationMovement && SerialToStop != 0)
	{
		if (UGP_MovementComponent* Movement = ResolveMovementComponent())
		{
			if (Movement->IsMoving() && Movement->GetActiveMoveSerial() == SerialToStop)
			{
				Movement->StopMove(EGP_MovementStopReason::Manual);
			}
		}
	}
}

void UGP_UnitCommandComponent::BindRetaliationAttackerDeath(AGP_UnitBase* Attacker)
{
	if (BoundRetaliationAttacker.Get() == Attacker && RetaliationAttackerDiedHandle.IsValid())
	{
		return;
	}

	UnbindRetaliationAttackerDeath();
	if (!IsValid(Attacker))
	{
		return;
	}

	RetaliationAttackerDiedHandle = Attacker->OnUnitDied().AddUObject(
		this,
		&UGP_UnitCommandComponent::HandleRetaliationAttackerDied);
	BoundRetaliationAttacker = Attacker;
}

void UGP_UnitCommandComponent::UnbindRetaliationAttackerDeath()
{
	if (AGP_UnitBase* Bound = BoundRetaliationAttacker.Get())
	{
		if (RetaliationAttackerDiedHandle.IsValid())
		{
			Bound->OnUnitDied().Remove(RetaliationAttackerDiedHandle);
		}
	}
	RetaliationAttackerDiedHandle.Reset();
	BoundRetaliationAttacker.Reset();
}

void UGP_UnitCommandComponent::HandleRetaliationAttackerDied(AGP_UnitBase* DeadUnit)
{
	if (!bRetaliationActive)
	{
		return;
	}
	if (DeadUnit != nullptr && RetaliationTarget.Get() != nullptr && DeadUnit != RetaliationTarget.Get())
	{
		return;
	}
	CancelRetaliation(TEXT("AttackerDied"), true);
}

void UGP_UnitCommandComponent::ArmRetaliationTimeout(float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(RetaliationTimeoutHandle);
	World->GetTimerManager().SetTimer(
		RetaliationTimeoutHandle,
		this,
		&UGP_UnitCommandComponent::OnRetaliationTimeout,
		FMath::Max(0.05f, DurationSeconds),
		false);
}

void UGP_UnitCommandComponent::ClearRetaliationTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetaliationTimeoutHandle);
		World->GetTimerManager().ClearTimer(RetaliationEvaluateHandle);
	}
	RetaliationTimeoutHandle.Invalidate();
	RetaliationEvaluateHandle.Invalidate();
}

void UGP_UnitCommandComponent::OnRetaliationTimeout()
{
	CancelRetaliation(TEXT("Timeout"), true);
}

void UGP_UnitCommandComponent::OnRetaliationEvaluate()
{
	if (!bRetaliationActive)
	{
		ClearRetaliationTimers();
		return;
	}

	AGP_UnitBase* Attacker = RetaliationTarget.Get();
	if (!IsRetaliationAttackerValid(Attacker))
	{
		CancelRetaliation(TEXT("AttackerInvalid"), true);
		return;
	}

	if (CanEngageRetaliationTarget(Attacker))
	{
		TryEngageRetaliationTarget(Attacker);
		return;
	}

	RequestRetaliationPursuitMove(Attacker, false);
}

void UGP_UnitCommandComponent::RequestRetaliationPursuitMove(AGP_UnitBase* Attacker, bool bForceIssue)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !IsValid(Attacker))
	{
		return;
	}

	UGP_MovementComponent* Movement = ResolveMovementComponent();
	if (Movement == nullptr)
	{
		CancelRetaliation(TEXT("MissingMovement"), false);
		return;
	}

	const FVector Destination = MakeApproachDestination(Owner, Attacker);
	const UWorld* World = Owner->GetWorld();
	const double Now = World != nullptr ? World->GetTimeSeconds() : 0.0;
	if (!bForceIssue)
	{
		const float DestDelta = FVector::Dist2D(Destination, LastRetaliationDestination);
		const bool bIntervalOk = LastRetaliationIssueTime < 0.0
			|| (Now - LastRetaliationIssueTime) >= static_cast<double>(AttackReissueInterval);
		const bool bDistanceOk = DestDelta >= AttackReissueDistance;
		if (!bIntervalOk || !bDistanceOk)
		{
			return;
		}
	}

	if (RetaliationMovementSerial == 0)
	{
		RetaliationMovementSerial = AllocateCommandSerial();
	}

	const FGP_MovementRequestOutcome Outcome = Movement->RequestMove(Destination, RetaliationMovementSerial);
	if (!Outcome.IsAccepted())
	{
		UE_LOG(LogGPUnitCommandExecution, Warning,
			TEXT("GP UnitCommandExecution RetaliationMoveRejected: Unit=%s Serial=%u Reason=%s"),
			*GetNameSafe(Owner),
			RetaliationMovementSerial,
			GPUnitCommandStatePrivate::RejectReasonToString(Outcome.RejectReason));
		return;
	}

	LastRetaliationMovementSerial = RetaliationMovementSerial;
	LastRetaliationDestination = Destination;
	LastRetaliationIssueTime = Now;
}

void UGP_UnitCommandComponent::TryEngageRetaliationTarget(AGP_UnitBase* Attacker)
{
	if (!IsValid(Attacker))
	{
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	FGP_UnitCommand AttackCommand;
	AttackCommand.CommandTag = GPTags.Command_Attack;
	AttackCommand.TargetActor = Attacker;
	AttackCommand.TargetLocation = Attacker->GetActorLocation();
	AttackCommand.bQueue = false;

	CancelRetaliation(TEXT("EngageAttack"), true);
	bIssuingRetaliationEngageCommand = true;
	HandleCommand(AttackCommand);
	bIssuingRetaliationEngageCommand = false;
}

bool UGP_UnitCommandComponent::TryConsumeRetaliationMovementResult(
	uint32 Serial,
	EGP_MovementResult Result,
	EGP_MovementResultReason Reason)
{
	(void)Result;
	(void)Reason;
	if (Serial == 0)
	{
		return false;
	}
	if (Serial != RetaliationMovementSerial && Serial != LastRetaliationMovementSerial)
	{
		return false;
	}

	UE_LOG(LogGPUnitCommandExecution, Log,
		TEXT("GP UnitCommandExecution RetaliationMoveResult: Unit=%s Serial=%u Active=%s Result=%s"),
		*GetNameSafe(GetOwner()),
		Serial,
		bRetaliationActive ? TEXT("true") : TEXT("false"),
		GPUnitCommandStatePrivate::MovementResultToString(Result));
	return true;
}

#if !UE_BUILD_SHIPPING
float UGP_UnitCommandComponent::DebugGetRetaliationRemainingSeconds() const
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetTimerManager().GetTimerRemaining(RetaliationTimeoutHandle);
	}
	return -1.0f;
}
#endif

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
