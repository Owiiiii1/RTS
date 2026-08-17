// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPUnitDropAuthority.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPMainBase.h"
#include "Components/SceneComponent.h"
#include "Effects/GPGE_SpendOrbital.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "Orbital/GPDropPod.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPOrbitalDeliverySettings.h"

namespace GPUnitDropAuthority
{
	bool ComputeManifestCosts(
		const FGP_UnitDropManifest& Manifest,
		int32& OutSlotCost,
		float& OutOrbitalCost,
		int32& OutUnitCount,
		EGP_UnitDropRejectReason& OutReject)
	{
		OutSlotCost = 0;
		OutOrbitalCost = 0.0f;
		OutUnitCount = 0;
		OutReject = EGP_UnitDropRejectReason::None;

		if (Manifest.WorkerCount < 0 || Manifest.SalvageWalkerCount < 0)
		{
			OutReject = EGP_UnitDropRejectReason::InvalidCounts;
			return false;
		}

		OutUnitCount = Manifest.GetTotalUnitCount();
		if (OutUnitCount <= 0)
		{
			OutReject = EGP_UnitDropRejectReason::EmptyManifest;
			return false;
		}

		const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
		if (Settings == nullptr)
		{
			OutReject = EGP_UnitDropRejectReason::InvalidCounts;
			return false;
		}

		const int32 WorkerSlots = FMath::Max(1, Settings->WorkerTransportSlotCost);
		const int32 WalkerSlots = FMath::Max(1, Settings->SalvageWalkerTransportSlotCost);
		const int32 Capacity = FMath::Max(1, Settings->PodTransportSlotCapacity);

		OutSlotCost =
			Manifest.WorkerCount * WorkerSlots
			+ Manifest.SalvageWalkerCount * WalkerSlots;
		OutOrbitalCost =
			static_cast<float>(Manifest.WorkerCount) * Settings->WorkerOrbitalDropCost
			+ static_cast<float>(Manifest.SalvageWalkerCount) * Settings->SalvageWalkerOrbitalDropCost;

		if (OutSlotCost > Capacity)
		{
			OutReject = EGP_UnitDropRejectReason::SlotOverflow;
			return false;
		}

		return true;
	}

	FEvalResult AuthorityRequestUnitDrop(
		UWorld* World,
		AGP_PlayerState* RequestingPlayerState,
		const FGP_UnitDropManifest& Manifest)
	{
		FEvalResult Result;

		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			Result.RejectReason = EGP_UnitDropRejectReason::SpawnFailed;
			return Result;
		}

		if (!IsValid(RequestingPlayerState))
		{
			Result.RejectReason = EGP_UnitDropRejectReason::SpendFailed;
			return Result;
		}

		const int32 TeamId = RequestingPlayerState->GetTeamId();
		if (TeamId < 1)
		{
			Result.RejectReason = EGP_UnitDropRejectReason::MissingMainBase;
			return Result;
		}

		EGP_UnitDropRejectReason CostReject = EGP_UnitDropRejectReason::None;
		if (!ComputeManifestCosts(
			Manifest,
			Result.SlotCost,
			Result.OrbitalCost,
			Result.UnitCount,
			CostReject))
		{
			Result.RejectReason = CostReject;
			return Result;
		}

		UGP_AbilitySystemComponent* ASC = RequestingPlayerState->GetGPAbilitySystemComponent();
		const UGP_PlayerAttributeSet* Attr = RequestingPlayerState->GetPlayerAttributeSet();
		if (!IsValid(ASC) || Attr == nullptr)
		{
			Result.RejectReason = EGP_UnitDropRejectReason::SpendFailed;
			return Result;
		}

		const float Orbital = Attr->GetOrbitalFerronite();
		if (Orbital + KINDA_SMALL_NUMBER < Result.OrbitalCost)
		{
			Result.RejectReason = EGP_UnitDropRejectReason::InsufficientOrbital;
			return Result;
		}

		if (!RequestingPlayerState->CanAcceptManifestUnitCount(Result.UnitCount))
		{
			Result.RejectReason = EGP_UnitDropRejectReason::UnitCapReached;
			return Result;
		}

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		AGP_MainBase* MainBase = GS != nullptr ? GS->FindMainBaseForTeam(TeamId) : nullptr;
		if (!IsValid(MainBase))
		{
			Result.RejectReason = EGP_UnitDropRejectReason::MissingMainBase;
			return Result;
		}

		USceneComponent* DropZone = MainBase->GetUnitDropZone();
		if (!IsValid(DropZone))
		{
			Result.RejectReason = EGP_UnitDropRejectReason::MissingDropZone;
			return Result;
		}

		const FTransform DropXform = DropZone->GetComponentTransform();
		const FVector DropLoc = DropXform.GetLocation();
		if (DropLoc.ContainsNaN() || !FMath::IsFinite(DropLoc.X) || !FMath::IsFinite(DropLoc.Y) || !FMath::IsFinite(DropLoc.Z))
		{
			Result.RejectReason = EGP_UnitDropRejectReason::MissingDropZone;
			return Result;
		}

		const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
		if (Settings == nullptr)
		{
			Result.RejectReason = EGP_UnitDropRejectReason::SpawnFailed;
			return Result;
		}

		if (!RequestingPlayerState->TryReserveOrbitalUnits(Result.UnitCount))
		{
			Result.RejectReason = EGP_UnitDropRejectReason::UnitCapReached;
			return Result;
		}

		// Spend exactly once after validation, before spawn.
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(RequestingPlayerState);
		FGameplayEffectSpecHandle Spec =
			ASC->MakeOutgoingSpec(UGP_GE_SpendOrbital::StaticClass(), 1.0f, Context);
		if (!Spec.IsValid())
		{
			RequestingPlayerState->ReleaseOrbitalUnitReservation(Result.UnitCount);
			Result.RejectReason = EGP_UnitDropRejectReason::SpendFailed;
			return Result;
		}
		Spec.Data->SetSetByCallerMagnitude(
			UGP_GE_SpendOrbital::GetMagnitudeDataName(),
			-FMath::Abs(Result.OrbitalCost));
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

		const float OrbitalAfter = Attr->GetOrbitalFerronite();
		if (OrbitalAfter > Orbital - Result.OrbitalCost + 1.0f)
		{
			RequestingPlayerState->ReleaseOrbitalUnitReservation(Result.UnitCount);
			Result.RejectReason = EGP_UnitDropRejectReason::SpendFailed;
			return Result;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = RequestingPlayerState;
		SpawnParams.ObjectFlags |= RF_Transient;

		const FVector LandingLoc = DropLoc;
		const FRotator LandingRot = DropXform.GetRotation().Rotator();
		const float Altitude = Settings->UnitDropSpawnAltitudeCm;
		const FVector StartLoc = LandingLoc + FVector(0.0f, 0.0f, Altitude);

		AGP_DropPod* Pod = World->SpawnActor<AGP_DropPod>(
			Settings->ResolveUnitDropPodClass(),
			StartLoc,
			LandingRot,
			SpawnParams);
		if (!IsValid(Pod))
		{
			RequestingPlayerState->ReleaseOrbitalUnitReservation(Result.UnitCount);
			Result.RejectReason = EGP_UnitDropRejectReason::SpawnFailed;
			return Result;
		}

		Pod->AuthorityInitUnitDrop(
			RequestingPlayerState,
			TeamId,
			LandingLoc,
			LandingRot,
			Manifest,
			Settings->UnitDropDescentDurationSeconds,
			Settings->UnitDropSpawnAltitudeCm,
			Settings->UnitDropSpawnSpacingCm,
			Settings->UnitDropPayloadDeployDelaySeconds,
			Settings->UnitDropCleanupDelaySeconds);

		Result.bAccepted = true;
		Result.SpawnedPod = Pod;
		return Result;
	}
}
