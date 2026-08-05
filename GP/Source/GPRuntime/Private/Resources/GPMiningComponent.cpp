// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPMiningComponent.h"

#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPResourceDefinition.h"
#include "TimerManager.h"

#include <limits>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#endif

DEFINE_LOG_CATEGORY(LogGPMining);

namespace GPMiningPrivate
{
	static const TCHAR* NetModeToString(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone: return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer: return TEXT("ListenServer");
		case NM_Client: return TEXT("Client");
		default: return TEXT("Unknown");
		}
	}

	static const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None: return TEXT("None");
		case ROLE_SimulatedProxy: return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy: return TEXT("AutonomousProxy");
		case ROLE_Authority: return TEXT("Authority");
		default: return TEXT("Unknown");
		}
	}

	static const TCHAR* MiningStateToString(EGP_MiningState State)
	{
		switch (State)
		{
		case EGP_MiningState::Idle: return TEXT("Idle");
		case EGP_MiningState::WaitingForSlot: return TEXT("WaitingForSlot");
		case EGP_MiningState::Mining: return TEXT("Mining");
		case EGP_MiningState::CargoFull: return TEXT("CargoFull");
		case EGP_MiningState::DepositDepleted: return TEXT("DepositDepleted");
		case EGP_MiningState::OutOfRange: return TEXT("OutOfRange");
		case EGP_MiningState::Invalid: return TEXT("Invalid");
		default: return TEXT("Unknown");
		}
	}

	static const TCHAR* BeginResultToString(EGP_BeginMiningResult Result)
	{
		switch (Result)
		{
		case EGP_BeginMiningResult::Started: return TEXT("Started");
		case EGP_BeginMiningResult::WaitingForSlot: return TEXT("WaitingForSlot");
		case EGP_BeginMiningResult::AlreadyMiningTarget: return TEXT("AlreadyMiningTarget");
		case EGP_BeginMiningResult::RejectedNoAuthority: return TEXT("RejectedNoAuthority");
		case EGP_BeginMiningResult::RejectedInvalidOwner: return TEXT("RejectedInvalidOwner");
		case EGP_BeginMiningResult::RejectedMissingCargo: return TEXT("RejectedMissingCargo");
		case EGP_BeginMiningResult::RejectedInvalidNode: return TEXT("RejectedInvalidNode");
		case EGP_BeginMiningResult::RejectedDepleted: return TEXT("RejectedDepleted");
		case EGP_BeginMiningResult::RejectedCargoFull: return TEXT("RejectedCargoFull");
		case EGP_BeginMiningResult::RejectedOutOfRange: return TEXT("RejectedOutOfRange");
		case EGP_BeginMiningResult::RejectedResourceMismatch: return TEXT("RejectedResourceMismatch");
		default: return TEXT("Unknown");
		}
	}

	static const TCHAR* StopReasonToString(EGP_MiningStopReason Reason)
	{
		switch (Reason)
		{
		case EGP_MiningStopReason::None: return TEXT("None");
		case EGP_MiningStopReason::ManualStop: return TEXT("ManualStop");
		case EGP_MiningStopReason::CargoFull: return TEXT("CargoFull");
		case EGP_MiningStopReason::DepositDepleted: return TEXT("DepositDepleted");
		case EGP_MiningStopReason::OutOfRange: return TEXT("OutOfRange");
		case EGP_MiningStopReason::InvalidTarget: return TEXT("InvalidTarget");
		case EGP_MiningStopReason::MissingCargo: return TEXT("MissingCargo");
		case EGP_MiningStopReason::InvariantFailure: return TEXT("InvariantFailure");
		case EGP_MiningStopReason::OwnerEndPlay: return TEXT("OwnerEndPlay");
		case EGP_MiningStopReason::ComponentEndPlay: return TEXT("ComponentEndPlay");
		case EGP_MiningStopReason::TargetEndPlay: return TEXT("TargetEndPlay");
		case EGP_MiningStopReason::ResourceMismatch: return TEXT("ResourceMismatch");
		default: return TEXT("Unknown");
		}
	}

	static int32 WholeUnits(float Value)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.0f)
		{
			return 0;
		}
		return FMath::FloorToInt(Value + KINDA_SMALL_NUMBER);
	}
}

UGP_MiningComponent::UGP_MiningComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGP_MiningComponent::BeginPlay()
{
	Super::BeginPlay();
	CachedCargoComponent = FindOwnerCargoComponent();
}

void UGP_MiningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthorityOwner())
	{
		StopMining(EGP_MiningStopReason::ComponentEndPlay);
	}
	else
	{
		ClearMiningTimer();
		UnbindOccupancyEvents();
		ClearTargetReferences();
	}

	Super::EndPlay(EndPlayReason);
}

void UGP_MiningComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGP_MiningComponent, CurrentMiningState);
	DOREPLIFETIME(UGP_MiningComponent, CurrentResourceNode);
	DOREPLIFETIME(UGP_MiningComponent, LastStopReason);
}

bool UGP_MiningComponent::HasAuthorityOwner() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr && Owner->HasAuthority();
}

UGP_CargoComponent* UGP_MiningComponent::FindOwnerCargoComponent() const
{
	AActor* Owner = GetOwner();
	return Owner != nullptr ? Owner->FindComponentByClass<UGP_CargoComponent>() : nullptr;
}

UGP_CargoComponent* UGP_MiningComponent::GetCargoComponent() const
{
	if (CachedCargoComponent.IsValid())
	{
		return CachedCargoComponent.Get();
	}
	return FindOwnerCargoComponent();
}

bool UGP_MiningComponent::IsMining() const
{
	return CurrentMiningState == EGP_MiningState::Mining;
}

bool UGP_MiningComponent::IsWaitingForSlot() const
{
	return CurrentMiningState == EGP_MiningState::WaitingForSlot;
}

EGP_MiningState UGP_MiningComponent::GetMiningState() const
{
	return CurrentMiningState;
}

EGP_MiningStopReason UGP_MiningComponent::GetLastStopReason() const
{
	return LastStopReason;
}

AGP_ResourceNode* UGP_MiningComponent::GetCurrentResourceNode() const
{
	return CurrentResourceNode;
}

float UGP_MiningComponent::GetMiningCycleDuration() const
{
	return CachedMiningCycleDurationSeconds;
}

float UGP_MiningComponent::GetAmountPerMiningCycle() const
{
	return CachedAmountPerMiningCycle;
}

float UGP_MiningComponent::GetInteractionRangeCm() const
{
	return CachedInteractionRangeCm;
}

float UGP_MiningComponent::GetDistanceToCurrentNode() const
{
	const AActor* Owner = GetOwner();
	const AGP_ResourceNode* Node = CurrentResourceNode;
	if (!IsValid(Owner) || !IsValid(Node))
	{
		return std::numeric_limits<float>::max();
	}
	return FVector::Dist(Owner->GetActorLocation(), Node->GetActorLocation());
}

bool UGP_MiningComponent::IsInRangeOfCurrentNode() const
{
	if (!FMath::IsFinite(CachedInteractionRangeCm) || CachedInteractionRangeCm <= 0.0f)
	{
		return false;
	}
	return GetDistanceToCurrentNode() <= CachedInteractionRangeCm;
}

bool UGP_MiningComponent::IsMiningTimerActive() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetTimerManager().IsTimerActive(MiningCycleTimerHandle);
}

void UGP_MiningComponent::ClearMiningTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MiningCycleTimerHandle);
	}
}

void UGP_MiningComponent::StartMiningTimer()
{
	ClearMiningTimer();

	UWorld* World = GetWorld();
	if (World == nullptr || !HasAuthorityOwner())
	{
		return;
	}

	if (!FMath::IsFinite(CachedMiningCycleDurationSeconds) || CachedMiningCycleDurationSeconds <= 0.0f)
	{
		UE_LOG(LogGPMining, Error,
			TEXT("GP Mining.StartMiningTimer invalid duration Owner=%s Duration=%.3f"),
			*GetNameSafe(GetOwner()),
			CachedMiningCycleDurationSeconds);
		StopMining(EGP_MiningStopReason::InvalidTarget);
		return;
	}

	// First cycle after a full duration; looping thereafter.
	World->GetTimerManager().SetTimer(
		MiningCycleTimerHandle,
		this,
		&UGP_MiningComponent::ExecuteMiningCycle,
		CachedMiningCycleDurationSeconds,
		true);
}

void UGP_MiningComponent::ClearTargetReferences()
{
	CurrentResourceNode = nullptr;
	CachedAmountPerMiningCycle = 0.0f;
	CachedMiningCycleDurationSeconds = 0.0f;
	CachedInteractionRangeCm = 0.0f;
}

void UGP_MiningComponent::BindOccupancyEvents(AGP_ResourceNode* Node)
{
	UnbindOccupancyEvents();
	if (!IsValid(Node))
	{
		return;
	}

	BoundOccupancyNode = Node;
	OccupancyDelegateHandle = Node->GetOnMinerSlotStateChanged().AddUObject(
		this,
		&UGP_MiningComponent::HandleMinerSlotStateChanged);
}

void UGP_MiningComponent::UnbindOccupancyEvents()
{
	if (BoundOccupancyNode.IsValid() && OccupancyDelegateHandle.IsValid())
	{
		BoundOccupancyNode->GetOnMinerSlotStateChanged().Remove(OccupancyDelegateHandle);
	}
	OccupancyDelegateHandle.Reset();
	BoundOccupancyNode.Reset();
}

void UGP_MiningComponent::ReleaseSlotOnCurrentNode()
{
	if (!HasAuthorityOwner())
	{
		return;
	}

	AGP_ResourceNode* Node = CurrentResourceNode;
	AActor* Owner = GetOwner();
	if (IsValid(Node) && IsValid(Owner))
	{
		if (Node->HasActiveMiningSlot(Owner) || Node->IsWaitingForMiningSlot(Owner))
		{
			Node->ReleaseMiningSlot(Owner);
		}
	}
}

void UGP_MiningComponent::SetMiningState(EGP_MiningState NewState, EGP_MiningStopReason Reason)
{
	const EGP_MiningState Previous = CurrentMiningState;
	LastStopReason = Reason;

	if (Previous == NewState)
	{
		return;
	}

	CurrentMiningState = NewState;

	if (HasAuthorityOwner())
	{
		OnMiningStateChanged.Broadcast(Previous, NewState, Reason);
	}
}

void UGP_MiningComponent::OnRep_CurrentMiningState(EGP_MiningState PreviousState)
{
	// Clients only — authority already broadcast in SetMiningState.
	OnMiningStateChanged.Broadcast(PreviousState, CurrentMiningState, LastStopReason);
}

bool UGP_MiningComponent::AreResourceIdentitiesCompatible(
	const AGP_ResourceNode* Node,
	const UGP_CargoComponent* Cargo,
	bool bAllowSyncLoad) const
{
	if (!IsValid(Node) || !IsValid(Cargo))
	{
		return false;
	}

	const UGP_ResourceDefinition* NodeDef = Node->ResolveResourceDefinition(bAllowSyncLoad);
	const UGP_ResourceDefinition* CargoDef = Cargo->ResolveResourceDefinition(bAllowSyncLoad);
	if (NodeDef == nullptr || CargoDef == nullptr)
	{
		return false;
	}

	if (NodeDef->ResourceType == EGP_ResourceType::None || CargoDef->ResourceType == EGP_ResourceType::None)
	{
		return false;
	}

	if (NodeDef->ResourceType != CargoDef->ResourceType)
	{
		return false;
	}

	if (!NodeDef->ResourceGameplayTag.IsValid()
		|| !CargoDef->ResourceGameplayTag.IsValid()
		|| NodeDef->ResourceGameplayTag != CargoDef->ResourceGameplayTag)
	{
		return false;
	}

	return true;
}

bool UGP_MiningComponent::ResolveMiningTunables(bool bAllowSyncLoad)
{
	AGP_ResourceNode* Node = CurrentResourceNode;
	if (!IsValid(Node))
	{
		return false;
	}

	const UGP_ResourceDefinition* Definition = Node->ResolveResourceDefinition(bAllowSyncLoad);
	if (Definition == nullptr)
	{
		return false;
	}

	CachedAmountPerMiningCycle = Definition->AmountPerMiningCycle;
	CachedMiningCycleDurationSeconds = Definition->MiningCycleDurationSeconds;
	CachedInteractionRangeCm = Definition->InteractionRangeCm;

	return FMath::IsFinite(CachedAmountPerMiningCycle) && CachedAmountPerMiningCycle > 0.0f
		&& FMath::IsFinite(CachedMiningCycleDurationSeconds) && CachedMiningCycleDurationSeconds > 0.0f
		&& FMath::IsFinite(CachedInteractionRangeCm) && CachedInteractionRangeCm > 0.0f;
}

void UGP_MiningComponent::HandleMinerSlotStateChanged(
	AActor* Miner,
	EGP_MinerOccupancyState OldState,
	EGP_MinerOccupancyState NewState)
{
	if (!HasAuthorityOwner() || Miner != GetOwner())
	{
		return;
	}

	if (CurrentMiningState == EGP_MiningState::WaitingForSlot
		&& OldState == EGP_MinerOccupancyState::Waiting
		&& NewState == EGP_MinerOccupancyState::Active)
	{
		if (!IsValid(CurrentResourceNode) || CurrentResourceNode->IsDepleted())
		{
			StopMining(EGP_MiningStopReason::DepositDepleted);
			return;
		}

		UGP_CargoComponent* Cargo = GetCargoComponent();
		if (!IsValid(Cargo))
		{
			StopMining(EGP_MiningStopReason::MissingCargo);
			return;
		}

		if (Cargo->IsFull())
		{
			StopMining(EGP_MiningStopReason::CargoFull);
			return;
		}

		if (!IsInRangeOfCurrentNode())
		{
			StopMining(EGP_MiningStopReason::OutOfRange);
			return;
		}

		SetMiningState(EGP_MiningState::Mining, EGP_MiningStopReason::None);
		StartMiningTimer();
	}
	else if (NewState == EGP_MinerOccupancyState::None
		&& (CurrentMiningState == EGP_MiningState::Mining || CurrentMiningState == EGP_MiningState::WaitingForSlot))
	{
		// Target EndPlay / cleanup removed our slot.
		StopMining(EGP_MiningStopReason::TargetEndPlay);
	}
}

void UGP_MiningComponent::StopMining(EGP_MiningStopReason Reason)
{
	if (!HasAuthorityOwner())
	{
		UE_LOG(LogGPMining, Warning,
			TEXT("GP Mining.StopMining rejected (no authority): Owner=%s"),
			*GetNameSafe(GetOwner()));
		return;
	}

	const bool bBusy =
		CurrentMiningState == EGP_MiningState::Mining
		|| CurrentMiningState == EGP_MiningState::WaitingForSlot;

	ClearMiningTimer();
	if (bBusy)
	{
		ReleaseSlotOnCurrentNode();
	}
	UnbindOccupancyEvents();
	ClearTargetReferences();

	EGP_MiningState Terminal = EGP_MiningState::Idle;
	switch (Reason)
	{
	case EGP_MiningStopReason::CargoFull:
		Terminal = EGP_MiningState::CargoFull;
		break;
	case EGP_MiningStopReason::DepositDepleted:
		Terminal = EGP_MiningState::DepositDepleted;
		break;
	case EGP_MiningStopReason::OutOfRange:
		Terminal = EGP_MiningState::OutOfRange;
		break;
	case EGP_MiningStopReason::InvalidTarget:
	case EGP_MiningStopReason::InvariantFailure:
	case EGP_MiningStopReason::MissingCargo:
	case EGP_MiningStopReason::ResourceMismatch:
	case EGP_MiningStopReason::TargetEndPlay:
		Terminal = EGP_MiningState::Invalid;
		break;
	default:
		Terminal = EGP_MiningState::Idle;
		break;
	}

	if (!bBusy && CurrentMiningState == Terminal)
	{
		LastStopReason = Reason;
		return;
	}

	SetMiningState(Terminal, Reason);
}

EGP_BeginMiningResult UGP_MiningComponent::BeginMining(AGP_ResourceNode* ResourceNode)
{
	if (!HasAuthorityOwner())
	{
		return EGP_BeginMiningResult::RejectedNoAuthority;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed())
	{
		return EGP_BeginMiningResult::RejectedInvalidOwner;
	}

	UGP_CargoComponent* Cargo = FindOwnerCargoComponent();
	CachedCargoComponent = Cargo;
	if (!IsValid(Cargo))
	{
		return EGP_BeginMiningResult::RejectedMissingCargo;
	}

	if (!IsValid(ResourceNode) || ResourceNode->IsActorBeingDestroyed())
	{
		return EGP_BeginMiningResult::RejectedInvalidNode;
	}

	if (ResourceNode->GetWorld() != GetWorld())
	{
		return EGP_BeginMiningResult::RejectedInvalidNode;
	}

	if (CurrentResourceNode == ResourceNode
		&& (CurrentMiningState == EGP_MiningState::Mining || CurrentMiningState == EGP_MiningState::WaitingForSlot))
	{
		return EGP_BeginMiningResult::AlreadyMiningTarget;
	}

	if (CurrentMiningState == EGP_MiningState::Mining || CurrentMiningState == EGP_MiningState::WaitingForSlot)
	{
		StopMining(EGP_MiningStopReason::ManualStop);
	}

	if (ResourceNode->IsDepleted() || !ResourceNode->CanAcceptMineCommand(true))
	{
		return EGP_BeginMiningResult::RejectedDepleted;
	}

	if (Cargo->IsFull())
	{
		return EGP_BeginMiningResult::RejectedCargoFull;
	}

	if (!AreResourceIdentitiesCompatible(ResourceNode, Cargo, true))
	{
		return EGP_BeginMiningResult::RejectedResourceMismatch;
	}

	CurrentResourceNode = ResourceNode;
	if (!ResolveMiningTunables(true))
	{
		ClearTargetReferences();
		return EGP_BeginMiningResult::RejectedInvalidNode;
	}

	const float Distance = FVector::Dist(Owner->GetActorLocation(), ResourceNode->GetActorLocation());
	if (Distance > CachedInteractionRangeCm)
	{
		ClearTargetReferences();
		SetMiningState(EGP_MiningState::OutOfRange, EGP_MiningStopReason::OutOfRange);
		return EGP_BeginMiningResult::RejectedOutOfRange;
	}

	BindOccupancyEvents(ResourceNode);

	const EGP_MiningSlotRequestResult SlotResult = ResourceNode->RequestMiningSlot(Owner);
	switch (SlotResult)
	{
	case EGP_MiningSlotRequestResult::Granted:
	case EGP_MiningSlotRequestResult::AlreadyActive:
		SetMiningState(EGP_MiningState::Mining, EGP_MiningStopReason::None);
		StartMiningTimer();
		return EGP_BeginMiningResult::Started;

	case EGP_MiningSlotRequestResult::Waiting:
	case EGP_MiningSlotRequestResult::AlreadyWaiting:
		SetMiningState(EGP_MiningState::WaitingForSlot, EGP_MiningStopReason::None);
		return EGP_BeginMiningResult::WaitingForSlot;

	default:
		UnbindOccupancyEvents();
		ClearTargetReferences();
		SetMiningState(EGP_MiningState::Invalid, EGP_MiningStopReason::InvalidTarget);
		return EGP_BeginMiningResult::RejectedInvalidNode;
	}
}

void UGP_MiningComponent::ExecuteMiningCycle()
{
	if (!HasAuthorityOwner())
	{
		return;
	}

	if (CurrentMiningState != EGP_MiningState::Mining)
	{
		ClearMiningTimer();
		return;
	}

	AActor* Owner = GetOwner();
	AGP_ResourceNode* Node = CurrentResourceNode;
	UGP_CargoComponent* Cargo = GetCargoComponent();

	if (!IsValid(Owner) || !IsValid(Node) || Node->IsActorBeingDestroyed())
	{
		StopMining(EGP_MiningStopReason::InvalidTarget);
		return;
	}

	if (!IsValid(Cargo))
	{
		StopMining(EGP_MiningStopReason::MissingCargo);
		return;
	}

	if (!Node->HasActiveMiningSlot(Owner))
	{
		StopMining(EGP_MiningStopReason::InvalidTarget);
		return;
	}

	if (!ResolveMiningTunables(true))
	{
		StopMining(EGP_MiningStopReason::InvalidTarget);
		return;
	}

	if (!AreResourceIdentitiesCompatible(Node, Cargo, true))
	{
		StopMining(EGP_MiningStopReason::ResourceMismatch);
		return;
	}

	if (!IsInRangeOfCurrentNode())
	{
		StopMining(EGP_MiningStopReason::OutOfRange);
		return;
	}

	if (Cargo->IsFull())
	{
		StopMining(EGP_MiningStopReason::CargoFull);
		return;
	}

	if (Node->IsDepleted())
	{
		StopMining(EGP_MiningStopReason::DepositDepleted);
		return;
	}

	const int32 CycleAmount = GPMiningPrivate::WholeUnits(CachedAmountPerMiningCycle);
	const int32 RemainingCargo = GPMiningPrivate::WholeUnits(Cargo->GetRemainingCapacity());
	const int32 NodeAmount = Node->GetCurrentAmount();
	const int32 RequestedTransfer = FMath::Min3(CycleAmount, RemainingCargo, NodeAmount);

	if (RequestedTransfer <= 0)
	{
		if (RemainingCargo <= 0 || Cargo->IsFull())
		{
			StopMining(EGP_MiningStopReason::CargoFull);
		}
		else
		{
			StopMining(EGP_MiningStopReason::DepositDepleted);
		}
		return;
	}

	const int32 Consumed = Node->ConsumeResource(RequestedTransfer);
	if (Consumed <= 0)
	{
		StopMining(EGP_MiningStopReason::DepositDepleted);
		return;
	}

	const float Accepted = Cargo->AddCargo(static_cast<float>(Consumed));
	if (!FMath::IsNearlyEqual(Accepted, static_cast<float>(Consumed)))
	{
		UE_LOG(LogGPMining, Error,
			TEXT("GP Mining invariant failure: Owner=%s Node=%s Consumed=%d Accepted=%.3f — stopping"),
			*GetNameSafe(Owner),
			*GetNameSafe(Node),
			Consumed,
			Accepted);
		StopMining(EGP_MiningStopReason::InvariantFailure);
		return;
	}

	OnMiningCycleCompleted.Broadcast(
		Node,
		RequestedTransfer,
		Consumed,
		Accepted,
		Node->GetCurrentAmount(),
		Cargo->GetCurrentCargoAmount());

	UE_LOG(LogGPMining, Verbose,
		TEXT("GP Mining.Cycle: Owner=%s Node=%s Requested=%d Consumed=%d CargoAccepted=%.3f NodeAfter=%d CargoAfter=%.3f"),
		*GetNameSafe(Owner),
		*GetNameSafe(Node),
		RequestedTransfer,
		Consumed,
		Accepted,
		Node->GetCurrentAmount(),
		Cargo->GetCurrentCargoAmount());

	if (Cargo->IsFull())
	{
		StopMining(EGP_MiningStopReason::CargoFull);
		return;
	}

	if (Node->IsDepleted())
	{
		StopMining(EGP_MiningStopReason::DepositDepleted);
	}
}

#if !UE_BUILD_SHIPPING
void UGP_MiningComponent::DebugForceExecuteMiningCycle()
{
	ExecuteMiningCycle();
}
#endif

bool UGP_MiningComponent::ValidateMiningContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		OutErrors.Add(NSLOCTEXT("GPMining", "ErrOwner", "Owner must be valid."));
	}

	const UGP_CargoComponent* Cargo = GetCargoComponent();
	if (!IsValid(Cargo))
	{
		OutErrors.Add(NSLOCTEXT("GPMining", "ErrCargo", "Owner must have UGP_CargoComponent."));
	}

	if (CurrentResourceNode != nullptr)
	{
		if (!IsValid(CurrentResourceNode))
		{
			OutErrors.Add(NSLOCTEXT("GPMining", "ErrNodeInvalid", "CurrentResourceNode is set but invalid."));
		}
		else if (Cargo != nullptr && !AreResourceIdentitiesCompatible(CurrentResourceNode, Cargo, true))
		{
			OutErrors.Add(NSLOCTEXT("GPMining", "ErrMismatch", "Node/Cargo ResourceDefinition identity mismatch."));
		}
	}

	if (CurrentMiningState == EGP_MiningState::Mining || CurrentMiningState == EGP_MiningState::WaitingForSlot)
	{
		if (!IsValid(CurrentResourceNode))
		{
			OutErrors.Add(NSLOCTEXT("GPMining", "ErrActiveNoNode", "Active mining state requires CurrentResourceNode."));
		}
		if (CachedAmountPerMiningCycle <= 0.0f || CachedMiningCycleDurationSeconds <= 0.0f || CachedInteractionRangeCm <= 0.0f)
		{
			OutErrors.Add(NSLOCTEXT("GPMining", "ErrTunables", "Active mining requires positive resolved mining tunables."));
		}
	}

	const bool bTimerActive = IsMiningTimerActive();
	if (CurrentMiningState == EGP_MiningState::Mining)
	{
		if (!bTimerActive && HasAuthorityOwner())
		{
			OutWarnings.Add(NSLOCTEXT("GPMining", "WarnTimerMissing", "Mining state without active timer (may be between transitions)."));
		}
	}
	else if (bTimerActive)
	{
		OutErrors.Add(NSLOCTEXT("GPMining", "ErrTimerLeak", "Timer active outside Mining state."));
	}

	if (CurrentMiningState == EGP_MiningState::Mining && IsValid(CurrentResourceNode) && IsValid(Owner))
	{
		if (!CurrentResourceNode->HasActiveMiningSlot(const_cast<AActor*>(Owner)))
		{
			OutErrors.Add(NSLOCTEXT("GPMining", "ErrSlot", "Mining state requires active mining slot."));
		}
	}

	return OutErrors.Num() == 0;
}

#if WITH_EDITOR
EDataValidationResult UGP_MiningComponent::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const bool bOk = ValidateMiningContract(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}
	return bOk ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

AGP_MiningDiagnosticHost::AGP_MiningDiagnosticHost()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	SceneRoot->SetMobility(EComponentMobility::Movable);
	SceneRoot->SetCanEverAffectNavigation(false);
	SceneRoot->SetVisibility(false);

	CargoComponent = CreateDefaultSubobject<UGP_CargoComponent>(TEXT("CargoComponent"));
	MiningComponent = CreateDefaultSubobject<UGP_MiningComponent>(TEXT("MiningComponent"));
}

UGP_CargoComponent* AGP_MiningDiagnosticHost::GetCargoComponent() const
{
	return CargoComponent;
}

UGP_MiningComponent* AGP_MiningDiagnosticHost::GetMiningComponent() const
{
	return MiningComponent;
}

USceneComponent* AGP_MiningDiagnosticHost::GetSceneRoot() const
{
	return SceneRoot;
}

#if !UE_BUILD_SHIPPING
namespace GPMiningDebug
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

	static AGP_MiningDiagnosticHost* FindHost(UWorld* World, const FString& OptionalName)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		AGP_MiningDiagnosticHost* Best = nullptr;
		for (TActorIterator<AGP_MiningDiagnosticHost> It(World); It; ++It)
		{
			AGP_MiningDiagnosticHost* Host = *It;
			if (!IsValid(Host))
			{
				continue;
			}
			if (!OptionalName.IsEmpty())
			{
				if (Host->GetName().Equals(OptionalName, ESearchCase::IgnoreCase)
					|| Host->GetPathName().Contains(OptionalName))
				{
					return Host;
				}
				continue;
			}
			if (Best == nullptr || Host->GetName() < Best->GetName())
			{
				Best = Host;
			}
		}
		return Best;
	}

	static AGP_MiningDiagnosticHost* SpawnHostNearNode(UWorld* World, AGP_ResourceNode* Node, float RangeCm)
	{
		if (World == nullptr || !IsValid(Node) || !FMath::IsFinite(RangeCm) || RangeCm <= 0.0f)
		{
			return nullptr;
		}

		// Guaranteed inside InteractionRangeCm (half range, capped). No collision-driven teleport.
		const float OffsetDistance = FMath::Min(RangeCm * 0.5f, 100.0f);
		const FVector NodeLocation = Node->GetActorLocation();
		const FVector RequestedLocation = NodeLocation + FVector(OffsetDistance, 0.0f, 0.0f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;

		AGP_MiningDiagnosticHost* Host = World->SpawnActor<AGP_MiningDiagnosticHost>(
			AGP_MiningDiagnosticHost::StaticClass(),
			RequestedLocation,
			FRotator::ZeroRotator,
			Params);
		if (Host == nullptr)
		{
			UE_LOG(LogGPMining, Error, TEXT("GP Mining.SpawnHostNearNode: SpawnActor failed Node=%s"), *Node->GetName());
			return nullptr;
		}

		if (Host->GetSceneRoot() == nullptr || Host->GetRootComponent() != Host->GetSceneRoot())
		{
			UE_LOG(LogGPMining, Error,
				TEXT("GP Mining.SpawnHostNearNode: Host=%s missing SceneRoot — destroying"),
				*Host->GetName());
			Host->Destroy();
			return nullptr;
		}

		float Dist = FVector::Dist(Host->GetActorLocation(), NodeLocation);
		if (Dist > RangeCm || !Host->GetActorLocation().Equals(RequestedLocation, 1.0f))
		{
			Host->SetActorLocation(RequestedLocation, false, nullptr, ETeleportType::TeleportPhysics);
			Dist = FVector::Dist(Host->GetActorLocation(), NodeLocation);
		}

		const bool bWithinRange = Dist < RangeCm;
		const bool bLocationOk = Host->GetActorLocation().Equals(RequestedLocation, 1.0f);
		UE_LOG(LogGPMining, Log,
			TEXT("GP Mining.SpawnHostNearNode: Host=%s Node=%s Requested=(%.1f,%.1f,%.1f) Actual=(%.1f,%.1f,%.1f) Dist=%.1f Range=%.1f SpawnWithinRange=%s LocationMatchesRequested=%s HasSceneRoot=%s"),
			*Host->GetName(),
			*Node->GetName(),
			RequestedLocation.X, RequestedLocation.Y, RequestedLocation.Z,
			Host->GetActorLocation().X, Host->GetActorLocation().Y, Host->GetActorLocation().Z,
			Dist,
			RangeCm,
			bWithinRange ? TEXT("true") : TEXT("false"),
			bLocationOk ? TEXT("true") : TEXT("false"),
			Host->GetSceneRoot() != nullptr ? TEXT("true") : TEXT("false"));

		if (!bWithinRange || !bLocationOk)
		{
			UE_LOG(LogGPMining, Error,
				TEXT("GP Mining.SpawnHostNearNode: invariant failed — destroying Host=%s Dist=%.1f Range=%.1f"),
				*Host->GetName(),
				Dist,
				RangeCm);
			Host->Destroy();
			return nullptr;
		}

		return Host;
	}

	static void MiningSpawnHost(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.SpawnDiagnosticHost: missing world"));
			return;
		}
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.SpawnDiagnosticHost: rejected on client"));
			return;
		}

		const FString NodeName = Args.Num() > 0 ? Args[0] : FString();
		AGP_ResourceNode* Node = FindNode(World, NodeName);
		if (Node == nullptr)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.SpawnDiagnosticHost: no ResourceNode found"));
			return;
		}

		const UGP_ResourceDefinition* Def = Node->ResolveResourceDefinition(true);
		const float Range = Def != nullptr ? Def->InteractionRangeCm : 200.0f;
		AGP_MiningDiagnosticHost* Host = SpawnHostNearNode(World, Node, Range);
		const float Dist = (Host != nullptr)
			? FVector::Dist(Host->GetActorLocation(), Node->GetActorLocation())
			: -1.0f;
		UE_LOG(LogGPMining, Log,
			TEXT("GP Mining.SpawnDiagnosticHost: Host=%s Node=%s Dist=%.1f Range=%.1f SpawnWithinRange=%s HasSceneRoot=%s"),
			*GetNameSafe(Host),
			*Node->GetName(),
			Dist,
			Range,
			(Host != nullptr && Dist >= 0.0f && Dist < Range) ? TEXT("true") : TEXT("false"),
			(Host != nullptr && Host->GetSceneRoot() != nullptr) ? TEXT("true") : TEXT("false"));
	}

	static void MiningInspect(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.Inspect: missing world"));
			return;
		}

		const FString HostName = Args.Num() > 0 ? Args[0] : FString();
		AGP_MiningDiagnosticHost* Host = FindHost(World, HostName);
		if (Host == nullptr)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.Inspect: no AGP_MiningDiagnosticHost (gp.Mining.SpawnDiagnosticHost)"));
			return;
		}

		UGP_MiningComponent* Mining = Host->GetMiningComponent();
		UGP_CargoComponent* Cargo = Host->GetCargoComponent();
		AGP_ResourceNode* CurrentNode = Mining != nullptr ? Mining->GetCurrentResourceNode() : nullptr;
		AGP_ResourceNode* DiagnosticNode = CurrentNode;
		if (DiagnosticNode == nullptr)
		{
			DiagnosticNode = FindNode(World, FString());
		}

		UGP_ResourceDefinition* Resolved = DiagnosticNode != nullptr
			? DiagnosticNode->ResolveResourceDefinition(true)
			: nullptr;
		FString PrimaryId = TEXT("none");
		float AmountPerCycle = 0.0f;
		float CycleDuration = 0.0f;
		float InteractionRange = 0.0f;
		if (Resolved != nullptr)
		{
			PrimaryId = Resolved->GetPrimaryAssetId().ToString();
			AmountPerCycle = Resolved->AmountPerMiningCycle;
			CycleDuration = Resolved->MiningCycleDurationSeconds;
			InteractionRange = Resolved->InteractionRangeCm;
		}

		// Prefer live MiningComponent tunables when already targeting a node.
		if (CurrentNode != nullptr && Mining != nullptr)
		{
			if (Mining->GetAmountPerMiningCycle() > 0.0f)
			{
				AmountPerCycle = Mining->GetAmountPerMiningCycle();
			}
			if (Mining->GetMiningCycleDuration() > 0.0f)
			{
				CycleDuration = Mining->GetMiningCycleDuration();
			}
			if (Mining->GetInteractionRangeCm() > 0.0f)
			{
				InteractionRange = Mining->GetInteractionRangeCm();
			}
		}

		float Distance = -1.0f;
		bool bInRange = false;
		if (DiagnosticNode != nullptr)
		{
			Distance = FVector::Dist(Host->GetActorLocation(), DiagnosticNode->GetActorLocation());
			bInRange = FMath::IsFinite(InteractionRange) && InteractionRange > 0.0f && Distance <= InteractionRange;
		}

		TArray<FText> Errors;
		TArray<FText> Warnings;
		const bool bValid = Mining != nullptr && Mining->ValidateMiningContract(Errors, Warnings);

		UE_LOG(LogGPMining, Log,
			TEXT("GP Mining.Inspect: Owner=%s Path=%s Class=%s Role=%s NetMode=%s HasAuthority=%s MiningState=%s LastStopReason=%s CurrentNode=%s DiagnosticNode=%s SoftDefinition=%s PrimaryAssetId=%s AmountPerCycle=%.3f CycleDuration=%.3f InteractionRangeCm=%.3f Distance=%.3f InRange=%s HasActiveSlot=%s IsWaitingForSlot=%s NodeCurrent=%d NodeMax=%d CargoCurrent=%.3f CargoCapacity=%.3f CargoRemaining=%.3f TimerActive=%s ComponentTick=%s ActorTick=%s HasSceneRoot=%s Replicates=%s ValidationOk=%s Errors=%d Warnings=%d"),
			*Host->GetName(),
			*Host->GetPathName(),
			*GetNameSafe(Host->GetClass()),
			GPMiningPrivate::RoleToString(Host->GetLocalRole()),
			GPMiningPrivate::NetModeToString(World->GetNetMode()),
			Host->HasAuthority() ? TEXT("true") : TEXT("false"),
			Mining != nullptr ? GPMiningPrivate::MiningStateToString(Mining->GetMiningState()) : TEXT("n/a"),
			Mining != nullptr ? GPMiningPrivate::StopReasonToString(Mining->GetLastStopReason()) : TEXT("n/a"),
			CurrentNode != nullptr ? *CurrentNode->GetName() : TEXT("none"),
			DiagnosticNode != nullptr ? *DiagnosticNode->GetName() : TEXT("none"),
			DiagnosticNode != nullptr ? *DiagnosticNode->GetResourceDefinitionSoft().ToSoftObjectPath().ToString() : TEXT("none"),
			*PrimaryId,
			AmountPerCycle,
			CycleDuration,
			InteractionRange,
			Distance,
			bInRange ? TEXT("true") : TEXT("false"),
			(DiagnosticNode != nullptr && DiagnosticNode->HasActiveMiningSlot(Host)) ? TEXT("true") : TEXT("false"),
			(Mining != nullptr && Mining->IsWaitingForSlot()) ? TEXT("true") : TEXT("false"),
			DiagnosticNode != nullptr ? DiagnosticNode->GetCurrentAmount() : -1,
			DiagnosticNode != nullptr ? DiagnosticNode->GetMaxAmount() : -1,
			Cargo != nullptr ? Cargo->GetCurrentCargoAmount() : -1.0f,
			Cargo != nullptr ? Cargo->GetCargoCapacity() : -1.0f,
			Cargo != nullptr ? Cargo->GetRemainingCapacity() : -1.0f,
			(Mining != nullptr && Mining->IsMiningTimerActive()) ? TEXT("true") : TEXT("false"),
			(Mining != nullptr && Mining->IsComponentTickEnabled()) ? TEXT("true") : TEXT("false"),
			Host->IsActorTickEnabled() ? TEXT("true") : TEXT("false"),
			Host->GetSceneRoot() != nullptr ? TEXT("true") : TEXT("false"),
			(Mining != nullptr && Mining->GetIsReplicated()) ? TEXT("true") : TEXT("false"),
			bValid ? TEXT("true") : TEXT("false"),
			Errors.Num(),
			Warnings.Num());
	}

	static void MiningBegin(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.Begin: missing world"));
			return;
		}
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.Begin: rejected on client (authority required)"));
			return;
		}

		const FString HostName = Args.Num() > 0 ? Args[0] : FString();
		const FString NodeName = Args.Num() > 1 ? Args[1] : FString();
		AGP_MiningDiagnosticHost* Host = FindHost(World, HostName);
		AGP_ResourceNode* Node = FindNode(World, NodeName);
		if (Host == nullptr || Host->GetMiningComponent() == nullptr || Node == nullptr)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.Begin: usage gp.Mining.Begin [HostName] [NodeName]"));
			return;
		}

		const EGP_BeginMiningResult Result = Host->GetMiningComponent()->BeginMining(Node);
		UE_LOG(LogGPMining, Log,
			TEXT("GP Mining.Begin: Host=%s Node=%s Result=%s State=%s TimerActive=%s"),
			*Host->GetName(),
			*Node->GetName(),
			GPMiningPrivate::BeginResultToString(Result),
			GPMiningPrivate::MiningStateToString(Host->GetMiningComponent()->GetMiningState()),
			Host->GetMiningComponent()->IsMiningTimerActive() ? TEXT("true") : TEXT("false"));
	}

	static void MiningStop(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.Stop: missing world"));
			return;
		}
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.Stop: rejected on client (authority required)"));
			return;
		}

		const FString HostName = Args.Num() > 0 ? Args[0] : FString();
		AGP_MiningDiagnosticHost* Host = FindHost(World, HostName);
		if (Host == nullptr || Host->GetMiningComponent() == nullptr)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.Stop: no host found"));
			return;
		}

		Host->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		Host->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop); // idempotent
		UE_LOG(LogGPMining, Log,
			TEXT("GP Mining.Stop: Host=%s State=%s Reason=%s TimerActive=%s"),
			*Host->GetName(),
			GPMiningPrivate::MiningStateToString(Host->GetMiningComponent()->GetMiningState()),
			GPMiningPrivate::StopReasonToString(Host->GetMiningComponent()->GetLastStopReason()),
			Host->GetMiningComponent()->IsMiningTimerActive() ? TEXT("true") : TEXT("false"));
	}

	static void MiningRunContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.RunContractTest: missing world"));
			return;
		}
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Mining.RunContractTest: rejected on client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (!bOk)
			{
				++Failures;
				UE_LOG(LogGPMining, Error, TEXT("GP Mining.RunContractTest FAIL: %s"), Label);
			}
			else
			{
				UE_LOG(LogGPMining, Log, TEXT("GP Mining.RunContractTest PASS: %s"), Label);
			}
		};

		AGP_ResourceNode* LiveNode = FindNode(World, FString());
		Expect(LiveNode != nullptr, TEXT("HasResourceNode"));

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;

		if (LiveNode == nullptr)
		{
			UE_LOG(LogGPMining, Log, TEXT("GP Mining.RunContractTest: Complete Failures=%d (no node)"), Failures);
			return;
		}

		const int32 NodeBefore = LiveNode->GetCurrentAmount();
		const UGP_ResourceDefinition* LiveDef = LiveNode->ResolveResourceDefinition(true);
		const float LiveRange = LiveDef != nullptr ? LiveDef->InteractionRangeCm : 200.0f;
		AGP_MiningDiagnosticHost* Host = SpawnHostNearNode(World, LiveNode, LiveRange);
		Expect(Host != nullptr && Host->GetMiningComponent() != nullptr && Host->GetCargoComponent() != nullptr, TEXT("SpawnHost"));
		UGP_MiningComponent* Mining = Host != nullptr ? Host->GetMiningComponent() : nullptr;
		UGP_CargoComponent* Cargo = Host != nullptr ? Host->GetCargoComponent() : nullptr;
		if (Host == nullptr || Mining == nullptr || Cargo == nullptr)
		{
			UE_LOG(LogGPMining, Log, TEXT("GP Mining.RunContractTest: Complete Failures=%d (host spawn failed)"), Failures);
			return;
		}

		Expect(Host->GetSceneRoot() != nullptr && Host->GetRootComponent() == Host->GetSceneRoot(), TEXT("SpawnedHostHasSceneRoot"));
		{
			const float HostDist = FVector::Dist(Host->GetActorLocation(), LiveNode->GetActorLocation());
			Expect(HostDist < LiveRange, TEXT("SpawnedHostWithinInteractionRange"));
			const float ExpectedOffset = FMath::Min(LiveRange * 0.5f, 100.0f);
			const FVector ExpectedLoc = LiveNode->GetActorLocation() + FVector(ExpectedOffset, 0.0f, 0.0f);
			Expect(Host->GetActorLocation().Equals(ExpectedLoc, 1.0f), TEXT("SpawnedHostLocationMatchesRequested"));
		}
		Expect(LiveDef != nullptr, TEXT("LiveNodeDefinitionResolved"));
		if (LiveDef != nullptr)
		{
			Expect(FMath::IsNearlyEqual(LiveDef->AmountPerMiningCycle, 10.0f), TEXT("InitialAmountPerCycle10"));
			Expect(FMath::IsNearlyEqual(LiveDef->MiningCycleDurationSeconds, 1.0f), TEXT("InitialCycleDuration1"));
			Expect(FMath::IsNearlyEqual(LiveDef->InteractionRangeCm, 200.0f), TEXT("InitialInteractionRange200"));
		}

		Expect(Mining->IsComponentTickEnabled() == false, TEXT("ComponentTickDisabled"));
		Expect(Host->IsActorTickEnabled() == false, TEXT("ActorTickDisabled"));

		// Missing cargo: destroy cargo component then BeginMining.
		AGP_MiningDiagnosticHost* NoCargoHost = SpawnHostNearNode(World, LiveNode, LiveRange);
		Expect(NoCargoHost != nullptr, TEXT("SpawnNoCargoHost"));
		if (NoCargoHost != nullptr)
		{
			Expect(FVector::Dist(NoCargoHost->GetActorLocation(), LiveNode->GetActorLocation()) < LiveRange, TEXT("NoCargoHostWithinRange"));
			if (NoCargoHost->GetCargoComponent() != nullptr)
			{
				NoCargoHost->GetCargoComponent()->DestroyComponent();
				Expect(NoCargoHost->GetMiningComponent()->BeginMining(LiveNode) == EGP_BeginMiningResult::RejectedMissingCargo, TEXT("MissingCargoRejection"));
			}
			NoCargoHost->Destroy();
		}

		const FVector FarLocation = LiveNode->GetActorLocation() + FVector(5000.0f, 0.0f, 0.0f);
		AGP_MiningDiagnosticHost* FarHost = World->SpawnActor<AGP_MiningDiagnosticHost>(
			AGP_MiningDiagnosticHost::StaticClass(),
			FarLocation,
			FRotator::ZeroRotator,
			Params);
		Expect(FarHost != nullptr, TEXT("SpawnFarHost"));
		if (FarHost != nullptr)
		{
			if (!FarHost->GetActorLocation().Equals(FarLocation, 1.0f))
			{
				FarHost->SetActorLocation(FarLocation, false, nullptr, ETeleportType::TeleportPhysics);
			}
			Expect(FVector::Dist(FarHost->GetActorLocation(), LiveNode->GetActorLocation()) > LiveRange, TEXT("FarHostRemainsOutOfRange"));
			Expect(FarHost->GetMiningComponent()->BeginMining(LiveNode) == EGP_BeginMiningResult::RejectedOutOfRange, TEXT("OutOfRangeRejection"));
			Expect(!LiveNode->HasActiveMiningSlot(FarHost) && !LiveNode->IsWaitingForMiningSlot(FarHost), TEXT("OutOfRangeNoSlot"));
			FarHost->Destroy();
		}

		Expect(Mining->BeginMining(nullptr) == EGP_BeginMiningResult::RejectedInvalidNode, TEXT("InvalidNodeRejection"));

		const EGP_BeginMiningResult BeginResult = Mining->BeginMining(LiveNode);
		Expect(BeginResult == EGP_BeginMiningResult::Started, TEXT("ValidSlotGrant"));
		Expect(Mining->GetMiningState() == EGP_MiningState::Mining, TEXT("StateMiningAfterBegin"));
		Expect(Mining->IsMiningTimerActive(), TEXT("TimerActiveAfterBegin"));
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 0.0f), TEXT("FirstCycleDelayNoInstantTransfer"));
		Expect(Mining->BeginMining(LiveNode) == EGP_BeginMiningResult::AlreadyMiningTarget, TEXT("DuplicateBeginSameTarget"));
		Expect(Mining->IsMiningTimerActive(), TEXT("NoDuplicateTimerBreak"));

		Mining->DebugForceExecuteMiningCycle();
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 10.0f), TEXT("OneCycleTransfers10"));
		Expect(LiveNode->GetCurrentAmount() == NodeBefore - 10, TEXT("NodeDecreasedBy10"));

		for (int32 Cycle = 0; Cycle < 4; ++Cycle)
		{
			Mining->DebugForceExecuteMiningCycle();
		}
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 50.0f), TEXT("FiveCyclesFillCargo50"));
		Expect(LiveNode->GetCurrentAmount() == NodeBefore - 50, TEXT("NodeDecreasedBy50"));
		Expect(Mining->GetMiningState() == EGP_MiningState::CargoFull, TEXT("FullCargoStops"));
		Expect(!Mining->IsMiningTimerActive(), TEXT("FullCargoTimerStopped"));
		Expect(!LiveNode->HasActiveMiningSlot(Host), TEXT("FullCargoReleasesSlot"));

		Cargo->ClearCargo();
		Expect(Mining->BeginMining(LiveNode) == EGP_BeginMiningResult::Started, TEXT("RestartAfterClear"));

		// Partial deplete cycle on remaining capacity path: fill cargo to 45, force cycle of 5.
		Cargo->ClearCargo();
		Cargo->AddCargo(45.0f);
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		Expect(Mining->BeginMining(LiveNode) == EGP_BeginMiningResult::Started, TEXT("BeginForPartialCargo"));
		const int32 NodeBeforePartial = LiveNode->GetCurrentAmount();
		Mining->DebugForceExecuteMiningCycle();
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 50.0f), TEXT("PartialCargoCycleFills"));
		Expect(LiveNode->GetCurrentAmount() == NodeBeforePartial - 5, TEXT("PartialCargoNodeMinus5"));
		Expect(Mining->GetMiningState() == EGP_MiningState::CargoFull, TEXT("PartialCargoStateFull"));

		// Depleted partial: consume node to 5 remaining, clear cargo, mine once.
		Cargo->ClearCargo();
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		const int32 ToLeave = 5;
		const int32 ToConsume = LiveNode->GetCurrentAmount() - ToLeave;
		if (ToConsume > 0)
		{
			LiveNode->ConsumeResource(ToConsume);
		}
		Expect(LiveNode->GetCurrentAmount() == ToLeave, TEXT("NodePreparedWith5"));
		Expect(Mining->BeginMining(LiveNode) == EGP_BeginMiningResult::Started, TEXT("BeginDepletedPartial"));
		Mining->DebugForceExecuteMiningCycle();
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 5.0f), TEXT("DepletedPartialTransfers5"));
		Expect(LiveNode->IsDepleted(), TEXT("NodeDepletedAfterPartial"));
		Expect(Mining->GetMiningState() == EGP_MiningState::DepositDepleted, TEXT("StateDepositDepleted"));
		Expect(!LiveNode->HasActiveMiningSlot(Host), TEXT("DepletedReleasesSlot"));

		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		Expect(Mining->GetMiningState() == EGP_MiningState::Idle, TEXT("StopMiningIdempotent"));

		// FIFO promotion: restore node amount for slot tests via... we can't restore easily.
		// Use a fresh temporary ResourceNode for occupancy if possible.
		AGP_ResourceNode* SlotNode = World->SpawnActor<AGP_ResourceNode>(
			AGP_ResourceNode::StaticClass(),
			FVector(-40000.0f, 0.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		Expect(SlotNode != nullptr, TEXT("SpawnSlotTestNode"));
		TArray<AGP_MiningDiagnosticHost*> SlotHosts;
		if (SlotNode != nullptr)
		{
			for (int32 Index = 0; Index < 5; ++Index)
			{
				AGP_MiningDiagnosticHost* SlotHost = SpawnHostNearNode(World, SlotNode, 200.0f);
				if (SlotHost != nullptr)
				{
					SlotHosts.Add(SlotHost);
					SlotHost->GetMiningComponent()->BeginMining(SlotNode);
				}
			}
			Expect(SlotHosts.Num() == 5, TEXT("SpawnedFiveMiners"));
			int32 MiningCount = 0;
			int32 WaitingCount = 0;
			AGP_MiningDiagnosticHost* WaitingHost = nullptr;
			for (AGP_MiningDiagnosticHost* SlotHost : SlotHosts)
			{
				if (SlotHost->GetMiningComponent()->GetMiningState() == EGP_MiningState::Mining)
				{
					++MiningCount;
				}
				else if (SlotHost->GetMiningComponent()->GetMiningState() == EGP_MiningState::WaitingForSlot)
				{
					++WaitingCount;
					WaitingHost = SlotHost;
				}
			}
			Expect(MiningCount == 4, TEXT("FourActiveMiners"));
			Expect(WaitingCount == 1, TEXT("OneWaitingMiner"));
			Expect(SlotNode->GetActiveMinerCount() == 4 && SlotNode->GetWaitingMinerCount() == 1, TEXT("NodeOccupancyCounts"));

			if (SlotHosts.Num() > 0 && WaitingHost != nullptr)
			{
				AGP_MiningDiagnosticHost* ActiveToStop = nullptr;
				for (AGP_MiningDiagnosticHost* SlotHost : SlotHosts)
				{
					if (SlotHost != WaitingHost && SlotHost->GetMiningComponent()->IsMining())
					{
						ActiveToStop = SlotHost;
						break;
					}
				}
				Expect(ActiveToStop != nullptr, TEXT("FoundActiveToStop"));
				if (ActiveToStop != nullptr)
				{
					ActiveToStop->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
					Expect(WaitingHost->GetMiningComponent()->GetMiningState() == EGP_MiningState::Mining, TEXT("WaitingPromotedToMining"));
					Expect(WaitingHost->GetMiningComponent()->IsMiningTimerActive(), TEXT("PromotedTimerStarted"));
				}
			}

			// Duplicate queue entry: AlreadyWaiting
			if (WaitingHost == nullptr && SlotHosts.Num() > 0)
			{
				// after promotion no waiter — request again on full node from a stopped host
			}

			for (AGP_MiningDiagnosticHost* SlotHost : SlotHosts)
			{
				if (IsValid(SlotHost))
				{
					SlotHost->Destroy();
				}
			}
			Expect(SlotNode->GetActiveMinerCount() == 0 && SlotNode->GetWaitingMinerCount() == 0, TEXT("EndPlayReleasesSlots"));
			SlotNode->Destroy();
		}

		if (IsValid(Host))
		{
			Host->Destroy();
		}

		UE_LOG(LogGPMining, Log,
			TEXT("GP Mining.RunContractTest: Complete Failures=%d (map/assets not saved)"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMiningSpawnHost(
		TEXT("gp.Mining.SpawnDiagnosticHost"),
		TEXT("Authority: spawn transient MiningDiagnosticHost near ResourceNode. Usage: gp.Mining.SpawnDiagnosticHost [NodeName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MiningSpawnHost));

	static FAutoConsoleCommandWithWorldAndArgs GMiningInspect(
		TEXT("gp.Mining.Inspect"),
		TEXT("Inspect mining diagnostic host. Usage: gp.Mining.Inspect [HostName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MiningInspect));

	static FAutoConsoleCommandWithWorldAndArgs GMiningBegin(
		TEXT("gp.Mining.Begin"),
		TEXT("Authority BeginMining. Usage: gp.Mining.Begin [HostName] [NodeName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MiningBegin));

	static FAutoConsoleCommandWithWorldAndArgs GMiningStop(
		TEXT("gp.Mining.Stop"),
		TEXT("Authority StopMining. Usage: gp.Mining.Stop [HostName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MiningStop));

	static FAutoConsoleCommandWithWorldAndArgs GMiningRunContractTest(
		TEXT("gp.Mining.RunContractTest"),
		TEXT("Authority deterministic mining contract checks (uses DebugForceExecuteMiningCycle). Does not save maps."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MiningRunContractTest));
}
#endif
