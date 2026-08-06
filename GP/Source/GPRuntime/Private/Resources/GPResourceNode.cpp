// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPResourceNode.h"

#include "Components/BoxComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "Net/UnrealNetwork.h"
#include "Resources/GPResourceDefinition.h"
#include "Settings/GPResourceGameplaySettings.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "Visual/GPResourceNodeVisualComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#endif

DEFINE_LOG_CATEGORY(LogGPResourceNode);

namespace GPResourceNodePrivate
{
	/** Documented collision policy for S27A1 ore piles. */
	static const FName CollisionProfileName(TEXT("BlockAll"));

	static const TCHAR* NetModeToString(ENetMode NetMode)
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

	static const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None:
			return TEXT("None");
		case ROLE_SimulatedProxy:
			return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy:
			return TEXT("AutonomousProxy");
		case ROLE_Authority:
			return TEXT("Authority");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* MiningSlotResultToString(EGP_MiningSlotRequestResult Result)
	{
		switch (Result)
		{
		case EGP_MiningSlotRequestResult::Granted:
			return TEXT("Granted");
		case EGP_MiningSlotRequestResult::Waiting:
			return TEXT("Waiting");
		case EGP_MiningSlotRequestResult::AlreadyActive:
			return TEXT("AlreadyActive");
		case EGP_MiningSlotRequestResult::AlreadyWaiting:
			return TEXT("AlreadyWaiting");
		case EGP_MiningSlotRequestResult::RejectedInvalidMiner:
			return TEXT("RejectedInvalidMiner");
		case EGP_MiningSlotRequestResult::RejectedNoAuthority:
			return TEXT("RejectedNoAuthority");
		case EGP_MiningSlotRequestResult::RejectedDepositInvalid:
			return TEXT("RejectedDepositInvalid");
		default:
			return TEXT("Unknown");
		}
	}
}

AGP_ResourceNode::AGP_ResourceNode()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	// Pile-sized gameplay volume (~120×120×80 uu), not a single crystal.
	CollisionBox->InitBoxExtent(FVector(60.0f, 60.0f, 40.0f));
	CollisionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	CollisionBox->SetCollisionProfileName(GPResourceNodePrivate::CollisionProfileName);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetGenerateOverlapEvents(false);
	CollisionBox->SetCanEverAffectNavigation(true);
	CollisionBox->SetSimulatePhysics(false);

	ResourceNodeVisualComponent = CreateDefaultSubobject<UGP_ResourceNodeVisualComponent>(TEXT("ResourceNodeVisualComponent"));

	ResourceDefinition = TSoftObjectPtr<UGP_ResourceDefinition>(
		FSoftObjectPath(UGP_ResourceDefinition::DefaultFerroniteAssetPath()));
	ResourceType = EGP_ResourceType::Ore;
	MaxAmount = 5000;
	CurrentAmount = 5000;
	MaxConcurrentMiners = 4;
	ActiveMinerCount = 0;
	WaitingMinerCount = 0;
}

void AGP_ResourceNode::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		NormalizeAmountsOnConstruction();
		// Prefer already-loaded AlwaysCook primary asset; no silent sync load here.
		if (UGP_ResourceDefinition* Definition = ResolveResourceDefinition(false))
		{
			ApplyIdentityFromDefinition(Definition);
		}
		if (!bHasDepleted && !IsDepleted())
		{
			RegisterWithGameState();
		}
	}
}

void AGP_ResourceNode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DepletionDestroyTimerHandle);
	}
	bDestroyPending = false;

	UnregisterFromGameState();

	if (HasAuthority() && !bIsClearingOccupancy)
	{
		ClearOccupancyWithoutPromotion();
		OnMinerSlotStateChanged.Clear();
	}

	CachedResourceDefinition.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGP_ResourceNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGP_ResourceNode, ResourceType);
	DOREPLIFETIME(AGP_ResourceNode, MaxAmount);
	DOREPLIFETIME(AGP_ResourceNode, CurrentAmount);
	DOREPLIFETIME(AGP_ResourceNode, bHasDepleted);
	DOREPLIFETIME(AGP_ResourceNode, ActiveMinerCount);
	DOREPLIFETIME(AGP_ResourceNode, WaitingMinerCount);
}

void AGP_ResourceNode::NormalizeAmountsOnConstruction()
{
	if (MaxAmount < 0)
	{
		MaxAmount = 0;
	}
	ClampCurrentAmountToMax();
}

void AGP_ResourceNode::ClampCurrentAmountToMax()
{
	CurrentAmount = FMath::Clamp(CurrentAmount, 0, MaxAmount);
}

void AGP_ResourceNode::ApplyIdentityFromDefinition(const UGP_ResourceDefinition* Definition)
{
	if (Definition == nullptr)
	{
		return;
	}

	if (Definition->ResourceType != EGP_ResourceType::None)
	{
		ResourceType = Definition->ResourceType;
	}
}

EGP_ResourceType AGP_ResourceNode::GetResourceType() const
{
	if (const UGP_ResourceDefinition* Definition = GetResolvedResourceDefinition())
	{
		if (Definition->ResourceType != EGP_ResourceType::None)
		{
			return Definition->ResourceType;
		}
	}
	return ResourceType;
}

int32 AGP_ResourceNode::GetMaxAmount() const
{
	return MaxAmount;
}

int32 AGP_ResourceNode::GetCurrentAmount() const
{
	return CurrentAmount;
}

bool AGP_ResourceNode::IsDepleted() const
{
	return bHasDepleted || CurrentAmount <= 0;
}

USceneComponent* AGP_ResourceNode::GetPresentationRoot() const
{
	return CollisionBox;
}

float AGP_ResourceNode::GetRemainingNormalized() const
{
	if (MaxAmount <= 0)
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>(CurrentAmount) / static_cast<float>(MaxAmount), 0.0f, 1.0f);
}

void AGP_ResourceNode::SetUseGeneratedPrototypeVisual(bool bUse)
{
	if (bUseGeneratedPrototypeVisual == bUse)
	{
		return;
	}

	bUseGeneratedPrototypeVisual = bUse;
	if (IsValid(ResourceNodeVisualComponent))
	{
		ResourceNodeVisualComponent->RefreshVisualMode();
	}
}

#if WITH_EDITOR
void AGP_ResourceNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetPropertyName()
		== GET_MEMBER_NAME_CHECKED(AGP_ResourceNode, bUseGeneratedPrototypeVisual))
	{
		if (IsValid(ResourceNodeVisualComponent))
		{
			ResourceNodeVisualComponent->RefreshVisualMode();
		}
	}
}
#endif

TSoftObjectPtr<UGP_ResourceDefinition> AGP_ResourceNode::GetResourceDefinitionSoft() const
{
	return ResourceDefinition;
}

void AGP_ResourceNode::SetResourceDefinitionSoft(TSoftObjectPtr<UGP_ResourceDefinition> InDefinition)
{
	ResourceDefinition = MoveTemp(InDefinition);
	CachedResourceDefinition.Reset();
}

UGP_ResourceDefinition* AGP_ResourceNode::GetResolvedResourceDefinition() const
{
	return ResolveResourceDefinition(false);
}

UGP_ResourceDefinition* AGP_ResourceNode::ResolveResourceDefinition(bool bAllowSynchronousLoad) const
{
	if (CachedResourceDefinition.IsValid())
	{
		return CachedResourceDefinition.Get();
	}

	if (ResourceDefinition.IsNull())
	{
		return nullptr;
	}

	if (UGP_ResourceDefinition* AlreadyLoaded = ResourceDefinition.Get())
	{
		CachedResourceDefinition = AlreadyLoaded;
		return AlreadyLoaded;
	}

	if (UAssetManager::IsInitialized())
	{
		const FSoftObjectPath SoftPath = ResourceDefinition.ToSoftObjectPath();
		const FPrimaryAssetId PrimaryAssetId(
			FPrimaryAssetType(UGP_ResourceDefinition::PrimaryAssetTypeName()),
			FName(*SoftPath.GetAssetName()));
		if (UObject* PrimaryObject = UAssetManager::Get().GetPrimaryAssetObject(PrimaryAssetId))
		{
			if (UGP_ResourceDefinition* AsDefinition = Cast<UGP_ResourceDefinition>(PrimaryObject))
			{
				CachedResourceDefinition = AsDefinition;
				return AsDefinition;
			}
		}
	}

	if (bAllowSynchronousLoad)
	{
		// Explicit AlwaysCook primary-asset resolve for Mine validation / diagnostics only.
		UE_LOG(LogGPResourceNode, Verbose,
			TEXT("GP ResourceNode.ResolveResourceDefinition sync load (AlwaysCook primary): Actor=%s Path=%s"),
			*GetName(),
			*ResourceDefinition.ToSoftObjectPath().ToString());
		if (UGP_ResourceDefinition* Loaded = ResourceDefinition.LoadSynchronous())
		{
			CachedResourceDefinition = Loaded;
			return Loaded;
		}
	}

	return nullptr;
}

void AGP_ResourceNode::GetResourceCapabilityTags(FGameplayTagContainer& OutTags) const
{
	OutTags.Reset();
	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	OutTags.AddTag(GPTags.Resource_Node);

	if (const UGP_ResourceDefinition* Definition = GetResolvedResourceDefinition())
	{
		if (Definition->ResourceGameplayTag.IsValid())
		{
			OutTags.AddTag(Definition->ResourceGameplayTag);
			return;
		}
	}

	// Ferronite deposit contract default when definition is not yet resolved.
	OutTags.AddTag(GPTags.Resource_Type_Ferronite);
}

bool AGP_ResourceNode::HasResourceCapabilityTag(FGameplayTag CapabilityTag) const
{
	if (!CapabilityTag.IsValid())
	{
		return false;
	}

	FGameplayTagContainer CapabilityTags;
	GetResourceCapabilityTags(CapabilityTags);
	return CapabilityTags.HasTagExact(CapabilityTag);
}

bool AGP_ResourceNode::IsDepositStateValidForMining() const
{
	if (!IsValid(this) || IsActorBeingDestroyed() || bIsClearingOccupancy || bHasDepleted || bDestroyPending)
	{
		return false;
	}

	if (MaxAmount <= 0 || CurrentAmount <= 0)
	{
		return false;
	}

	if (ResourceDefinition.IsNull())
	{
		return false;
	}

	return true;
}

bool AGP_ResourceNode::CanAcceptMineCommand(bool bAllowSynchronousDefinitionLoad, FString* OutFailReason) const
{
	auto Fail = [OutFailReason](const TCHAR* Reason) -> bool
	{
		if (OutFailReason != nullptr)
		{
			*OutFailReason = Reason;
		}
		return false;
	};

	if (!IsValid(this) || IsActorBeingDestroyed())
	{
		return Fail(TEXT("InvalidOrPendingKill"));
	}

	const UWorld* World = GetWorld();
	if (World == nullptr || World->bIsTearingDown)
	{
		return Fail(TEXT("InvalidWorld"));
	}

	if (bHasDepleted || bDestroyPending)
	{
		return Fail(TEXT("Depleted"));
	}

	if (MaxAmount <= 0)
	{
		return Fail(TEXT("MaxAmountInvalid"));
	}

	if (CurrentAmount <= 0)
	{
		return Fail(TEXT("Depleted"));
	}

	if (ResourceDefinition.IsNull())
	{
		return Fail(TEXT("ResourceDefinitionUnset"));
	}

	const UGP_ResourceDefinition* Definition = ResolveResourceDefinition(bAllowSynchronousDefinitionLoad);
	if (Definition == nullptr)
	{
		return Fail(TEXT("ResourceDefinitionUnresolved"));
	}

	if (Definition->ResourceType == EGP_ResourceType::None)
	{
		return Fail(TEXT("ResourceTypeNone"));
	}

	if (!Definition->ResourceGameplayTag.IsValid())
	{
		return Fail(TEXT("ResourceGameplayTagInvalid"));
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (!HasResourceCapabilityTag(GPTags.Resource_Node))
	{
		return Fail(TEXT("MissingResourceNodeTag"));
	}

	if (!HasResourceCapabilityTag(Definition->ResourceGameplayTag)
		&& !HasResourceCapabilityTag(GPTags.Resource_Type_Ferronite))
	{
		return Fail(TEXT("MissingResourceTypeTag"));
	}

	if (OutFailReason != nullptr)
	{
		OutFailReason->Reset();
	}
	return true;
}

int32 AGP_ResourceNode::ConsumeResource(int32 RequestedAmount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogGPResourceNode, Warning,
			TEXT("GP ResourceNode.ConsumeResource rejected (no authority): Actor=%s Requested=%d"),
			*GetName(),
			RequestedAmount);
		return 0;
	}

	if (RequestedAmount <= 0 || bHasDepleted || bDestroyPending)
	{
		return 0;
	}

	NormalizeAmountsOnConstruction();
	const int32 PreviousAmount = CurrentAmount;
	const int32 Consumed = FMath::Min(RequestedAmount, CurrentAmount);
	if (Consumed <= 0)
	{
		return 0;
	}

	CurrentAmount -= Consumed;
	ClampCurrentAmountToMax();

	if (PreviousAmount > 0 && CurrentAmount <= 0 && !bHasDepleted)
	{
		HandleDepletionTransition(PreviousAmount);
	}

	return Consumed;
}

void AGP_ResourceNode::ClearOccupancyWithoutPromotion()
{
	if (bIsClearingOccupancy)
	{
		return;
	}

	const TArray<TWeakObjectPtr<AActor>> ActiveSnapshot = ActiveMiners;
	const TArray<TWeakObjectPtr<AActor>> WaitingSnapshot = WaitingMiners;

	bIsClearingOccupancy = true;
	ActiveMiners.Reset();
	WaitingMiners.Reset();
	ActiveMinerCount = 0;
	WaitingMinerCount = 0;

	for (const TWeakObjectPtr<AActor>& Ptr : ActiveSnapshot)
	{
		if (AActor* Miner = Ptr.Get())
		{
			BroadcastMinerSlotStateChanged(Miner, EGP_MinerOccupancyState::Active, EGP_MinerOccupancyState::None);
		}
	}
	for (const TWeakObjectPtr<AActor>& Ptr : WaitingSnapshot)
	{
		if (AActor* Miner = Ptr.Get())
		{
			BroadcastMinerSlotStateChanged(Miner, EGP_MinerOccupancyState::Waiting, EGP_MinerOccupancyState::None);
		}
	}
}

void AGP_ResourceNode::DisableGameplayInteraction()
{
	if (IsValid(CollisionBox))
	{
		CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionBox->SetCanEverAffectNavigation(false);
		CollisionBox->SetGenerateOverlapEvents(false);
	}

	if (IsValid(ResourceNodeVisualComponent))
	{
		// Clear generated parts only — authored BP meshes remain until actor Destroy.
		ResourceNodeVisualComponent->ClearVisual();
	}
}

void AGP_ResourceNode::BroadcastDepletionPresentation(int32 PreviousAmount)
{
	if (bDepletionPresentationBroadcast)
	{
		return;
	}
	bDepletionPresentationBroadcast = true;
	DepletionPreviousAmountCached = PreviousAmount;
	OnResourceDepleted.Broadcast(this, PreviousAmount);
}

void AGP_ResourceNode::HandleDepletionTransition(int32 PreviousAmount)
{
	if (!HasAuthority() || bHasDepleted)
	{
		return;
	}

	bHasDepleted = true;
	DepletionPreviousAmountCached = PreviousAmount;

	UnregisterFromGameState();
	ClearOccupancyWithoutPromotion();
	DisableGameplayInteraction();
	BroadcastDepletionPresentation(PreviousAmount);
	ScheduleDeferredDestroy();

	UE_LOG(LogGPResourceNode, Log,
		TEXT("GP ResourceNode.DepletionTransition: Actor=%s PreviousAmount=%d DestroyDelay=%.3f"),
		*GetName(),
		PreviousAmount,
		GetDepletionDestroyDelaySeconds());
}

float AGP_ResourceNode::GetDepletionDestroyDelaySeconds() const
{
	if (const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get())
	{
		return Settings->DepletionDestroyDelaySeconds;
	}
	return 0.25f;
}

void AGP_ResourceNode::ScheduleDeferredDestroy()
{
	if (!HasAuthority() || bDestroyPending)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	bDestroyPending = true;
	World->GetTimerManager().ClearTimer(DepletionDestroyTimerHandle);

	const float Delay = FMath::Max(0.0f, GetDepletionDestroyDelaySeconds());
	if (Delay <= KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &AGP_ResourceNode::ExecuteDeferredDestroy));
	}
	else
	{
		World->GetTimerManager().SetTimer(
			DepletionDestroyTimerHandle,
			this,
			&AGP_ResourceNode::ExecuteDeferredDestroy,
			Delay,
			false);
	}
}

void AGP_ResourceNode::ExecuteDeferredDestroy()
{
	if (!HasAuthority() || !IsValid(this) || IsActorBeingDestroyed())
	{
		return;
	}

	Destroy();
}

void AGP_ResourceNode::RegisterWithGameState()
{
	if (!HasAuthority() || bRegisteredWithGameState || bHasDepleted || bDestroyPending)
	{
		return;
	}

	UWorld* World = GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	if (GS == nullptr)
	{
		return;
	}

	const AGP_GameState::EGP_ResourceNodeRegisterResult Result = GS->RegisterResourceNode(this);
	bRegisteredWithGameState =
		Result == AGP_GameState::EGP_ResourceNodeRegisterResult::Registered
		|| Result == AGP_GameState::EGP_ResourceNodeRegisterResult::AlreadyRegistered;
}

void AGP_ResourceNode::UnregisterFromGameState()
{
	if (!bRegisteredWithGameState && !HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	AGP_GameState* GS = World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	if (GS != nullptr)
	{
		GS->UnregisterResourceNode(this);
	}
	bRegisteredWithGameState = false;
}

void AGP_ResourceNode::OnRep_bHasDepleted()
{
	if (!bHasDepleted)
	{
		return;
	}

	DisableGameplayInteraction();
	const int32 PreviousAmount =
		DepletionPreviousAmountCached > 0 ? DepletionPreviousAmountCached : FMath::Max(1, MaxAmount);
	BroadcastDepletionPresentation(PreviousAmount);
}

#if !UE_BUILD_SHIPPING
void AGP_ResourceNode::DebugSetCurrentAmountForTest(int32 NewAmount, bool bAllowDepletionTransition)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 PreviousAmount = CurrentAmount;
	CurrentAmount = FMath::Max(0, NewAmount);
	ClampCurrentAmountToMax();
	if (bAllowDepletionTransition && PreviousAmount > 0 && CurrentAmount <= 0 && !bHasDepleted)
	{
		HandleDepletionTransition(PreviousAmount);
	}
}
#endif

int32 AGP_ResourceNode::GetMaxConcurrentMiners() const
{
	return MaxConcurrentMiners;
}

int32 AGP_ResourceNode::GetActiveMinerCount() const
{
	// Authority queries use live occupancy arrays (not stale replicated counts).
	if (HasAuthority())
	{
		int32 Count = 0;
		for (const TWeakObjectPtr<AActor>& Ptr : ActiveMiners)
		{
			const AActor* Miner = Ptr.Get();
			if (IsValid(Miner) && !Miner->IsActorBeingDestroyed())
			{
				++Count;
			}
		}
		return Count;
	}
	return ActiveMinerCount;
}

int32 AGP_ResourceNode::GetWaitingMinerCount() const
{
	if (HasAuthority())
	{
		int32 Count = 0;
		for (const TWeakObjectPtr<AActor>& Ptr : WaitingMiners)
		{
			const AActor* Miner = Ptr.Get();
			if (IsValid(Miner) && !Miner->IsActorBeingDestroyed())
			{
				++Count;
			}
		}
		return Count;
	}
	return WaitingMinerCount;
}

bool AGP_ResourceNode::IsValidMinerActor(const AActor* Miner) const
{
	return IsValid(Miner) && !Miner->IsActorBeingDestroyed() && Miner->GetWorld() == GetWorld();
}

void AGP_ResourceNode::BroadcastMinerSlotStateChanged(
	AActor* Miner,
	EGP_MinerOccupancyState OldState,
	EGP_MinerOccupancyState NewState)
{
	if (!IsValid(Miner) || Miner->IsActorBeingDestroyed() || OldState == NewState)
	{
		return;
	}

	OnMinerSlotStateChanged.Broadcast(Miner, OldState, NewState);
}

void AGP_ResourceNode::CleanupInvalidMiners()
{
	if (bIsClearingOccupancy)
	{
		return;
	}

	// Silent removal only — do not broadcast into pending-kill / destroyed miners.
	for (int32 Index = ActiveMiners.Num() - 1; Index >= 0; --Index)
	{
		AActor* Miner = ActiveMiners[Index].Get();
		if (!IsValid(Miner) || Miner->IsActorBeingDestroyed())
		{
			ActiveMiners.RemoveAt(Index);
		}
	}

	for (int32 Index = WaitingMiners.Num() - 1; Index >= 0; --Index)
	{
		AActor* Miner = WaitingMiners[Index].Get();
		if (!IsValid(Miner) || Miner->IsActorBeingDestroyed())
		{
			WaitingMiners.RemoveAt(Index);
		}
	}
}

void AGP_ResourceNode::RefreshOccupancyCounts()
{
	if (bIsClearingOccupancy)
	{
		ActiveMinerCount = 0;
		WaitingMinerCount = 0;
		return;
	}

	ActiveMinerCount = ActiveMiners.Num();
	WaitingMinerCount = WaitingMiners.Num();
}

void AGP_ResourceNode::PromoteWaitingMiners(TArray<AActor*>& OutPromotedMiners)
{
	OutPromotedMiners.Reset();
	if (bIsClearingOccupancy)
	{
		return;
	}

	CleanupInvalidMiners();

	const int32 Cap = FMath::Max(0, MaxConcurrentMiners);
	while (ActiveMiners.Num() < Cap && WaitingMiners.Num() > 0)
	{
		TWeakObjectPtr<AActor> Next = WaitingMiners[0];
		WaitingMiners.RemoveAt(0);
		AActor* Miner = Next.Get();
		if (!IsValid(Miner) || Miner->IsActorBeingDestroyed())
		{
			continue;
		}

		ActiveMiners.Add(Miner);
		OutPromotedMiners.Add(Miner);
		BroadcastMinerSlotStateChanged(Miner, EGP_MinerOccupancyState::Waiting, EGP_MinerOccupancyState::Active);
	}
}

EGP_MiningSlotRequestResult AGP_ResourceNode::RequestMiningSlot(AActor* Miner)
{
	if (!HasAuthority())
	{
		UE_LOG(LogGPResourceNode, Warning,
			TEXT("GP ResourceNode.RequestMiningSlot rejected (no authority): Deposit=%s Miner=%s"),
			*GetName(),
			*GetNameSafe(Miner));
		return EGP_MiningSlotRequestResult::RejectedNoAuthority;
	}

	if (bIsClearingOccupancy)
	{
		return EGP_MiningSlotRequestResult::RejectedDepositInvalid;
	}

	if (!IsValidMinerActor(Miner))
	{
		return EGP_MiningSlotRequestResult::RejectedInvalidMiner;
	}

	if (!IsDepositStateValidForMining() || MaxConcurrentMiners <= 0)
	{
		return EGP_MiningSlotRequestResult::RejectedDepositInvalid;
	}

	CleanupInvalidMiners();

	if (HasActiveMiningSlot(Miner))
	{
		RefreshOccupancyCounts();
		return EGP_MiningSlotRequestResult::AlreadyActive;
	}

	if (IsWaitingForMiningSlot(Miner))
	{
		RefreshOccupancyCounts();
		return EGP_MiningSlotRequestResult::AlreadyWaiting;
	}

	if (ActiveMiners.Num() < MaxConcurrentMiners)
	{
		ActiveMiners.Add(Miner);
		RefreshOccupancyCounts();
		BroadcastMinerSlotStateChanged(Miner, EGP_MinerOccupancyState::None, EGP_MinerOccupancyState::Active);
		return EGP_MiningSlotRequestResult::Granted;
	}

	WaitingMiners.Add(Miner);
	RefreshOccupancyCounts();
	BroadcastMinerSlotStateChanged(Miner, EGP_MinerOccupancyState::None, EGP_MinerOccupancyState::Waiting);
	return EGP_MiningSlotRequestResult::Waiting;
}

void AGP_ResourceNode::ReleaseMiningSlot(AActor* Miner)
{
	if (!HasAuthority())
	{
		UE_LOG(LogGPResourceNode, Warning,
			TEXT("GP ResourceNode.ReleaseMiningSlot rejected (no authority): Deposit=%s Miner=%s"),
			*GetName(),
			*GetNameSafe(Miner));
		return;
	}

	// EndPlay already cleared queues and emits snapshot terminal transitions.
	// Listener Release during teardown must be a harmless idempotent no-op.
	if (bIsClearingOccupancy)
	{
		return;
	}

	CleanupInvalidMiners();

	bool bWasActive = false;
	bool bWasWaiting = false;
	for (int32 Index = ActiveMiners.Num() - 1; Index >= 0; --Index)
	{
		if (ActiveMiners[Index].Get() == Miner)
		{
			ActiveMiners.RemoveAt(Index);
			bWasActive = true;
		}
	}

	for (int32 Index = WaitingMiners.Num() - 1; Index >= 0; --Index)
	{
		if (WaitingMiners[Index].Get() == Miner)
		{
			WaitingMiners.RemoveAt(Index);
			bWasWaiting = true;
		}
	}

	if (bWasActive)
	{
		BroadcastMinerSlotStateChanged(Miner, EGP_MinerOccupancyState::Active, EGP_MinerOccupancyState::None);
		// Depletion / destroy-pending must never promote FIFO heads.
		if (!bHasDepleted && !bDestroyPending)
		{
			TArray<AActor*> Promoted;
			PromoteWaitingMiners(Promoted);
		}
	}
	else if (bWasWaiting)
	{
		BroadcastMinerSlotStateChanged(Miner, EGP_MinerOccupancyState::Waiting, EGP_MinerOccupancyState::None);
	}

	RefreshOccupancyCounts();
}

bool AGP_ResourceNode::HasActiveMiningSlot(AActor* Miner) const
{
	if (!IsValid(Miner))
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& Ptr : ActiveMiners)
	{
		if (Ptr.Get() == Miner)
		{
			return true;
		}
	}
	return false;
}

bool AGP_ResourceNode::IsWaitingForMiningSlot(AActor* Miner) const
{
	if (!IsValid(Miner))
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& Ptr : WaitingMiners)
	{
		if (Ptr.Get() == Miner)
		{
			return true;
		}
	}
	return false;
}

EGP_MinerOccupancyState AGP_ResourceNode::GetMinerOccupancyState(AActor* Miner) const
{
	if (HasActiveMiningSlot(Miner))
	{
		return EGP_MinerOccupancyState::Active;
	}
	if (IsWaitingForMiningSlot(Miner))
	{
		return EGP_MinerOccupancyState::Waiting;
	}
	return EGP_MinerOccupancyState::None;
}

bool AGP_ResourceNode::ValidateDepositContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	if (ResourceDefinition.IsNull())
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceNode",
			"ErrDefinitionUnset",
			"ResourceDefinition soft reference must be assigned."));
	}
	else
	{
		const UGP_ResourceDefinition* Definition = ResolveResourceDefinition(true);
		if (Definition == nullptr)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT(
					"GPResourceNode",
					"ErrDefinitionUnresolved",
					"ResourceDefinition could not be resolved: {0}"),
				FText::FromString(ResourceDefinition.ToSoftObjectPath().ToString())));
		}
		else
		{
			if (Definition->ResourceType == EGP_ResourceType::None)
			{
				OutErrors.Add(NSLOCTEXT(
					"GPResourceNode",
					"ErrDefinitionTypeNone",
					"ResourceDefinition.ResourceType must not be None."));
			}
			else if (ResourceType != EGP_ResourceType::None && ResourceType != Definition->ResourceType)
			{
				OutWarnings.Add(FText::Format(
					NSLOCTEXT(
						"GPResourceNode",
						"WarnTypeMismatch",
						"ResourceNode.ResourceType ({0}) differs from ResourceDefinition ({1}); definition wins at runtime."),
					FText::FromString(GPResourceTypePrivate::ToString(ResourceType)),
					FText::FromString(GPResourceTypePrivate::ToString(Definition->ResourceType))));
			}

			if (!Definition->ResourceGameplayTag.IsValid())
			{
				OutErrors.Add(NSLOCTEXT(
					"GPResourceNode",
					"ErrDefinitionTagInvalid",
					"ResourceDefinition.ResourceGameplayTag must be valid."));
			}

			TArray<FText> DefErrors;
			TArray<FText> DefWarnings;
			Definition->ValidateDefinition(DefErrors, DefWarnings);
			OutErrors.Append(DefErrors);
			OutWarnings.Append(DefWarnings);
		}
	}

	if (MaxAmount <= 0)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceNode",
			"ErrMaxAmount",
			"MaxAmount must be > 0."));
	}

	if (CurrentAmount < 0 || CurrentAmount > MaxAmount)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceNode",
			"ErrCurrentAmount",
			"CurrentAmount must be in range [0, MaxAmount]."));
	}

	if (MaxConcurrentMiners <= 0)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPResourceNode",
			"ErrMaxConcurrent",
			"MaxConcurrentMiners must be > 0."));
	}

	return OutErrors.Num() == 0;
}

#if WITH_EDITOR
EDataValidationResult AGP_ResourceNode::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const bool bOk = ValidateDepositContract(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}

	if (!bOk)
	{
		return EDataValidationResult::Invalid;
	}
	return EDataValidationResult::Valid;
}
#endif

UGP_ResourceNodeVisualComponent* AGP_ResourceNode::GetResourceNodeVisualComponent() const
{
	return ResourceNodeVisualComponent;
}

UBoxComponent* AGP_ResourceNode::GetCollisionBox() const
{
	return CollisionBox;
}

void AGP_ResourceNode::OnRep_CurrentAmount()
{
	ClampCurrentAmountToMax();
	UE_LOG(LogGPResourceNode, Verbose,
		TEXT("GP ResourceNode.OnRep_CurrentAmount: Actor=%s Current=%d Max=%d"),
		*GetName(),
		CurrentAmount,
		MaxAmount);
}

#if !UE_BUILD_SHIPPING
namespace GPResourceNodeDebug
{
	static AGP_ResourceNode* FindNode(UWorld* World, const FString& OptionalName)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		AGP_ResourceNode* Best = nullptr;
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			AGP_ResourceNode* Node = *It;
			if (!IsValid(Node))
			{
				continue;
			}

			if (!OptionalName.IsEmpty())
			{
				if (Node->GetName().Equals(OptionalName, ESearchCase::IgnoreCase)
					|| Node->GetPathName().Contains(OptionalName))
				{
					return Node;
				}
				continue;
			}

			if (Best == nullptr || Node->GetName() < Best->GetName())
			{
				Best = Node;
			}
		}

		return Best;
	}

	static AActor* FindActorByName(UWorld* World, const FString& Name)
	{
		if (World == nullptr || Name.IsEmpty())
		{
			return nullptr;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor))
			{
				continue;
			}
			if (Actor->GetName().Equals(Name, ESearchCase::IgnoreCase)
				|| Actor->GetPathName().Contains(Name))
			{
				return Actor;
			}
		}
		return nullptr;
	}

	static void ResourceNodeInspect(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Inspect: missing world"));
			return;
		}

		const FString OptionalName = Args.Num() > 0 ? Args[0] : FString();
		AGP_ResourceNode* Node = FindNode(World, OptionalName);
		if (Node == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Inspect: no AGP_ResourceNode found"));
			return;
		}

		const UBoxComponent* Box = Node->GetCollisionBox();
		const UGP_ResourceNodeVisualComponent* Visual = Node->GetResourceNodeVisualComponent();
		TArray<FName> PartNames;
		if (Visual != nullptr)
		{
			Visual->GetPartNames(PartNames);
		}

		FString PartNamesJoined = TEXT("none");
		if (PartNames.Num() > 0)
		{
			PartNamesJoined.Reset();
			for (int32 Index = 0; Index < PartNames.Num(); ++Index)
			{
				if (Index > 0)
				{
					PartNamesJoined += TEXT(",");
				}
				PartNamesJoined += PartNames[Index].ToString();
			}
		}

		const TSoftObjectPtr<UGP_ResourceDefinition> SoftDef = Node->GetResourceDefinitionSoft();
		UGP_ResourceDefinition* Resolved = Node->ResolveResourceDefinition(true);
		FString PrimaryAssetIdStr = TEXT("none");
		FString ResourceTagStr = TEXT("none");
		if (Resolved != nullptr)
		{
			PrimaryAssetIdStr = Resolved->GetPrimaryAssetId().ToString();
			ResourceTagStr = Resolved->ResourceGameplayTag.ToString();
		}

		FGameplayTagContainer CapabilityTags;
		Node->GetResourceCapabilityTags(CapabilityTags);

		TArray<FText> Errors;
		TArray<FText> Warnings;
		const bool bValid = Node->ValidateDepositContract(Errors, Warnings);
		FString MineFail;
		const bool bCanMine = Node->CanAcceptMineCommand(true, &MineFail);

		UE_LOG(LogGPResourceNode, Log,
			TEXT("GP ResourceNode.Inspect: Actor=%s Path=%s SoftDefinition=%s ResolvedPrimaryAssetId=%s ResourceType=%s ResourceTag=%s CapabilityTags=%s MaxAmount=%d CurrentAmount=%d Depleted=%s MaxConcurrentMiners=%d ActiveMinerCount=%d WaitingMinerCount=%d ValidationOk=%s ValidationErrors=%d ValidationWarnings=%d CanAcceptMine=%s MineFail=%s Role=%s NetMode=%s Replicates=%s AlwaysRelevant=%s CollisionComponent=%s CollisionEnabled=%s CollisionProfile=%s AffectsNavigation=%s VisualComponent=%s VisualBuilt=%s Parts=%d PartNames=[%s] DedicatedVisualSuppressed=%s TickEnabled=%s VisualCollisionDisabled=%s VisualSourceMode=%s GeneratedPartCount=%d AuthoredPrimitiveComponentCount=%d NativeVisualBuilt=%s UsesAuthoredComponents=%s GeneratedCollisionDisabled=%s AuthoredCollisionWarnings=%d AuthoredNavigationWarnings=%d DuplicateGeneratedParts=%d"),
			*Node->GetName(),
			*Node->GetPathName(),
			*SoftDef.ToSoftObjectPath().ToString(),
			*PrimaryAssetIdStr,
			GPResourceTypePrivate::ToString(Node->GetResourceType()),
			*ResourceTagStr,
			*CapabilityTags.ToStringSimple(),
			Node->GetMaxAmount(),
			Node->GetCurrentAmount(),
			Node->IsDepleted() ? TEXT("true") : TEXT("false"),
			Node->GetMaxConcurrentMiners(),
			Node->GetActiveMinerCount(),
			Node->GetWaitingMinerCount(),
			bValid ? TEXT("true") : TEXT("false"),
			Errors.Num(),
			Warnings.Num(),
			bCanMine ? TEXT("true") : TEXT("false"),
			MineFail.IsEmpty() ? TEXT("none") : *MineFail,
			GPResourceNodePrivate::RoleToString(Node->GetLocalRole()),
			GPResourceNodePrivate::NetModeToString(World->GetNetMode()),
			Node->GetIsReplicated() ? TEXT("true") : TEXT("false"),
			Node->bAlwaysRelevant ? TEXT("true") : TEXT("false"),
			Box != nullptr ? *Box->GetName() : TEXT("missing"),
			Box != nullptr
				? (Box->GetCollisionEnabled() == ECollisionEnabled::NoCollision
					? TEXT("NoCollision")
					: (Box->GetCollisionEnabled() == ECollisionEnabled::QueryOnly
						? TEXT("QueryOnly")
						: (Box->GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly
							? TEXT("PhysicsOnly")
							: TEXT("QueryAndPhysics"))))
				: TEXT("n/a"),
			Box != nullptr ? *Box->GetCollisionProfileName().ToString() : TEXT("n/a"),
			(Box != nullptr && Box->CanEverAffectNavigation()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr ? TEXT("present") : TEXT("missing"),
			(Visual != nullptr && Visual->HasBuiltVisual()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr ? Visual->GetPartCount() : 0,
			*PartNamesJoined,
			(Visual != nullptr && Visual->IsDedicatedVisualSuppressed()) ? TEXT("true") : TEXT("false"),
			Node->IsActorTickEnabled() ? TEXT("true") : TEXT("false"),
			(Visual == nullptr || Visual->AreVisualPartCollisionsDisabled()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr
				? GPPrimitiveVisualDefaults::VisualSourceModeToString(Visual->GetVisualSourceMode())
				: TEXT("n/a"),
			Visual != nullptr ? Visual->GetGeneratedPartCount() : 0,
			Visual != nullptr ? Visual->GetAuthoredPrimitiveComponentCount() : 0,
			(Visual != nullptr && Visual->HasBuiltVisual()) ? TEXT("true") : TEXT("false"),
			(Visual != nullptr && Visual->UsesAuthoredComponents()) ? TEXT("true") : TEXT("false"),
			(Visual == nullptr || Visual->AreGeneratedCollisionsDisabled()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr ? Visual->GetAuthoredCollisionWarningCount() : 0,
			Visual != nullptr ? Visual->GetAuthoredNavigationWarningCount() : 0,
			Visual != nullptr ? Visual->GetDuplicateGeneratedPartCount() : 0);
	}

	static void ResourceNodeConsume(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Consume: missing world"));
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPResourceNode, Warning,
				TEXT("GP ResourceNode.Consume: rejected on client (authority required)"));
			return;
		}

		int32 Requested = 0;
		if (Args.Num() < 1 || !LexTryParseString(Requested, *Args[0]))
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Consume: usage gp.ResourceNode.Consume <Amount> [NodeName]"));
			return;
		}

		const FString OptionalName = Args.Num() > 1 ? Args[1] : FString();
		AGP_ResourceNode* Node = FindNode(World, OptionalName);
		if (Node == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Consume: no AGP_ResourceNode found"));
			return;
		}

		if (!Node->HasAuthority())
		{
			UE_LOG(LogGPResourceNode, Warning,
				TEXT("GP ResourceNode.Consume: rejected (node has no authority) Actor=%s"),
				*Node->GetName());
			return;
		}

		const int32 Before = Node->GetCurrentAmount();
		const int32 Consumed = Node->ConsumeResource(Requested);
		const int32 After = Node->GetCurrentAmount();

		UE_LOG(LogGPResourceNode, Log,
			TEXT("GP ResourceNode.Consume: Actor=%s Requested=%d Consumed=%d Before=%d After=%d Depleted=%s"),
			*Node->GetName(),
			Requested,
			Consumed,
			Before,
			After,
			Node->IsDepleted() ? TEXT("true") : TEXT("false"));
	}

	static void ResourceNodeInspectOccupancy(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.InspectOccupancy: missing world"));
			return;
		}

		const FString OptionalName = Args.Num() > 0 ? Args[0] : FString();
		AGP_ResourceNode* Node = FindNode(World, OptionalName);
		if (Node == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.InspectOccupancy: no AGP_ResourceNode found"));
			return;
		}

		UE_LOG(LogGPResourceNode, Log,
			TEXT("GP ResourceNode.InspectOccupancy: Actor=%s MaxConcurrentMiners=%d ActiveMinerCount=%d WaitingMinerCount=%d HasAuthority=%s"),
			*Node->GetName(),
			Node->GetMaxConcurrentMiners(),
			Node->GetActiveMinerCount(),
			Node->GetWaitingMinerCount(),
			Node->HasAuthority() ? TEXT("true") : TEXT("false"));
	}

	static void ResourceNodeRequestSlot(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.RequestSlot: missing world"));
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.RequestSlot: rejected on client"));
			return;
		}

		if (Args.Num() < 1)
		{
			UE_LOG(LogGPResourceNode, Warning,
				TEXT("GP ResourceNode.RequestSlot: usage gp.ResourceNode.RequestSlot <MinerActorName> [NodeName]"));
			return;
		}

		AActor* Miner = FindActorByName(World, Args[0]);
		if (Miner == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning,
				TEXT("GP ResourceNode.RequestSlot: miner not found Name=%s"),
				*Args[0]);
			return;
		}

		const FString OptionalNode = Args.Num() > 1 ? Args[1] : FString();
		AGP_ResourceNode* Node = FindNode(World, OptionalNode);
		if (Node == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.RequestSlot: no AGP_ResourceNode found"));
			return;
		}

		const EGP_MiningSlotRequestResult Result = Node->RequestMiningSlot(Miner);
		UE_LOG(LogGPResourceNode, Log,
			TEXT("GP ResourceNode.RequestSlot: Deposit=%s Miner=%s Result=%s Active=%d Waiting=%d"),
			*Node->GetName(),
			*Miner->GetName(),
			GPResourceNodePrivate::MiningSlotResultToString(Result),
			Node->GetActiveMinerCount(),
			Node->GetWaitingMinerCount());
	}

	static void ResourceNodeReleaseSlot(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.ReleaseSlot: missing world"));
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.ReleaseSlot: rejected on client"));
			return;
		}

		if (Args.Num() < 1)
		{
			UE_LOG(LogGPResourceNode, Warning,
				TEXT("GP ResourceNode.ReleaseSlot: usage gp.ResourceNode.ReleaseSlot <MinerActorName> [NodeName]"));
			return;
		}

		AActor* Miner = FindActorByName(World, Args[0]);
		if (Miner == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning,
				TEXT("GP ResourceNode.ReleaseSlot: miner not found Name=%s"),
				*Args[0]);
			return;
		}

		const FString OptionalNode = Args.Num() > 1 ? Args[1] : FString();
		AGP_ResourceNode* Node = FindNode(World, OptionalNode);
		if (Node == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.ReleaseSlot: no AGP_ResourceNode found"));
			return;
		}

		Node->ReleaseMiningSlot(Miner);
		UE_LOG(LogGPResourceNode, Log,
			TEXT("GP ResourceNode.ReleaseSlot: Deposit=%s Miner=%s Active=%d Waiting=%d"),
			*Node->GetName(),
			*Miner->GetName(),
			Node->GetActiveMinerCount(),
			Node->GetWaitingMinerCount());
	}

	static FAutoConsoleCommandWithWorldAndArgs GResourceNodeInspectCommand(
		TEXT("gp.ResourceNode.Inspect"),
		TEXT("Inspect AGP_ResourceNode (optional name). Non-shipping."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceNodeInspect));

	static FAutoConsoleCommandWithWorldAndArgs GResourceNodeConsumeCommand(
		TEXT("gp.ResourceNode.Consume"),
		TEXT("Authority-only ConsumeResource. Usage: gp.ResourceNode.Consume <Amount> [NodeName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceNodeConsume));

	static FAutoConsoleCommandWithWorldAndArgs GResourceNodeInspectOccupancyCommand(
		TEXT("gp.ResourceNode.InspectOccupancy"),
		TEXT("Inspect deposit miner occupancy counts. Usage: gp.ResourceNode.InspectOccupancy [NodeName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceNodeInspectOccupancy));

	static FAutoConsoleCommandWithWorldAndArgs GResourceNodeRequestSlotCommand(
		TEXT("gp.ResourceNode.RequestSlot"),
		TEXT("Authority debug: request mining slot. Usage: gp.ResourceNode.RequestSlot <MinerActorName> [NodeName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceNodeRequestSlot));

	static FAutoConsoleCommandWithWorldAndArgs GResourceNodeReleaseSlotCommand(
		TEXT("gp.ResourceNode.ReleaseSlot"),
		TEXT("Authority debug: release mining slot. Usage: gp.ResourceNode.ReleaseSlot <MinerActorName> [NodeName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceNodeReleaseSlot));
}
#endif
