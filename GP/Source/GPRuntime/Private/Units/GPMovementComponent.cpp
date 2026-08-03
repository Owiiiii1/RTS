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

bool UGP_MovementComponent::RequestMove(const FVector& Destination, uint32 CommandSerial)
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	auto LogRejected = [&](const TCHAR* Reason)
	{
		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement MoveRejected: Unit=%s Serial=%u Destination=%s Reason=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			CommandSerial,
			*Destination.ToCompactString(),
			Reason,
			GPUnitMovementPrivate::RoleToString(Role),
			GPUnitMovementPrivate::NetModeToString(NetMode));
	};

	if (Owner == nullptr)
	{
		LogRejected(TEXT("MissingOwner"));
		return false;
	}

	if (!Owner->HasAuthority())
	{
		LogRejected(TEXT("NoAuthority"));
		return false;
	}

	if (CommandSerial == 0)
	{
		LogRejected(TEXT("InvalidSerial"));
		return false;
	}

	if (Destination.ContainsNaN()
		|| !FMath::IsFinite(Destination.X)
		|| !FMath::IsFinite(Destination.Y)
		|| !FMath::IsFinite(Destination.Z))
	{
		LogRejected(TEXT("InvalidDestination"));
		return false;
	}

	if (!(MoveSpeed > 0.0f))
	{
		LogRejected(TEXT("InvalidMoveSpeed"));
		return false;
	}

	if (AcceptanceRadius < 0.0f)
	{
		LogRejected(TEXT("InvalidAcceptanceRadius"));
		return false;
	}

	const FVector StartLocation = Owner->GetActorLocation();

	if (bIsMoving)
	{
		const uint32 PreviousSerial = ActiveMoveSerial;
		const FVector PreviousDestination = MoveDestination;

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
		return true;
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
	return true;
}

void UGP_MovementComponent::StopMove()
{
	AActor* Owner = GetOwner();
	const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

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

	if (!bIsMoving)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const uint32 PreviousSerial = ActiveMoveSerial;
	const FVector Location = Owner->GetActorLocation();

	bIsMoving = false;
	ActiveMoveSerial = 0;
	SetComponentTickEnabled(false);

	UE_LOG(LogGPUnitMovement, Log,
		TEXT("GP UnitMovement MoveStopped: Unit=%s Serial=%u Reason=Manual Location=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		PreviousSerial,
		*Location.ToCompactString(),
		GPUnitMovementPrivate::RoleToString(Role),
		GPUnitMovementPrivate::NetModeToString(NetMode));
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
		const uint32 ReachedSerial = ActiveMoveSerial;
		const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
		const ENetRole Role = Owner->GetLocalRole();

		bIsMoving = false;
		ActiveMoveSerial = 0;
		SetComponentTickEnabled(false);

		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement MoveReached: Unit=%s Serial=%u Destination=%s FinalLocation=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			ReachedSerial,
			*MoveDestination.ToCompactString(),
			*FinalLocation.ToCompactString(),
			GPUnitMovementPrivate::RoleToString(Role),
			GPUnitMovementPrivate::NetModeToString(NetMode));
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
	}
}

void UGP_MovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bIsMoving)
	{
		AActor* Owner = GetOwner();
		const uint32 PreviousSerial = ActiveMoveSerial;
		const FVector Location = Owner != nullptr ? Owner->GetActorLocation() : FVector::ZeroVector;
		const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
		const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

		bIsMoving = false;
		ActiveMoveSerial = 0;
		SetComponentTickEnabled(false);

		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement MoveStopped: Unit=%s Serial=%u Reason=EndPlay Location=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			PreviousSerial,
			*Location.ToCompactString(),
			GPUnitMovementPrivate::RoleToString(Role),
			GPUnitMovementPrivate::NetModeToString(NetMode));
	}
	else
	{
		SetComponentTickEnabled(false);
	}

	Super::EndPlay(EndPlayReason);
}

#if !UE_BUILD_SHIPPING
namespace GPMovementConsolePrivate
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
		const bool bAccepted = Movement->RequestMove(Destination, Serial);
		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement Console: gp.Movement.Test Unit=%s Accepted=%s Serial=%u Destination=%s"),
			*MobileUnit->GetName(),
			bAccepted ? TEXT("true") : TEXT("false"),
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

		Movement->StopMove();
		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement Console: gp.Movement.Stop Unit=%s"),
			*MobileUnit->GetName());
	}

	static FAutoConsoleCommandWithWorldAndArgs GMovementTestCommand(
		TEXT("gp.Movement.Test"),
		TEXT("GP-S20: RequestMove first authority AGP_MobileUnit to X Y [Serial]. Z from unit."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementTest));

	static FAutoConsoleCommandWithWorldAndArgs GMovementStopCommand(
		TEXT("gp.Movement.Stop"),
		TEXT("GP-S20: StopMove on first authority AGP_MobileUnit."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementStop));
}
#endif
