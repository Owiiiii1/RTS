// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingDropAuthority.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPBuildingBase.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPMainBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Effects/GPGE_SpendOrbital.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPOrbitalDeliverySettings.h"

namespace GPBuildingDropAuthorityPrivate
{
	static UGP_OrbitalBuildingInventoryComponent* GetInventory(AGP_PlayerState* PS)
	{
		return IsValid(PS) ? PS->FindComponentByClass<UGP_OrbitalBuildingInventoryComponent>() : nullptr;
	}

	static UGP_OrbitalDropDefinition* ResolveLegacyDrop(EGP_OrbitalBuildingType BuildingType)
	{
		if (BuildingType != EGP_OrbitalBuildingType::LogisticsHub)
		{
			return nullptr;
		}
		return UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop();
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
}

float GPBuildingDropAuthority::GetPurchaseCost(const UGP_OrbitalDropDefinition* DropDefinition)
{
	return UGP_BuildingDropCatalog::Get().GetPurchaseCost(DropDefinition);
}

float GPBuildingDropAuthority::GetPurchaseCostForType(EGP_OrbitalBuildingType BuildingType)
{
	return GetPurchaseCost(GPBuildingDropAuthorityPrivate::ResolveLegacyDrop(BuildingType));
}

bool GPBuildingDropAuthority::ValidateInterimPlacement(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	const UGP_OrbitalDropDefinition* DropDefinition,
	const FTransform& WorldTransform,
	EGP_BuildingDropRejectReason& OutReject)
{
	OutReject = EGP_BuildingDropRejectReason::None;

	if (World == nullptr || !IsValid(RequestingPlayerState))
	{
		OutReject = EGP_BuildingDropRejectReason::MissingPlayerState;
		return false;
	}

	if (!IsValid(DropDefinition))
	{
		OutReject = EGP_BuildingDropRejectReason::InvalidDefinition;
		return false;
	}

	if (DropDefinition->ResolveLoadedBuildingDefinition() == nullptr)
	{
		OutReject = EGP_BuildingDropRejectReason::MissingBuildingDefinition;
		return false;
	}

	const TSubclassOf<AGP_BuildingBase> PayloadClass =
		UGP_BuildingDropCatalog::Get().ResolvePayloadClass(DropDefinition);
	if (PayloadClass == nullptr)
	{
		OutReject = EGP_BuildingDropRejectReason::MissingSpawnedClass;
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

	float PlaceRadius = 80.0f;
	float PlaceHalfHeight = 120.0f;
	GPBuildingDropAuthorityPrivate::GetPlacementCapsuleExtent(*PayloadClass, PlaceRadius, PlaceHalfHeight);
	const float Margin = Settings != nullptr
		? FMath::Max(0.0f, Settings->BuildingPlacementOverlapMarginCm)
		: 25.0f;

	// INTERIM_MVP_PLACEMENT_VALIDATION (BuildGrid deferred):
	// Building capsules intentionally Ignore ECC_Pawn (Visibility-only selection).
	for (TActorIterator<AGP_BuildingBase> It(World); It; ++It)
	{
		AGP_BuildingBase* OtherBuilding = *It;
		if (!IsValid(OtherBuilding))
		{
			continue;
		}

		float OtherRadius = 80.0f;
		float OtherHalfHeight = 120.0f;
		GPBuildingDropAuthorityPrivate::GetPlacementCapsuleExtent(
			OtherBuilding->GetClass(),
			OtherRadius,
			OtherHalfHeight);

		const float MinSeparation2D = PlaceRadius + OtherRadius + Margin;
		const float Dist2DToOther = FVector::Dist2D(Loc, OtherBuilding->GetActorLocation());
		if (Dist2DToOther <= MinSeparation2D + KINDA_SMALL_NUMBER)
		{
			const float VerticalGap = FMath::Abs(Loc.Z - OtherBuilding->GetActorLocation().Z);
			const float MinSeparationZ = PlaceHalfHeight + OtherHalfHeight + Margin;
			if (VerticalGap <= MinSeparationZ + KINDA_SMALL_NUMBER)
			{
				OutReject = EGP_BuildingDropRejectReason::PlacementOverlap;
				return false;
			}
		}
	}

	return true;
}

bool GPBuildingDropAuthority::ValidateInterimPlacement(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	EGP_OrbitalBuildingType BuildingType,
	const FTransform& WorldTransform,
	EGP_BuildingDropRejectReason& OutReject)
{
	return ValidateInterimPlacement(
		World,
		RequestingPlayerState,
		GPBuildingDropAuthorityPrivate::ResolveLegacyDrop(BuildingType),
		WorldTransform,
		OutReject);
}

GPBuildingDropAuthority::FPurchaseResult GPBuildingDropAuthority::AuthorityPurchaseBuilding(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	const UGP_OrbitalDropDefinition* DropDefinition)
{
	FPurchaseResult Result;

	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpawnFailed;
		return Result;
	}

	if (!AGP_GameState::AreEconomicOrdersAllowedInWorld(World))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MatchFinished;
		return Result;
	}

	if (!IsValid(RequestingPlayerState))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingPlayerState;
		return Result;
	}

	if (!IsValid(DropDefinition))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::InvalidDefinition;
		return Result;
	}

	Result.DropDefinitionId = DropDefinition->GetPrimaryAssetId();
	if (!Result.DropDefinitionId.IsValid())
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::InvalidDefinition;
		return Result;
	}

	if (DropDefinition->ResolveLoadedBuildingDefinition() == nullptr)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingBuildingDefinition;
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

	Result.OrbitalCost = GetPurchaseCost(DropDefinition);
	if (Result.OrbitalCost <= KINDA_SMALL_NUMBER)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::InvalidDefinition;
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

	if (!Inventory->AuthorityAddReady(DropDefinition, 1))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::InvalidDefinition;
		return Result;
	}

	Result.bAccepted = true;
	Result.ReadyAfter = Inventory->GetReadyCount(DropDefinition);
	return Result;
}

GPBuildingDropAuthority::FPurchaseResult GPBuildingDropAuthority::AuthorityPurchaseBuilding(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	EGP_OrbitalBuildingType BuildingType)
{
	if (BuildingType == EGP_OrbitalBuildingType::None)
	{
		FPurchaseResult Result;
		Result.RejectReason = EGP_BuildingDropRejectReason::InvalidType;
		return Result;
	}

	return AuthorityPurchaseBuilding(
		World,
		RequestingPlayerState,
		GPBuildingDropAuthorityPrivate::ResolveLegacyDrop(BuildingType));
}

GPBuildingDropAuthority::FDeployResult GPBuildingDropAuthority::AuthorityDeployBuilding(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	const UGP_OrbitalDropDefinition* DropDefinition,
	const FTransform& WorldTransform)
{
	FDeployResult Result;

	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpawnFailed;
		return Result;
	}

	if (!AGP_GameState::AreEconomicOrdersAllowedInWorld(World))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MatchFinished;
		return Result;
	}

	if (!IsValid(RequestingPlayerState))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingPlayerState;
		return Result;
	}

	if (!IsValid(DropDefinition))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::InvalidDefinition;
		return Result;
	}

	Result.DropDefinitionId = DropDefinition->GetPrimaryAssetId();

	UGP_OrbitalBuildingInventoryComponent* Inventory =
		GPBuildingDropAuthorityPrivate::GetInventory(RequestingPlayerState);
	if (Inventory == nullptr)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingPlayerState;
		return Result;
	}

	if (Inventory->GetReadyCount(DropDefinition) <= 0)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::NoReadyInventory;
		return Result;
	}

	EGP_BuildingDropRejectReason PlacementReject = EGP_BuildingDropRejectReason::None;
	if (!ValidateInterimPlacement(World, RequestingPlayerState, DropDefinition, WorldTransform, PlacementReject))
	{
		Result.RejectReason = PlacementReject;
		return Result;
	}

	Result.PayloadClass = UGP_BuildingDropCatalog::Get().ResolvePayloadClass(DropDefinition);
	if (Result.PayloadClass == nullptr)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::MissingSpawnedClass;
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

	if (!Inventory->AuthorityTryConsumeReady(DropDefinition, 1))
	{
		Pod->Destroy();
		Result.RejectReason = EGP_BuildingDropRejectReason::NoReadyInventory;
		return Result;
	}

	Pod->AuthorityInitBuildingDrop(
		RequestingPlayerState,
		TeamId,
		Result.DropDefinitionId,
		Result.PayloadClass,
		LandingLoc,
		LandingRot,
		Settings->BuildingDropDescentDurationSeconds,
		Settings->BuildingDropSpawnAltitudeCm,
		Settings->BuildingDropPayloadDeployDelaySeconds,
		Settings->BuildingDropCleanupDelaySeconds);

	Result.bAccepted = true;
	Result.ReadyAfter = Inventory->GetReadyCount(DropDefinition);
	Result.SpawnedPod = Pod;
	return Result;
}

GPBuildingDropAuthority::FDeployResult GPBuildingDropAuthority::AuthorityDeployBuilding(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	EGP_OrbitalBuildingType BuildingType,
	const FTransform& WorldTransform)
{
	return AuthorityDeployBuilding(
		World,
		RequestingPlayerState,
		GPBuildingDropAuthorityPrivate::ResolveLegacyDrop(BuildingType),
		WorldTransform);
}
