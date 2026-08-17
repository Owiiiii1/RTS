// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildingDropAuthority.h"

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPBuildingBase.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPMainBase.h"
#include "Buildings/Grid/GPBuildGridSubsystem.h"
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

	static EGP_BuildingDropRejectReason MapGridReason(EGP_GridRejectReason GridReason)
	{
		switch (GridReason)
		{
		case EGP_GridRejectReason::InvalidFootprint:
			return EGP_BuildingDropRejectReason::InvalidFootprint;
		case EGP_GridRejectReason::CellOccupied:
			return EGP_BuildingDropRejectReason::GridOccupied;
		case EGP_GridRejectReason::NotNavigable:
			return EGP_BuildingDropRejectReason::NotNavigable;
		case EGP_GridRejectReason::WorldBlocked:
			return EGP_BuildingDropRejectReason::PlacementOverlap;
		default:
			return EGP_BuildingDropRejectReason::SpawnFailed;
		}
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

bool GPBuildingDropAuthority::ValidateBuildingPlacement(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	const UGP_OrbitalDropDefinition* DropDefinition,
	const FTransform& WorldTransform,
	EGP_BuildingDropRejectReason& OutReject,
	FIntPoint* OutOriginCell,
	FIntPoint* OutFootprintSize,
	FVector* OutSnappedGroundLocation)
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

	const UGP_BuildingDefinition* BuildingDef = DropDefinition->ResolveLoadedBuildingDefinition();
	if (BuildingDef == nullptr)
	{
		OutReject = EGP_BuildingDropRejectReason::MissingBuildingDefinition;
		return false;
	}

	const FIntPoint Footprint = BuildingDef->FootprintCells;
	if (Footprint.X <= 0 || Footprint.Y <= 0)
	{
		OutReject = EGP_BuildingDropRejectReason::InvalidFootprint;
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

	UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>();
	if (Grid == nullptr)
	{
		OutReject = EGP_BuildingDropRejectReason::SpawnFailed;
		return false;
	}

	FIntPoint OriginCell = FIntPoint::ZeroValue;
	FVector SnappedGround = FVector::ZeroVector;
	if (!Grid->ResolveSnappedPlacement(Loc, Footprint, OriginCell, SnappedGround))
	{
		OutReject = EGP_BuildingDropRejectReason::InvalidTransform;
		return false;
	}

	if (OutOriginCell != nullptr)
	{
		*OutOriginCell = OriginCell;
	}
	if (OutFootprintSize != nullptr)
	{
		*OutFootprintSize = Footprint;
	}
	if (OutSnappedGroundLocation != nullptr)
	{
		*OutSnappedGroundLocation = SnappedGround;
	}

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	const float MaxRadius = Settings != nullptr
		? FMath::Max(100.0f, Settings->BuildingMaxDeployRadiusFromMainBaseCm)
		: 5000.0f;
	const float Dist2D = FVector::Dist2D(SnappedGround, MainBase->GetActorLocation());
	if (Dist2D > MaxRadius + KINDA_SMALL_NUMBER)
	{
		OutReject = EGP_BuildingDropRejectReason::OutOfDeployRadius;
		return false;
	}

	EGP_GridRejectReason GridReason = EGP_GridRejectReason::Free;
	if (!Grid->CanPlaceFootprint(OriginCell, Footprint, GridReason))
	{
		OutReject = GPBuildingDropAuthorityPrivate::MapGridReason(GridReason);
		return false;
	}

	if (!Grid->IsFootprintNavigable(OriginCell, Footprint, SnappedGround.Z))
	{
		OutReject = EGP_BuildingDropRejectReason::NotNavigable;
		return false;
	}

	if (Grid->IsFootprintEnvironmentBlocked(OriginCell, Footprint, SnappedGround.Z))
	{
		OutReject = EGP_BuildingDropRejectReason::PlacementOverlap;
		return false;
	}

	return true;
}

bool GPBuildingDropAuthority::ValidateBuildingPlacement(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	EGP_OrbitalBuildingType BuildingType,
	const FTransform& WorldTransform,
	EGP_BuildingDropRejectReason& OutReject)
{
	return ValidateBuildingPlacement(
		World,
		RequestingPlayerState,
		GPBuildingDropAuthorityPrivate::ResolveLegacyDrop(BuildingType),
		WorldTransform,
		OutReject);
}

namespace GPBuildingDropAuthorityPrivate
{
	static bool DoesReplicatedOccupantOverlapFootprint(
		UWorld* World,
		UGP_BuildGridSubsystem* Grid,
		FIntPoint OriginCell,
		FIntPoint FootprintSize)
	{
		if (World == nullptr || Grid == nullptr)
		{
			return false;
		}

		for (TActorIterator<AGP_BuildingBase> It(World); It; ++It)
		{
			const AGP_BuildingBase* Building = *It;
			if (!IsValid(Building))
			{
				continue;
			}
			const FIntPoint OtherSize = Building->GetGridFootprintSize();
			if (OtherSize.X <= 0 || OtherSize.Y <= 0)
			{
				continue;
			}
			if (Grid->DoFootprintsOverlap(OriginCell, FootprintSize, Building->GetGridOriginCell(), OtherSize))
			{
				return true;
			}
		}

		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			const AGP_DropPod* Pod = *It;
			if (!IsValid(Pod) || Pod->GetPayloadKind() != EGP_DropPodPayloadKind::Building)
			{
				continue;
			}
			const FIntPoint OtherSize = Pod->GetBuildingGridFootprintSize();
			if (OtherSize.X <= 0 || OtherSize.Y <= 0)
			{
				continue;
			}
			if (Grid->DoFootprintsOverlap(OriginCell, FootprintSize, Pod->GetBuildingGridOriginCell(), OtherSize))
			{
				return true;
			}
		}

		return false;
	}
}

bool GPBuildingDropAuthority::EvaluateLocalPlacementPreview(
	UWorld* World,
	AGP_PlayerState* RequestingPlayerState,
	const UGP_OrbitalDropDefinition* DropDefinition,
	const FTransform& WorldTransform,
	FPlacementPreview& OutPreview)
{
	OutPreview = FPlacementPreview();
	EGP_BuildingDropRejectReason Reject = EGP_BuildingDropRejectReason::None;
	if (!ValidateBuildingPlacement(
			World,
			RequestingPlayerState,
			DropDefinition,
			WorldTransform,
			Reject,
			&OutPreview.OriginCell,
			&OutPreview.FootprintSize,
			&OutPreview.SnappedGround))
	{
		OutPreview.bValid = false;
		OutPreview.RejectReason = Reject;
		return false;
	}

	if (UGP_BuildGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UGP_BuildGridSubsystem>() : nullptr)
	{
		if (GPBuildingDropAuthorityPrivate::DoesReplicatedOccupantOverlapFootprint(
				World, Grid, OutPreview.OriginCell, OutPreview.FootprintSize))
		{
			OutPreview.bValid = false;
			OutPreview.RejectReason = EGP_BuildingDropRejectReason::GridOccupied;
			return false;
		}
	}

	OutPreview.bValid = true;
	OutPreview.RejectReason = EGP_BuildingDropRejectReason::None;
	return true;
}

const TCHAR* GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(
	bool bValid,
	EGP_BuildingDropRejectReason RejectReason)
{
	if (bValid)
	{
		return TEXT("VALID");
	}

	switch (RejectReason)
	{
	case EGP_BuildingDropRejectReason::GridOccupied:
		return TEXT("BLOCKED: OCCUPIED");
	case EGP_BuildingDropRejectReason::OutOfDeployRadius:
		return TEXT("BLOCKED: OUT OF RANGE");
	case EGP_BuildingDropRejectReason::NotNavigable:
		return TEXT("BLOCKED: NOT NAVIGABLE");
	case EGP_BuildingDropRejectReason::PlacementOverlap:
		return TEXT("BLOCKED: WORLD");
	default:
		return TEXT("BLOCKED");
	}
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

	FIntPoint OriginCell = FIntPoint::ZeroValue;
	FIntPoint FootprintSize = FIntPoint::ZeroValue;
	FVector SnappedGround = FVector::ZeroVector;
	EGP_BuildingDropRejectReason PlacementReject = EGP_BuildingDropRejectReason::None;
	if (!ValidateBuildingPlacement(
			World,
			RequestingPlayerState,
			DropDefinition,
			WorldTransform,
			PlacementReject,
			&OriginCell,
			&FootprintSize,
			&SnappedGround))
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

	UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>();
	if (Grid == nullptr)
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::SpawnFailed;
		return Result;
	}

	const FGuid ReservationId = FGuid::NewGuid();
	if (!Grid->TryReserveFootprint(ReservationId, OriginCell, FootprintSize))
	{
		Result.RejectReason = EGP_BuildingDropRejectReason::GridOccupied;
		return Result;
	}

	const UGP_OrbitalDeliverySettings* Settings = UGP_OrbitalDeliverySettings::Get();
	if (Settings == nullptr)
	{
		Grid->ReleaseReservation(ReservationId);
		Result.RejectReason = EGP_BuildingDropRejectReason::SpawnFailed;
		return Result;
	}

	const int32 TeamId = RequestingPlayerState->GetTeamId();
	const FVector LandingLoc = SnappedGround;
	const FRotator LandingRot = FRotator::ZeroRotator;
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
		Grid->ReleaseReservation(ReservationId);
		Result.RejectReason = EGP_BuildingDropRejectReason::SpawnFailed;
		return Result;
	}

	Grid->BindReservationOwner(ReservationId, Pod);

	if (!Inventory->AuthorityTryConsumeReady(DropDefinition, 1))
	{
		Grid->ReleaseReservation(ReservationId);
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
		Settings->BuildingDropCleanupDelaySeconds,
		OriginCell,
		FootprintSize,
		ReservationId);

	Result.bAccepted = true;
	Result.ReadyAfter = Inventory->GetReadyCount(DropDefinition);
	Result.SpawnedPod = Pod;
	Result.OriginCell = OriginCell;
	Result.FootprintSize = FootprintSize;
	Result.SnappedLocation = LandingLoc;
	Result.ReservationId = ReservationId;
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
