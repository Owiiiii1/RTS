// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitCommandComponent.h"

#include "Command/GPUnitCommand.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"

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

UGP_UnitCommandComponent::UGP_UnitCommandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

void UGP_UnitCommandComponent::HandleMovementResult(
	uint32 Serial,
	EGP_MovementResult Result,
	EGP_MovementResultReason Reason)
{
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

	const bool bHeldRemainsAfterSync = SynchronizeMovementWithHeldCommand(PreviousCommand);
	if (!bHeldRemainsAfterSync || !HeldCommand.IsSet())
	{
		// Move RequestMove rejected and cleared Held — do not emit HeldAccepted/HeldReplaced.
		return;
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

void UGP_UnitCommandComponent::ClearHeldCommand()
{
	HeldCommand.Reset();
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

	static FAutoConsoleCommandWithWorldAndArgs GUnitCommandTestRejectedMoveCommand(
		TEXT("gp.UnitCommand.TestRejectedMove"),
		TEXT("GP-S23 non-shipping: HandleCommand Move with non-finite destination to validate phantom-Held clear."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UnitCommandTestRejectedMove));
}
#endif
