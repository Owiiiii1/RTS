// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPWallPackageAuthority.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPMainBase.h"
#include "Buildings/GPWallSegmentInventoryComponent.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Effects/GPGE_SpendOrbital.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPWallPackageCatalog.h"
#include "Orbital/GPWallPackageDefinition.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPOrbitalDeliverySettings.h"

namespace GPWallPackageAuthorityPrivate
{
#if !UE_BUILD_SHIPPING
	static bool GForceNextPodSpawnFailure = false;
#endif

	static void RefundOrbital(AGP_PlayerState* PS, float Amount)
	{
		if (!IsValid(PS) || Amount <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		UGP_AbilitySystemComponent* ASC = PS->GetGPAbilitySystemComponent();
		if (!IsValid(ASC))
		{
			return;
		}

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(PS);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGP_GE_AddOrbital::StaticClass(), 1.0f, Context);
		if (!Spec.IsValid())
		{
			return;
		}
		Spec.Data->SetSetByCallerMagnitude(UGP_GE_AddOrbital::GetMagnitudeDataName(), FMath::Abs(Amount));
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

#if !UE_BUILD_SHIPPING
void GPWallPackageAuthority::DebugForceNextPodSpawnFailure(bool bForce)
{
	GPWallPackageAuthorityPrivate::GForceNextPodSpawnFailure = bForce;
}

bool GPWallPackageAuthority::DebugConsumeForcedPodSpawnFailure()
{
	const bool bForce = GPWallPackageAuthorityPrivate::GForceNextPodSpawnFailure;
	GPWallPackageAuthorityPrivate::GForceNextPodSpawnFailure = false;
	return bForce;
}
#endif

GPWallPackageAuthority::FPurchaseResult GPWallPackageAuthority::AuthorityPurchaseWallPackage(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	const UGP_WallPackageDefinition* PackageDefinition)
{
	FPurchaseResult Result;

	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		Result.RejectReason = EGP_WallPackageRejectReason::SpawnFailed;
		return Result;
	}

	if (!AGP_GameState::AreEconomicOrdersAllowedInWorld(World))
	{
		Result.RejectReason = EGP_WallPackageRejectReason::MatchFinished;
		return Result;
	}

	if (!IsValid(RequestingPlayerState))
	{
		Result.RejectReason = EGP_WallPackageRejectReason::MissingPlayerState;
		return Result;
	}

	UGP_WallPackageCatalog& Catalog = UGP_WallPackageCatalog::Get();
	if (Catalog.IsWallPackageDefinitionPending())
	{
		Result.RejectReason = EGP_WallPackageRejectReason::DefinitionNotReady;
		return Result;
	}

	const UGP_WallPackageDefinition* Package = PackageDefinition;
	if (!IsValid(Package))
	{
		Package = Catalog.GetWallPackage();
	}
	if (!IsValid(Package))
	{
		Result.RejectReason = Catalog.IsWallPackageDefinitionPending()
			? EGP_WallPackageRejectReason::DefinitionNotReady
			: EGP_WallPackageRejectReason::InvalidDefinition;
		return Result;
	}

	if (!Package->IsValidForDelivery(UGP_WallSegmentInventoryComponent::DefaultCapacity))
	{
		Result.RejectReason = EGP_WallPackageRejectReason::InvalidDefinition;
		return Result;
	}

	const int32 TeamId = RequestingPlayerState->GetTeamId();
	if (TeamId < 1)
	{
		Result.RejectReason = EGP_WallPackageRejectReason::MissingMainBase;
		return Result;
	}

	AGP_GameState* GS = World->GetGameState<AGP_GameState>();
	AGP_MainBase* MainBase = GS != nullptr ? GS->FindMainBaseForTeam(TeamId) : nullptr;
	if (!IsValid(MainBase) || MainBase->IsDead())
	{
		Result.RejectReason = EGP_WallPackageRejectReason::MissingMainBase;
		return Result;
	}

	UGP_WallSegmentInventoryComponent* Inventory = MainBase->GetWallSegmentInventoryComponent();
	if (!IsValid(Inventory))
	{
		Result.RejectReason = EGP_WallPackageRejectReason::InvalidInventory;
		return Result;
	}

	if (Inventory->GetWallSegmentCount() != 0)
	{
		Result.RejectReason = EGP_WallPackageRejectReason::InventoryFull;
		return Result;
	}

	if (Inventory->IsWallPackagePending())
	{
		Result.RejectReason = EGP_WallPackageRejectReason::PackagePending;
		return Result;
	}

	USceneComponent* DropZone = MainBase->GetWallPackageDropZone();
	if (!IsValid(DropZone))
	{
		Result.RejectReason = EGP_WallPackageRejectReason::MissingMainBase;
		return Result;
	}

	const FTransform DropXform = DropZone->GetComponentTransform();
	const FVector DropLoc = DropXform.GetLocation();
	if (DropLoc.ContainsNaN() || !FMath::IsFinite(DropLoc.X) || !FMath::IsFinite(DropLoc.Y) || !FMath::IsFinite(DropLoc.Z))
	{
		Result.RejectReason = EGP_WallPackageRejectReason::MissingMainBase;
		return Result;
	}

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	if (Settings == nullptr)
	{
		Result.RejectReason = EGP_WallPackageRejectReason::SpawnFailed;
		return Result;
	}

	TSubclassOf<AGP_DropPod> PodClass = Settings->ResolveUnitDropPodClass();
	if (PodClass == nullptr)
	{
		Result.RejectReason = EGP_WallPackageRejectReason::SpawnFailed;
		return Result;
	}

	Result.OrbitalCost = FMath::Max(0.0f, Package->Cost);

	UGP_AbilitySystemComponent* ASC = RequestingPlayerState->GetGPAbilitySystemComponent();
	const UGP_PlayerAttributeSet* Attr = RequestingPlayerState->GetPlayerAttributeSet();
	if (!IsValid(ASC) || Attr == nullptr)
	{
		Result.RejectReason = EGP_WallPackageRejectReason::SpendFailed;
		return Result;
	}

	const float Orbital = Attr->GetOrbitalFerronite();
	if (Orbital + KINDA_SMALL_NUMBER < Result.OrbitalCost)
	{
		Result.RejectReason = EGP_WallPackageRejectReason::InsufficientOrbital;
		return Result;
	}

	if (!Inventory->AuthorityBeginPackageDelivery())
	{
		Result.RejectReason = EGP_WallPackageRejectReason::PackagePending;
		return Result;
	}

	const int32 DeliveryGeneration = Inventory->GetDeliveryGeneration();

	if (Result.OrbitalCost > KINDA_SMALL_NUMBER)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(RequestingPlayerState);
		FGameplayEffectSpecHandle Spec =
			ASC->MakeOutgoingSpec(UGP_GE_SpendOrbital::StaticClass(), 1.0f, Context);
		if (!Spec.IsValid())
		{
			Inventory->AuthorityCancelPackageDelivery();
			Result.RejectReason = EGP_WallPackageRejectReason::SpendFailed;
			return Result;
		}
		Spec.Data->SetSetByCallerMagnitude(
			UGP_GE_SpendOrbital::GetMagnitudeDataName(),
			-FMath::Abs(Result.OrbitalCost));
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

		const float OrbitalAfter = Attr->GetOrbitalFerronite();
		if (OrbitalAfter > Orbital - Result.OrbitalCost + 1.0f)
		{
			Inventory->AuthorityCancelPackageDelivery();
			Result.RejectReason = EGP_WallPackageRejectReason::SpendFailed;
			return Result;
		}
	}

#if !UE_BUILD_SHIPPING
	if (DebugConsumeForcedPodSpawnFailure())
	{
		Inventory->AuthorityCancelPackageDelivery();
		GPWallPackageAuthorityPrivate::RefundOrbital(RequestingPlayerState, Result.OrbitalCost);
		Result.RejectReason = EGP_WallPackageRejectReason::SpawnFailed;
		Result.StockAfter = Inventory->GetWallSegmentCount();
		Result.bPending = Inventory->IsWallPackagePending();
		return Result;
	}
#endif

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = RequestingPlayerState;
	SpawnParams.ObjectFlags |= RF_Transient;

	const FRotator LandingRot = DropXform.GetRotation().Rotator();
	const float Altitude = Settings->BuildingDropSpawnAltitudeCm;
	const FVector StartLoc = DropLoc + FVector(0.0f, 0.0f, Altitude);

	AGP_DropPod* Pod = World->SpawnActor<AGP_DropPod>(PodClass, StartLoc, LandingRot, SpawnParams);
	if (!IsValid(Pod))
	{
		Inventory->AuthorityCancelPackageDelivery();
		GPWallPackageAuthorityPrivate::RefundOrbital(RequestingPlayerState, Result.OrbitalCost);
		Result.RejectReason = EGP_WallPackageRejectReason::SpawnFailed;
		Result.StockAfter = Inventory->GetWallSegmentCount();
		Result.bPending = Inventory->IsWallPackagePending();
		return Result;
	}

	Pod->AuthorityInitWallPackageDrop(
		RequestingPlayerState,
		TeamId,
		MainBase,
		DeliveryGeneration,
		Package->SegmentCount,
		DropLoc,
		LandingRot,
		Package->DeliveryDescentSeconds,
		Altitude,
		Package->PayloadDeployDelaySeconds,
		Settings->BuildingDropCleanupDelaySeconds);

	Result.bAccepted = true;
	Result.bPending = true;
	Result.StockAfter = Inventory->GetWallSegmentCount();
	Result.SpawnedPod = Pod;
	return Result;
}
