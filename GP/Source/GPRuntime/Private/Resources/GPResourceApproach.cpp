// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPResourceApproach.h"

#include "Components/BoxComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Resources/GPResourceNode.h"

const TCHAR* GPResourceApproach::DistanceModeToString(EGP_RangeApproachDistanceMode Mode)
{
	switch (Mode)
	{
	case EGP_RangeApproachDistanceMode::ThreeDimensional: return TEXT("ThreeDimensional");
	case EGP_RangeApproachDistanceMode::GroundPlane2D: return TEXT("GroundPlane2D");
	default: return TEXT("Unknown");
	}
}

const TCHAR* GPResourceApproach::RejectReasonToString(EGP_ResourceCandidateRejectReason Reason)
{
	switch (Reason)
	{
	case EGP_ResourceCandidateRejectReason::None: return TEXT("None");
	case EGP_ResourceCandidateRejectReason::InvalidNode: return TEXT("InvalidNode");
	case EGP_ResourceCandidateRejectReason::ExcludedNode: return TEXT("ExcludedNode");
	case EGP_ResourceCandidateRejectReason::Depleted: return TEXT("Depleted");
	case EGP_ResourceCandidateRejectReason::DestroyPending: return TEXT("DestroyPending");
	case EGP_ResourceCandidateRejectReason::ClearingOccupancy: return TEXT("ClearingOccupancy");
	case EGP_ResourceCandidateRejectReason::MineRejected: return TEXT("MineRejected");
	case EGP_ResourceCandidateRejectReason::IncompatibleResourceType: return TEXT("IncompatibleResourceType");
	case EGP_ResourceCandidateRejectReason::UnresolvedDefinition: return TEXT("UnresolvedDefinition");
	case EGP_ResourceCandidateRejectReason::NoFreeSlot: return TEXT("NoFreeSlot");
	case EGP_ResourceCandidateRejectReason::OutsideSearchRadius: return TEXT("OutsideSearchRadius");
	case EGP_ResourceCandidateRejectReason::NoNavSystem: return TEXT("NoNavSystem");
	case EGP_ResourceCandidateRejectReason::PathStartProjectionFailed: return TEXT("PathStartProjectionFailed");
	case EGP_ResourceCandidateRejectReason::CandidateProjectionFailed: return TEXT("CandidateProjectionFailed");
	case EGP_ResourceCandidateRejectReason::ApproachGeometryFailed: return TEXT("ApproachGeometryFailed");
	case EGP_ResourceCandidateRejectReason::PathInvalid: return TEXT("PathInvalid");
	case EGP_ResourceCandidateRejectReason::PathPartial: return TEXT("PathPartial");
	case EGP_ResourceCandidateRejectReason::PathTooLong: return TEXT("PathTooLong");
	case EGP_ResourceCandidateRejectReason::Accepted: return TEXT("Accepted");
	default: return TEXT("Unknown");
	}
}

bool GPResourceApproach::TryComputeDesiredHorizontalDistance(
	const FVector& PathStart,
	const FVector& NodeLocation,
	float InteractionRangeCm,
	float AcceptanceRadiusCm,
	float SafetyMarginCm,
	float CollisionHalfExtentXY,
	float& OutDesiredHorizontal,
	EGP_RangeApproachDistanceMode DistanceMode)
{
	OutDesiredHorizontal = -1.0f;
	if (!FMath::IsFinite(InteractionRangeCm) || InteractionRangeCm <= 0.0f
		|| !FMath::IsFinite(AcceptanceRadiusCm) || AcceptanceRadiusCm < 0.0f
		|| !FMath::IsFinite(SafetyMarginCm) || SafetyMarginCm < 0.0f)
	{
		return false;
	}

	const float DeltaZ = PathStart.Z - NodeLocation.Z;
	const float AbsDeltaZ = FMath::Abs(DeltaZ);
	const float DeltaZSq = FMath::Square(AbsDeltaZ);
	const float RangeSq = FMath::Square(InteractionRangeCm);

	float MaxHorizontalBudget = InteractionRangeCm;
	if (DistanceMode == EGP_RangeApproachDistanceMode::ThreeDimensional)
	{
		if (DeltaZSq >= RangeSq)
		{
			return false;
		}
		MaxHorizontalBudget = FMath::Sqrt(RangeSq - DeltaZSq);
	}

	const float TotalInward = AcceptanceRadiusCm + SafetyMarginCm;
	if (MaxHorizontalBudget <= TotalInward + 1.0f)
	{
		return false;
	}

	float DesiredHorizontal = MaxHorizontalBudget - TotalInward;
	// Stay outside collision/nav obstacle footprint when possible.
	const float MinOutsideBox = FMath::Max(0.0f, CollisionHalfExtentXY + 10.0f);
	if (DesiredHorizontal < MinOutsideBox)
	{
		if (MinOutsideBox + TotalInward >= MaxHorizontalBudget - 1.0f)
		{
			return false;
		}
		DesiredHorizontal = MinOutsideBox;
	}

	if (DistanceMode == EGP_RangeApproachDistanceMode::GroundPlane2D)
	{
		const float PredictedWorst2D = DesiredHorizontal + AcceptanceRadiusCm;
		if (PredictedWorst2D >= InteractionRangeCm)
		{
			return false;
		}
	}
	else
	{
		const float PredictedWorst = FMath::Sqrt(FMath::Square(DesiredHorizontal + AcceptanceRadiusCm) + DeltaZSq);
		if (PredictedWorst >= InteractionRangeCm)
		{
			return false;
		}
	}

	OutDesiredHorizontal = DesiredHorizontal;
	return true;
}

bool GPResourceApproach::TryMakeApproachPoint(
	const FVector& PathStart,
	const FVector& NodeLocation,
	const FVector& DirectionFromNodeXY,
	float DesiredHorizontal,
	float InteractionRangeCm,
	FVector& OutApproachPoint,
	EGP_RangeApproachDistanceMode DistanceMode)
{
	OutApproachPoint = FVector::ZeroVector;
	FVector Dir = DirectionFromNodeXY;
	Dir.Z = 0.0f;
	if (!Dir.Normalize())
	{
		return false;
	}

	const FVector Candidate(NodeLocation.X + Dir.X * DesiredHorizontal,
		NodeLocation.Y + Dir.Y * DesiredHorizontal,
		PathStart.Z);
	const float DistToTarget = (DistanceMode == EGP_RangeApproachDistanceMode::GroundPlane2D)
		? FVector::Dist2D(Candidate, NodeLocation)
		: FVector::Dist(Candidate, NodeLocation);
	if (DistToTarget > InteractionRangeCm - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutApproachPoint = Candidate;
	return true;
}

GPResourceApproach::FRangeApproachResult GPResourceApproach::EvaluateRangeApproachPath(
	UWorld* World,
	const FVector& TargetLocation,
	float CollisionHalfExtentXY,
	const FRangeApproachParams& Params)
{
	return EvaluateRangeApproachPath(
		World,
		TargetLocation,
		CollisionHalfExtentXY,
		Params,
		[](const FRangeApproachCandidateInfo&) {});
}

GPResourceApproach::FRangeApproachResult GPResourceApproach::EvaluateRangeApproachPath(
	UWorld* World,
	const FVector& TargetLocation,
	float CollisionHalfExtentXY,
	const FRangeApproachParams& Params,
	TFunctionRef<void(const FRangeApproachCandidateInfo&)> OnCandidate)
{
	FRangeApproachResult Result;
	if (!IsValid(World) || Params.PathfindingActor.Get() == nullptr)
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::InvalidNode;
		return Result;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSys == nullptr)
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::NoNavSystem;
		return Result;
	}

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	if (NavData == nullptr)
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::NoNavSystem;
		return Result;
	}

	Result.DistanceMode = Params.DistanceMode;
	Result.DeltaZCm = Params.PathStart.Z - TargetLocation.Z;
	Result.Distance2DCm = FVector::Dist2D(Params.PathStart, TargetLocation);

	FNavLocation ProjectedStart;
	if (!NavSys->ProjectPointToNavigation(Params.PathStart, ProjectedStart, FVector(250.0f, 250.0f, 400.0f)))
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::PathStartProjectionFailed;
		return Result;
	}

	float DesiredHorizontal = -1.0f;
	if (!TryComputeDesiredHorizontalDistance(
			ProjectedStart.Location,
			TargetLocation,
			Params.InteractionRangeCm,
			Params.AcceptanceRadiusCm,
			Params.SafetyMarginCm,
			CollisionHalfExtentXY,
			DesiredHorizontal,
			Params.DistanceMode))
	{
		// Populate budget diagnostics even on geometry failure (operator DeltaZ repro).
		if (Params.DistanceMode == EGP_RangeApproachDistanceMode::GroundPlane2D)
		{
			Result.MaxHorizontalBudgetCm = Params.InteractionRangeCm;
		}
		else
		{
			const float AbsDeltaZ = FMath::Abs(Result.DeltaZCm);
			const float RangeSq = FMath::Square(Params.InteractionRangeCm);
			const float DeltaZSq = FMath::Square(AbsDeltaZ);
			Result.MaxHorizontalBudgetCm = (DeltaZSq < RangeSq)
				? FMath::Sqrt(RangeSq - DeltaZSq)
				: -1.0f;
		}
		Result.RejectReason = EGP_ResourceCandidateRejectReason::ApproachGeometryFailed;
		return Result;
	}
	Result.DesiredHorizontalCm = DesiredHorizontal;
	if (Params.DistanceMode == EGP_RangeApproachDistanceMode::GroundPlane2D)
	{
		Result.MaxHorizontalBudgetCm = Params.InteractionRangeCm;
	}
	else
	{
		const float AbsDeltaZ = FMath::Abs(ProjectedStart.Location.Z - TargetLocation.Z);
		Result.MaxHorizontalBudgetCm = FMath::Sqrt(
			FMath::Square(Params.InteractionRangeCm) - FMath::Square(AbsDeltaZ));
	}

	const int32 DirCount = FMath::Clamp(Params.DirectionCount, 4, 16);
	Result.CandidateCount = DirCount;

	FVector TowardWorker = ProjectedStart.Location - TargetLocation;
	TowardWorker.Z = 0.0f;
	if (!TowardWorker.Normalize())
	{
		TowardWorker = FVector::ForwardVector;
	}
	if (!FMath::IsNearlyZero(Params.StartAngleBiasDegrees))
	{
		TowardWorker = TowardWorker.RotateAngleAxis(Params.StartAngleBiasDegrees, FVector::UpVector);
	}

	bool bAnyProjected = false;
	bool bSawPathInvalid = false;
	bool bSawPathPartial = false;
	bool bSawPathTooLong = false;

	for (int32 Index = 0; Index < DirCount; ++Index)
	{
		FRangeApproachCandidateInfo Info;
		Info.Index = Index;

#if !UE_BUILD_SHIPPING
		if ((Params.DebugSkipCandidateMask & (1 << Index)) != 0)
		{
			Info.bSkipped = true;
			OnCandidate(Info);
			continue;
		}
#endif

		const float AngleRad = (2.0f * PI) * (static_cast<float>(Index) / static_cast<float>(DirCount));
		const FVector Rotated = TowardWorker.RotateAngleAxis(FMath::RadiansToDegrees(AngleRad), FVector::UpVector);

		if (!TryMakeApproachPoint(
				ProjectedStart.Location,
				TargetLocation,
				Rotated,
				DesiredHorizontal,
				Params.InteractionRangeCm,
				Info.RawCandidate,
				Params.DistanceMode))
		{
			OnCandidate(Info);
			continue;
		}

		FNavLocation ProjectedApproach;
		if (!NavSys->ProjectPointToNavigation(Info.RawCandidate, ProjectedApproach, FVector(250.0f, 250.0f, 400.0f)))
		{
			OnCandidate(Info);
			continue;
		}
		Info.bProjected = true;
		Info.Projected = ProjectedApproach.Location;
		bAnyProjected = true;

		const float DistToTarget = (Params.DistanceMode == EGP_RangeApproachDistanceMode::GroundPlane2D)
			? FVector::Dist2D(ProjectedApproach.Location, TargetLocation)
			: FVector::Dist(ProjectedApproach.Location, TargetLocation);
		Info.bWithinRange = DistToTarget <= Params.InteractionRangeCm - KINDA_SMALL_NUMBER;
		if (!Info.bWithinRange)
		{
			OnCandidate(Info);
			continue;
		}

		// Prefer projected points that remain outside the authored obstacle footprint.
		const float MinOutside = FMath::Max(0.0f, CollisionHalfExtentXY + 5.0f);
		if (FVector::Dist2D(ProjectedApproach.Location, TargetLocation) + KINDA_SMALL_NUMBER < MinOutside)
		{
			OnCandidate(Info);
			continue;
		}

		FPathFindingQuery PathQuery(
			Params.PathfindingActor.Get(),
			*NavData,
			ProjectedStart.Location,
			ProjectedApproach.Location);
		PathQuery.SetAllowPartialPaths(false);

		const FPathFindingResult PathResult = NavSys->FindPathSync(PathQuery);
		if (!PathResult.IsSuccessful() || !PathResult.Path.IsValid())
		{
			bSawPathInvalid = true;
			OnCandidate(Info);
			continue;
		}
		if (PathResult.IsPartial())
		{
			bSawPathPartial = true;
			OnCandidate(Info);
			continue;
		}

		const float PathLength = PathResult.Path->GetLength();
		Info.PathLengthCm = PathLength;
		if (!FMath::IsFinite(PathLength) || PathLength > Params.MaxPathLengthCm + KINDA_SMALL_NUMBER)
		{
			bSawPathTooLong = true;
			OnCandidate(Info);
			continue;
		}

		Info.bPathOk = true;
		OnCandidate(Info);

		const bool bBetter =
			!Result.bReachable
			|| PathLength < Result.PathLengthCm - KINDA_SMALL_NUMBER
			|| (FMath::IsNearlyEqual(PathLength, Result.PathLengthCm) && Index < Result.BestCandidateIndex);
		if (bBetter)
		{
			Result.bReachable = true;
			Result.BestApproachLocation = ProjectedApproach.Location;
			Result.PathLengthCm = PathLength;
			Result.BestCandidateIndex = Index;
			Result.RejectReason = EGP_ResourceCandidateRejectReason::Accepted;
		}
	}

	if (Result.bReachable)
	{
		return Result;
	}

	if (!bAnyProjected)
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::CandidateProjectionFailed;
	}
	else if (bSawPathPartial)
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::PathPartial;
	}
	else if (bSawPathTooLong)
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::PathTooLong;
	}
	else if (bSawPathInvalid)
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::PathInvalid;
	}
	else
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::CandidateProjectionFailed;
	}
	return Result;
}

GPResourceApproach::FEvaluateResult GPResourceApproach::EvaluateNodeApproachPath(
	UWorld* World,
	const AGP_ResourceNode* Node,
	const FEvaluateParams& Params)
{
	FEvaluateResult Result;
	if (!IsValid(Node))
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::InvalidNode;
		return Result;
	}

	float CollisionHalfXY = 60.0f;
	if (const UBoxComponent* Box = Node->GetCollisionBox())
	{
		const FVector Extent = Box->GetScaledBoxExtent();
		CollisionHalfXY = FMath::Max(Extent.X, Extent.Y);
	}

	FRangeApproachParams RangeParams;
	RangeParams.PathStart = Params.PathStart;
	RangeParams.InteractionRangeCm = Params.InteractionRangeCm;
	RangeParams.AcceptanceRadiusCm = Params.AcceptanceRadiusCm;
	RangeParams.SafetyMarginCm = Params.SafetyMarginCm;
	RangeParams.MaxPathLengthCm = Params.MaxPathLengthCm;
	RangeParams.DirectionCount = Params.DirectionCount;
	RangeParams.PathfindingActor = Params.PathfindingActor;

	const FRangeApproachResult RangeResult = EvaluateRangeApproachPath(
		World,
		Node->GetActorLocation(),
		CollisionHalfXY,
		RangeParams);

	Result.bReachable = RangeResult.bReachable;
	Result.BestApproachLocation = RangeResult.BestApproachLocation;
	Result.PathLengthCm = RangeResult.PathLengthCm;
	Result.RejectReason = RangeResult.RejectReason;
	return Result;
}
