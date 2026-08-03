// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPMovementComponent.h"

#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Units/GPMobileUnit.h"

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGPUnitMovement, Log, All);

namespace GPUnitMovementPrivate
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
}

UGP_MovementComponent::UGP_MovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetComponentTickEnabled(false);
	SetIsReplicatedByDefault(false);
}

void UGP_MovementComponent::ClearActiveMovementState()
{
	bIsMoving = false;
	ActiveMoveSerial = 0;
	SetComponentTickEnabled(false);
}

const TCHAR* UGP_MovementComponent::StopReasonToString(EGP_MovementStopReason Reason)
{
	switch (Reason)
	{
	case EGP_MovementStopReason::Manual:
		return TEXT("Manual");
	case EGP_MovementStopReason::CommandReplaced:
		return TEXT("CommandReplaced");
	case EGP_MovementStopReason::EndPlay:
		return TEXT("EndPlay");
	case EGP_MovementStopReason::OwnerDied:
		return TEXT("OwnerDied");
	default:
		return TEXT("Unknown");
	}
}

const TCHAR* UGP_MovementComponent::MovementResultToString(EGP_MovementResult Result)
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

const TCHAR* UGP_MovementComponent::MovementResultReasonToString(EGP_MovementResultReason Reason)
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

const TCHAR* UGP_MovementComponent::RejectReasonToString(EGP_MovementRejectReason Reason)
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

const TCHAR* UGP_MovementComponent::RequestStatusToString(EGP_MovementRequestStatus Status)
{
	switch (Status)
	{
	case EGP_MovementRequestStatus::Accepted:
		return TEXT("Accepted");
	case EGP_MovementRequestStatus::Rejected:
		return TEXT("Rejected");
	default:
		return TEXT("Unknown");
	}
}

void UGP_MovementComponent::BroadcastMovementResult(
	uint32 Serial,
	EGP_MovementResult Result,
	EGP_MovementResultReason Reason,
	const FVector& DestinationForLog)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	UE_LOG(LogGPUnitMovement, Log,
		TEXT("GP UnitMovement MovementResultBroadcast: Unit=%s Serial=%u Result=%s Reason=%s Destination=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		Serial,
		MovementResultToString(Result),
		MovementResultReasonToString(Reason),
		*DestinationForLog.ToCompactString(),
		GPUnitMovementPrivate::RoleToString(Role),
		GPUnitMovementPrivate::NetModeToString(NetMode));

	MovementResultDelegate.Broadcast(Serial, Result, Reason);
}

FGP_MovementRequestOutcome UGP_MovementComponent::RequestMove(const FVector& Destination, uint32 CommandSerial)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	auto MakeRejected = [&](EGP_MovementRejectReason Reason) -> FGP_MovementRequestOutcome
	{
		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement MoveRejected: Unit=%s Serial=%u Destination=%s Reason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			CommandSerial,
			*Destination.ToCompactString(),
			RejectReasonToString(Reason),
			GPUnitMovementPrivate::RoleToString(Role),
			GPUnitMovementPrivate::NetModeToString(NetMode));

		FGP_MovementRequestOutcome Outcome;
		Outcome.Status = EGP_MovementRequestStatus::Rejected;
		Outcome.RejectReason = Reason;
		return Outcome;
	};

	if (Owner == nullptr)
	{
		return MakeRejected(EGP_MovementRejectReason::MissingOwner);
	}

	if (!Owner->HasAuthority())
	{
		return MakeRejected(EGP_MovementRejectReason::NoAuthority);
	}

	if (CommandSerial == 0)
	{
		return MakeRejected(EGP_MovementRejectReason::InvalidSerial);
	}

	if (Destination.ContainsNaN()
		|| !FMath::IsFinite(Destination.X)
		|| !FMath::IsFinite(Destination.Y)
		|| !FMath::IsFinite(Destination.Z))
	{
		return MakeRejected(EGP_MovementRejectReason::InvalidDestination);
	}

	if (!(MoveSpeed > 0.0f) || !FMath::IsFinite(MoveSpeed))
	{
		return MakeRejected(EGP_MovementRejectReason::InvalidMoveSpeed);
	}

	if (AcceptanceRadius < 0.0f || !FMath::IsFinite(AcceptanceRadius))
	{
		return MakeRejected(EGP_MovementRejectReason::InvalidAcceptanceRadius);
	}

	const FVector StartLocation = Owner->GetActorLocation();

	FGP_MovementRequestOutcome Accepted;
	Accepted.Status = EGP_MovementRequestStatus::Accepted;
	Accepted.RejectReason = EGP_MovementRejectReason::None;

	if (bIsMoving)
	{
		const uint32 PreviousSerial = ActiveMoveSerial;
		const FVector PreviousDestination = MoveDestination;

		// Commit new active state before broadcasting Cancelled for the old serial.
		MoveDestination = Destination;
		ActiveMoveSerial = CommandSerial;
		bIsMoving = true;
		SetComponentTickEnabled(true);

		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement MoveReplaced: Unit=%s PreviousSerial=%u NewSerial=%u PreviousDestination=%s NewDestination=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			PreviousSerial,
			ActiveMoveSerial,
			*PreviousDestination.ToCompactString(),
			*MoveDestination.ToCompactString(),
			GPUnitMovementPrivate::RoleToString(Role),
			GPUnitMovementPrivate::NetModeToString(NetMode));

		BroadcastMovementResult(
			PreviousSerial,
			EGP_MovementResult::Cancelled,
			EGP_MovementResultReason::Superseded,
			PreviousDestination);
		// No further mutation of the superseded serial; reentrant RequestMove is allowed.
		return Accepted;
	}

	MoveDestination = Destination;
	ActiveMoveSerial = CommandSerial;
	bIsMoving = true;
	SetComponentTickEnabled(true);

	UE_LOG(LogGPUnitMovement, Log,
		TEXT("GP UnitMovement MoveStarted: Unit=%s Serial=%u Destination=%s StartLocation=%s Speed=%.1f AcceptanceRadius=%.1f Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveMoveSerial,
		*MoveDestination.ToCompactString(),
		*StartLocation.ToCompactString(),
		MoveSpeed,
		AcceptanceRadius,
		GPUnitMovementPrivate::RoleToString(Role),
		GPUnitMovementPrivate::NetModeToString(NetMode));
	return Accepted;
}

void UGP_MovementComponent::StopMove(EGP_MovementStopReason Reason)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	// EndPlay / OwnerDied teardown must not reject for missing authority.
	if (Reason != EGP_MovementStopReason::EndPlay
		&& Reason != EGP_MovementStopReason::OwnerDied)
	{
		if (Owner == nullptr || !Owner->HasAuthority())
		{
			UE_LOG(LogGPUnitMovement, Log,
				TEXT("GP UnitMovement MoveRejected: Unit=%s Serial=%u Destination=%s Reason=%s Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				ActiveMoveSerial,
				*MoveDestination.ToCompactString(),
				Owner == nullptr ? TEXT("MissingOwner") : TEXT("NoAuthority"),
				GPUnitMovementPrivate::RoleToString(Role),
				GPUnitMovementPrivate::NetModeToString(NetMode));
			return;
		}
	}

	if (!bIsMoving)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const uint32 PreviousSerial = ActiveMoveSerial;
	const FVector PreviousDestination = MoveDestination;
	const FVector Location = Owner != nullptr ? Owner->GetActorLocation() : FVector::ZeroVector;

	ClearActiveMovementState();

	UE_LOG(LogGPUnitMovement, Log,
		TEXT("GP UnitMovement MoveStopped: Unit=%s Serial=%u Reason=%s Location=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		PreviousSerial,
		StopReasonToString(Reason),
		*Location.ToCompactString(),
		GPUnitMovementPrivate::RoleToString(Role),
		GPUnitMovementPrivate::NetModeToString(NetMode));

	// Silent clear — command/death owners clear Held themselves.
	if (Reason == EGP_MovementStopReason::EndPlay
		|| Reason == EGP_MovementStopReason::OwnerDied)
	{
		return;
	}

	const EGP_MovementResultReason ResultReason =
		(Reason == EGP_MovementStopReason::CommandReplaced)
			? EGP_MovementResultReason::CommandReplaced
			: EGP_MovementResultReason::Manual;

	BroadcastMovementResult(
		PreviousSerial,
		EGP_MovementResult::Cancelled,
		ResultReason,
		PreviousDestination);
}

bool UGP_MovementComponent::IsMoving() const
{
	return bIsMoving;
}

uint32 UGP_MovementComponent::GetActiveMoveSerial() const
{
	return ActiveMoveSerial;
}

const FVector& UGP_MovementComponent::GetMoveDestination() const
{
	return MoveDestination;
}

void UGP_MovementComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsMoving)
	{
		SetComponentTickEnabled(false);
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
		const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

		UE_LOG(LogGPUnitMovement, Warning,
			TEXT("GP UnitMovement MoveRejected: Unit=%s Serial=%u Destination=%s Reason=TickWithoutAuthority Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ActiveMoveSerial,
			*MoveDestination.ToCompactString(),
			GPUnitMovementPrivate::RoleToString(Role),
			GPUnitMovementPrivate::NetModeToString(NetMode));

		ClearActiveMovementState();
		return;
	}

	const FVector CurrentLocation = Owner->GetActorLocation();
	const FVector2D Delta2D(MoveDestination.X - CurrentLocation.X, MoveDestination.Y - CurrentLocation.Y);
	const float Distance2D = Delta2D.Size();

	auto FinishReached = [&](const FVector& FinalLocation)
	{
		const uint32 CompletedSerial = ActiveMoveSerial;
		const FVector DestinationForLog = MoveDestination;
		const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
		const ENetRole Role = Owner->GetLocalRole();

		ClearActiveMovementState();

		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement MoveReached: Unit=%s Serial=%u Destination=%s FinalLocation=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			CompletedSerial,
			*DestinationForLog.ToCompactString(),
			*FinalLocation.ToCompactString(),
			GPUnitMovementPrivate::RoleToString(Role),
			GPUnitMovementPrivate::NetModeToString(NetMode));

		BroadcastMovementResult(
			CompletedSerial,
			EGP_MovementResult::Reached,
			EGP_MovementResultReason::None,
			DestinationForLog);
		// No further movement-state mutation after Broadcast (reentrancy-safe).
	};

	if (Distance2D <= AcceptanceRadius)
	{
		FinishReached(CurrentLocation);
		return;
	}

	const float StepDistance = FMath::Min(MoveSpeed * DeltaTime, Distance2D);
	const FVector2D Direction2D = Delta2D / Distance2D;
	const FVector NextLocation(
		CurrentLocation.X + Direction2D.X * StepDistance,
		CurrentLocation.Y + Direction2D.Y * StepDistance,
		CurrentLocation.Z);

	Owner->SetActorLocation(NextLocation, false);

	if (bRotateToMovement && RotationSpeed > 0.0f)
	{
		const FRotator CurrentRotation = Owner->GetActorRotation();
		const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Direction2D.Y, Direction2D.X));
		const FRotator TargetRotation(0.0f, TargetYaw, 0.0f);
		const FRotator NewRotation = FMath::RInterpConstantTo(CurrentRotation, TargetRotation, DeltaTime, RotationSpeed);
		Owner->SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
	}

	if (StepDistance >= Distance2D)
	{
		const FVector ExactXY(MoveDestination.X, MoveDestination.Y, CurrentLocation.Z);
		Owner->SetActorLocation(ExactXY, false);
		FinishReached(ExactXY);
		return;
	}
}

FGP_OnMovementResult& UGP_MovementComponent::OnMovementResult()
{
	return MovementResultDelegate;
}

#if !UE_BUILD_SHIPPING
void UGP_MovementComponent::DebugBroadcastResult(
	uint32 Serial,
	EGP_MovementResult Result,
	EGP_MovementResultReason Reason)
{
	BroadcastMovementResult(Serial, Result, Reason, MoveDestination);
}
#endif

void UGP_MovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopMove(EGP_MovementStopReason::EndPlay);
	Super::EndPlay(EndPlayReason);
}

#if !UE_BUILD_SHIPPING
namespace GPMovementConsolePrivate
{
	static const TCHAR* ResultToString(EGP_MovementResult Result)
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

	static const TCHAR* ReasonToString(EGP_MovementResultReason Reason)
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

	static const TCHAR* RejectToString(EGP_MovementRejectReason Reason)
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

	static const TCHAR* StatusToString(EGP_MovementRequestStatus Status)
	{
		switch (Status)
		{
		case EGP_MovementRequestStatus::Accepted:
			return TEXT("Accepted");
		case EGP_MovementRequestStatus::Rejected:
			return TEXT("Rejected");
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

	static AGP_MobileUnit* FindFirstAuthorityMovingMobileUnit(UWorld* World)
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

			UGP_MovementComponent* Movement = MobileUnit->GetUnitMovementComponent();
			if (Movement != nullptr && Movement->IsMoving())
			{
				return MobileUnit;
			}
		}

		return nullptr;
	}

	static AGP_MobileUnit* ResolveTestResultTarget(UWorld* World, const TCHAR*& OutSelection)
	{
		OutSelection = TEXT("MovingUnit");
		AGP_MobileUnit* MobileUnit = FindFirstAuthorityMovingMobileUnit(World);
		if (MobileUnit == nullptr)
		{
			OutSelection = TEXT("FallbackFirstAuthority");
			MobileUnit = FindFirstAuthorityMobileUnit(World);
			if (MobileUnit != nullptr)
			{
				UE_LOG(LogGPUnitMovement, Log,
					TEXT("GP UnitMovement Console: gp.Movement.TestResult no moving authority unit; using fallback first authority"));
			}
		}
		return MobileUnit;
	}

	static void MovementTest(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning, TEXT("GP UnitMovement Console: gp.Movement.Test missing world"));
			return;
		}

		if (Args.Num() < 2)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: usage gp.Movement.Test X Y [Serial]"));
			return;
		}

		const float X = FCString::Atof(*Args[0]);
		const float Y = FCString::Atof(*Args[1]);
		uint32 Serial = 1;
		if (Args.Num() >= 3)
		{
			const int64 Parsed = FCString::Atoi64(*Args[2]);
			if (Parsed > 0)
			{
				Serial = static_cast<uint32>(Parsed);
			}
			else
			{
				Serial = 0;
			}
		}

		AGP_MobileUnit* MobileUnit = FindFirstAuthorityMobileUnit(World);
		if (MobileUnit == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: no authority AGP_MobileUnit found"));
			return;
		}

		UGP_MovementComponent* Movement = MobileUnit->GetUnitMovementComponent();
		if (Movement == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: Unit=%s missing MovementComponent"),
				*MobileUnit->GetName());
			return;
		}

		const FVector Current = MobileUnit->GetActorLocation();
		const FVector Destination(X, Y, Current.Z);
		const FGP_MovementRequestOutcome Outcome = Movement->RequestMove(Destination, Serial);
		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement Console: gp.Movement.Test Unit=%s Status=%s RejectReason=%s Serial=%u Destination=%s"),
			*MobileUnit->GetName(),
			StatusToString(Outcome.Status),
			RejectToString(Outcome.RejectReason),
			Serial,
			*Destination.ToCompactString());
	}

	static void MovementStop(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning, TEXT("GP UnitMovement Console: gp.Movement.Stop missing world"));
			return;
		}

		AGP_MobileUnit* MobileUnit = FindFirstAuthorityMovingMobileUnit(World);
		if (MobileUnit == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: gp.Movement.Stop no moving authority unit"));
			return;
		}

		UGP_MovementComponent* Movement = MobileUnit->GetUnitMovementComponent();
		if (Movement == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: Unit=%s missing MovementComponent"),
				*MobileUnit->GetName());
			return;
		}

		const uint32 ActiveSerialBefore = Movement->GetActiveMoveSerial();
		const bool bWasMovingBefore = Movement->IsMoving();

		Movement->StopMove(EGP_MovementStopReason::Manual);
		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement Console: gp.Movement.Stop Unit=%s ActiveSerialBefore=%u WasMovingBefore=%s Selection=MovingUnit"),
			*MobileUnit->GetName(),
			ActiveSerialBefore,
			bWasMovingBefore ? TEXT("true") : TEXT("false"));
	}

	static bool ParseResultToken(const FString& Token, EGP_MovementResult& OutResult)
	{
		if (Token.Equals(TEXT("Reached"), ESearchCase::IgnoreCase))
		{
			OutResult = EGP_MovementResult::Reached;
			return true;
		}
		if (Token.Equals(TEXT("Cancelled"), ESearchCase::IgnoreCase))
		{
			OutResult = EGP_MovementResult::Cancelled;
			return true;
		}
		return false;
	}

	static bool ParseReasonToken(const FString& Token, EGP_MovementResultReason& OutReason)
	{
		if (Token.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			OutReason = EGP_MovementResultReason::None;
			return true;
		}
		if (Token.Equals(TEXT("Manual"), ESearchCase::IgnoreCase))
		{
			OutReason = EGP_MovementResultReason::Manual;
			return true;
		}
		if (Token.Equals(TEXT("Superseded"), ESearchCase::IgnoreCase))
		{
			OutReason = EGP_MovementResultReason::Superseded;
			return true;
		}
		if (Token.Equals(TEXT("CommandReplaced"), ESearchCase::IgnoreCase))
		{
			OutReason = EGP_MovementResultReason::CommandReplaced;
			return true;
		}
		return false;
	}

	static void DispatchTestResult(
		UWorld* World,
		uint32 Serial,
		EGP_MovementResult Result,
		EGP_MovementResultReason Reason,
		const TCHAR* CommandName)
	{
		if (Serial == 0)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: %s requires nonzero Serial"),
				CommandName);
			return;
		}

		if (Result == EGP_MovementResult::Reached && Reason != EGP_MovementResultReason::None)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: %s Reached requires Reason=None"),
				CommandName);
			return;
		}

		if (Result == EGP_MovementResult::Cancelled
			&& Reason != EGP_MovementResultReason::Manual
			&& Reason != EGP_MovementResultReason::Superseded
			&& Reason != EGP_MovementResultReason::CommandReplaced)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: %s Cancelled requires Reason=Manual|Superseded|CommandReplaced"),
				CommandName);
			return;
		}

		const TCHAR* Selection = TEXT("MovingUnit");
		AGP_MobileUnit* MobileUnit = ResolveTestResultTarget(World, Selection);
		if (MobileUnit == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: no authority AGP_MobileUnit found"));
			return;
		}

		UGP_MovementComponent* Movement = MobileUnit->GetUnitMovementComponent();
		if (Movement == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: Unit=%s missing MovementComponent"),
				*MobileUnit->GetName());
			return;
		}

		Movement->DebugBroadcastResult(Serial, Result, Reason);
		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement Console: %s Unit=%s InjectedSerial=%u ActiveMoveSerial=%u IsMoving=%s Selection=%s Result=%s Reason=%s"),
			CommandName,
			*MobileUnit->GetName(),
			Serial,
			Movement->GetActiveMoveSerial(),
			Movement->IsMoving() ? TEXT("true") : TEXT("false"),
			Selection,
			ResultToString(Result),
			ReasonToString(Reason));
	}

	static void MovementTestResult(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: gp.Movement.TestResult missing world"));
			return;
		}

		if (Args.Num() < 2)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: usage gp.Movement.TestResult <Serial> <Reached|Cancelled> [Reason]"));
			return;
		}

		const int64 Parsed = FCString::Atoi64(*Args[0]);
		if (Parsed <= 0)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: gp.Movement.TestResult requires nonzero Serial"));
			return;
		}

		EGP_MovementResult Result;
		if (!ParseResultToken(Args[1], Result))
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: usage gp.Movement.TestResult <Serial> <Reached|Cancelled> [Reason]"));
			return;
		}

		EGP_MovementResultReason Reason = EGP_MovementResultReason::None;
		if (Result == EGP_MovementResult::Cancelled)
		{
			Reason = EGP_MovementResultReason::Manual;
		}

		if (Args.Num() >= 3)
		{
			if (!ParseReasonToken(Args[2], Reason))
			{
				UE_LOG(LogGPUnitMovement, Warning,
					TEXT("GP UnitMovement Console: usage gp.Movement.TestResult <Serial> <Reached|Cancelled> [None|Manual|Superseded|CommandReplaced]"));
				return;
			}
		}

		DispatchTestResult(
			World,
			static_cast<uint32>(Parsed),
			Result,
			Reason,
			TEXT("gp.Movement.TestResult"));
	}

	static void MovementTestCompletion(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: gp.Movement.TestCompletion missing world"));
			return;
		}

		if (Args.Num() < 1)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: usage gp.Movement.TestCompletion <Serial> (deprecated alias → TestResult Reached None)"));
			return;
		}

		const int64 Parsed = FCString::Atoi64(*Args[0]);
		if (Parsed <= 0)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: gp.Movement.TestCompletion requires nonzero Serial"));
			return;
		}

		DispatchTestResult(
			World,
			static_cast<uint32>(Parsed),
			EGP_MovementResult::Reached,
			EGP_MovementResultReason::None,
			TEXT("gp.Movement.TestCompletion"));
	}

	static FAutoConsoleCommandWithWorldAndArgs GMovementTestCommand(
		TEXT("gp.Movement.Test"),
		TEXT("GP-S20/S23: RequestMove first authority AGP_MobileUnit to X Y [Serial]. Z from unit."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementTest));

	static FAutoConsoleCommandWithWorldAndArgs GMovementStopCommand(
		TEXT("gp.Movement.Stop"),
		TEXT("GP-S23: StopMove(Manual) on first moving authority AGP_MobileUnit (no idle fallback)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementStop));

	static FAutoConsoleCommandWithWorldAndArgs GMovementTestResultCommand(
		TEXT("gp.Movement.TestResult"),
		TEXT("GP-S23: synthetic MovementResult broadcast <Serial> <Reached|Cancelled> [Reason]. No movement mutation."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementTestResult));

	static FAutoConsoleCommandWithWorldAndArgs GMovementTestCompletionCommand(
		TEXT("gp.Movement.TestCompletion"),
		TEXT("GP-S23 deprecated alias: synthetic Reached/None (maps to gp.Movement.TestResult)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementTestCompletion));
}
#endif
