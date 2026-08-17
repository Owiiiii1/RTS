// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/GPCombatLOS.h"

#include "CollisionQueryParams.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Units/GPUnitBase.h"

namespace GPCombatLOS
{
	namespace Private
	{
		static USceneComponent* FindCombatOriginAnchor(const AActor* Actor)
		{
			if (Actor == nullptr)
			{
				return nullptr;
			}

			TArray<USceneComponent*> SceneComps;
			Actor->GetComponents<USceneComponent>(SceneComps);
			for (USceneComponent* Comp : SceneComps)
			{
				if (Comp == nullptr)
				{
					continue;
				}
				const FName CompName = Comp->GetFName();
				if (CompName == TEXT("CombatOrigin") || CompName == TEXT("MuzzleAnchor"))
				{
					return Comp;
				}
			}
			return nullptr;
		}

		static bool TryGetSocketLocation(const AActor* Actor, FName SocketName, FVector& OutLocation)
		{
			if (Actor == nullptr || SocketName.IsNone())
			{
				return false;
			}

			TArray<UActorComponent*> MeshComps;
			Actor->GetComponents(UMeshComponent::StaticClass(), MeshComps);
			for (UActorComponent* Comp : MeshComps)
			{
				if (USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Comp))
				{
					if (Skel->DoesSocketExist(SocketName))
					{
						OutLocation = Skel->GetSocketLocation(SocketName);
						return true;
					}
				}
				if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Comp))
				{
					if (StaticMesh->DoesSocketExist(SocketName))
					{
						OutLocation = StaticMesh->GetSocketLocation(SocketName);
						return true;
					}
				}
			}
			return false;
		}

		static bool ResolveBoundsPoints(
			const AActor* Actor,
			FVector& OutTop,
			FVector& OutCenter,
			FVector& OutFeet)
		{
			if (Actor == nullptr)
			{
				return false;
			}

			FVector Origin = FVector::ZeroVector;
			FVector Extent = FVector::ZeroVector;
			Actor->GetActorBounds(false, Origin, Extent);
			if (!Origin.ContainsNaN() && Extent.GetAbsMax() > KINDA_SMALL_NUMBER)
			{
				OutCenter = Origin;
				OutTop = Origin + FVector(0.0f, 0.0f, Extent.Z);
				OutFeet = Origin - FVector(0.0f, 0.0f, Extent.Z) + FVector(0.0f, 0.0f, 10.0f);
				return true;
			}

			const FVector Loc = Actor->GetActorLocation();
			if (Loc.ContainsNaN())
			{
				return false;
			}
			OutCenter = Loc;
			OutTop = Loc + FVector(0.0f, 0.0f, 88.0f);
			OutFeet = Loc + FVector(0.0f, 0.0f, 10.0f);
			return true;
		}

		static bool IsTraceClearToTarget(
			UWorld* World,
			const FVector& Start,
			const FVector& End,
			const AActor* Source,
			const AActor* Target)
		{
			if (World == nullptr || Source == nullptr || Target == nullptr)
			{
				return false;
			}

			FCollisionQueryParams Params(SCENE_QUERY_STAT(GPCombatLOS), false, Source);
			Params.AddIgnoredActor(Source);
			Params.bReturnPhysicalMaterial = false;

			FHitResult Hit;
			const bool bHit = World->LineTraceSingleByChannel(
				Hit,
				Start,
				End,
				ECC_Visibility,
				Params);

			if (!bHit)
			{
				return true;
			}

			return Hit.GetActor() == Target;
		}
	}

	FGP_LOSTracePoints ResolveAttackOriginPoints(const AActor* Attacker)
	{
		FGP_LOSTracePoints Points;
		if (Attacker == nullptr)
		{
			return Points;
		}

		FVector Top;
		FVector Center;
		FVector Feet;
		if (!Private::ResolveBoundsPoints(Attacker, Top, Center, Feet))
		{
			return Points;
		}

		Points.Eye = Top;
		Points.Chest = Center;
		Points.Feet = Feet;
		if (USceneComponent* Anchor = Private::FindCombatOriginAnchor(Attacker))
		{
			Points.Eye = Anchor->GetComponentLocation();
		}
		Private::TryGetSocketLocation(Attacker, TEXT("AttackOrigin_Eye"), Points.Eye);
		Private::TryGetSocketLocation(Attacker, TEXT("AttackOrigin_Chest"), Points.Chest);
		Private::TryGetSocketLocation(Attacker, TEXT("AttackOrigin_Feet"), Points.Feet);
		Points.bValid = true;
		return Points;
	}

	FGP_LOSTracePoints ResolveHitPoints(const AActor* Target)
	{
		FGP_LOSTracePoints Points;
		if (Target == nullptr)
		{
			return Points;
		}

		FVector Top;
		FVector Center;
		FVector Feet;
		if (!Private::ResolveBoundsPoints(Target, Top, Center, Feet))
		{
			return Points;
		}

		Points.Eye = Top;   // Head
		Points.Chest = Center;
		Points.Feet = Feet;
		Private::TryGetSocketLocation(Target, TEXT("Hit_Head"), Points.Eye);
		Private::TryGetSocketLocation(Target, TEXT("Hit_Chest"), Points.Chest);
		Private::TryGetSocketLocation(Target, TEXT("Hit_Feet"), Points.Feet);
		Points.bValid = true;
		return Points;
	}

	bool HasLineOfSight(UWorld* World, const AActor* Source, const AActor* Target)
	{
		if (World == nullptr || Source == nullptr || Target == nullptr)
		{
			return false;
		}

		if (!IsValid(Source) || !IsValid(Target))
		{
			return false;
		}

		const FGP_LOSTracePoints Origins = ResolveAttackOriginPoints(Source);
		const FGP_LOSTracePoints Hits = ResolveHitPoints(Target);
		if (!Origins.bValid || !Hits.bValid)
		{
			return false;
		}

		if (Private::IsTraceClearToTarget(World, Origins.Eye, Hits.Eye, Source, Target))
		{
			return true;
		}
		if (Private::IsTraceClearToTarget(World, Origins.Chest, Hits.Chest, Source, Target))
		{
			return true;
		}
		if (Private::IsTraceClearToTarget(World, Origins.Feet, Hits.Feet, Source, Target))
		{
			return true;
		}

		return false;
	}
}
