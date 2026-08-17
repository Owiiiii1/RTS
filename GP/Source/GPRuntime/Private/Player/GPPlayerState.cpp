// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GPPlayerState.h"
#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPLogisticsHub.h"
#include "Effects/GPGE_UnitCap_Base5.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Game/GPGameMode.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameplayEffectTypes.h"
#include "Net/UnrealNetwork.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Units/GPSalvageWalker.h"
#include "Units/GPUnitBase.h"
#include "Units/GPWorker.h"

AGP_PlayerState::AGP_PlayerState()
{
	PrimaryActorTick.bCanEverTick = false;

	AbilitySystemComponent = CreateDefaultSubobject<UGP_AbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetProjectReplicationMode(EGameplayEffectReplicationMode::Mixed);

	PlayerAttributeSet = CreateDefaultSubobject<UGP_PlayerAttributeSet>(TEXT("PlayerAttributeSet"));
	OrbitalBuildingInventoryComponent =
		CreateDefaultSubobject<UGP_OrbitalBuildingInventoryComponent>(TEXT("OrbitalBuildingInventoryComponent"));
}

void AGP_PlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(AGP_PlayerState, TeamId, COND_None, REPNOTIFY_OnChanged);
}

UAbilitySystemComponent* AGP_PlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UGP_AbilitySystemComponent* AGP_PlayerState::GetGPAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

const UGP_PlayerAttributeSet* AGP_PlayerState::GetPlayerAttributeSet() const
{
	return PlayerAttributeSet;
}

UGP_OrbitalBuildingInventoryComponent* AGP_PlayerState::GetOrbitalBuildingInventoryComponent() const
{
	return OrbitalBuildingInventoryComponent;
}

int32 AGP_PlayerState::GetTeamId() const
{
	return TeamId;
}

void AGP_PlayerState::SetTeamId(int32 NewTeamId)
{
	if (!HasAuthority())
	{
		return;
	}

	if (TeamId == NewTeamId)
	{
		return;
	}

	const int32 OldTeamId = TeamId;
	TeamId = NewTeamId;
	if (TeamId >= 1)
	{
		ApplyBaseUnitCapIfNeeded();
		AuthorityCatchUpExistingUnits();
	}
	BroadcastTeamIdChanged(OldTeamId, TeamId);
}

void AGP_PlayerState::OnRep_TeamId(int32 OldTeamId)
{
	BroadcastTeamIdChanged(OldTeamId, TeamId);
}

void AGP_PlayerState::BroadcastTeamIdChanged(int32 OldTeamId, int32 NewTeamId)
{
	OnTeamIdChanged.Broadcast(OldTeamId, NewTeamId);
}

void AGP_PlayerState::BeginPlay()
{
	Super::BeginPlay();
	InitializeAbilitySystemActorInfo();
	ApplyBaseUnitCapIfNeeded();
	if (HasAuthority() && TeamId >= 1)
	{
		AuthorityCatchUpExistingUnits();
	}
}

void AGP_PlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFerroniteScoreMatchNotify();
	Super::EndPlay(EndPlayReason);
}

void AGP_PlayerState::ClientInitialize(AController* C)
{
	Super::ClientInitialize(C);
	InitializeAbilitySystemActorInfo();
}

void AGP_PlayerState::InitializeAbilitySystemActorInfo()
{
	if (AbilitySystemComponent == nullptr)
	{
		ensureMsgf(false, TEXT("AGP_PlayerState::InitializeAbilitySystemActorInfo: AbilitySystemComponent is null."));
		UE_LOG(LogTemp, Error, TEXT("AGP_PlayerState::InitializeAbilitySystemActorInfo: AbilitySystemComponent is null on %s."),
			*GetNameSafe(this));
		return;
	}

	const AActor* CurrentOwner = AbilitySystemComponent->GetOwnerActor();
	const AActor* CurrentAvatar = AbilitySystemComponent->GetAvatarActor();
	if (CurrentOwner == this && CurrentAvatar == this)
	{
		ApplyBaseUnitCapIfNeeded();
		BindFerroniteScoreMatchNotify();
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	ApplyBaseUnitCapIfNeeded();
	BindFerroniteScoreMatchNotify();

	UE_LOG(LogTemp, Verbose,
		TEXT("AGP_PlayerState::InitializeAbilitySystemActorInfo: InitAbilityActorInfo(this, this) on %s."),
		*GetName());
}

void AGP_PlayerState::ApplyBaseUnitCapIfNeeded()
{
	if (!HasAuthority() || bBaseUnitCapApplied || AbilitySystemComponent == nullptr)
	{
		return;
	}

	if (AbilitySystemComponent->GetOwnerActor() != this || AbilitySystemComponent->GetAvatarActor() != this)
	{
		return;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec =
		AbilitySystemComponent->MakeOutgoingSpec(UGP_GE_UnitCap_Base5::StaticClass(), 1.0f, Context);
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("GP UnitCap Base5 apply failed: invalid spec on %s"),
			*GetName());
		return;
	}

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	bBaseUnitCapApplied = true;

	const float MaxUnits = PlayerAttributeSet != nullptr ? PlayerAttributeSet->GetMaxUnits() : 0.0f;
	UE_LOG(LogTemp, Log,
		TEXT("GP UnitCap Base5 applied: PS=%s MaxUnits=%.0f"),
		*GetName(),
		MaxUnits);
}

int32 AGP_PlayerState::GetCommittedUnitCount() const
{
	const int32 Current = PlayerAttributeSet != nullptr
		? FMath::Max(0, FMath::RoundToInt(PlayerAttributeSet->GetCurrentUnits()))
		: 0;
	return Current + FMath::Max(0, PendingOrbitalUnitCount);
}

bool AGP_PlayerState::CanAcceptManifestUnitCount(int32 ManifestUnitCount) const
{
	if (ManifestUnitCount <= 0)
	{
		return false;
	}

	const float MaxUnits = PlayerAttributeSet != nullptr ? PlayerAttributeSet->GetMaxUnits() : 0.0f;
	const int32 MaxCount = FMath::Max(0, FMath::RoundToInt(MaxUnits));
	return GetCommittedUnitCount() + ManifestUnitCount <= MaxCount;
}

bool AGP_PlayerState::TryReserveOrbitalUnits(int32 Count)
{
	if (!HasAuthority() || Count <= 0)
	{
		return false;
	}

	if (!CanAcceptManifestUnitCount(Count))
	{
		return false;
	}

	PendingOrbitalUnitCount += Count;
	UE_LOG(LogTemp, Log,
		TEXT("GP UnitCap Reserve: PS=%s Delta=%d Pending=%d Committed=%d Max=%.0f"),
		*GetName(),
		Count,
		PendingOrbitalUnitCount,
		GetCommittedUnitCount(),
		PlayerAttributeSet != nullptr ? PlayerAttributeSet->GetMaxUnits() : 0.0f);
	return true;
}

void AGP_PlayerState::ReleaseOrbitalUnitReservation(int32 Count)
{
	if (!HasAuthority() || Count <= 0)
	{
		return;
	}

	PendingOrbitalUnitCount = FMath::Max(0, PendingOrbitalUnitCount - Count);
	UE_LOG(LogTemp, Log,
		TEXT("GP UnitCap ReleaseReservation: PS=%s Delta=%d Pending=%d"),
		*GetName(),
		Count,
		PendingOrbitalUnitCount);
}

void AGP_PlayerState::AuthorityAdjustCurrentUnits(int32 Delta)
{
	if (!HasAuthority() || PlayerAttributeSet == nullptr || Delta == 0)
	{
		return;
	}

	const int32 Current = FMath::Max(0, FMath::RoundToInt(PlayerAttributeSet->GetCurrentUnits()));
	const int32 Next = FMath::Max(0, Current + Delta);
	PlayerAttributeSet->SetCurrentUnits(static_cast<float>(Next));
}

void AGP_PlayerState::NotifyPlayerUnitBecameLive(AGP_UnitBase* Unit)
{
	if (!HasAuthority() || !IsValid(Unit) || !Unit->CountsTowardPlayerUnitCap() || Unit->IsDead())
	{
		return;
	}

	if (Unit->HasBeenCountedTowardPlayerUnitCap())
	{
		return;
	}

	Unit->MarkCountedTowardPlayerUnitCap(this);
	AuthorityAdjustCurrentUnits(1);
	if (PendingOrbitalUnitCount > 0)
	{
		--PendingOrbitalUnitCount;
	}

	UE_LOG(LogTemp, Log,
		TEXT("GP UnitCap Register: PS=%s Unit=%s Current=%.0f Pending=%d Max=%.0f"),
		*GetName(),
		*GetNameSafe(Unit),
		PlayerAttributeSet != nullptr ? PlayerAttributeSet->GetCurrentUnits() : 0.0f,
		PendingOrbitalUnitCount,
		PlayerAttributeSet != nullptr ? PlayerAttributeSet->GetMaxUnits() : 0.0f);
}

void AGP_PlayerState::NotifyPlayerUnitDied(AGP_UnitBase* Unit)
{
	if (!HasAuthority() || Unit == nullptr || !Unit->HasBeenCountedTowardPlayerUnitCap())
	{
		return;
	}

	Unit->ClearCountedTowardPlayerUnitCap();
	AuthorityAdjustCurrentUnits(-1);

	UE_LOG(LogTemp, Log,
		TEXT("GP UnitCap Unregister: PS=%s Unit=%s Current=%.0f Pending=%d"),
		*GetName(),
		*GetNameSafe(Unit),
		PlayerAttributeSet != nullptr ? PlayerAttributeSet->GetCurrentUnits() : 0.0f,
		PendingOrbitalUnitCount);
}

AGP_PlayerState* AGP_PlayerState::FindAuthoritativeForTeam(const UWorld* World, int32 InTeamId)
{
	if (World == nullptr || InTeamId < 1)
	{
		return nullptr;
	}

	const AGameStateBase* GameState = World->GetGameState();
	if (GameState == nullptr)
	{
		return nullptr;
	}

	AGP_PlayerState* Found = nullptr;
	int32 Count = 0;
	for (APlayerState* Candidate : GameState->PlayerArray)
	{
		AGP_PlayerState* GPPS = Cast<AGP_PlayerState>(Candidate);
		if (!IsValid(GPPS) || GPPS->GetTeamId() != InTeamId)
		{
			continue;
		}
		++Count;
		if (Found == nullptr)
		{
			Found = GPPS;
		}
	}

	if (Count > 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("GP UnitCap FindAuthoritativeForTeam: multiple PlayerStates TeamId=%d Count=%d using %s"),
			InTeamId,
			Count,
			*GetNameSafe(Found));
	}

	return Found;
}

void AGP_PlayerState::BindFerroniteScoreMatchNotify()
{
	if (!HasAuthority() || AbilitySystemComponent == nullptr || FerroniteScoreMatchHandle.IsValid())
	{
		return;
	}

	FerroniteScoreMatchHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		UGP_PlayerAttributeSet::GetFerroniteScoreAttribute()).AddUObject(
		this, &AGP_PlayerState::HandleFerroniteScoreChangedForMatch);
}

void AGP_PlayerState::UnbindFerroniteScoreMatchNotify()
{
	if (AbilitySystemComponent != nullptr && FerroniteScoreMatchHandle.IsValid())
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UGP_PlayerAttributeSet::GetFerroniteScoreAttribute()).Remove(FerroniteScoreMatchHandle);
	}
	FerroniteScoreMatchHandle.Reset();
}

void AGP_PlayerState::HandleFerroniteScoreChangedForMatch(const FOnAttributeChangeData& Data)
{
	(void)Data;
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown)
	{
		return;
	}

	if (AGP_GameMode* GameMode = World->GetAuthGameMode<AGP_GameMode>())
	{
		GameMode->NotifyFerroniteScoreChanged(this);
	}
}

void AGP_PlayerState::AuthorityCatchUpExistingUnits()
{
	if (!HasAuthority() || TeamId < 1)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AGP_Worker> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetTeamId() == TeamId)
		{
			It->TryRegisterPlayerUnitCap();
		}
	}
	for (TActorIterator<AGP_SalvageWalker> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetTeamId() == TeamId)
		{
			It->TryRegisterPlayerUnitCap();
		}
	}

	for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetTeamId() == TeamId)
		{
			It->TryApplyUnitCapBonus();
		}
	}
}
