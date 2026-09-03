// Copyright Epic Games, Inc. All Rights Reserved.

#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

#if !UE_BUILD_SHIPPING

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "VoxelComponents/VoxelInvokerComponent.h"
#include "VoxelGenerators/VoxelFlatGenerator.h"
#include "VoxelIntBox.h"
#include "VoxelRender/VoxelProceduralMeshComponent.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelDataTools.h"
#include "VoxelWorld.h"

namespace GPVoxelRuntimeProbeAdapter
{
	static AVoxelWorld* AsVoxelWorld(AActor* Actor)
	{
		return Cast<AVoxelWorld>(Actor);
	}

	static const AVoxelWorld* AsVoxelWorld(const AActor* Actor)
	{
		return Cast<AVoxelWorld>(Actor);
	}

	AActor* FindExistingProbe(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		for (TActorIterator<AVoxelWorld> It(World); It; ++It)
		{
			AVoxelWorld* Candidate = *It;
			if (Candidate != nullptr && Candidate->ActorHasTag(ProbeActorTag))
			{
				return Candidate;
			}
		}
		return nullptr;
	}

	AActor* SpawnConfiguredProbeWorld(UWorld* World, const FVector& ActorLocation)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		if (AActor* Existing = FindExistingProbe(World))
		{
			return Existing;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		AVoxelWorld* VoxelWorld = World->SpawnActor<AVoxelWorld>(ActorLocation, FRotator::ZeroRotator, SpawnParams);
		if (VoxelWorld == nullptr)
		{
			return nullptr;
		}

		VoxelWorld->Tags.AddUnique(ProbeActorTag);
#if WITH_EDITOR
		VoxelWorld->SetActorLabel(TEXT("GP_VoxelRuntimeProbe"), false);
#endif
		VoxelWorld->bCreateWorldAutomatically = false;
		VoxelWorld->bUseCameraIfNoInvokersFound = true;
		VoxelWorld->bMergeAssetActors = false;
		VoxelWorld->bMergeDisableEditsBoxes = false;
		VoxelWorld->bEnableUndoRedo = false;
		VoxelWorld->bEnableMultiplayer = false;
		VoxelWorld->bEnableNavmesh = false;
		VoxelWorld->bRenderWorld = true;
		VoxelWorld->bEnableCollisions = true;
		VoxelWorld->bComputeVisibleChunksCollisions = true;
		VoxelWorld->VisibleChunksCollisionsMaxLOD = 0;
		VoxelWorld->CollisionTraceFlag = CTF_UseComplexAsSimple;
		VoxelWorld->CollisionPresets.SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		VoxelWorld->CollisionPresets.SetObjectType(ECC_WorldDynamic);
		VoxelWorld->CollisionPresets.SetResponseToAllChannels(ECR_Block);
		VoxelWorld->VoxelSize = 100.f;
		VoxelWorld->MaterialConfig = EVoxelMaterialConfig::RGB;
		VoxelWorld->VoxelMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		VoxelWorld->MaxLOD = 0;
		VoxelWorld->MinLOD = 0;
		VoxelWorld->bConstantLOD = false;
		VoxelWorld->DataOctreeInitialSubdivisionDepth = 1;
		VoxelWorld->SetRenderOctreeDepth(1);
		VoxelWorld->SetGeneratorClass(UVoxelFlatGenerator::StaticClass());

		UVoxelSimpleInvokerComponent* Invoker = NewObject<UVoxelSimpleInvokerComponent>(VoxelWorld);
		Invoker->LODRange = 20000.f;
		Invoker->CollisionsRange = 20000.f;
		Invoker->NavmeshRange = 0.f;
		Invoker->bUseForLOD = true;
		Invoker->bUseForCollisions = true;
		Invoker->bUseForNavmesh = false;
		Invoker->LODToSet = 0;
		Invoker->SetupAttachment(VoxelWorld->GetRootComponent());
		Invoker->RegisterComponent();
		VoxelWorld->AddInstanceComponent(Invoker);

		return VoxelWorld;
	}

	bool CreateWorldIfNeeded(AActor* VoxelWorldActor)
	{
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr)
		{
			return false;
		}
		if (VoxelWorld->IsCreated())
		{
			return true;
		}
		VoxelWorld->CreateWorld();
		return VoxelWorld->IsCreated();
	}

	bool IsCreated(const AActor* VoxelWorldActor)
	{
		const AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		return VoxelWorld != nullptr && VoxelWorld->IsCreated();
	}

	bool IsLoaded(const AActor* VoxelWorldActor)
	{
		const AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		return VoxelWorld != nullptr && VoxelWorld->IsLoaded();
	}

	bool IsMeshIdle(const AActor* VoxelWorldActor)
	{
		const AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr || !VoxelWorld->IsCreated())
		{
			return false;
		}
		return !UVoxelBlueprintLibrary::IsVoxelWorldMeshLoading(const_cast<AVoxelWorld*>(VoxelWorld))
			&& UVoxelBlueprintLibrary::GetTaskCount(const_cast<AVoxelWorld*>(VoxelWorld)) <= 0;
	}

	int32 GetMeshTaskCount(const AActor* VoxelWorldActor)
	{
		const AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr)
		{
			return -1;
		}
		return UVoxelBlueprintLibrary::GetTaskCount(const_cast<AVoxelWorld*>(VoxelWorld));
	}

	int32 CountProcMeshComponents(const AActor* VoxelWorldActor)
	{
		const AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr)
		{
			return 0;
		}
		TArray<UVoxelProceduralMeshComponent*> Meshes;
		VoxelWorld->GetComponents(Meshes);
		return Meshes.Num();
	}

	bool ApplySphereSubtract(
		AActor* VoxelWorldActor,
		const FGPVoxelSphereSubtractRequest& Request,
		FGPVoxelIntBoxReport& OutEditedBounds)
	{
		OutEditedBounds = FGPVoxelIntBoxReport();
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr || !VoxelWorld->IsCreated())
		{
			return false;
		}

		FVoxelIntBox EditedBounds;
		UVoxelSphereTools::RemoveSphere(
			VoxelWorld,
			Request.WorldLocation,
			Request.RadiusCm,
			nullptr,
			&EditedBounds,
			false,
			true,
			true);

		OutEditedBounds.Min = EditedBounds.Min;
		OutEditedBounds.Max = EditedBounds.Max;
		OutEditedBounds.bValid = EditedBounds.IsValid();
		OutEditedBounds.bInfinite = (EditedBounds == FVoxelIntBox::Infinite);
		return OutEditedBounds.bValid && !OutEditedBounds.bInfinite;
	}

	bool QueryDensityAtVoxel(AActor* VoxelWorldActor, const FIntVector& VoxelCoord, float& OutDensity)
	{
		OutDensity = 0.f;
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr || !VoxelWorld->IsCreated())
		{
			return false;
		}
		UVoxelDataTools::GetValue(OutDensity, VoxelWorld, VoxelCoord);
		return true;
	}

	bool QueryDensityAtWorld(AActor* VoxelWorldActor, const FVector& WorldLocation, float& OutDensity, FIntVector& OutVoxelCoord)
	{
		OutDensity = 0.f;
		OutVoxelCoord = FIntVector::ZeroValue;
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr || !VoxelWorld->IsCreated())
		{
			return false;
		}
		OutVoxelCoord = VoxelWorld->GlobalToLocal(WorldLocation);
		UVoxelDataTools::GetValue(OutDensity, VoxelWorld, OutVoxelCoord);
		return true;
	}

	FVector VoxelToWorld(AActor* VoxelWorldActor, const FIntVector& VoxelCoord)
	{
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr)
		{
			return FVector::ZeroVector;
		}
		return VoxelWorld->LocalToGlobal(VoxelCoord);
	}

	FIntVector WorldToVoxel(AActor* VoxelWorldActor, const FVector& WorldLocation)
	{
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr)
		{
			return FIntVector::ZeroValue;
		}
		return VoxelWorld->GlobalToLocal(WorldLocation);
	}

	float GetVoxelSizeCm(AActor* VoxelWorldActor)
	{
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		return VoxelWorld != nullptr ? VoxelWorld->VoxelSize : 0.f;
	}

	FIntVector GetWorldBoundsSize(AActor* VoxelWorldActor)
	{
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr || !VoxelWorld->IsCreated())
		{
			return FIntVector::ZeroValue;
		}
		return VoxelWorld->GetWorldBounds().Size();
	}

	bool LineTraceHitsProbe(
		UWorld* World,
		AActor* VoxelWorldActor,
		const FVector& Start,
		const FVector& End,
		FHitResult& OutHit)
	{
		OutHit = FHitResult();
		if (World == nullptr || VoxelWorldActor == nullptr)
		{
			return false;
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(GPVoxelRuntimeProbeTrace), true);
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		TArray<FHitResult> Hits;
		World->LineTraceMultiByObjectType(Hits, Start, End, ObjectParams, Params);
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() == VoxelWorldActor)
			{
				OutHit = Hit;
				return Hit.bBlockingHit;
			}
		}

		Hits.Reset();
		World->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, Params);
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() == VoxelWorldActor)
			{
				OutHit = Hit;
				return Hit.bBlockingHit;
			}
		}

		Hits.Reset();
		World->LineTraceMultiByChannel(Hits, Start, End, ECC_WorldDynamic, Params);
		for (const FHitResult& Hit : Hits)
		{
			if (Hit.GetActor() == VoxelWorldActor)
			{
				OutHit = Hit;
				return Hit.bBlockingHit;
			}
		}

		return false;
	}

	bool QueryClosestNonEmpty(
		AActor* VoxelWorldActor,
		const FVector& WorldLocation,
		bool& bSuccess,
		FIntVector& OutVoxel,
		float& OutDensity)
	{
		bSuccess = false;
		OutVoxel = FIntVector::ZeroValue;
		OutDensity = 0.f;
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr || !VoxelWorld->IsCreated())
		{
			return false;
		}

		FVoxelFindClosestNonEmptyVoxelResult Result;
		UVoxelDataTools::FindClosestNonEmptyVoxel(Result, VoxelWorld, WorldLocation, false, true);
		bSuccess = Result.bSuccess;
		OutVoxel = Result.Position;
		OutDensity = Result.Value;
		return true;
	}

	bool QueryDensitySurfaceWorldZ(
		AActor* VoxelWorldActor,
		float WorldX,
		float WorldY,
		float& OutWorldZ)
	{
		OutWorldZ = 0.f;
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr || !VoxelWorld->IsCreated())
		{
			return false;
		}

		const FVector ColumnWorld(WorldX, WorldY, VoxelWorld->GetActorLocation().Z);
		const FIntVector ColumnVoxel = VoxelWorld->GlobalToLocal(ColumnWorld);
		for (int32 VoxelZ = 16; VoxelZ >= -16; --VoxelZ)
		{
			float Density = 0.f;
			UVoxelDataTools::GetValue(Density, VoxelWorld, FIntVector(ColumnVoxel.X, ColumnVoxel.Y, VoxelZ));
			if (Density < 0.f)
			{
				OutWorldZ = VoxelWorld->LocalToGlobal(FIntVector(ColumnVoxel.X, ColumnVoxel.Y, VoxelZ)).Z;
				return true;
			}
		}
		return false;
	}

	void DestroyProbeWorld(AActor* VoxelWorldActor)
	{
		AVoxelWorld* VoxelWorld = AsVoxelWorld(VoxelWorldActor);
		if (VoxelWorld == nullptr)
		{
			return;
		}
		if (VoxelWorld->IsCreated())
		{
			VoxelWorld->DestroyWorld();
		}
		VoxelWorld->Destroy();
	}
}

#endif
