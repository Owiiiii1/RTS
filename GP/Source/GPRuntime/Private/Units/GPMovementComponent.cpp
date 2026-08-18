// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPMovementComponent.h"

#include "AI/NavigationSystemBase.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
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

	/** Map TryBuildNavigationPath failure to reject reason (nav was available). */
	static EGP_MovementRejectReason MapNavBuildFailureToReject(
		UWorld* World,
		const FVector& Destination,
		float ExtentXY,
		float ExtentZ)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		if (NavSys != nullptr)
		{
			FNavLocation ProjectedDest;
			const FVector Extent(ExtentXY, ExtentXY, ExtentZ);
			if (!NavSys->ProjectPointToNavigation(Destination, ProjectedDest, Extent))
			{
				return EGP_MovementRejectReason::DestinationOffNav;
			}
		}
		return EGP_MovementRejectReason::PathNotFound;
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
	PathPoints.Reset();
	PathIndex = 0;
	bActivePathFromNavigation = false;
	LastRepathWorldTime = -1.0;
	LastProgressWorldTime = -1.0;
	LastProgressLocation = FVector::ZeroVector;
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
	case EGP_MovementResult::Failed:
		return TEXT("Failed");
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
	case EGP_MovementRejectReason::PathNotFound:
		return TEXT("PathNotFound");
	case EGP_MovementRejectReason::DestinationOffNav:
		return TEXT("DestinationOffNav");
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

bool UGP_MovementComponent::TryBuildNavigationPath(
	const FVector& Start,
	const FVector& Dest,
	TArray<FVector>& OutPoints,
	bool& bOutUsedNav)
{
	OutPoints.Reset();
	bOutUsedNav = false;

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (World == nullptr || Owner == nullptr)
	{
		OutPoints.Add(Start);
		OutPoints.Add(Dest);
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavData =
		(NavSys != nullptr) ? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;

	if (NavSys == nullptr || NavData == nullptr)
	{
		// Straight-line fallback when NavMesh is unavailable.
		OutPoints.Add(Start);
		OutPoints.Add(Dest);
		bOutUsedNav = false;
		return true;
	}

	const FVector Extent(NavProjectionExtentXY, NavProjectionExtentXY, NavProjectionExtentZ);
	const float OwnerZ = Start.Z;

	FNavLocation ProjectedStart;
	if (!NavSys->ProjectPointToNavigation(Start, ProjectedStart, Extent))
	{
		// Unit is outside nav coverage (isolation coords / missing local bounds).
		// Straight-line fallback preserves Move/Attack contracts; do not reject.
		OutPoints.Add(Start);
		OutPoints.Add(Dest);
		bOutUsedNav = false;
		return true;
	}

	const FVector PathStart = ProjectedStart.Location;

	FNavLocation ProjectedDest;
	if (!NavSys->ProjectPointToNavigation(Dest, ProjectedDest, Extent))
	{
		return false;
	}

	const float DistNav2D = FVector::Dist2D(PathStart, ProjectedDest.Location);
	const float TrivialPathCm = FMath::Max(AcceptanceRadius * 2.0f, 200.0f);

	auto MakeProjectedStraightPath = [&]()
	{
		OutPoints.Reset();
		OutPoints.Add(FVector(PathStart.X, PathStart.Y, OwnerZ));
		OutPoints.Add(FVector(ProjectedDest.Location.X, ProjectedDest.Location.Y, OwnerZ));
		bOutUsedNav = true;
	};

	// Short on-nav legs (mine corrective / micro-adjust) skip FindPathSync — Recast often fails these.
	if (DistNav2D <= TrivialPathCm)
	{
		MakeProjectedStraightPath();
		FinalizeNavRuntimePath(Start, PathStart, OutPoints);
		return true;
	}

	FPathFindingQuery PathQuery(Owner, *NavData, PathStart, ProjectedDest.Location);
	PathQuery.SetAllowPartialPaths(false);

	const FPathFindingResult PathResult = NavSys->FindPathSync(PathQuery);
	if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid())
	{
		// Medium on-nav distance with sync-path failure: still allow projected straight
		// (avoid clearing Mine/Haul held on flaky local Recast). Far unreachable stays rejected.
		constexpr float SoftStraightMaxCm = 900.0f;
		if (DistNav2D <= SoftStraightMaxCm)
		{
			MakeProjectedStraightPath();
			FinalizeNavRuntimePath(Start, PathStart, OutPoints);
			return true;
		}
		return false;
	}

	const TArray<FNavPathPoint>& NavPoints = PathResult.Path->GetPathPoints();
	OutPoints.Reserve(FMath::Max(NavPoints.Num(), 1));
	for (const FNavPathPoint& NavPoint : NavPoints)
	{
		// Keep actor Z for movement; store XY from nav.
		OutPoints.Add(FVector(NavPoint.Location.X, NavPoint.Location.Y, OwnerZ));
	}

	if (OutPoints.Num() == 0)
	{
		OutPoints.Add(FVector(ProjectedDest.Location.X, ProjectedDest.Location.Y, OwnerZ));
	}
	else
	{
		FVector& FinalPoint = OutPoints.Last();
		FinalPoint.X = ProjectedDest.Location.X;
		FinalPoint.Y = ProjectedDest.Location.Y;
		FinalPoint.Z = OwnerZ;
	}

	bOutUsedNav = true;
	FinalizeNavRuntimePath(Start, PathStart, OutPoints);
	return true;
}

void UGP_MovementComponent::StripProjectedStartAnchor(const FVector& ProjectedStart, TArray<FVector>& InOutPoints) const
{
	if (InOutPoints.Num() <= 1)
	{
		return;
	}

	const float AnchorTolCm = FMath::Max(AcceptanceRadius, 25.0f);
	if (FVector::Dist2D(InOutPoints[0], ProjectedStart) > AnchorTolCm)
	{
		return;
	}

	// Recast / projected-straight first point is the FindPathSync query anchor.
	// The actor already exists at ActualStart and must not walk back to that anchor.
	InOutPoints.RemoveAt(0);
}

void UGP_MovementComponent::FinalizeNavRuntimePath(
	const FVector& ActualStart,
	const FVector& ProjectedStart,
	TArray<FVector>& InOutPoints)
{
#if !UE_BUILD_SHIPPING
	DebugLastActualStart = ActualStart;
	DebugLastProjectedStart = ProjectedStart;
	DebugLastRawNavPath0 = InOutPoints.Num() > 0 ? InOutPoints[0] : FVector::ZeroVector;
#endif
	StripProjectedStartAnchor(ProjectedStart, InOutPoints);
	const FVector Raw0 =
#if !UE_BUILD_SHIPPING
		DebugLastRawNavPath0;
#else
		FVector::ZeroVector;
#endif
	UE_LOG(LogGPUnitMovement, Log,
		TEXT("GP UnitMovement PathAnchor: ActualStart=%s ProjectedStart=%s RawPath0=%s Path0=%s Path1=%s DistActualProjected=%.1f PathPoints=%d"),
		*ActualStart.ToCompactString(),
		*ProjectedStart.ToCompactString(),
		*Raw0.ToCompactString(),
		InOutPoints.Num() > 0 ? *InOutPoints[0].ToCompactString() : TEXT("none"),
		InOutPoints.Num() > 1 ? *InOutPoints[1].ToCompactString() : TEXT("none"),
		FVector::Dist2D(ActualStart, ProjectedStart),
		InOutPoints.Num());
}

bool UGP_MovementComponent::TryGetActivePathPoint(int32 Index, FVector& OutPoint) const
{
	if (!PathPoints.IsValidIndex(Index))
	{
		return false;
	}
	OutPoint = PathPoints[Index];
	return true;
}

FVector UGP_MovementComponent::ComputeSteeringOffset(
	const FVector& OwnerLocation,
	const FVector& DesiredDir2D) const
{
	(void)DesiredDir2D;

	if (SeparationRadius <= KINDA_SMALL_NUMBER
		|| MaxSteeringContribution <= KINDA_SMALL_NUMBER
		|| SeparationStrength <= KINDA_SMALL_NUMBER
		|| !(MoveSpeed > 0.0f))
	{
		return FVector::ZeroVector;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (Owner == nullptr || World == nullptr)
	{
		return FVector::ZeroVector;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GPMoveSeparation), false, Owner);
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(SeparationRadius);
	World->OverlapMultiByChannel(
		Overlaps,
		OwnerLocation,
		FQuat::Identity,
		ECC_Pawn,
		Sphere,
		QueryParams);

	FVector Separation2D = FVector::ZeroVector;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Other = Overlap.GetActor();
		if (Other == nullptr || Other == Owner)
		{
			continue;
		}

		if (!Cast<AGP_MobileUnit>(Other))
		{
			continue;
		}

		FVector Away = OwnerLocation - Other->GetActorLocation();
		Away.Z = 0.0f;
		const float Dist = Away.Size();
		if (Dist <= KINDA_SMALL_NUMBER)
		{
			Away = FVector(1.0f, 0.0f, 0.0f);
		}
		else
		{
			Away /= Dist;
		}

		const float Falloff = 1.0f - FMath::Clamp(Dist / SeparationRadius, 0.0f, 1.0f);
		Separation2D += Away * (Falloff * SeparationStrength);
	}

	if (Separation2D.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const float DeltaTime = FMath::Max(World->GetDeltaSeconds(), 0.0f);
	const float MaxSteerLen = MaxSteeringContribution * MoveSpeed * DeltaTime;
	FVector Offset = Separation2D * MoveSpeed * DeltaTime;
	Offset.Z = 0.0f;

	const float OffsetLen = Offset.Size();
	if (OffsetLen > MaxSteerLen && OffsetLen > KINDA_SMALL_NUMBER)
	{
		Offset *= (MaxSteerLen / OffsetLen);
	}

	return Offset;
}

void UGP_MovementComponent::FinishMoveReached(AActor* Owner, const FVector& FinalLocation)
{
	const uint32 CompletedSerial = ActiveMoveSerial;
	const FVector DestinationForLog = MoveDestination;
	const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

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
}

void UGP_MovementComponent::FinishMoveFailed(AActor* Owner, EGP_MovementResultReason Reason)
{
	const uint32 FailedSerial = ActiveMoveSerial;
	const FVector DestinationForLog = MoveDestination;
	const ENetMode NetMode = GPUnitMovementPrivate::GetOwnerNetMode(Owner);
	const ENetRole Role = Owner != nullptr ? Owner->GetLocalRole() : ROLE_None;

	ClearActiveMovementState();

	UE_LOG(LogGPUnitMovement, Log,
		TEXT("GP UnitMovement MoveFailed: Unit=%s Serial=%u Reason=%s Destination=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		FailedSerial,
		MovementResultReasonToString(Reason),
		*DestinationForLog.ToCompactString(),
		GPUnitMovementPrivate::RoleToString(Role),
		GPUnitMovementPrivate::NetModeToString(NetMode));

	BroadcastMovementResult(
		FailedSerial,
		EGP_MovementResult::Failed,
		Reason,
		DestinationForLog);
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

	TArray<FVector> NewPath;
	bool bUsedNav = false;
	if (!TryBuildNavigationPath(StartLocation, Destination, NewPath, bUsedNav))
	{
		// Failure only occurs when nav is available but projection/pathfinding failed.
		if (bRequireNavigationWhenAvailable)
		{
			return MakeRejected(GPUnitMovementPrivate::MapNavBuildFailureToReject(
				GetWorld(),
				Destination,
				NavProjectionExtentXY,
				NavProjectionExtentZ));
		}

		NewPath.Reset();
		NewPath.Add(StartLocation);
		NewPath.Add(Destination);
		bUsedNav = false;
	}

	if (NewPath.Num() == 0)
	{
		NewPath.Add(StartLocation);
		NewPath.Add(Destination);
		bUsedNav = false;
	}

	FGP_MovementRequestOutcome Accepted;
	Accepted.Status = EGP_MovementRequestStatus::Accepted;
	Accepted.RejectReason = EGP_MovementRejectReason::None;

	UWorld* World = GetWorld();
	const double Now = (World != nullptr) ? World->GetTimeSeconds() : 0.0;

	if (bIsMoving)
	{
		const uint32 PreviousSerial = ActiveMoveSerial;
		const FVector PreviousDestination = MoveDestination;

		// Commit new active state (path + dest + serial) before broadcasting Cancelled.
		MoveDestination = Destination;
		ActiveMoveSerial = CommandSerial;
		bIsMoving = true;
		PathPoints = MoveTemp(NewPath);
		PathIndex = 0;
		bActivePathFromNavigation = bUsedNav;
		LastRepathWorldTime = Now;
		LastProgressWorldTime = Now;
		LastProgressLocation = StartLocation;
		SetComponentTickEnabled(true);

		UE_LOG(LogGPUnitMovement, Log,
			TEXT("GP UnitMovement MoveReplaced: Unit=%s PreviousSerial=%u NewSerial=%u PreviousDestination=%s NewDestination=%s PathPoints=%d UsedNav=%s Role=%s NetMode=%s"),
			*GetNameSafe(Owner),
			PreviousSerial,
			ActiveMoveSerial,
			*PreviousDestination.ToCompactString(),
			*MoveDestination.ToCompactString(),
			PathPoints.Num(),
			bActivePathFromNavigation ? TEXT("true") : TEXT("false"),
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
	PathPoints = MoveTemp(NewPath);
	PathIndex = 0;
	bActivePathFromNavigation = bUsedNav;
	LastRepathWorldTime = Now;
	LastProgressWorldTime = Now;
	LastProgressLocation = StartLocation;
	SetComponentTickEnabled(true);

	UE_LOG(LogGPUnitMovement, Log,
		TEXT("GP UnitMovement MoveStarted: Unit=%s Serial=%u Destination=%s StartLocation=%s Speed=%.1f AcceptanceRadius=%.1f PathPoints=%d UsedNav=%s Role=%s NetMode=%s"),
		*GetNameSafe(Owner),
		ActiveMoveSerial,
		*MoveDestination.ToCompactString(),
		*StartLocation.ToCompactString(),
		MoveSpeed,
		AcceptanceRadius,
		PathPoints.Num(),
		bActivePathFromNavigation ? TEXT("true") : TEXT("false"),
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

	UWorld* World = GetWorld();
	const double Now = (World != nullptr) ? World->GetTimeSeconds() : 0.0;
	const FVector CurrentLocation = Owner->GetActorLocation();

	if (PathPoints.Num() == 0)
	{
		FinishMoveFailed(Owner, EGP_MovementResultReason::PathInvalid);
		return;
	}

	PathIndex = FMath::Clamp(PathIndex, 0, PathPoints.Num() - 1);

	// Advance through reached intermediate waypoints.
	while (PathIndex < PathPoints.Num())
	{
		const FVector& Waypoint = PathPoints[PathIndex];
		const float DistToWaypoint2D = FVector::Dist2D(CurrentLocation, Waypoint);
		const bool bIsLastPoint = (PathIndex >= PathPoints.Num() - 1);

		if (DistToWaypoint2D > AcceptanceRadius)
		{
			break;
		}

		if (bIsLastPoint)
		{
			FinishMoveReached(Owner, CurrentLocation);
			return;
		}

		++PathIndex;
	}

	if (!PathPoints.IsValidIndex(PathIndex))
	{
		FinishMoveFailed(Owner, EGP_MovementResultReason::PathInvalid);
		return;
	}

	const FVector CurrentWaypoint = PathPoints[PathIndex];
	const FVector2D Delta2D(CurrentWaypoint.X - CurrentLocation.X, CurrentWaypoint.Y - CurrentLocation.Y);
	float Distance2D = Delta2D.Size();

	FVector2D DesiredDir2D = FVector2D::ZeroVector;
	if (Distance2D > KINDA_SMALL_NUMBER)
	{
		DesiredDir2D = Delta2D / Distance2D;
	}

	const float StepDist = FMath::Min(MoveSpeed * DeltaTime, Distance2D);
	const FVector DesiredDir3D(DesiredDir2D.X, DesiredDir2D.Y, 0.0f);
	const FVector Steering = ComputeSteeringOffset(CurrentLocation, DesiredDir3D);

	FVector Step = DesiredDir3D * StepDist + Steering;
	Step.Z = 0.0f;

	const float MaxStepLen = StepDist + (MaxSteeringContribution * MoveSpeed * DeltaTime);
	const float StepLen = Step.Size();
	if (StepLen > MaxStepLen && StepLen > KINDA_SMALL_NUMBER)
	{
		Step *= (MaxStepLen / StepLen);
	}

	const FVector NextLocation(
		CurrentLocation.X + Step.X,
		CurrentLocation.Y + Step.Y,
		CurrentLocation.Z);

	FHitResult Hit;
	// Sweep enabled for any future blocking channels; unit↔unit uses Overlap+separation, static via NavMesh.
	Owner->SetActorLocation(NextLocation, true, &Hit);

	const FVector AfterLocation = Owner->GetActorLocation();
	const float MovedDist2D = FVector::Dist2D(CurrentLocation, AfterLocation);
	const float ExpectedMove = FMath::Max(StepDist * 0.25f, 1.0f);
	// Only treat as blocked when a blocking hit actually stopped the step (avoid false Blocked on soft/overlap).
	const bool bBlockedSignificantly =
		Hit.bBlockingHit && Hit.Time < 0.99f && MovedDist2D < ExpectedMove && StepDist > KINDA_SMALL_NUMBER;

	if (!bBlockedSignificantly && MovedDist2D > KINDA_SMALL_NUMBER)
	{
		LastProgressWorldTime = Now;
		LastProgressLocation = AfterLocation;
	}
	else if (bBlockedSignificantly
		&& Now - LastProgressWorldTime >= static_cast<double>(BlockedFailSeconds))
	{
		FinishMoveFailed(Owner, EGP_MovementResultReason::Blocked);
		return;
	}
	else if (!bBlockedSignificantly && MovedDist2D <= KINDA_SMALL_NUMBER && StepDist <= KINDA_SMALL_NUMBER)
	{
		// Already at waypoint / zero step — still counts as progress for timer purposes.
		LastProgressWorldTime = Now;
		LastProgressLocation = AfterLocation;
	}

	// Rate-limited repath when following a nav path and stuck / waypoint is far.
	const float DistToWaypointAfter = FVector::Dist2D(AfterLocation, CurrentWaypoint);
	const float WaypointFarThreshold = FMath::Max(AcceptanceRadius * 4.0f, MoveSpeed * 0.5f);
	const bool bStuck = (Now - LastProgressWorldTime) >= static_cast<double>(RepathIntervalSeconds * 0.5f);
	const bool bWaypointFar = DistToWaypointAfter > WaypointFarThreshold;
	if (bActivePathFromNavigation
		&& (Now - LastRepathWorldTime) >= static_cast<double>(RepathIntervalSeconds)
		&& (bStuck || bWaypointFar))
	{
		TArray<FVector> RebuiltPath;
		bool bRebuiltUsedNav = false;
		LastRepathWorldTime = Now;

		if (!TryBuildNavigationPath(AfterLocation, MoveDestination, RebuiltPath, bRebuiltUsedNav)
			|| RebuiltPath.Num() == 0)
		{
			FinishMoveFailed(Owner, EGP_MovementResultReason::PathNotFound);
			return;
		}

		PathPoints = MoveTemp(RebuiltPath);
		PathIndex = 0;
		bActivePathFromNavigation = bRebuiltUsedNav;
	}

	if (bRotateToMovement && RotationSpeed > 0.0f)
	{
		FVector2D RotateDir = DesiredDir2D;
		const FVector2D Step2D(Step.X, Step.Y);
		if (Step2D.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			RotateDir = Step2D.GetSafeNormal();
		}

		if (RotateDir.SizeSquared() > KINDA_SMALL_NUMBER)
		{
			const FRotator CurrentRotation = Owner->GetActorRotation();
			const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(RotateDir.Y, RotateDir.X));
			const float NewYaw = ComputeShortestYawStep(
				CurrentRotation.Yaw,
				TargetYaw,
				RotationSpeed * DeltaTime);
			Owner->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
		}
	}

	// If this step closed the final waypoint, finish reached.
	if (PathIndex >= PathPoints.Num() - 1)
	{
		const float DistToFinal = FVector::Dist2D(AfterLocation, PathPoints.Last());
		if (DistToFinal <= AcceptanceRadius)
		{
			FinishMoveReached(Owner, AfterLocation);
		}
	}
}

FGP_OnMovementResult& UGP_MovementComponent::OnMovementResult()
{
	return MovementResultDelegate;
}

float UGP_MovementComponent::ComputeShortestYawStep(float CurrentYaw, float TargetYaw, float MaxAbsDeltaDegrees)
{
	if (!(MaxAbsDeltaDegrees > 0.0f) || !FMath::IsFinite(MaxAbsDeltaDegrees))
	{
		return FRotator::NormalizeAxis(CurrentYaw);
	}

	const float Delta = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
	if (!FMath::IsFinite(Delta) || FMath::Abs(Delta) <= MaxAbsDeltaDegrees)
	{
		return FRotator::NormalizeAxis(TargetYaw);
	}

	const float AppliedDelta = FMath::Clamp(Delta, -MaxAbsDeltaDegrees, MaxAbsDeltaDegrees);
	return FRotator::NormalizeAxis(CurrentYaw + AppliedDelta);
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
		case EGP_MovementResult::Failed:
			return TEXT("Failed");
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
		case EGP_MovementRejectReason::PathNotFound:
			return TEXT("PathNotFound");
		case EGP_MovementRejectReason::DestinationOffNav:
			return TEXT("DestinationOffNav");
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
		if (Token.Equals(TEXT("Failed"), ESearchCase::IgnoreCase))
		{
			OutResult = EGP_MovementResult::Failed;
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
		if (Token.Equals(TEXT("PathNotFound"), ESearchCase::IgnoreCase))
		{
			OutReason = EGP_MovementResultReason::PathNotFound;
			return true;
		}
		if (Token.Equals(TEXT("PathInvalid"), ESearchCase::IgnoreCase))
		{
			OutReason = EGP_MovementResultReason::PathInvalid;
			return true;
		}
		if (Token.Equals(TEXT("DestinationOffNav"), ESearchCase::IgnoreCase))
		{
			OutReason = EGP_MovementResultReason::DestinationOffNav;
			return true;
		}
		if (Token.Equals(TEXT("Blocked"), ESearchCase::IgnoreCase))
		{
			OutReason = EGP_MovementResultReason::Blocked;
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

		if (Result == EGP_MovementResult::Failed
			&& Reason != EGP_MovementResultReason::PathNotFound
			&& Reason != EGP_MovementResultReason::PathInvalid
			&& Reason != EGP_MovementResultReason::DestinationOffNav
			&& Reason != EGP_MovementResultReason::Blocked)
		{
			UE_LOG(LogGPUnitMovement, Warning,
				TEXT("GP UnitMovement Console: %s Failed requires Reason=PathNotFound|PathInvalid|DestinationOffNav|Blocked"),
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
				TEXT("GP UnitMovement Console: usage gp.Movement.TestResult <Serial> <Reached|Cancelled|Failed> [Reason]"));
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
				TEXT("GP UnitMovement Console: usage gp.Movement.TestResult <Serial> <Reached|Cancelled|Failed> [Reason]"));
			return;
		}

		EGP_MovementResultReason Reason = EGP_MovementResultReason::None;
		if (Result == EGP_MovementResult::Cancelled)
		{
			Reason = EGP_MovementResultReason::Manual;
		}
		else if (Result == EGP_MovementResult::Failed)
		{
			Reason = EGP_MovementResultReason::PathNotFound;
		}

		if (Args.Num() >= 3)
		{
			if (!ParseReasonToken(Args[2], Reason))
			{
				UE_LOG(LogGPUnitMovement, Warning,
					TEXT("GP UnitMovement Console: usage gp.Movement.TestResult <Serial> <Reached|Cancelled|Failed> [None|Manual|Superseded|CommandReplaced|PathNotFound|PathInvalid|DestinationOffNav|Blocked]"));
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
		TEXT("GP-S20/S23/S33M: RequestMove first authority AGP_MobileUnit to X Y [Serial]. Z from unit."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementTest));

	static FAutoConsoleCommandWithWorldAndArgs GMovementStopCommand(
		TEXT("gp.Movement.Stop"),
		TEXT("GP-S23: StopMove(Manual) on first moving authority AGP_MobileUnit (no idle fallback)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementStop));

	static FAutoConsoleCommandWithWorldAndArgs GMovementTestResultCommand(
		TEXT("gp.Movement.TestResult"),
		TEXT("GP-S23/S33M: synthetic MovementResult broadcast <Serial> <Reached|Cancelled|Failed> [Reason]. No movement mutation."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementTestResult));

	static FAutoConsoleCommandWithWorldAndArgs GMovementTestCompletionCommand(
		TEXT("gp.Movement.TestCompletion"),
		TEXT("GP-S23 deprecated alias: synthetic Reached/None (maps to gp.Movement.TestResult)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MovementTestCompletion));
}
#endif
