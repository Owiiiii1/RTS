// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPVoxelAuthoredCrater, Log, All);

namespace GPVoxelAuthoredCraterProbe
{
	constexpr float DefaultRadiusCm = 300.f;
	constexpr float MinRadiusCm = 50.f;
	constexpr float MaxRadiusCm = 1500.f;
	constexpr float CursorTraceDistanceCm = 1000000.f;
	constexpr float VerticalTraceHalfExtentCm = 8000.f;
	constexpr float CollisionVerifyDelaySec = 0.75f;
	constexpr float MinSurfaceDropCm = 50.f;
	constexpr int32 CraterMessageKey = 0x47504331;
	constexpr int32 CollisionMessageKey = 0x47504332;

	struct FPendingCollisionCheck
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AActor> VoxelWorld;
		FVector Center = FVector::ZeroVector;
		float RadiusCm = 0.f;
		float BeforeSurfaceZ = 0.f;
		float VoxelSizeCm = 0.f;
		bool bBaselineHit = false;
		bool bFill = false;
		FTimerHandle Timer;
	};

	static FPendingCollisionCheck Pending;

	static const TCHAR* ResolvePathName(EGPVoxelWorldResolvePath Path)
	{
		switch (Path)
		{
		case EGPVoxelWorldResolvePath::HitActor:
			return TEXT("HitActor");
		case EGPVoxelWorldResolvePath::ComponentOuter:
			return TEXT("ComponentOuter");
		case EGPVoxelWorldResolvePath::AttachParent:
			return TEXT("AttachParent");
		case EGPVoxelWorldResolvePath::ActorOuter:
			return TEXT("ActorOuter");
		default:
			return TEXT("None");
		}
	}

	static const TCHAR* NetModeName(ENetMode NetMode)
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

	static void Screen(int32 Key, const FColor& Color, const FString& Text)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(Key, 8.f, Color, Text);
		}
	}

	static bool ClosestChannelHit(
		UWorld* World,
		const FVector& Start,
		const FVector& End,
		ECollisionChannel Channel,
		const FCollisionQueryParams& Params,
		FHitResult& InOutBest,
		bool& bInOutHasBest)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, Channel, Params) || !Hit.bBlockingHit)
		{
			return false;
		}
		if (!bInOutHasBest || Hit.Distance < InOutBest.Distance)
		{
			InOutBest = Hit;
			bInOutHasBest = true;
		}
		return true;
	}

	static bool TraceCursor(UWorld* World, APlayerController* PlayerController, FHitResult& OutHit)
	{
		OutHit = FHitResult();
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (!PlayerController->GetMousePosition(MouseX, MouseY))
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning,
				TEXT("gp.Voxel cursor: no mouse position"));
			return false;
		}

		FVector Origin = FVector::ZeroVector;
		FVector Direction = FVector::ZeroVector;
		if (!PlayerController->DeprojectScreenPositionToWorld(MouseX, MouseY, Origin, Direction))
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning,
				TEXT("gp.Voxel cursor: deproject failed"));
			return false;
		}

		const FVector End = Origin + Direction * CursorTraceDistanceCm;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GPVoxelAuthoredCraterCursor), true);
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			Params.AddIgnoredActor(Pawn);
		}

		bool bHasBest = false;
		ClosestChannelHit(World, Origin, End, ECC_Visibility, Params, OutHit, bHasBest);
		ClosestChannelHit(World, Origin, End, ECC_WorldStatic, Params, OutHit, bHasBest);
		ClosestChannelHit(World, Origin, End, ECC_WorldDynamic, Params, OutHit, bHasBest);
		return bHasBest;
	}

	static void VerifyCollision();

	static void ScheduleCollisionCheck(
		UWorld* World,
		AActor* VoxelWorld,
		const FVector& Center,
		float RadiusCm,
		float BeforeSurfaceZ,
		float VoxelSizeCm,
		bool bBaselineHit,
		bool bFill)
	{
		if (Pending.World.IsValid())
		{
			Pending.World->GetTimerManager().ClearTimer(Pending.Timer);
		}

		Pending.World = World;
		Pending.VoxelWorld = VoxelWorld;
		Pending.Center = Center;
		Pending.RadiusCm = RadiusCm;
		Pending.BeforeSurfaceZ = BeforeSurfaceZ;
		Pending.VoxelSizeCm = VoxelSizeCm;
		Pending.bBaselineHit = bBaselineHit;
		Pending.bFill = bFill;

		World->GetTimerManager().SetTimer(
			Pending.Timer,
			FTimerDelegate::CreateStatic(&VerifyCollision),
			CollisionVerifyDelaySec,
			false);
	}

	static void VerifyCollision()
	{
		UWorld* World = Pending.World.Get();
		AActor* VoxelWorld = Pending.VoxelWorld.Get();
		if (World == nullptr || VoxelWorld == nullptr)
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning,
				TEXT("gp.Voxel collision check: world or VoxelWorld gone"));
			return;
		}

		const FVector Start = Pending.Center + FVector(0.f, 0.f, VerticalTraceHalfExtentCm);
		const FVector End = Pending.Center - FVector(0.f, 0.f, VerticalTraceHalfExtentCm);
		FHitResult AfterHit;
		const bool bAfterHit = GPVoxelRuntimeProbeAdapter::LineTraceHitsProbe(World, VoxelWorld, Start, End, AfterHit);
		const float AfterZ = bAfterHit ? AfterHit.ImpactPoint.Z : 0.f;
		const float DeltaZ = bAfterHit ? (Pending.BeforeSurfaceZ - AfterZ) : 0.f;
		const float Threshold = FMath::Max(MinSurfaceDropCm, Pending.VoxelSizeCm * 0.5f);

		DrawDebugLine(World, Start, End, FColor::Yellow, false, 8.f, 0, 2.f);

		bool bPass = false;
		const TCHAR* Reason = TEXT("not updated");
		if (!Pending.bFill)
		{
			if (Pending.bBaselineHit && !bAfterHit)
			{
				bPass = true;
				Reason = TEXT("hole");
			}
			else if (bAfterHit && DeltaZ >= Threshold)
			{
				bPass = true;
				Reason = TEXT("lower");
			}
		}
		else if (bAfterHit && (AfterZ - Pending.BeforeSurfaceZ) >= Threshold)
		{
			bPass = true;
			Reason = TEXT("higher");
		}

		UE_LOG(LogGPVoxelAuthoredCrater, Log,
			TEXT("gp.Voxel collision check: BeforeSurfaceZ=%.1f AfterHit=%s AfterSurfaceZ=%.1f DeltaZ=%.1f threshold=%.1f %s (%s)"),
			Pending.BeforeSurfaceZ,
			bAfterHit ? TEXT("true") : TEXT("false"),
			bAfterHit ? AfterZ : 0.f,
			Pending.bFill ? (bAfterHit ? AfterZ - Pending.BeforeSurfaceZ : 0.f) : DeltaZ,
			Threshold,
			bPass ? TEXT("PASS") : TEXT("WAIT"),
			Reason);

		if (bPass)
		{
			Screen(CollisionMessageKey, FColor::Green,
				FString::Printf(TEXT("COLLISION UPDATED  dZ=%.0f cm  %s"),
					Pending.bFill ? (AfterZ - Pending.BeforeSurfaceZ) : DeltaZ, Reason));
		}
		else
		{
			Screen(CollisionMessageKey, FColor::Yellow,
				FString::Printf(TEXT("collision delta  dZ=%.0f cm  %s"),
					bAfterHit ? (Pending.bFill ? AfterZ - Pending.BeforeSurfaceZ : DeltaZ) : 0.f, Reason));
		}

		UE_LOG(LogGPVoxelAuthoredCrater, Log,
			TEXT("gp.Voxel collision check: navmesh not rebuilt (dynamic Recast deferred Stage 3E)"));
	}

	static void ApplyUnderCursor(const TArray<FString>& Args, UWorld* World, bool bFill)
	{
		const TCHAR* Command = bFill ? TEXT("gp.Voxel.FillUnderCursor") : TEXT("gp.Voxel.CraterUnderCursor");
		if (World == nullptr)
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning, TEXT("%s: missing world"), Command);
			return;
		}
		if (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game)
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning,
				TEXT("%s: PIE or game world only (map is not saved)"), Command);
			return;
		}
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning,
				TEXT("%s: client world refused. Run on the listen-server viewport. This probe does not replicate."),
				Command);
			Screen(CraterMessageKey, FColor::Red, TEXT("client refused — listen-server only"));
			return;
		}

		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController == nullptr || !PlayerController->IsLocalController())
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning,
				TEXT("%s: no local player controller"), Command);
			return;
		}

		float RequestedRadius = bFill ? DefaultRadiusCm : GPVoxelCraterMath::DefaultRadiusCm;
		float RequestedDepth = GPVoxelCraterMath::DefaultDepthCm;
		if (Args.Num() > 0 && !Args[0].IsEmpty())
		{
			RequestedRadius = FCString::Atof(*Args[0]);
		}
		if (!bFill && Args.Num() > 1 && !Args[1].IsEmpty())
		{
			RequestedDepth = FCString::Atof(*Args[1]);
		}
		const float RadiusCm = FMath::Clamp(
			FMath::IsFinite(RequestedRadius) ? RequestedRadius : (bFill ? DefaultRadiusCm : GPVoxelCraterMath::DefaultRadiusCm),
			MinRadiusCm,
			MaxRadiusCm);

		FHitResult CursorHit;
		if (!TraceCursor(World, PlayerController, CursorHit))
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning,
				TEXT("%s: cursor ray hit nothing"), Command);
			Screen(CraterMessageKey, FColor::Red, TEXT("cursor ray hit nothing"));
			return;
		}

		EGPVoxelWorldResolvePath ResolvePath = EGPVoxelWorldResolvePath::None;
		AActor* VoxelWorld = GPVoxelRuntimeProbeAdapter::ResolveVoxelWorldFromHit(CursorHit, ResolvePath);
		const UPrimitiveComponent* HitComponent = CursorHit.GetComponent();
		const AActor* HitActor = CursorHit.GetActor();
		if (VoxelWorld == nullptr)
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning,
				TEXT("%s: hit is not an AVoxelWorld. Actor=%s Component=%s Class=%s Impact=%s"),
				Command,
				HitActor != nullptr ? *HitActor->GetName() : TEXT("none"),
				HitComponent != nullptr ? *HitComponent->GetName() : TEXT("none"),
				HitComponent != nullptr ? *HitComponent->GetClass()->GetName() : TEXT("none"),
				*CursorHit.ImpactPoint.ToString());
			Screen(CraterMessageKey, FColor::Red, TEXT("not a VoxelWorld — point at empty voxel ground"));
			return;
		}
		if (!GPVoxelRuntimeProbeAdapter::IsCreated(VoxelWorld))
		{
			UE_LOG(LogGPVoxelAuthoredCrater, Warning,
				TEXT("%s: %s is not created. This command does not spawn or create a VoxelWorld."),
				Command, *VoxelWorld->GetName());
			return;
		}

		const FVector ImpactPoint = CursorHit.ImpactPoint;
		const GPVoxelCraterMath::FShallowCrater Shallow = bFill
			? GPVoxelCraterMath::FShallowCrater{}
			: GPVoxelCraterMath::Make(ImpactPoint, RequestedRadius, RequestedDepth);
		const FVector EditCenter = bFill ? ImpactPoint : Shallow.EditCenter;
		const float EditRadius = bFill ? RadiusCm : Shallow.RadiusCm;
		const FVector TraceStart = ImpactPoint + FVector(0.f, 0.f, VerticalTraceHalfExtentCm);
		const FVector TraceEnd = ImpactPoint - FVector(0.f, 0.f, VerticalTraceHalfExtentCm);
		FHitResult BaselineHit;
		const bool bBaselineHit = GPVoxelRuntimeProbeAdapter::LineTraceHitsProbe(
			World, VoxelWorld, TraceStart, TraceEnd, BaselineHit);
		const float BeforeSurfaceZ = bBaselineHit ? BaselineHit.ImpactPoint.Z : ImpactPoint.Z;

		FGPVoxelSphereSubtractRequest Request;
		Request.WorldLocation = EditCenter;
		Request.RadiusCm = EditRadius;
		FGPVoxelIntBoxReport Edited;
		const bool bApplied = bFill
			? GPVoxelRuntimeProbeAdapter::ApplySphereAdd(VoxelWorld, Request, Edited)
			: GPVoxelRuntimeProbeAdapter::ApplySphereSubtract(VoxelWorld, Request, Edited);

		const float VoxelSizeCm = GPVoxelRuntimeProbeAdapter::GetVoxelSizeCm(VoxelWorld);
		FString Label = VoxelWorld->GetName();
#if WITH_EDITOR
		Label = VoxelWorld->GetActorLabel();
#endif

		UE_LOG(LogGPVoxelAuthoredCrater, Log,
			TEXT("%s: NetMode=%s VoxelWorld=%s Label=%s Resolve=%s HitComponent=%s HitClass=%s Impact=%s EditCenter=%s RadiusRequested=%.1f Radius=%.1f Depth=%.1f CenterZOffset=%.1f VoxelSize=%.1f BeforeSurfaceZ=%.1f BaselineHit=%s Applied=%s EditedBounds valid=%s infinite=%s (%d/%d, %d/%d, %d/%d) bMultiThreaded=false bConvertToVoxelSpace=true bUpdateRender=true"),
			Command,
			NetModeName(World->GetNetMode()),
			*VoxelWorld->GetName(),
			*Label,
			ResolvePathName(ResolvePath),
			HitComponent != nullptr ? *HitComponent->GetName() : TEXT("none"),
			HitComponent != nullptr ? *HitComponent->GetClass()->GetName() : TEXT("none"),
			*ImpactPoint.ToString(),
			*EditCenter.ToString(),
			RequestedRadius,
			EditRadius,
			bFill ? 0.f : Shallow.DepthCm,
			bFill ? 0.f : Shallow.CenterZOffsetCm,
			VoxelSizeCm,
			BeforeSurfaceZ,
			bBaselineHit ? TEXT("true") : TEXT("false"),
			bApplied ? TEXT("true") : TEXT("false"),
			Edited.bValid ? TEXT("true") : TEXT("false"),
			Edited.bInfinite ? TEXT("true") : TEXT("false"),
			Edited.Min.X, Edited.Max.X, Edited.Min.Y, Edited.Max.Y, Edited.Min.Z, Edited.Max.Z);

		UE_LOG(LogGPVoxelAuthoredCrater, Log,
			TEXT("%s: runtime memory only. Map and content are not saved. Navmesh is not rebuilt (dynamic Recast deferred Stage 3E)."),
			Command);

		DrawDebugSphere(World, EditCenter, EditRadius, 24, bFill ? FColor::Cyan : FColor::Orange, false, 8.f, 0, 3.f);
		DrawDebugLine(World, TraceStart, TraceEnd, FColor::White, false, 8.f, 0, 1.5f);

		if (!bApplied)
		{
			Screen(CraterMessageKey, FColor::Red, TEXT("edit failed"));
			return;
		}

		Screen(CraterMessageKey, bFill ? FColor::Cyan : FColor::Orange,
			bFill ? TEXT("FILL APPLIED") : TEXT("CRATER APPLIED"));
		ScheduleCollisionCheck(World, VoxelWorld, ImpactPoint, EditRadius, BeforeSurfaceZ, VoxelSizeCm, bBaselineHit, bFill);
	}

	static void CraterUnderCursor(const TArray<FString>& Args, UWorld* World)
	{
		ApplyUnderCursor(Args, World, false);
	}

	static void FillUnderCursor(const TArray<FString>& Args, UWorld* World)
	{
		ApplyUnderCursor(Args, World, true);
	}

	static FAutoConsoleCommandWithWorldAndArgs GCraterUnderCursor(
		TEXT("gp.Voxel.CraterUnderCursor"),
		TEXT("PIE debug: shallow RemoveSphere on the authored AVoxelWorld under the mouse. Args: RadiusCm DepthCm. Defaults 400 80. Does not save or spawn a world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CraterUnderCursor));

	static FAutoConsoleCommandWithWorldAndArgs GFillUnderCursor(
		TEXT("gp.Voxel.FillUnderCursor"),
		TEXT("PIE debug: AddSphere on the authored AVoxelWorld under the mouse. Same targeting as CraterUnderCursor."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FillUnderCursor));
}

#endif
