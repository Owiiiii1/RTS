// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingDropAuthority.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPBuildingBase.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Effects/GPGE_SpendOrbital.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPOrbitalDeliverySettings.h"

namespace GPBuildingDropAuthorityPrivate
{
	static UGP_OrbitalBuildingInventoryComponent* GetInventory(AGP_PlayerState* PS)
	{
		return IsValid(PS) ? PS->FindComponentByClass<UGP_OrbitalBuildingInventoryComponent>() : nullptr;
	}

	static bool GetPlacementCapsuleExtent(UClass* BuildingClass, float& OutRadius, float& OutHalfHeight)
	{
		OutRadius = 80.0f;
		OutHalfHeight = 120.0f;
		if (BuildingClass == nullptr)
		{
			return false;
		}

		const AActor* CDO = BuildingClass->GetDefaultObject<AActor>();
		if (CDO == nullptr)
		{
			return false;
		}

		const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(CDO->GetRootComponent());
		if (Capsule == nullptr)
		{
			Capsule = CDO->FindComponentByClass<UCapsuleComponent>();
		}
		if (Capsule != nullptr)
		{
			OutRadius = Capsule->GetScaledCapsuleRadius();
			OutHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			return true;
		}

		const UBoxComponent* Box = CDO->FindComponentByClass<UBoxComponent>();
		if (Box != nullptr)
		{
			const FVector Extent = Box->GetScaledBoxExtent();
			OutRadius = FMath::Max(Extent.X, Extent.Y);
			OutHalfHeight = Extent.Z;
			return true;
		}

		return false;
	}

	static UClass* ResolvePayloadClassForType(EGP_OrbitalBuildingType BuildingType)
	{
		const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
		switch (BuildingType)
		{
		case EGP_OrbitalBuildingType::LogisticsHub:
			return Settings != nullptr
				? *Settings->ResolveBuildingPayloadClass()
				: AGP_LogisticsHub::StaticClass();
		default:
			return nullptr;
		}
	}
}

float GPBuildingDropAuthority::GetPurchaseCostForType(EGP_OrbitalBuildingType BuildingType)
{
	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	if (Settings == nullptr)
	{
		return 0.0f;
	}

	switch (BuildingType)
	{
	case EGP_OrbitalBuildingType::LogisticsHub:
		return FMath::Max(0.0f, Settings->BuildingOrbitalPurchaseCost);
	default:
		return 0.0f;
	}
}

bool GPBuildingDropAuthority::ValidateInterimPlacement(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	EGP_OrbitalBuildingType BuildingType,
	const FTransform& WorldTransform,
	EGP_BuildingDropRejectReason& OutReject)
{
	OutReject = EGP_BuildingDropRejectReason::None;

	if (World == nullptr || !IsValid(RequestingPlayerState))
	{
		OutReject = EGP_BuildingDropRejectReason::MissingPlayerState;
		return false;
	}

	if (BuildingType == EGP_OrbitalBuildingType::None)
	{
		OutReject = EGP_BuildingDropRejectReason::InvalidType;
		return false;
	}

	const FVector Loc = WorldTransform.GetLocation();
	if (Loc.ContainsNaN()
		|| !FMath::IsFinite(Loc.X) || !FMath::IsFinite(Loc.Y) || !FMath::IsFinite(Loc.Z))
	{
		OutReject = EGP_BuildingDropRejectReason::InvalidTransform;
		return false;
	}

	const int32 TeamId = RequestingPlayerState->GetTeamId();
	if (TeamId < 1)
	{
		OutReject = EGP_BuildingDropRejectReason::MissingMainBase;
		return false;
	}

	AGP_GameState* GS = World->GetGameState<AGP_GameState>();
	AGP_MainBase* MainBase = GS != nullptr ? GS->FindMainBaseForTeam(TeamId) : nullptr;
	if (!IsValid(MainBase))
	{
		OutReject = EGP_BuildingDropRejectReason::MissingMainBase;
		return false;
	}

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const float MaxRadius = Settings != nullptr
		? FMath::Max(100.0f, Settings->BuildingMaxDeployRadiusFromMainBaseCm)
		: 5000.0f;
	const float Dist2D = FVector::Dist2D(Loc, MainBase->GetActorLocation());
	if (Dist2D > MaxRadius + KINDA_SMALL_NUMBER)
	{
		OutReject = EGP_BuildingDropRejectReason::OutOfDeployRadius;
		return false;
	}

	UClass* PayloadClass = GPBuildingDropAuthorityPrivate::ResolvePayloadClassForType(BuildingType);
	if (PayloadClass == nullptr)
	{
		OutReject = EGP_BuildingDropRejectReason::InvalidType;
		return false;
	}

	float PlaceRadius = 80.0f;
	float PlaceHalfHeight = 120.0f;
	GPBuildingDropAuthorityPrivate::GetPlacementCapsuleExtent(PayloadClass, PlaceRadius, PlaceHalfHeight);
	const float Margin = Settings != nullptr
		? FMath::Max(0.0f, Settings->BuildingPlacementOverlapMarginCm)
		: 25.0f;
	PlaceRadius += Margin;

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(PlaceRadius, PlaceHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GPBuildingPlacementOverlap), false);
	QueryParams.AddIgnoredActor(RequestingPlayerState);

	TArray<FOverlapResult> Overlaps;
	const bool bHit = World->OverlapMultiByChannel(
		Overlaps,
		Loc,
		WorldTransform.GetRotation(),
		ECC_Pawn,
		Shape,
		QueryParams);

	if (bHit)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Other = Overlap.GetActor();
			if (!IsValid(Other))
			{
				continue;
			}
			if (Other->IsA(AGP_BuildingBase::StaticClass()))
			{
				OutReject = EGP_BuildingDropRejectReason::PlacementOverlap;
				return false;
			}
		}
	}

	return true;
}

GPBuildingDropAuthority::FPurchaseResult GPBuildingDropAuthority::AuthorityPurchaseBuilding(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	EGP_OrbitalBuildingType BuildingType)
{
	FPurchaseResult Result;

	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpawnFailed;
		return Result;
	}

	if (!IsValid(RequestingPlayerState))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingPlayerState;
		return Result;
	}

	if (BuildingType == EGP_OrbitalBuildingType::None)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::InvalidType;
		return Result;
	}

	const int32 TeamId = RequestingPlayerState->GetTeamId();
	if (TeamId < 1)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingMainBase;
		return Result;
	}

	AGP_GameState* GS = World->GetGameState<AGP_GameState>();
	if (GS == nullptr || !IsValid(GS->FindMainBaseForTeam(TeamId)))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingMainBase;
		return Result;
	}

	UGP_OrbitalBuildingInventoryComponent* Inventory =
		GPBuildingDropAuthorityPrivate::GetInventory(RequestingPlayerState);
	if (Inventory == nullptr)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingPlayerState;
		return Result;
	}

	Result.OrbitalCost = GetPurchaseCostForType(BuildingType);
	if (Result.OrbitalCost <= KINDA_SMALL_NUMBER)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::InvalidType;
		return Result;
	}

	UGP_AbilitySystemComponent* ASC = RequestingPlayerState->GetGPAbilitySystemComponent();
	const UGP_PlayerAttributeSet* Attr = RequestingPlayerState->GetPlayerAttributeSet();
	if (!IsValid(ASC) || Attr == nullptr)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpendFailed;
		return Result;
	}

	const float Orbital = Attr->GetOrbitalFerronite();
	if (Orbital + KINDA_SMALL_NUMBER < Result.OrbitalCost)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::InsufficientOrbital;
		return Result;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(RequestingPlayerState);
	FGameplayEffectSpecHandle Spec =
		ASC->MakeOutgoingSpec(UGP_GE_SpendOrbital::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpendFailed;
		return Result;
	}
	Spec.Data->SetSetByCallerMagnitude(
		UGP_GE_SpendOrbital::GetMagnitudeDataName(),
		-FMath::Abs(Result.OrbitalCost));
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	const float OrbitalAfter = Attr->GetOrbitalFerronite();
	if (OrbitalAfter > Orbital - Result.OrbitalCost + 1.0f)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpendFailed;
		return Result;
	}

	if (!Inventory->AuthorityAddReady(BuildingType, 1))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::InvalidType;
		return Result;
	}

	Result.bAccepted = true;
	Result.ReadyAfter = Inventory->GetReadyCount(BuildingType);
	return Result;
}

GPBuildingDropAuthority::FDeployResult GPBuildingDropAuthority::AuthorityDeployBuilding(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	EGP_OrbitalBuildingType BuildingType,
	const FTransform& WorldTransform)
{
	FDeployResult Result;

	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpawnFailed;
		return Result;
	}

	if (!IsValid(RequestingPlayerState))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingPlayerState;
		return Result;
	}

	UGP_OrbitalBuildingInventoryComponent* Inventory =
		GPBuildingDropAuthorityPrivate::GetInventory(RequestingPlayerState);
	if (Inventory == nullptr)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingPlayerState;
		return Result;
	}

	if (Inventory->GetReadyCount(BuildingType) <= 0)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::NoReadyInventory;
		return Result;
	}

	EGP_BuildingDropRejectReason PlacementReject = EGP_BuildingDropRejectReason::None;
	if (!ValidateInterimPlacement(World, RequestingPlayerState, BuildingType, WorldTransform, PlacementReject))
	{
		Result.RejectReason = PlacementReject;
		return Result;
	}

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	if (Settings == nullptr)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpawnFailed;
		return Result;
	}

	const int32 TeamId = RequestingPlayerState->GetTeamId();
	const FVector LandingLoc = WorldTransform.GetLocation();
	const FRotator LandingRot = WorldTransform.GetRotation().Rotator();
	const float Altitude = Settings->BuildingDropSpawnAltitudeCm;
	const FVector StartLoc = LandingLoc + FVector(0.0f, 0.0f, Altitude);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = RequestingPlayerState;
	SpawnParams.ObjectFlags |= RF_Transient;

	AGP_DropPod* Pod = World->SpawnActor<AGP_DropPod>(
		Settings->ResolveBuildingDropPodClass(),
		StartLoc,
		LandingRot,
		SpawnParams);
	if (!IsValid(Pod))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpawnFailed;
		return Result;
	}

	if (!Inventory->AuthorityTryConsumeReady(BuildingType, 1))
	{
		Pod->Destroy();
		Result.RejectReason = EGP_BuildingDropRejectReason::NoReadyInventory;
		return Result;
	}

	Pod->AuthorityInitBuildingDrop(
		RequestingPlayerState,
		TeamId,
		BuildingType,
		LandingLoc,
		LandingRot,
		Settings->BuildingDropDescentDurationSeconds,
		Settings->BuildingDropSpawnAltitudeCm,
		Settings->BuildingDropPayloadDeployDelaySeconds,
		Settings->BuildingDropCleanupDelaySeconds);

	Result.bAccepted = true;
	Result.ReadyAfter = Inventory->GetReadyCount(BuildingType);
	Result.SpawnedPod = Pod;
	return Result;
}
