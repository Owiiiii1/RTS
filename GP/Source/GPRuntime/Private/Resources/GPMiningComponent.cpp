// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPMiningComponent.h"

#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPResourceDefinition.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"

#include <limits>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if !UE_BUILD_SHIPPING
#include "Buildings/GPMainBase.h"
#include "Command/GPUnitCommand.h"
#include "Debug/GPContractTestCoordinator.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPResourceLoopDiagnostics.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPUnitCommandComponent.h"
#include "Units/GPWorker.h"
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
	if (UGP_CargoComponent* Cached = CachedCargoComponent.Get())
	{
		if (IsValid(Cached))
		{
			return Cached;
		}
	}

	UGP_CargoComponent* Found = FindOwnerCargoComponent();
	if (IsValid(Found))
	{
		const_cast<UGP_MiningComponent*>(this)->CachedCargoComponent = Found;
		return Found;
	}

	const_cast<UGP_MiningComponent*>(this)->CachedCargoComponent.Reset();
	return nullptr;
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
	if (!IsValid(Node) || !IsValid(Owner) || Node->IsActorBeingDestroyed() || Node->IsClearingOccupancy())
	{
		return;
	}

	if (Node->HasActiveMiningSlot(Owner) || Node->IsWaitingForMiningSlot(Owner))
	{
		Node->ReleaseMiningSlot(Owner);
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

		// Direct UnitCommand notify after multicast: covers unbound listeners after reassignment remine.
		const bool bTerminal =
			NewState == EGP_MiningState::CargoFull
			|| NewState == EGP_MiningState::DepositDepleted
			|| NewState == EGP_MiningState::OutOfRange
			|| NewState == EGP_MiningState::Invalid
			|| NewState == EGP_MiningState::Idle;
		if (bTerminal)
		{
			if (AGP_Worker* Worker = Cast<AGP_Worker>(GetOwner()))
			{
				if (UGP_UnitCommandComponent* Commands = Worker->GetUnitCommandComponent())
				{
					Commands->NotifyMiningComponentTerminal(Previous, NewState, Reason);
				}
			}
		}
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
	if (!IsValid(this) || bIsStoppingMining || !HasAuthorityOwner()
		|| !IsValid(GetOwner()) || !IsValid(Miner) || Miner != GetOwner())
	{
		return;
	}

	if (CurrentMiningState == EGP_MiningState::WaitingForSlot
		&& OldState == EGP_MinerOccupancyState::Waiting
		&& NewState == EGP_MinerOccupancyState::Active)
	{
		if (!IsValid(CurrentResourceNode) || CurrentResourceNode->IsClearingOccupancy())
		{
			StopMining(EGP_MiningStopReason::TargetEndPlay);
			return;
		}
		if (CurrentResourceNode->IsDepleted())
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
		// Consume→depletion ClearOccupancy can broadcast None mid-cycle; defer until AddCargo finishes.
		if (bExecutingMiningCycle)
		{
			return;
		}
		// External removal only (node EndPlay / cleanup). Own StopMining unbinds before Release.
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

	if (bIsStoppingMining)
	{
		return;
	}

	TGuardValue<bool> StoppingGuard(bIsStoppingMining, true);

	const bool bBusy =
		CurrentMiningState == EGP_MiningState::Mining
		|| CurrentMiningState == EGP_MiningState::WaitingForSlot;

	ClearMiningTimer();
	// Unbind before ReleaseMiningSlot so occupancy Broadcast(None) cannot re-enter StopMining.
	UnbindOccupancyEvents();
	if (bBusy)
	{
		ReleaseSlotOnCurrentNode();
	}
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

	// Atomic consume→credit: depletion ClearOccupancy must not terminal-stop before AddCargo.
	TGuardValue<bool> CycleGuard(bExecutingMiningCycle, true);
	UnbindOccupancyEvents();

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

	if (Node->IsDepleted()
		|| Node->HasCompletedDepletionTransition()
		|| Node->IsDestroyPending()
		|| !Node->HasActiveMiningSlot(Owner))
	{
		StopMining(EGP_MiningStopReason::DepositDepleted);
		return;
	}

	BindOccupancyEvents(Node);
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

AGP_MiningNoCargoDiagnosticHost::AGP_MiningNoCargoDiagnosticHost()
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

	MiningComponent = CreateDefaultSubobject<UGP_MiningComponent>(TEXT("MiningComponent"));
}

UGP_MiningComponent* AGP_MiningNoCargoDiagnosticHost::GetMiningComponent() const
{
	return MiningComponent;
}

USceneComponent* AGP_MiningNoCargoDiagnosticHost::GetSceneRoot() const
{
	return SceneRoot;
}

#if !UE_BUILD_SHIPPING
namespace GPMiningDebug
{
	TWeakObjectPtr<UGP_MiningContractTestRunner> GActiveContractTestRunner;
	TWeakObjectPtr<UGP_ResourceNodeEndPlayContractTestRunner> GActiveEndPlayContractTestRunner;
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

	AGP_MiningDiagnosticHost* SpawnHostNearNode(UWorld* World, AGP_ResourceNode* Node, float RangeCm)
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
		if (GActiveContractTestRunner.IsValid())
		{
			UE_LOG(LogGPMining, Warning,
				TEXT("GP Mining.RunContractTest: rejected — previous staged test still running"));
			return;
		}
		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(World, TEXT("MiningContract"), TEXT("Mining"), Token))
		{
			return;
		}

		UGP_MiningContractTestRunner* Runner = NewObject<UGP_MiningContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveContractTestRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
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
		TEXT("Authority staged mining contract checks (lifecycle-safe). Does not save maps."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MiningRunContractTest));

	static void ResourceRunEndPlayContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMining, Warning, TEXT("GP Resource.RunEndPlayContractTest: missing world or client"));
			return;
		}
		if (GActiveEndPlayContractTestRunner.IsValid())
		{
			UE_LOG(LogGPMining, Warning,
				TEXT("GP Resource.RunEndPlayContractTest: rejected — previous staged test still running"));
			return;
		}
		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(World, TEXT("ResourceEndPlayContract"), TEXT("ResourceEndPlay"), Token))
		{
			return;
		}

		UGP_ResourceNodeEndPlayContractTestRunner* Runner =
			NewObject<UGP_ResourceNodeEndPlayContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveEndPlayContractTestRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GResourceRunEndPlayContractTest(
		TEXT("gp.Resource.RunEndPlayContractTest"),
		TEXT("Authority: ResourceNode EndPlay occupancy teardown contract (active+waiting + haul-loop)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceRunEndPlayContractTest));
} // namespace GPMiningDebug

void UGP_MiningContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_MiningContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_MiningContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		Abort(TEXT("WorldEndPlay"));
	}
}

void UGP_MiningContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	bCancelled = false;
	CancelReason = NAME_None;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	FifoHostsWeak.Reset();
	WaitingHostWeak.Reset();
	PrimaryHostWeak.Reset();
	TestNodeWeak.Reset();
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UGP_MiningContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPMining, Log, TEXT("GP Mining.RunContractTest Stage=Start (staged lifecycle-safe runner)"));
	ScheduleNext();
}

void UGP_MiningContractTestRunner::Abort(const TCHAR* Reason)
{
	if (bFinished)
	{
		return;
	}
	UE_LOG(LogGPMining, Error, TEXT("GP Mining.RunContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

void UGP_MiningContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalid"));
		return;
	}

	StageTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_MiningContractTestRunner::AdvanceStage));
}

bool UGP_MiningContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPMining, Error, TEXT("GP Mining.RunContractTest FAIL: %s"), Label);
		return false;
	}

	UE_LOG(LogGPMining, Log, TEXT("GP Mining.RunContractTest PASS: %s"), Label);
	return true;
}

void UGP_MiningContractTestRunner::LogStage(const TCHAR* StageName) const
{
	AGP_ResourceNode* Node = TestNodeWeak.Get();
	AGP_MiningDiagnosticHost* Host = PrimaryHostWeak.Get();
	UE_LOG(LogGPMining, Log,
		TEXT("GP Mining.RunContractTest Stage=%s Failures=%d TestNodeValid=%s HostValid=%s NodeActive=%d NodeWaiting=%d"),
		StageName,
		Failures,
		IsValid(Node) ? TEXT("true") : TEXT("false"),
		IsValid(Host) ? TEXT("true") : TEXT("false"),
		IsValid(Node) ? Node->GetActiveMinerCount() : -1,
		IsValid(Node) ? Node->GetWaitingMinerCount() : -1);
}

AGP_ResourceNode* UGP_MiningContractTestRunner::SpawnTransientNode(const FVector& Location) const
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	return World->SpawnActor<AGP_ResourceNode>(
		AGP_ResourceNode::StaticClass(),
		Location,
		FRotator::ZeroRotator,
		Params);
}

AGP_MiningDiagnosticHost* UGP_MiningContractTestRunner::SpawnHostNear(AGP_ResourceNode* Node, float RangeCm) const
{
	return GPMiningDebug::SpawnHostNearNode(WorldWeak.Get(), Node, RangeCm);
}

void UGP_MiningContractTestRunner::SafeStopAndDestroyHost(TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak)
{
	AGP_MiningDiagnosticHost* Host = HostWeak.Get();
	if (!IsValid(Host))
	{
		HostWeak.Reset();
		return;
	}

	if (UGP_MiningComponent* Mining = Host->GetMiningComponent())
	{
		if (IsValid(Mining))
		{
			Mining->StopMining(EGP_MiningStopReason::ManualStop);
		}
	}

	Host->Destroy();
	HostWeak.Reset();
}

void UGP_MiningContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	UnbindWorldCleanup();

	UWorld* World = WorldWeak.Get();
	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
	}

	SafeStopAndDestroyHost(PrimaryHostWeak);
	for (TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak : FifoHostsWeak)
	{
		SafeStopAndDestroyHost(HostWeak);
	}
	FifoHostsWeak.Reset();

	if (AGP_ResourceNode* Node = TestNodeWeak.Get())
	{
		if (IsValid(Node))
		{
			Node->Destroy();
		}
	}
	TestNodeWeak.Reset();

	UE_LOG(LogGPMining, Log,
		TEXT("GP Mining.RunContractTest: Complete Failures=%d (map/assets not saved; staged runner)"),
		Failures);

	GPContractTestCoordinator::Release(ExecutionId, Failures, bCancelled, *CancelReason.ToString());
	RemoveFromRoot();
	GPMiningDebug::GActiveContractTestRunner.Reset();
	WorldWeak.Reset();
}

void UGP_MiningContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (bFinished || !GPContractTestCoordinator::IsTokenActive(ExecutionId))
	{
		return;
	}
	if (GPContractTestCoordinator::IsWorldTearingDown(World))
	{
		bCancelled = true;
		CancelReason = FName(TEXT("WorldEndPlay"));
		Abort(TEXT("WorldEndPlay"));
		return;
	}
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalidDuringStage"));
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		LogStage(TEXT("SpawnTransientNodeAndPrimaryHost"));
		AGP_ResourceNode* Node = SpawnTransientNode(FVector(-45000.0f, 0.0f, 100.0f));
		if (!Expect(IsValid(Node), TEXT("SpawnTransientTestNode")))
		{
			Finish();
			return;
		}
		TestNodeWeak = Node;

		const UGP_ResourceDefinition* Def = Node->ResolveResourceDefinition(true);
		InteractionRangeCm = Def != nullptr ? Def->InteractionRangeCm : 200.0f;
		Expect(Def != nullptr, TEXT("TestNodeDefinitionResolved"));
		if (Def != nullptr)
		{
			Expect(FMath::IsNearlyEqual(Def->AmountPerMiningCycle, 10.0f), TEXT("InitialAmountPerCycle10"));
			Expect(FMath::IsNearlyEqual(Def->MiningCycleDurationSeconds, 1.0f), TEXT("InitialCycleDuration1"));
			Expect(FMath::IsNearlyEqual(Def->InteractionRangeCm, 200.0f), TEXT("InitialInteractionRange200"));
		}

		AGP_MiningDiagnosticHost* Host = SpawnHostNear(Node, InteractionRangeCm);
		if (!Expect(IsValid(Host) && IsValid(Host->GetMiningComponent()) && IsValid(Host->GetCargoComponent()), TEXT("SpawnHost")))
		{
			Finish();
			return;
		}
		PrimaryHostWeak = Host;
		Expect(Host->GetSceneRoot() != nullptr && Host->GetRootComponent() == Host->GetSceneRoot(), TEXT("SpawnedHostHasSceneRoot"));
		Expect(FVector::Dist(Host->GetActorLocation(), Node->GetActorLocation()) < InteractionRangeCm, TEXT("SpawnedHostWithinInteractionRange"));
		{
			const float ExpectedOffset = FMath::Min(InteractionRangeCm * 0.5f, 100.0f);
			Expect(Host->GetActorLocation().Equals(Node->GetActorLocation() + FVector(ExpectedOffset, 0.0f, 0.0f), 1.0f),
				TEXT("SpawnedHostLocationMatchesRequested"));
		}
		Expect(Host->GetMiningComponent()->IsComponentTickEnabled() == false, TEXT("ComponentTickDisabled"));
		Expect(Host->IsActorTickEnabled() == false, TEXT("ActorTickDisabled"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		LogStage(TEXT("MissingCargoAndRangeRejects"));
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Node), TEXT("TestNodeStillValid")))
		{
			Finish();
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		const float Offset = FMath::Min(InteractionRangeCm * 0.5f, 100.0f);
		AGP_MiningNoCargoDiagnosticHost* NoCargo = World->SpawnActor<AGP_MiningNoCargoDiagnosticHost>(
			AGP_MiningNoCargoDiagnosticHost::StaticClass(),
			Node->GetActorLocation() + FVector(Offset, 0.0f, 0.0f),
			FRotator::ZeroRotator,
			Params);
		if (!Expect(IsValid(NoCargo) && IsValid(NoCargo->GetMiningComponent()), TEXT("SpawnNoCargoHost")))
		{
			Finish();
			return;
		}
		Expect(FVector::Dist(NoCargo->GetActorLocation(), Node->GetActorLocation()) < InteractionRangeCm, TEXT("NoCargoHostWithinRange"));
		Expect(NoCargo->GetMiningComponent()->BeginMining(Node) == EGP_BeginMiningResult::RejectedMissingCargo, TEXT("MissingCargoRejection"));
		NoCargo->Destroy();

		const FVector FarLocation = Node->GetActorLocation() + FVector(5000.0f, 0.0f, 0.0f);
		AGP_MiningDiagnosticHost* FarHost = World->SpawnActor<AGP_MiningDiagnosticHost>(
			AGP_MiningDiagnosticHost::StaticClass(),
			FarLocation,
			FRotator::ZeroRotator,
			Params);
		if (!Expect(IsValid(FarHost), TEXT("SpawnFarHost")))
		{
			Finish();
			return;
		}
		if (!FarHost->GetActorLocation().Equals(FarLocation, 1.0f))
		{
			FarHost->SetActorLocation(FarLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
		Expect(FVector::Dist(FarHost->GetActorLocation(), Node->GetActorLocation()) > InteractionRangeCm, TEXT("FarHostRemainsOutOfRange"));
		Expect(FarHost->GetMiningComponent()->BeginMining(Node) == EGP_BeginMiningResult::RejectedOutOfRange, TEXT("OutOfRangeRejection"));
		Expect(!Node->HasActiveMiningSlot(FarHost) && !Node->IsWaitingForMiningSlot(FarHost), TEXT("OutOfRangeNoSlot"));
		if (UGP_MiningComponent* FarMining = FarHost->GetMiningComponent())
		{
			FarMining->StopMining(EGP_MiningStopReason::ManualStop);
		}
		FarHost->Destroy();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		LogStage(TEXT("CyclesFillAndPartial"));
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		AGP_MiningDiagnosticHost* Host = PrimaryHostWeak.Get();
		if (!Expect(IsValid(Node) && IsValid(Host), TEXT("PrimaryObjectsValid")))
		{
			Finish();
			return;
		}
		UGP_MiningComponent* Mining = Host->GetMiningComponent();
		UGP_CargoComponent* Cargo = Host->GetCargoComponent();
		if (!Expect(IsValid(Mining) && IsValid(Cargo), TEXT("PrimaryComponentsValid")))
		{
			Finish();
			return;
		}

		Expect(Mining->BeginMining(nullptr) == EGP_BeginMiningResult::RejectedInvalidNode, TEXT("InvalidNodeRejection"));
		NodeAmountBeforeCycles = Node->GetCurrentAmount();
		Expect(Mining->BeginMining(Node) == EGP_BeginMiningResult::Started, TEXT("ValidSlotGrant"));
		Expect(Mining->GetMiningState() == EGP_MiningState::Mining, TEXT("StateMiningAfterBegin"));
		Expect(Mining->IsMiningTimerActive(), TEXT("TimerActiveAfterBegin"));
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 0.0f), TEXT("FirstCycleDelayNoInstantTransfer"));
		Expect(Mining->BeginMining(Node) == EGP_BeginMiningResult::AlreadyMiningTarget, TEXT("DuplicateBeginSameTarget"));
		Expect(Mining->IsMiningTimerActive(), TEXT("NoDuplicateTimerBreak"));

		Mining->DebugForceExecuteMiningCycle();
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 10.0f), TEXT("OneCycleTransfers10"));
		Expect(Node->GetCurrentAmount() == NodeAmountBeforeCycles - 10, TEXT("NodeDecreasedBy10"));
		for (int32 Cycle = 0; Cycle < 4; ++Cycle)
		{
			Mining->DebugForceExecuteMiningCycle();
		}
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 50.0f), TEXT("FiveCyclesFillCargo50"));
		Expect(Node->GetCurrentAmount() == NodeAmountBeforeCycles - 50, TEXT("NodeDecreasedBy50"));
		Expect(Mining->GetMiningState() == EGP_MiningState::CargoFull, TEXT("FullCargoStops"));
		Expect(!Mining->IsMiningTimerActive(), TEXT("FullCargoTimerStopped"));
		Expect(!Node->HasActiveMiningSlot(Host), TEXT("FullCargoReleasesSlot"));

		Cargo->ClearCargo();
		Expect(Mining->BeginMining(Node) == EGP_BeginMiningResult::Started, TEXT("RestartAfterClear"));
		Cargo->ClearCargo();
		Cargo->AddCargo(45.0f);
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		Expect(Mining->BeginMining(Node) == EGP_BeginMiningResult::Started, TEXT("BeginForPartialCargo"));
		const int32 NodeBeforePartial = Node->GetCurrentAmount();
		Mining->DebugForceExecuteMiningCycle();
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 50.0f), TEXT("PartialCargoCycleFills"));
		Expect(Node->GetCurrentAmount() == NodeBeforePartial - 5, TEXT("PartialCargoNodeMinus5"));
		Expect(Mining->GetMiningState() == EGP_MiningState::CargoFull, TEXT("PartialCargoStateFull"));

		Cargo->ClearCargo();
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		const int32 ToLeave = 5;
		const int32 ToConsume = Node->GetCurrentAmount() - ToLeave;
		if (ToConsume > 0)
		{
			Node->ConsumeResource(ToConsume);
		}
		Expect(Node->GetCurrentAmount() == ToLeave, TEXT("NodePreparedWith5"));
		Expect(Mining->BeginMining(Node) == EGP_BeginMiningResult::Started, TEXT("BeginDepletedPartial"));
		Mining->DebugForceExecuteMiningCycle();
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), 5.0f), TEXT("DepletedPartialTransfers5"));
		Expect(Node->IsDepleted(), TEXT("NodeDepletedAfterPartial"));
		Expect(Mining->GetMiningState() == EGP_MiningState::DepositDepleted, TEXT("StateDepositDepleted"));
		Expect(!Node->HasActiveMiningSlot(Host), TEXT("DepletedReleasesSlot"));
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		Expect(Mining->GetMiningState() == EGP_MiningState::Idle, TEXT("StopMiningIdempotent"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 3:
	{
		LogStage(TEXT("FifoSpawnAndBegin"));
		// Fresh transient node for occupancy (do not reuse depleted test node).
		if (AGP_ResourceNode* OldNode = TestNodeWeak.Get())
		{
			if (IsValid(OldNode))
			{
				OldNode->Destroy();
			}
		}
		TestNodeWeak.Reset();
		SafeStopAndDestroyHost(PrimaryHostWeak);

		AGP_ResourceNode* SlotNode = SpawnTransientNode(FVector(-46000.0f, 0.0f, 100.0f));
		if (!Expect(IsValid(SlotNode), TEXT("SpawnSlotTestNode")))
		{
			Finish();
			return;
		}
		TestNodeWeak = SlotNode;
		FifoHostsWeak.Reset();
		WaitingHostWeak.Reset();
		for (int32 Index = 0; Index < 5; ++Index)
		{
			AGP_MiningDiagnosticHost* SlotHost = SpawnHostNear(SlotNode, InteractionRangeCm);
			if (!IsValid(SlotHost) || !IsValid(SlotHost->GetMiningComponent()))
			{
				Expect(false, TEXT("SpawnedFiveMiners"));
				Finish();
				return;
			}
			FifoHostsWeak.Add(SlotHost);
			SlotHost->GetMiningComponent()->BeginMining(SlotNode);
		}
		Expect(FifoHostsWeak.Num() == 5, TEXT("SpawnedFiveMiners"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 4:
	{
		LogStage(TEXT("FifoPromotion"));
		AGP_ResourceNode* SlotNode = TestNodeWeak.Get();
		if (!Expect(IsValid(SlotNode), TEXT("FifoNodeValid")))
		{
			Finish();
			return;
		}

		int32 MiningCount = 0;
		int32 WaitingCount = 0;
		WaitingHostWeak.Reset();
		TWeakObjectPtr<AGP_MiningDiagnosticHost> ActiveToStopWeak;
		for (TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak : FifoHostsWeak)
		{
			AGP_MiningDiagnosticHost* SlotHost = HostWeak.Get();
			if (!Expect(IsValid(SlotHost) && IsValid(SlotHost->GetMiningComponent()), TEXT("FifoHostStillValid")))
			{
				Finish();
				return;
			}
			const EGP_MiningState State = SlotHost->GetMiningComponent()->GetMiningState();
			if (State == EGP_MiningState::Mining)
			{
				++MiningCount;
				if (!ActiveToStopWeak.IsValid())
				{
					ActiveToStopWeak = SlotHost;
				}
			}
			else if (State == EGP_MiningState::WaitingForSlot)
			{
				++WaitingCount;
				WaitingHostWeak = SlotHost;
			}
		}
		Expect(MiningCount == 4, TEXT("FourActiveMiners"));
		Expect(WaitingCount == 1, TEXT("OneWaitingMiner"));
		Expect(SlotNode->GetActiveMinerCount() == 4 && SlotNode->GetWaitingMinerCount() == 1, TEXT("NodeOccupancyCounts"));

		AGP_MiningDiagnosticHost* ActiveToStop = ActiveToStopWeak.Get();
		AGP_MiningDiagnosticHost* WaitingHost = WaitingHostWeak.Get();
		if (!Expect(IsValid(ActiveToStop) && IsValid(WaitingHost), TEXT("FoundActiveToStop")))
		{
			Finish();
			return;
		}
		ActiveToStop->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 5:
	{
		LogStage(TEXT("FifoPromotionVerify"));
		AGP_MiningDiagnosticHost* WaitingHost = WaitingHostWeak.Get();
		if (!Expect(IsValid(WaitingHost) && IsValid(WaitingHost->GetMiningComponent()), TEXT("WaitingHostValidAfterPromote")))
		{
			Finish();
			return;
		}
		Expect(WaitingHost->GetMiningComponent()->GetMiningState() == EGP_MiningState::Mining, TEXT("WaitingPromotedToMining"));
		Expect(WaitingHost->GetMiningComponent()->IsMiningTimerActive(), TEXT("PromotedTimerStarted"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 6:
	{
		LogStage(TEXT("FifoCleanupStop"));
		for (TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak : FifoHostsWeak)
		{
			if (AGP_MiningDiagnosticHost* Host = HostWeak.Get())
			{
				if (IsValid(Host) && IsValid(Host->GetMiningComponent()))
				{
					Host->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
				}
			}
		}
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 7:
	{
		LogStage(TEXT("FifoCleanupDestroy"));
		for (TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak : FifoHostsWeak)
		{
			SafeStopAndDestroyHost(HostWeak);
		}
		FifoHostsWeak.Reset();
		WaitingHostWeak.Reset();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 8:
	{
		LogStage(TEXT("EndPlayDestroyWhileMining"));
		AGP_ResourceNode* SlotNode = TestNodeWeak.Get();
		if (!Expect(IsValid(SlotNode) && SlotNode->GetActiveMinerCount() == 0 && SlotNode->GetWaitingMinerCount() == 0,
			TEXT("FifoCleanupLeftNodeEmpty")))
		{
			Finish();
			return;
		}

		AGP_MiningDiagnosticHost* EndPlayHost = SpawnHostNear(SlotNode, InteractionRangeCm);
		if (!Expect(IsValid(EndPlayHost) && IsValid(EndPlayHost->GetMiningComponent()), TEXT("SpawnEndPlayHost")))
		{
			Finish();
			return;
		}
		PrimaryHostWeak = EndPlayHost;
		Expect(EndPlayHost->GetMiningComponent()->BeginMining(SlotNode) == EGP_BeginMiningResult::Started, TEXT("EndPlayHostBegan"));
		Expect(SlotNode->HasActiveMiningSlot(EndPlayHost), TEXT("EndPlayHostHasSlot"));
		// Destroy without prior StopMining — EndPlay must release the slot.
		EndPlayHost->Destroy();
		PrimaryHostWeak.Reset();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 9:
	{
		LogStage(TEXT("EndPlaySlotCleanupVerify"));
		AGP_ResourceNode* SlotNode = TestNodeWeak.Get();
		Expect(IsValid(SlotNode) && SlotNode->GetActiveMinerCount() == 0 && SlotNode->GetWaitingMinerCount() == 0,
			TEXT("EndPlayReleasesSlots"));
		if (IsValid(SlotNode))
		{
			SlotNode->Destroy();
		}
		TestNodeWeak.Reset();
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

void UGP_ResourceNodeEndPlayContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_ResourceNodeEndPlayContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_ResourceNodeEndPlayContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		Abort(TEXT("WorldEndPlay"));
	}
}

void UGP_ResourceNodeEndPlayContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	bCancelled = false;
	CancelReason = NAME_None;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	TerminalNoneCount = 0;
	PromotionCount = 0;
	ThreatBeforeNodeDestroy = 0.0f;
	OccupancyHostsWeak.Reset();
	WaitingHostWeak.Reset();
	HaulHostWeak.Reset();
	HaulWorkerWeak.Reset();
	HaulMainBaseWeak.Reset();
	TestNodeWeak.Reset();
	OccupancyObserveHandle.Reset();

	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_ResourceNodeEndPlayContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPMining, Log, TEXT("GP Resource.RunEndPlayContractTest Stage=Start"));
	ScheduleNext();
}

void UGP_ResourceNodeEndPlayContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalid"));
		return;
	}
	StageTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_ResourceNodeEndPlayContractTestRunner::AdvanceStage));
}

bool UGP_ResourceNodeEndPlayContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPMining, Error, TEXT("GP Resource.RunEndPlayContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPMining, Log, TEXT("GP Resource.RunEndPlayContractTest PASS: %s"), Label);
	return true;
}

void UGP_ResourceNodeEndPlayContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPMining, Error, TEXT("GP Resource.RunEndPlayContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

void UGP_ResourceNodeEndPlayContractTestRunner::SafeStopAndDestroyHost(TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak)
{
	AGP_MiningDiagnosticHost* Host = HostWeak.Get();
	if (!IsValid(Host))
	{
		HostWeak.Reset();
		return;
	}
	if (UGP_MiningComponent* Mining = Host->GetMiningComponent())
	{
		if (IsValid(Mining))
		{
			Mining->StopMining(EGP_MiningStopReason::ManualStop);
		}
	}
	Host->Destroy();
	HostWeak.Reset();
}

AGP_ResourceNode* UGP_ResourceNodeEndPlayContractTestRunner::SpawnTransientNode(const FVector& Location) const
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	return World->SpawnActor<AGP_ResourceNode>(
		AGP_ResourceNode::StaticClass(),
		Location,
		FRotator::ZeroRotator,
		Params);
}

AGP_MiningDiagnosticHost* UGP_ResourceNodeEndPlayContractTestRunner::SpawnHostNear(AGP_ResourceNode* Node, float RangeCm) const
{
	return GPMiningDebug::SpawnHostNearNode(WorldWeak.Get(), Node, RangeCm);
}

void UGP_ResourceNodeEndPlayContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	UnbindWorldCleanup();
	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
	}

	if (AGP_ResourceNode* Node = TestNodeWeak.Get())
	{
		if (IsValid(Node) && OccupancyObserveHandle.IsValid())
		{
			Node->GetOnMinerSlotStateChanged().Remove(OccupancyObserveHandle);
		}
	}
	OccupancyObserveHandle.Reset();

	for (TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak : OccupancyHostsWeak)
	{
		SafeStopAndDestroyHost(HostWeak);
	}
	OccupancyHostsWeak.Reset();
	SafeStopAndDestroyHost(HaulHostWeak);
	WaitingHostWeak.Reset();

	if (AGP_Worker* Worker = HaulWorkerWeak.Get())
	{
		if (IsValid(Worker))
		{
			Worker->Destroy();
		}
	}
	HaulWorkerWeak.Reset();
	if (AGP_MainBase* Base = HaulMainBaseWeak.Get())
	{
		if (IsValid(Base))
		{
			Base->Destroy();
		}
	}
	HaulMainBaseWeak.Reset();

	if (AGP_ResourceNode* Node = TestNodeWeak.Get())
	{
		if (IsValid(Node))
		{
			Node->Destroy();
		}
	}
	TestNodeWeak.Reset();

	UE_LOG(LogGPMining, Log, TEXT("GP Resource.RunEndPlayContractTest: Complete Failures=%d"), Failures);
	GPContractTestCoordinator::Release(ExecutionId, Failures, bCancelled, *CancelReason.ToString());
	RemoveFromRoot();
	GPMiningDebug::GActiveEndPlayContractTestRunner.Reset();
	WorldWeak.Reset();
}

void UGP_ResourceNodeEndPlayContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (bFinished || !GPContractTestCoordinator::IsTokenActive(ExecutionId))
	{
		return;
	}
	if (GPContractTestCoordinator::IsWorldTearingDown(World))
	{
		bCancelled = true;
		CancelReason = FName(TEXT("WorldEndPlay"));
		Abort(TEXT("WorldEndPlay"));
		return;
	}
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalidDuringStage"));
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		UE_LOG(LogGPMining, Log, TEXT("GP Resource.RunEndPlayContractTest Stage=ResourceNodeEndPlayWithActiveAndWaitingMiners"));
		AGP_ResourceNode* Node = SpawnTransientNode(FVector(-47000.0f, 0.0f, 100.0f));
		if (!Expect(IsValid(Node), TEXT("SpawnEndPlayOccupancyNode")))
		{
			Finish();
			return;
		}
		TestNodeWeak = Node;
		if (const UGP_ResourceDefinition* Def = Node->ResolveResourceDefinition(true))
		{
			InteractionRangeCm = Def->InteractionRangeCm;
		}

		OccupancyHostsWeak.Reset();
		for (int32 Index = 0; Index < 5; ++Index)
		{
			AGP_MiningDiagnosticHost* Host = SpawnHostNear(Node, InteractionRangeCm);
			if (!Expect(IsValid(Host) && IsValid(Host->GetMiningComponent()), TEXT("SpawnOccupancyMiner")))
			{
				Finish();
				return;
			}
			OccupancyHostsWeak.Add(Host);
			Expect(Host->GetMiningComponent()->BeginMining(Node) == EGP_BeginMiningResult::Started
				|| Host->GetMiningComponent()->IsWaitingForSlot()
				|| Host->GetMiningComponent()->IsMining(),
				TEXT("OccupancyMinerBeganOrQueued"));
		}
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Node), TEXT("OccupancyNodeStillValid")))
		{
			Finish();
			return;
		}

		int32 MiningCount = 0;
		int32 WaitingCount = 0;
		WaitingHostWeak.Reset();
		for (TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak : OccupancyHostsWeak)
		{
			AGP_MiningDiagnosticHost* Host = HostWeak.Get();
			if (!Expect(IsValid(Host) && IsValid(Host->GetMiningComponent()), TEXT("OccupancyHostAlive")))
			{
				Finish();
				return;
			}
			const EGP_MiningState State = Host->GetMiningComponent()->GetMiningState();
			if (State == EGP_MiningState::Mining)
			{
				++MiningCount;
			}
			else if (State == EGP_MiningState::WaitingForSlot)
			{
				++WaitingCount;
				WaitingHostWeak = Host;
			}
		}
		Expect(MiningCount == 4, TEXT("FourActiveMinersBeforeNodeDestroy"));
		Expect(WaitingCount == 1, TEXT("OneWaitingMinerBeforeNodeDestroy"));
		Expect(Node->GetActiveMinerCount() == 4 && Node->GetWaitingMinerCount() == 1, TEXT("NodeOccupancyCountsBeforeDestroy"));
		Expect(IsValid(WaitingHostWeak.Get()), TEXT("WaitingHostCaptured"));

		TerminalNoneCount = 0;
		PromotionCount = 0;
		OccupancyObserveHandle = Node->GetOnMinerSlotStateChanged().AddLambda(
			[this](AActor* /*Miner*/, EGP_MinerOccupancyState OldState, EGP_MinerOccupancyState NewState)
			{
				if (OldState == EGP_MinerOccupancyState::Waiting && NewState == EGP_MinerOccupancyState::Active)
				{
					++PromotionCount;
				}
				if (NewState == EGP_MinerOccupancyState::None
					&& (OldState == EGP_MinerOccupancyState::Active || OldState == EGP_MinerOccupancyState::Waiting))
				{
					++TerminalNoneCount;
				}
			});

		// Destroy while 4 Active + 1 Waiting — must not ensure on ranged-for mutation.
		Node->Destroy();
		TestNodeWeak.Reset();
		OccupancyObserveHandle.Reset();

		Expect(PromotionCount == 0, TEXT("WaitingMinerNotPromotedOnNodeEndPlay"));
		Expect(TerminalNoneCount == 5, TEXT("AllMinersReceivedNoneTerminal"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		for (TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak : OccupancyHostsWeak)
		{
			AGP_MiningDiagnosticHost* Host = HostWeak.Get();
			if (!Expect(IsValid(Host) && IsValid(Host->GetMiningComponent()), TEXT("HostAliveAfterNodeEndPlay")))
			{
				Finish();
				return;
			}
			UGP_MiningComponent* Mining = Host->GetMiningComponent();
			Expect(Mining->GetMiningState() == EGP_MiningState::Invalid, TEXT("MinerTerminalInvalidAfterNodeEndPlay"));
			Expect(Mining->GetLastStopReason() == EGP_MiningStopReason::TargetEndPlay, TEXT("MinerStopReasonTargetEndPlay"));
			Expect(!Mining->IsMiningTimerActive(), TEXT("MiningTimerOffAfterNodeEndPlay"));
			Expect(Mining->GetCurrentResourceNode() == nullptr, TEXT("MinerNodeRefCleared"));
		}

		AGP_MiningDiagnosticHost* WaitingHost = WaitingHostWeak.Get();
		Expect(IsValid(WaitingHost)
			&& WaitingHost->GetMiningComponent()->GetLastStopReason() == EGP_MiningStopReason::TargetEndPlay,
			TEXT("WaitingMinerTerminalNotPromoted"));

		for (TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak : OccupancyHostsWeak)
		{
			SafeStopAndDestroyHost(HostWeak);
		}
		OccupancyHostsWeak.Reset();
		WaitingHostWeak.Reset();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 3:
	{
		UE_LOG(LogGPMining, Log, TEXT("GP Resource.RunEndPlayContractTest Stage=ResourceNodeEndPlayDuringHaulLoop"));

		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && IsValid(Scenario.Worker) && IsValid(Scenario.ResourceNode) && IsValid(Scenario.MainBase),
			TEXT("HaulLoopScenarioSpawnOk")))
		{
			Finish();
			return;
		}

		TestNodeWeak = Scenario.ResourceNode;
		HaulWorkerWeak = Scenario.Worker;
		HaulMainBaseWeak = Scenario.MainBase;

		AGP_Worker* Worker = Scenario.Worker;
		AGP_ResourceNode* Node = Scenario.ResourceNode;
		FGP_UnitCommand Command;
		Command.CommandTag = FGPGameplayTags::Get().Command_Mine;
		Command.TargetActor = Node;
		Command.TargetLocation = Node->GetActorLocation();
		Command.bQueue = false;
		Worker->ReceiveCommand(Command);

		UGP_MiningComponent* Mining = Worker->GetMiningComponent();
		UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
		if (!Expect(IsValid(Mining) && IsValid(Cargo), TEXT("HaulLoopWorkerComponents")))
		{
			Finish();
			return;
		}

		// Place in range and force a partial cargo cycle (not CargoFull).
		Worker->SetActorLocation(Node->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f),
			false, nullptr, ETeleportType::TeleportPhysics);
		if (Mining->GetMiningState() != EGP_MiningState::Mining
			&& Mining->GetMiningState() != EGP_MiningState::WaitingForSlot)
		{
			Mining->BeginMining(Node);
		}
		Mining->DebugForceExecuteMiningCycle();
		Expect(Cargo->GetCurrentCargoAmount() > KINDA_SMALL_NUMBER, TEXT("HaulLoopPartialCargoBeforeNodeDestroy"));

		ThreatBeforeNodeDestroy = 0.0f;
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			ThreatBeforeNodeDestroy = GS->GetFerroniteThreatValueForTeam(Worker->GetTeamId());
		}

		Node->Destroy();
		TestNodeWeak.Reset();

		Expect(Mining->GetLastStopReason() == EGP_MiningStopReason::TargetEndPlay
			|| Mining->GetCurrentResourceNode() == nullptr,
			TEXT("HaulLoopMiningStoppedOnNodeDestroy"));
		Expect(!Mining->IsMiningTimerActive(), TEXT("HaulLoopMiningTimerOff"));

		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			const float ThreatAfter = GS->GetFerroniteThreatValueForTeam(Worker->GetTeamId());
			Expect(FMath::IsNearlyEqual(ThreatAfter, ThreatBeforeNodeDestroy, 0.01f),
				TEXT("HaulLoopNoFalseThreatTransaction"));
		}

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 4:
	{
		AGP_Worker* Worker = HaulWorkerWeak.Get();
		AGP_MainBase* Base = HaulMainBaseWeak.Get();
		if (IsValid(Worker))
		{
			UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
			UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
			const float CargoAmount = IsValid(Cargo) ? Cargo->GetCurrentCargoAmount() : 0.0f;
			// Zero cargo must not start haul; partial cargo may haul without return-to-deposit.
			if (CargoAmount <= KINDA_SMALL_NUMBER)
			{
				Expect(Cmd == nullptr || !Cmd->IsHaulActive(), TEXT("HaulLoopNoHaulWithZeroCargo"));
			}
			Expect(IsValid(Worker->GetMiningComponent())
				&& Worker->GetMiningComponent()->GetCurrentResourceNode() == nullptr,
				TEXT("HaulLoopNoStaleNodeRef"));
		}

		if (IsValid(Worker))
		{
			Worker->Destroy();
		}
		HaulWorkerWeak.Reset();
		if (IsValid(Base))
		{
			Base->Destroy();
		}
		HaulMainBaseWeak.Reset();

		Expect(true, TEXT("ResourceNodeEndPlayDuringHaulLoop"));
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}
#else
void UGP_MiningContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}

void UGP_MiningContractTestRunner::Start(UWorld* InWorld)
{
	(void)InWorld;
}

void UGP_MiningContractTestRunner::Abort(const TCHAR* Reason)
{
	(void)Reason;
}

void UGP_MiningContractTestRunner::ScheduleNext()
{
}

void UGP_MiningContractTestRunner::AdvanceStage()
{
}

bool UGP_MiningContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}

void UGP_MiningContractTestRunner::LogStage(const TCHAR* StageName) const
{
	(void)StageName;
}

void UGP_MiningContractTestRunner::Finish()
{
	bFinished = true;
}

void UGP_MiningContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}

void UGP_MiningContractTestRunner::UnbindWorldCleanup()
{
}

AGP_MiningDiagnosticHost* UGP_MiningContractTestRunner::SpawnHostNear(AGP_ResourceNode* Node, float RangeCm) const
{
	(void)Node;
	(void)RangeCm;
	return nullptr;
}

AGP_ResourceNode* UGP_MiningContractTestRunner::SpawnTransientNode(const FVector& Location) const
{
	(void)Location;
	return nullptr;
}

void UGP_MiningContractTestRunner::SafeStopAndDestroyHost(TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak)
{
	HostWeak.Reset();
}

void UGP_ResourceNodeEndPlayContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}

void UGP_ResourceNodeEndPlayContractTestRunner::Start(UWorld* InWorld)
{
	(void)InWorld;
}

void UGP_ResourceNodeEndPlayContractTestRunner::ScheduleNext() {}
void UGP_ResourceNodeEndPlayContractTestRunner::AdvanceStage() {}
bool UGP_ResourceNodeEndPlayContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_ResourceNodeEndPlayContractTestRunner::Abort(const TCHAR* Reason)
{
	(void)Reason;
}
void UGP_ResourceNodeEndPlayContractTestRunner::Finish()
{
	bFinished = true;
}
void UGP_ResourceNodeEndPlayContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_ResourceNodeEndPlayContractTestRunner::UnbindWorldCleanup() {}
AGP_ResourceNode* UGP_ResourceNodeEndPlayContractTestRunner::SpawnTransientNode(const FVector& Location) const
{
	(void)Location;
	return nullptr;
}
AGP_MiningDiagnosticHost* UGP_ResourceNodeEndPlayContractTestRunner::SpawnHostNear(AGP_ResourceNode* Node, float RangeCm) const
{
	(void)Node;
	(void)RangeCm;
	return nullptr;
}
void UGP_ResourceNodeEndPlayContractTestRunner::SafeStopAndDestroyHost(TWeakObjectPtr<AGP_MiningDiagnosticHost>& HostWeak)
{
	HostWeak.Reset();
}
#endif
