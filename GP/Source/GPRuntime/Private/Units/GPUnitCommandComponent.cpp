// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPUnitCommandComponent.h"

#include "Command/GPUnitCommand.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"

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
}

UGP_UnitCommandComponent::UGP_UnitCommandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UGP_UnitCommandComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

	SynchronizeMovementWithHeldCommand(PreviousCommand);

	if (bHadHeldCommand)
	{
		UE_LOG(LogGPUnitCommandState, Log,
			TEXT("GP UnitCommandState HeldReplaced: Unit=%s PreviousSerial=%u NewSerial=%u PreviousTag=%s NewTag=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			PreviousCommand.GetValue().CommandSerial,
			Stored.CommandSerial,
			*PreviousCommand.GetValue().CommandTag.ToString(),
			*Stored.CommandTag.ToString(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
	}
	else
	{
		UE_LOG(LogGPUnitCommandState, Log,
			TEXT("GP UnitCommandState HeldAccepted: Unit=%s Serial=%u Tag=%s TargetActor=%s Loc=%s Queue=false Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			Stored.CommandSerial,
			*Stored.CommandTag.ToString(),
			*GetNameSafe(Stored.TargetActor.Get()),
			*Stored.TargetLocation.ToCompactString(),
			GPUnitCommandStatePrivate::RoleToString(Role),
			GPUnitCommandStatePrivate::NetModeToString(NetMode));
	}
}

void UGP_UnitCommandComponent::SynchronizeMovementWithHeldCommand(
	const TOptional<FGP_StoredUnitCommand>& PreviousCommand)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitCommandStatePrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	if (!HeldCommand.IsSet())
	{
		return;
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
		return;
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
		return;
	}

	if (bCurrentIsMove)
	{
		const bool bAccepted = Movement->RequestMove(CurrentHeld.TargetLocation, CurrentHeld.CommandSerial);
		if (bAccepted)
		{
			UE_LOG(LogGPUnitCommandExecution, Log,
				TEXT("GP UnitCommandExecution MoveExecutionRequested: Unit=%s Serial=%u Destination=%s PreviousSerial=%u PreviousTag=%s Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				CurrentHeld.CommandSerial,
				*CurrentHeld.TargetLocation.ToCompactString(),
				PreviousSerial,
				*PreviousTagString,
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
		}
		else
		{
			UE_LOG(LogGPUnitCommandExecution, Warning,
				TEXT("GP UnitCommandExecution MoveExecutionRejected: Unit=%s Serial=%u Destination=%s Role=%s NetMode=%s"),
				*GetNameSafe(Owner),
				CurrentHeld.CommandSerial,
				*CurrentHeld.TargetLocation.ToCompactString(),
				GPUnitCommandStatePrivate::RoleToString(Role),
				GPUnitCommandStatePrivate::NetModeToString(NetMode));
		}
		return;
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
