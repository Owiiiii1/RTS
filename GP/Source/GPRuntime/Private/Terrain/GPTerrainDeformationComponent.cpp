// Copyright Epic Games, Inc. All Rights Reserved.

#include "Terrain/GPTerrainDeformationComponent.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPTerrainDeformation, Log, All);

namespace GPTerrainDeformationPrivate
{
	constexpr float ResolveHalfExtentCm = 8000.f;

	static bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	static const TCHAR* ReasonName(EGP_TerrainDeformationRejectReason Reason)
	{
		switch (Reason)
		{
		case EGP_TerrainDeformationRejectReason::None:
			return TEXT("None");
		case EGP_TerrainDeformationRejectReason::NoAuthority:
			return TEXT("NoAuthority");
		case EGP_TerrainDeformationRejectReason::NonFinite:
			return TEXT("NonFinite");
		case EGP_TerrainDeformationRejectReason::InvalidRadius:
			return TEXT("InvalidRadius");
		case EGP_TerrainDeformationRejectReason::InvalidDepth:
			return TEXT("InvalidDepth");
		case EGP_TerrainDeformationRejectReason::UnsupportedProfile:
			return TEXT("UnsupportedProfile");
		case EGP_TerrainDeformationRejectReason::NoVoxelWorld:
			return TEXT("NoVoxelWorld");
		case EGP_TerrainDeformationRejectReason::ApplyFailed:
			return TEXT("ApplyFailed");
		default:
			return TEXT("Unknown");
		}
	}
}

UGP_TerrainDeformationComponent::UGP_TerrainDeformationComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UGP_TerrainDeformationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGP_TerrainDeformationComponent, EventLog);
}

void UGP_TerrainDeformationComponent::OnRep_EventLog()
{
	ApplyPendingAuthoritativeEvents();
}

bool UGP_TerrainDeformationComponent::NormalizeRequest(
	const FGP_TerrainDeformationRequest& Request,
	FGP_TerrainDeformationRequest& OutNormalized,
	EGP_TerrainDeformationRejectReason& OutReason) const
{
	OutNormalized = Request;
	OutReason = EGP_TerrainDeformationRejectReason::None;

	if (!GPTerrainDeformationPrivate::IsFiniteVector(Request.WorldLocation)
		|| !FMath::IsFinite(Request.RadiusCm)
		|| !FMath::IsFinite(Request.DepthCm)
		|| !FMath::IsFinite(Request.RotationDegrees))
	{
		OutReason = EGP_TerrainDeformationRejectReason::NonFinite;
		return false;
	}

	if (Request.RadiusCm <= 0.f)
	{
		OutReason = EGP_TerrainDeformationRejectReason::InvalidRadius;
		return false;
	}

	if (Request.DepthCm <= 0.f)
	{
		OutReason = EGP_TerrainDeformationRejectReason::InvalidDepth;
		return false;
	}

	if (Request.Profile != EGP_TerrainDeformationProfile::ShallowSphereCap)
	{
		OutReason = EGP_TerrainDeformationRejectReason::UnsupportedProfile;
		return false;
	}

	OutNormalized.RadiusCm = FMath::Clamp(
		Request.RadiusCm, GPVoxelCraterMath::MinRadiusCm, GPVoxelCraterMath::MaxRadiusCm);
	OutNormalized.DepthCm = FMath::Clamp(
		Request.DepthCm, GPVoxelCraterMath::MinDepthCm, OutNormalized.RadiusCm);
	return true;
}

AActor* UGP_TerrainDeformationComponent::ResolveOwningVoxelWorld(const FVector& WorldLocation) const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector Start = WorldLocation + FVector(0.f, 0.f, GPTerrainDeformationPrivate::ResolveHalfExtentCm);
	const FVector End = WorldLocation - FVector(0.f, 0.f, GPTerrainDeformationPrivate::ResolveHalfExtentCm);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GPTerrainDeformationResolve), true);

	const ECollisionChannel Channels[] = { ECC_WorldStatic, ECC_WorldDynamic, ECC_Visibility };
	AActor* BestWorld = nullptr;
	float BestDistance = 0.f;
	bool bHasBest = false;

	for (const ECollisionChannel Channel : Channels)
	{
		TArray<FHitResult> Hits;
		if (!World->LineTraceMultiByChannel(Hits, Start, End, Channel, Params))
		{
			continue;
		}

		for (const FHitResult& Hit : Hits)
		{
			if (!Hit.bBlockingHit)
			{
				continue;
			}

			EGPVoxelWorldResolvePath Path = EGPVoxelWorldResolvePath::None;
			AActor* Resolved = GPVoxelRuntimeProbeAdapter::ResolveVoxelWorldFromHit(Hit, Path);
			if (Resolved == nullptr)
			{
				continue;
			}
			if (!bHasBest || Hit.Distance < BestDistance)
			{
				BestWorld = Resolved;
				BestDistance = Hit.Distance;
				bHasBest = true;
			}
		}
	}

	return BestWorld;
}

bool UGP_TerrainDeformationComponent::ApplyEventGeometry(
	const FGP_TerrainDeformationEvent& Event,
	FGP_TerrainEditBounds& OutBounds,
	FVector& OutEditCenter,
	AActor*& OutVoxelWorld) const
{
	OutBounds = FGP_TerrainEditBounds();
	OutEditCenter = FVector::ZeroVector;
	OutVoxelWorld = nullptr;

	if (Event.Profile != EGP_TerrainDeformationProfile::ShallowSphereCap)
	{
		return false;
	}

	AActor* VoxelWorld = ResolveOwningVoxelWorld(Event.WorldLocation);
	if (VoxelWorld == nullptr || !GPVoxelRuntimeProbeAdapter::IsCreated(VoxelWorld))
	{
		return false;
	}

	const GPVoxelCraterMath::FShallowCrater Shallow = GPVoxelCraterMath::Make(
		Event.WorldLocation, Event.RadiusCm, Event.DepthCm);

	FGPVoxelSphereSubtractRequest Subtract;
	Subtract.WorldLocation = Shallow.EditCenter;
	Subtract.RadiusCm = Shallow.RadiusCm;

	FGPVoxelIntBoxReport Edited;
	if (!GPVoxelRuntimeProbeAdapter::ApplySphereSubtract(VoxelWorld, Subtract, Edited))
	{
		return false;
	}

	OutBounds.Min = Edited.Min;
	OutBounds.Max = Edited.Max;
	OutBounds.bValid = Edited.bValid && !Edited.bInfinite;
	OutEditCenter = Shallow.EditCenter;
	OutVoxelWorld = VoxelWorld;
	return OutBounds.bValid;
}

FGP_TerrainDeformationResult UGP_TerrainDeformationComponent::RequestDeformation(
	const FGP_TerrainDeformationRequest& Request)
{
	FGP_TerrainDeformationResult Result;
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		Result.Reason = EGP_TerrainDeformationRejectReason::NoAuthority;
		UE_LOG(LogGPTerrainDeformation, Log,
			TEXT("RequestDeformation rejected reason=NoAuthority"));
		return Result;
	}

	FGP_TerrainDeformationRequest Normalized;
	if (!NormalizeRequest(Request, Normalized, Result.Reason))
	{
		UE_LOG(LogGPTerrainDeformation, Log,
			TEXT("RequestDeformation rejected reason=%s location=%s radius=%.1f depth=%.1f"),
			GPTerrainDeformationPrivate::ReasonName(Result.Reason),
			*Request.WorldLocation.ToString(),
			Request.RadiusCm,
			Request.DepthCm);
		return Result;
	}

	AActor* VoxelWorld = ResolveOwningVoxelWorld(Normalized.WorldLocation);
	if (VoxelWorld == nullptr || !GPVoxelRuntimeProbeAdapter::IsCreated(VoxelWorld))
	{
		Result.Reason = EGP_TerrainDeformationRejectReason::NoVoxelWorld;
		UE_LOG(LogGPTerrainDeformation, Log,
			TEXT("RequestDeformation rejected reason=NoVoxelWorld location=%s"),
			*Normalized.WorldLocation.ToString());
		return Result;
	}

	FGP_TerrainDeformationEvent Event;
	Event.SequenceId = NextSequenceId + 1;
	Event.WorldLocation = Normalized.WorldLocation;
	Event.RadiusCm = Normalized.RadiusCm;
	Event.DepthCm = Normalized.DepthCm;
	Event.Profile = Normalized.Profile;
	Event.Seed = Normalized.Seed;
	Event.RotationDegrees = Normalized.RotationDegrees;
	Event.SurfaceScar = Normalized.SurfaceScar;
	Event.SourceIdentity = Normalized.SourceIdentity;

	FVector EditCenter = FVector::ZeroVector;
	AActor* AppliedWorld = nullptr;
	if (!ApplyEventGeometry(Event, Result.EditedBounds, EditCenter, AppliedWorld))
	{
		Result.Reason = EGP_TerrainDeformationRejectReason::ApplyFailed;
		UE_LOG(LogGPTerrainDeformation, Warning,
			TEXT("RequestDeformation rejected reason=ApplyFailed location=%s radius=%.1f depth=%.1f"),
			*Event.WorldLocation.ToString(),
			Event.RadiusCm,
			Event.DepthCm);
		return Result;
	}

	NextSequenceId = Event.SequenceId;
	AppliedSequenceIds.Add(Event.SequenceId);
	EventLog.Add(Event);
	while (EventLog.Num() > MaxEventHistory)
	{
		EventLog.RemoveAt(0);
	}
	if (AActor* MutableOwner = GetOwner())
	{
		MutableOwner->ForceNetUpdate();
	}

	Result.bAccepted = true;
	Result.SequenceId = Event.SequenceId;
	Result.Reason = EGP_TerrainDeformationRejectReason::None;
	Result.AppliedRadiusCm = Event.RadiusCm;
	Result.AppliedDepthCm = Event.DepthCm;
	Result.EditCenter = EditCenter;
	Result.ResolvedVoxelWorld = AppliedWorld;
	OnTerrainDeformationApplied.Broadcast(Event);

	UE_LOG(LogGPTerrainDeformation, Log,
		TEXT("RequestDeformation accepted seq=%d profile=ShallowSphereCap location=%s radius=%.1f depth=%.1f seed=%d rotation=%.1f editCenter=%s source=%s bounds=(%d/%d, %d/%d, %d/%d) history=%d"),
		Event.SequenceId,
		*Event.WorldLocation.ToString(),
		Event.RadiusCm,
		Event.DepthCm,
		Event.Seed,
		Event.RotationDegrees,
		*EditCenter.ToString(),
		*Event.SourceIdentity.ToString(),
		Result.EditedBounds.Min.X,
		Result.EditedBounds.Max.X,
		Result.EditedBounds.Min.Y,
		Result.EditedBounds.Max.Y,
		Result.EditedBounds.Min.Z,
		Result.EditedBounds.Max.Z,
		EventLog.Num());
	return Result;
}

void UGP_TerrainDeformationComponent::ApplyPendingAuthoritativeEvents()
{
	for (const FGP_TerrainDeformationEvent& Event : EventLog)
	{
		if (Event.SequenceId == 0 || AppliedSequenceIds.Contains(Event.SequenceId))
		{
			continue;
		}

		FGP_TerrainEditBounds Bounds;
		FVector EditCenter = FVector::ZeroVector;
		AActor* VoxelWorld = nullptr;
		if (!ApplyEventGeometry(Event, Bounds, EditCenter, VoxelWorld))
		{
			UE_LOG(LogGPTerrainDeformation, Log,
				TEXT("ApplyPending left seq=%d pending (local voxel apply failed)"),
				Event.SequenceId);
			continue;
		}

		AppliedSequenceIds.Add(Event.SequenceId);
		OnTerrainDeformationApplied.Broadcast(Event);
		UE_LOG(LogGPTerrainDeformation, Log,
			TEXT("ApplyPending applied seq=%d editCenter=%s"),
			Event.SequenceId,
			*EditCenter.ToString());
	}
}
