// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPResourceApproach.h"

#include "Components/BoxComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Resources/GPResourceNode.h"

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
	float& OutDesiredHorizontal)
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
	const float RangeSq = FMath::Square(InteractionRangeCm);
	const float DeltaZSq = FMath::Square(AbsDeltaZ);
	if (DeltaZSq >= RangeSq)
	{
		return false;
	}

	const float MaxHorizontalBudget = FMath::Sqrt(RangeSq - DeltaZSq);
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

	const float PredictedWorst = FMath::Sqrt(FMath::Square(DesiredHorizontal + AcceptanceRadiusCm) + DeltaZSq);
	if (PredictedWorst >= InteractionRangeCm)
	{
		return false;
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
	FVector& OutApproachPoint)
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
	if (FVector::Dist(Candidate, NodeLocation) > InteractionRangeCm - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutApproachPoint = Candidate;
	return true;
}

GPResourceApproach::FEvaluateResult GPResourceApproach::EvaluateNodeApproachPath(
	UWorld* World,
	const AGP_ResourceNode* Node,
	const FEvaluateParams& Params)
{
	FEvaluateResult Result;
	if (!IsValid(World) || !IsValid(Node) || Params.PathfindingActor.Get() == nullptr)
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

	FNavLocation ProjectedStart;
	if (!NavSys->ProjectPointToNavigation(Params.PathStart, ProjectedStart, FVector(200.0f, 200.0f, 400.0f)))
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::PathStartProjectionFailed;
		return Result;
	}

	const FVector NodeLocation = Node->GetActorLocation();
	float CollisionHalfXY = 60.0f;
	if (const UBoxComponent* Box = Node->GetCollisionBox())
	{
		const FVector Extent = Box->GetScaledBoxExtent();
		CollisionHalfXY = FMath::Max(Extent.X, Extent.Y);
	}

	float DesiredHorizontal = -1.0f;
	if (!TryComputeDesiredHorizontalDistance(
			ProjectedStart.Location,
			NodeLocation,
			Params.InteractionRangeCm,
			Params.AcceptanceRadiusCm,
			Params.SafetyMarginCm,
			CollisionHalfXY,
			DesiredHorizontal))
	{
		Result.RejectReason = EGP_ResourceCandidateRejectReason::ApproachGeometryFailed;
		return Result;
	}

	const int32 DirCount = FMath::Clamp(Params.DirectionCount, 4, 16);
	FVector TowardWorker = ProjectedStart.Location - NodeLocation;
	TowardWorker.Z = 0.0f;
	if (!TowardWorker.Normalize())
	{
		TowardWorker = FVector::ForwardVector;
	}

	bool bAnyProjected = false;
	bool bSawPathInvalid = false;
	bool bSawPathPartial = false;
	bool bSawPathTooLong = false;

	for (int32 Index = 0; Index < DirCount; ++Index)
	{
		const float AngleRad = (2.0f * PI) * (static_cast<float>(Index) / static_cast<float>(DirCount));
		const FVector Rotated = TowardWorker.RotateAngleAxis(FMath::RadiansToDegrees(AngleRad), FVector::UpVector);

		FVector RawApproach = FVector::ZeroVector;
		if (!TryMakeApproachPoint(
				ProjectedStart.Location,
				NodeLocation,
				Rotated,
				DesiredHorizontal,
				Params.InteractionRangeCm,
				RawApproach))
		{
			continue;
		}

		FNavLocation ProjectedApproach;
		if (!NavSys->ProjectPointToNavigation(RawApproach, ProjectedApproach, FVector(250.0f, 250.0f, 400.0f)))
		{
			continue;
		}
		bAnyProjected = true;

		if (FVector::Dist(ProjectedApproach.Location, NodeLocation) > Params.InteractionRangeCm - KINDA_SMALL_NUMBER)
		{
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
			continue;
		}
		if (PathResult.IsPartial())
		{
			bSawPathPartial = true;
			continue;
		}

		const float PathLength = PathResult.Path->GetLength();
		if (!FMath::IsFinite(PathLength) || PathLength > Params.MaxPathLengthCm + KINDA_SMALL_NUMBER)
		{
			bSawPathTooLong = true;
			continue;
		}

		if (!Result.bReachable || PathLength < Result.PathLengthCm)
		{
			Result.bReachable = true;
			Result.BestApproachLocation = ProjectedApproach.Location;
			Result.PathLengthCm = PathLength;
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
