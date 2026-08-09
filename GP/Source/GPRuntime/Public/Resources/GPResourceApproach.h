// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "GPResourceApproach.generated.h"

class AGP_ResourceNode;
class AActor;
class UWorld;

/** Interaction-range distance mode for approach geometry (GP-S33M). */
UENUM(BlueprintType)
enum class EGP_RangeApproachDistanceMode : uint8
{
	/** Classic 3D budget: MaxHorizontal = sqrt(Range^2 - DeltaZ^2). ResourceNode mining. */
	ThreeDimensional UMETA(DisplayName = "Three Dimensional"),
	/** Planar XY budget: MaxHorizontal = Range. MainBase drop-off / building footprint. */
	GroundPlane2D UMETA(DisplayName = "Ground Plane 2D")
};

/** Exact rejection / acceptance reason for ResourceNode candidate evaluation (GP-S28P2). */
UENUM(BlueprintType)
enum class EGP_ResourceCandidateRejectReason : uint8
{
	None UMETA(DisplayName = "None"),
	InvalidNode UMETA(DisplayName = "Invalid Node"),
	ExcludedNode UMETA(DisplayName = "Excluded Node"),
	Depleted UMETA(DisplayName = "Depleted"),
	DestroyPending UMETA(DisplayName = "Destroy Pending"),
	ClearingOccupancy UMETA(DisplayName = "Clearing Occupancy"),
	MineRejected UMETA(DisplayName = "Mine Rejected"),
	IncompatibleResourceType UMETA(DisplayName = "Incompatible Resource Type"),
	UnresolvedDefinition UMETA(DisplayName = "Unresolved Definition"),
	NoFreeSlot UMETA(DisplayName = "No Free Slot"),
	OutsideSearchRadius UMETA(DisplayName = "Outside Search Radius"),
	NoNavSystem UMETA(DisplayName = "No Nav System"),
	PathStartProjectionFailed UMETA(DisplayName = "Path Start Projection Failed"),
	CandidateProjectionFailed UMETA(DisplayName = "Candidate Projection Failed"),
	ApproachGeometryFailed UMETA(DisplayName = "Approach Geometry Failed"),
	PathInvalid UMETA(DisplayName = "Path Invalid"),
	PathPartial UMETA(DisplayName = "Path Partial"),
	PathTooLong UMETA(DisplayName = "Path Too Long"),
	Accepted UMETA(DisplayName = "Accepted")
};

/** Shared Mine / search approach geometry (same 3D range budget as UnitCommand). */
namespace GPResourceApproach
{
	GPRUNTIME_API const TCHAR* RejectReasonToString(EGP_ResourceCandidateRejectReason Reason);

	GPRUNTIME_API const TCHAR* DistanceModeToString(EGP_RangeApproachDistanceMode Mode);

	/**
	 * Horizontal distance from target center so worst-case arrival stays inside InteractionRange.
	 * ThreeDimensional: consumes |DeltaZ| from the budget.
	 * GroundPlane2D: MaxHorizontalBudget = InteractionRangeCm (actor-origin Z ignored).
	 */
	GPRUNTIME_API bool TryComputeDesiredHorizontalDistance(
		const FVector& PathStart,
		const FVector& NodeLocation,
		float InteractionRangeCm,
		float AcceptanceRadiusCm,
		float SafetyMarginCm,
		float CollisionHalfExtentXY,
		float& OutDesiredHorizontal,
		EGP_RangeApproachDistanceMode DistanceMode = EGP_RangeApproachDistanceMode::ThreeDimensional);

	/** Single approach sample in DirectionFromNode (normalized XY); Z from PathStart. */
	GPRUNTIME_API bool TryMakeApproachPoint(
		const FVector& PathStart,
		const FVector& NodeLocation,
		const FVector& DirectionFromNodeXY,
		float DesiredHorizontal,
		float InteractionRangeCm,
		FVector& OutApproachPoint,
		EGP_RangeApproachDistanceMode DistanceMode = EGP_RangeApproachDistanceMode::ThreeDimensional);

	struct FEvaluateParams
	{
		FVector PathStart = FVector::ZeroVector;
		float InteractionRangeCm = 200.0f;
		float AcceptanceRadiusCm = 50.0f;
		float SafetyMarginCm = 25.0f;
		float MaxPathLengthCm = 6000.0f;
		int32 DirectionCount = 8;
		TWeakObjectPtr<AActor> PathfindingActor;
	};

	struct FEvaluateResult
	{
		bool bReachable = false;
		FVector BestApproachLocation = FVector::ZeroVector;
		float PathLengthCm = 0.0f;
		EGP_ResourceCandidateRejectReason RejectReason = EGP_ResourceCandidateRejectReason::None;
	};

	/**
	 * Find shortest non-partial nav path from PathStart to a projected approach point
	 * around Node (never requires path to actor center).
	 */
	GPRUNTIME_API FEvaluateResult EvaluateNodeApproachPath(
		UWorld* World,
		const AGP_ResourceNode* Node,
		const FEvaluateParams& Params);

	/** Generic interaction-range approach around any target center (MainBase haul / ResourceNode). */
	struct FRangeApproachParams
	{
		FVector PathStart = FVector::ZeroVector;
		float InteractionRangeCm = 400.0f;
		float AcceptanceRadiusCm = 50.0f;
		float SafetyMarginCm = 25.0f;
		float MaxPathLengthCm = 12000.0f;
		int32 DirectionCount = 8;
		TWeakObjectPtr<AActor> PathfindingActor;
		EGP_RangeApproachDistanceMode DistanceMode = EGP_RangeApproachDistanceMode::ThreeDimensional;
		/** Rotates the radial (index 0) sector; used for multi-worker approach diversity. */
		float StartAngleBiasDegrees = 0.0f;
#if !UE_BUILD_SHIPPING
		/** Bit i skips candidate i after geometry (contract: force alternate selection). */
		int32 DebugSkipCandidateMask = 0;
#endif
	};

	struct FRangeApproachResult
	{
		bool bReachable = false;
		FVector BestApproachLocation = FVector::ZeroVector;
		float PathLengthCm = 0.0f;
		float DesiredHorizontalCm = -1.0f;
		float DeltaZCm = 0.0f;
		float Distance2DCm = 0.0f;
		float MaxHorizontalBudgetCm = -1.0f;
		int32 BestCandidateIndex = -1;
		int32 CandidateCount = 0;
		EGP_RangeApproachDistanceMode DistanceMode = EGP_RangeApproachDistanceMode::ThreeDimensional;
		EGP_ResourceCandidateRejectReason RejectReason = EGP_ResourceCandidateRejectReason::None;
	};

	struct FRangeApproachCandidateInfo
	{
		int32 Index = 0;
		FVector RawCandidate = FVector::ZeroVector;
		FVector Projected = FVector::ZeroVector;
		bool bProjected = false;
		bool bWithinRange = false;
		bool bPathOk = false;
		bool bSkipped = false;
		float PathLengthCm = -1.0f;
	};

	/**
	 * 8-direction (configurable) reachable approach around TargetLocation.
	 * Index 0 = toward PathStart (plus StartAngleBiasDegrees); scores by nav path length,
	 * then lowest candidate index.
	 */
	GPRUNTIME_API FRangeApproachResult EvaluateRangeApproachPath(
		UWorld* World,
		const FVector& TargetLocation,
		float CollisionHalfExtentXY,
		const FRangeApproachParams& Params);

	/** Same as EvaluateRangeApproachPath with per-candidate callback (haul diagnostics). */
	GPRUNTIME_API FRangeApproachResult EvaluateRangeApproachPath(
		UWorld* World,
		const FVector& TargetLocation,
		float CollisionHalfExtentXY,
		const FRangeApproachParams& Params,
		TFunctionRef<void(const FRangeApproachCandidateInfo&)> OnCandidate);
}
