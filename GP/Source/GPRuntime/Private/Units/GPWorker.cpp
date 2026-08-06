// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#include "Buildings/GPMainBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Visual/GPResourceNodeVisualComponent.h"
#include "Command/GPUnitCommand.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceLoopDiagnostics.h"
#include "Resources/GPResourceNode.h"
#include "Resources/GPStorageComponent.h"
#include "Settings/GPResourceGameplaySettings.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPUnitCommandComponent.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGPWorker, Log, All);

void UGP_CargoVisualStateProbe::HandleCargoVisualStateChanged(
	bool bVisible,
	float FillNormalized,
	float CurrentAmount,
	float Capacity)
{
	++EventCount;
	bLastVisible = bVisible;
	LastFill = FillNormalized;
	LastAmount = CurrentAmount;
	LastCapacity = Capacity;
}

void UGP_MiningEffectStateProbe::HandleMiningEffectStateChanged(
	bool bEffectActive,
	EGP_MiningState PreviousState,
	EGP_MiningState NewState,
	EGP_MiningStopReason Reason)
{
	++EventCount;
	bLastEffectActive = bEffectActive;
	LastPreviousState = PreviousState;
	LastNewState = NewState;
	LastReason = Reason;
}

AGP_Worker::AGP_Worker()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->InitCapsuleSize(42.0f, 88.0f);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CapsuleComponent->SetGenerateOverlapEvents(false);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->SetSimulatePhysics(false);

	PresentationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot"));
	PresentationRoot->SetupAttachment(CapsuleComponent);
	PresentationRoot->SetCanEverAffectNavigation(false);

	CargoVisualAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("CargoVisualAnchor"));
	CargoVisualAnchor->SetupAttachment(PresentationRoot);
	CargoVisualAnchor->SetCanEverAffectNavigation(false);

	MiningEffectAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("MiningEffectAnchor"));
	MiningEffectAnchor->SetupAttachment(PresentationRoot);
	MiningEffectAnchor->SetCanEverAffectNavigation(false);

	CargoComponent = CreateDefaultSubobject<UGP_CargoComponent>(TEXT("CargoComponent"));
	MiningComponent = CreateDefaultSubobject<UGP_MiningComponent>(TEXT("MiningComponent"));

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	CapabilityTags.Reset();
	if (GPTags.Capability_Selectable.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Capability_Selectable);
	}
	if (GPTags.Capability_Inspectable.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Capability_Inspectable);
	}
	if (GPTags.Selection_Type_Unit.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Selection_Type_Unit);
	}
	if (GPTags.Unit_Type_Worker.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Unit_Type_Worker);
	}
}

void AGP_Worker::BeginPlay()
{
	Super::BeginPlay();
	BindCargoVisualEvents();
	SyncCargoVisualState();
	BindMiningEffectEvents();
	SyncMiningEffectState();
}

void AGP_Worker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindMiningEffectEvents();
	UnbindCargoVisualEvents();
	Super::EndPlay(EndPlayReason);
}

UGP_CargoComponent* AGP_Worker::GetCargoComponent() const
{
	return CargoComponent;
}

UGP_MiningComponent* AGP_Worker::GetMiningComponent() const
{
	return MiningComponent;
}

UCapsuleComponent* AGP_Worker::GetCapsuleComponent() const
{
	return CapsuleComponent;
}

USceneComponent* AGP_Worker::GetPresentationRoot() const
{
	return PresentationRoot;
}

USceneComponent* AGP_Worker::GetCargoVisualAnchor() const
{
	return CargoVisualAnchor;
}

USceneComponent* AGP_Worker::GetMiningEffectAnchor() const
{
	return MiningEffectAnchor;
}

float AGP_Worker::GetResourceSearchRadiusCm() const
{
	if (const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get())
	{
		return Settings->ResourceSearchRadiusCm;
	}
	return 3000.0f;
}

float AGP_Worker::GetMaxResourcePathLengthCm() const
{
	if (const UGP_ResourceGameplaySettings* Settings = UGP_ResourceGameplaySettings::Get())
	{
		return Settings->MaxResourcePathLengthCm;
	}
	return 6000.0f;
}

float AGP_Worker::GetCargoFillNormalized() const
{
	return IsValid(CargoComponent) ? CargoComponent->GetFillRatio() : 0.0f;
}

bool AGP_Worker::HasCargoForVisual() const
{
	return IsValid(CargoComponent) && CargoComponent->GetCurrentCargoAmount() > KINDA_SMALL_NUMBER;
}

void AGP_Worker::BindCargoVisualEvents()
{
	if (bCargoVisualEventsBound || !IsValid(CargoComponent))
	{
		return;
	}

	CargoComponent->OnCargoAmountChanged.AddDynamic(this, &AGP_Worker::HandleCargoAmountChanged);
	bCargoVisualEventsBound = true;
}

void AGP_Worker::UnbindCargoVisualEvents()
{
	if (!bCargoVisualEventsBound || !IsValid(CargoComponent))
	{
		bCargoVisualEventsBound = false;
		return;
	}

	CargoComponent->OnCargoAmountChanged.RemoveDynamic(this, &AGP_Worker::HandleCargoAmountChanged);
	bCargoVisualEventsBound = false;
}

void AGP_Worker::HandleCargoAmountChanged(float PreviousAmount, float NewAmount, float Capacity, float Delta)
{
	(void)PreviousAmount;
	(void)NewAmount;
	(void)Capacity;
	(void)Delta;
	SyncCargoVisualState();
}

void AGP_Worker::SyncCargoVisualState()
{
	const float CurrentAmount = IsValid(CargoComponent) ? CargoComponent->GetCurrentCargoAmount() : 0.0f;
	const float Capacity = IsValid(CargoComponent) ? CargoComponent->GetCargoCapacity() : 0.0f;
	const float FillNormalized = IsValid(CargoComponent) ? CargoComponent->GetFillRatio() : 0.0f;
	const bool bVisible = CurrentAmount > KINDA_SMALL_NUMBER;
	OnCargoVisualStateChanged.Broadcast(bVisible, FillNormalized, CurrentAmount, Capacity);
}

void AGP_Worker::BindMiningEffectEvents()
{
	if (bMiningEffectEventsBound || !IsValid(MiningComponent))
	{
		return;
	}

	MiningComponent->OnMiningStateChanged.AddDynamic(this, &AGP_Worker::HandleMiningStateChanged);
	bMiningEffectEventsBound = true;
}

void AGP_Worker::UnbindMiningEffectEvents()
{
	if (!bMiningEffectEventsBound || !IsValid(MiningComponent))
	{
		bMiningEffectEventsBound = false;
		return;
	}

	MiningComponent->OnMiningStateChanged.RemoveDynamic(this, &AGP_Worker::HandleMiningStateChanged);
	bMiningEffectEventsBound = false;
}

void AGP_Worker::HandleMiningStateChanged(
	EGP_MiningState PreviousState,
	EGP_MiningState NewState,
	EGP_MiningStopReason Reason)
{
	const bool bEffectActive = (NewState == EGP_MiningState::Mining);
	OnMiningEffectStateChanged.Broadcast(bEffectActive, PreviousState, NewState, Reason);
}

void AGP_Worker::SyncMiningEffectState()
{
	const EGP_MiningState CurrentState =
		IsValid(MiningComponent) ? MiningComponent->GetMiningState() : EGP_MiningState::Idle;
	const EGP_MiningStopReason Reason =
		IsValid(MiningComponent) ? MiningComponent->GetLastStopReason() : EGP_MiningStopReason::None;
	const bool bEffectActive = (CurrentState == EGP_MiningState::Mining);
	OnMiningEffectStateChanged.Broadcast(bEffectActive, CurrentState, CurrentState, Reason);
}

EGP_WorkerActivityState AGP_Worker::GetWorkerActivityState() const
{
	if (const UGP_UnitCommandComponent* Commands = GetUnitCommandComponent())
	{
		switch (Commands->GetHaulExecutionState())
		{
		case EGP_HaulExecutionState::ReturningToBase:
			return EGP_WorkerActivityState::ReturningToBase;
		case EGP_HaulExecutionState::DroppingOff:
			return EGP_WorkerActivityState::DroppingOff;
		case EGP_HaulExecutionState::ReturningToDeposit:
			return EGP_WorkerActivityState::ReturningToDeposit;
		case EGP_HaulExecutionState::WaitingForStorage:
			return EGP_WorkerActivityState::WaitingForStorage;
		case EGP_HaulExecutionState::Failed:
			return EGP_WorkerActivityState::CommandFailed;
		default:
			break;
		}

		if (Commands->GetMineExecutionState() == EGP_MineExecutionState::WaitingForResource)
		{
			return EGP_WorkerActivityState::WaitingForResource;
		}

		if (Commands->GetMineExecutionState() == EGP_MineExecutionState::Approaching)
		{
			return EGP_WorkerActivityState::MovingToMine;
		}
	}

	const UGP_MiningComponent* Mining = GetMiningComponent();
	if (IsValid(Mining))
	{
		switch (Mining->GetMiningState())
		{
		case EGP_MiningState::CargoFull:
			return EGP_WorkerActivityState::CargoFull;
		case EGP_MiningState::DepositDepleted:
			return EGP_WorkerActivityState::DepositDepleted;
		case EGP_MiningState::WaitingForSlot:
			return EGP_WorkerActivityState::WaitingForMiningSlot;
		case EGP_MiningState::Mining:
			return EGP_WorkerActivityState::Mining;
		default:
			break;
		}
	}

	return EGP_WorkerActivityState::Idle;
}

bool AGP_Worker::ValidateWorkerContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	if (!IsValid(CargoComponent))
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrCargo", "Worker requires CargoComponent."));
	}
	if (!IsValid(MiningComponent))
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrMining", "Worker requires MiningComponent."));
	}
	if (!IsValid(PresentationRoot))
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrPresentationRoot", "Worker requires PresentationRoot."));
	}
	if (!IsValid(CargoVisualAnchor))
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrCargoAnchor", "Worker requires CargoVisualAnchor."));
	}
	if (!IsValid(MiningEffectAnchor))
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrMiningEffectAnchor", "Worker requires MiningEffectAnchor."));
	}
	if (IsValid(PresentationRoot) && PresentationRoot->GetAttachParent() != CapsuleComponent)
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrPresentationAttach", "PresentationRoot must attach to Capsule."));
	}
	if (IsValid(CargoVisualAnchor) && CargoVisualAnchor->GetAttachParent() != PresentationRoot)
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrCargoAnchorAttach", "CargoVisualAnchor must attach to PresentationRoot."));
	}
	if (IsValid(MiningEffectAnchor) && MiningEffectAnchor->GetAttachParent() != PresentationRoot)
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrMiningEffectAttach",
			"MiningEffectAnchor must attach to PresentationRoot."));
	}
	if (GetUnitMovementComponent() == nullptr)
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrMovement", "Worker requires MovementComponent."));
	}
	if (FindComponentByClass<UActorComponent>() != nullptr)
	{
		// No dedicated Combat/Targeting components exist in this project; ensure Cargo/Mining owners.
	}

	if (IsValid(CargoComponent) && CargoComponent->GetOwner() != this)
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrCargoOwner", "CargoComponent owner must be Worker."));
	}
	if (IsValid(MiningComponent) && MiningComponent->GetOwner() != this)
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrMiningOwner", "MiningComponent owner must be Worker."));
	}

	if (const UGP_UnitCommandComponent* Commands = GetUnitCommandComponent())
	{
		if (Commands->GetMineExecutionState() == EGP_MineExecutionState::Approaching)
		{
			if (IsValid(MiningComponent) && MiningComponent->IsMiningTimerActive())
			{
				OutErrors.Add(NSLOCTEXT("GPWorker", "ErrTimerWhileMoving", "Mining timer must be inactive while MovingToMine."));
			}
			if (GetUnitMovementComponent() != nullptr && !GetUnitMovementComponent()->IsMoving())
			{
				OutWarnings.Add(NSLOCTEXT("GPWorker", "WarnApproachNotMoving", "Mine approach state without active movement."));
			}
		}

		if (Commands->GetMineExecutionState() == EGP_MineExecutionState::Active)
		{
			if (GetUnitMovementComponent() != nullptr && GetUnitMovementComponent()->IsMoving())
			{
				OutErrors.Add(NSLOCTEXT("GPWorker", "ErrMoveWhileMining", "Movement must be stopped while Mining/Waiting."));
			}
		}
	}

	if (IsValid(CargoComponent) && IsValid(MiningComponent)
		&& MiningComponent->GetMiningState() == EGP_MiningState::CargoFull
		&& !CargoComponent->IsFull())
	{
		OutErrors.Add(NSLOCTEXT("GPWorker", "ErrCargoFullMismatch", "CargoFull mining state requires full cargo."));
	}

	if (!GetCapabilityTags().HasTagExact(FGPGameplayTags::Get().Unit_Type_Worker))
	{
		OutWarnings.Add(NSLOCTEXT("GPWorker", "WarnWorkerTag", "Worker missing GP.Unit.Type.Worker capability tag."));
	}

	// UGP_UnitDefinition Worker asset is deferred (roadmap / GP unit-definition pass).
	// Do not emit a compile/data-validation warning for its absence in S28P1.

	return OutErrors.Num() == 0;
}

#if WITH_EDITOR
EDataValidationResult AGP_Worker::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const bool bOk = ValidateWorkerContract(Errors, Warnings);
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

#if !UE_BUILD_SHIPPING
namespace GPWorkerDebug
{
	TWeakObjectPtr<UGP_WorkerContractTestRunner> GActiveWorkerContractTest;

	static const TCHAR* ActivityToString(EGP_WorkerActivityState State)
	{
		switch (State)
		{
		case EGP_WorkerActivityState::Idle: return TEXT("Idle");
		case EGP_WorkerActivityState::MovingToMine: return TEXT("MovingToMine");
		case EGP_WorkerActivityState::WaitingForMiningSlot: return TEXT("WaitingForMiningSlot");
		case EGP_WorkerActivityState::Mining: return TEXT("Mining");
		case EGP_WorkerActivityState::CargoFull: return TEXT("CargoFull");
		case EGP_WorkerActivityState::DepositDepleted: return TEXT("DepositDepleted");
		case EGP_WorkerActivityState::ReturningToBase: return TEXT("ReturningToBase");
		case EGP_WorkerActivityState::DroppingOff: return TEXT("DroppingOff");
		case EGP_WorkerActivityState::ReturningToDeposit: return TEXT("ReturningToDeposit");
		case EGP_WorkerActivityState::WaitingForStorage: return TEXT("WaitingForStorage");
		case EGP_WorkerActivityState::CommandFailed: return TEXT("CommandFailed");
		default: return TEXT("Unknown");
		}
	}

	static const TCHAR* HaulStateToString(EGP_HaulExecutionState State)
	{
		switch (State)
		{
		case EGP_HaulExecutionState::Idle: return TEXT("Idle");
		case EGP_HaulExecutionState::ReturningToBase: return TEXT("ReturningToBase");
		case EGP_HaulExecutionState::DroppingOff: return TEXT("DroppingOff");
		case EGP_HaulExecutionState::ReturningToDeposit: return TEXT("ReturningToDeposit");
		case EGP_HaulExecutionState::WaitingForStorage: return TEXT("WaitingForStorage");
		case EGP_HaulExecutionState::Failed: return TEXT("Failed");
		default: return TEXT("Unknown");
		}
	}

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

	static AGP_ResourceNode* FindNode(UWorld* World, const FString& OptionalName)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		AGP_ResourceNode* Fallback = nullptr;
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			AGP_ResourceNode* Node = *It;
			if (!IsValid(Node))
			{
				continue;
			}
			if (!OptionalName.IsEmpty() && Node->GetName() == OptionalName)
			{
				return Node;
			}
			if (Fallback == nullptr)
			{
				Fallback = Node;
			}
		}
		return OptionalName.IsEmpty() ? Fallback : nullptr;
	}

	static AGP_Worker* FindWorker(UWorld* World, const FString& OptionalName)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		AGP_Worker* Fallback = nullptr;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			AGP_Worker* Worker = *It;
			if (!IsValid(Worker))
			{
				continue;
			}
			if (!OptionalName.IsEmpty() && Worker->GetName() == OptionalName)
			{
				return Worker;
			}
			if (Fallback == nullptr)
			{
				Fallback = Worker;
			}
		}
		return OptionalName.IsEmpty() ? Fallback : nullptr;
	}

	static AGP_Worker* SpawnWorkerAt(UWorld* World, const FVector& Location)
	{
		if (!IsValid(World))
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(AGP_Worker::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (IsValid(Worker) && !Worker->GetActorLocation().Equals(Location, 1.0f))
		{
			Worker->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
		}
		return Worker;
	}

	static void IssueMine(AGP_Worker* Worker, AGP_ResourceNode* Node)
	{
		if (!IsValid(Worker) || !IsValid(Node))
		{
			return;
		}
		FGP_UnitCommand Command;
		Command.CommandTag = FGPGameplayTags::Get().Command_Mine;
		Command.TargetActor = Node;
		Command.TargetLocation = Node->GetActorLocation();
		Command.bQueue = false;
		Worker->ReceiveCommand(Command);
	}

	static void IssueMove(AGP_Worker* Worker, const FVector& Location)
	{
		if (!IsValid(Worker))
		{
			return;
		}
		FGP_UnitCommand Command;
		Command.CommandTag = FGPGameplayTags::Get().Command_Move;
		Command.TargetActor = nullptr;
		Command.TargetLocation = Location;
		Command.bQueue = false;
		Worker->ReceiveCommand(Command);
	}

	static void WorkerSpawnDiagnostic(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.SpawnDiagnostic: missing world or client"));
			return;
		}

		int32 TeamId = 1;
		if (Args.Num() > 0)
		{
			int32 ParsedTeam = 1;
			if (LexTryParseString(ParsedTeam, *Args[0]) && ParsedTeam >= 1)
			{
				TeamId = ParsedTeam;
			}
		}

		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, TeamId, GPContractTestCoordinator::OwnerTagOperator);
		if (!Scenario.bOk)
		{
			UE_LOG(LogGPWorker, Error,
				TEXT("GP Worker.SpawnDiagnostic: FAILED TeamId=%d Error=%s PathFailure=%s — use gp.Resource.SpawnDiagnosticScenario %d"),
				TeamId,
				*Scenario.Error,
				*Scenario.PathFailureReason,
				TeamId);
			return;
		}

		UE_LOG(LogGPWorker, Log,
			TEXT("GP Worker.SpawnDiagnostic: Worker=%s TeamId=%d Node=%s MainBase=%s WorkerLoc=%s NodeLoc=%s BaseLoc=%s ReadyForHaulingTest=%s SuggestedCommand=gp.Worker.CommandMine %s %s"),
			*GetNameSafe(Scenario.Worker),
			Scenario.Worker != nullptr ? Scenario.Worker->GetTeamId() : -1,
			*GetNameSafe(Scenario.ResourceNode),
			*GetNameSafe(Scenario.MainBase),
			Scenario.Worker != nullptr ? *Scenario.Worker->GetActorLocation().ToCompactString() : TEXT("n/a"),
			Scenario.ResourceNode != nullptr ? *Scenario.ResourceNode->GetActorLocation().ToCompactString() : TEXT("n/a"),
			Scenario.MainBase != nullptr ? *Scenario.MainBase->GetActorLocation().ToCompactString() : TEXT("n/a"),
			Scenario.bReadyForHaulingTest ? TEXT("true") : TEXT("false"),
			*GetNameSafe(Scenario.Worker),
			*GetNameSafe(Scenario.ResourceNode));
	}

	static void WorkerInspect(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}
		AGP_Worker* Worker = FindWorker(World, Args.Num() > 0 ? Args[0] : FString());
		if (!IsValid(Worker))
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.Inspect: no worker found"));
			return;
		}

		UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
		UGP_MiningComponent* Mining = Worker->GetMiningComponent();
		UGP_MovementComponent* Movement = Worker->GetUnitMovementComponent();
		UGP_UnitCommandComponent* Commands = Worker->GetUnitCommandComponent();
		AGP_ResourceNode* MineTarget = Commands != nullptr ? Commands->GetMineTarget() : nullptr;
		if (!IsValid(MineTarget) && IsValid(Mining))
		{
			MineTarget = Mining->GetCurrentResourceNode();
		}

		float Distance = -1.0f;
		float Range = IsValid(Mining) ? Mining->GetInteractionRangeCm() : 0.0f;
		bool bInRange = false;
		if (IsValid(MineTarget))
		{
			Distance = FVector::Dist(Worker->GetActorLocation(), MineTarget->GetActorLocation());
			if (!(Range > 0.0f))
			{
				if (const UGP_ResourceDefinition* Def = MineTarget->ResolveResourceDefinition(true))
				{
					Range = Def->InteractionRangeCm;
				}
			}
			bInRange = Range > 0.0f && Distance <= Range;
		}

		TArray<FText> Errors;
		TArray<FText> Warnings;
		const bool bValid = Worker->ValidateWorkerContract(Errors, Warnings);
		const FGP_StoredUnitCommand* Held = Commands != nullptr ? Commands->GetHeldCommand() : nullptr;

		UE_LOG(LogGPWorker, Log,
			TEXT("GP Worker.Inspect: Owner=%s Path=%s Class=%s Role=%s NetMode=%s HasAuthority=%s TeamId=%d Selectable=%s Activity=%s HeldTag=%s HeldSerial=%u PendingMineNode=%s MiningState=%s MiningStop=%s Cargo=%.1f/%.1f Remaining=%.1f Distance=%.1f InteractionRangeCm=%.1f InRange=%s Moving=%s MoveDest=%s ApproachDestination=%s ApproachDesiredNodeDistance=%.1f ApproachSafetyMargin=%.1f ApproachAttempt=%d PredictedWorstCaseDistance=%.1f LastArrivalDistance=%.1f LastArrivalRangeError=%.1f MineExec=%s ActiveMineSerial=%u HaulExec=%s ActiveHaulSerial=%u LastHaulDeposit=%s HaulMainBase=%s HaulAccepted=%.1f HaulRejected=%.1f HaulThreatDelta=%.3f HaulDropOffRange=%.1f HaulApproachDest=%s HaulApproachAttempt=%d ReturnToDeposit=%s HasActiveSlot=%s WaitingSlot=%s MiningTimer=%s WorkerTick=%s MoveTick=%s CargoTick=%s MiningTick=%s ValidationOk=%s Errors=%d Warnings=%d Caps=%s"),
			*Worker->GetName(),
			*Worker->GetPathName(),
			*Worker->GetClass()->GetName(),
			RoleToString(Worker->GetLocalRole()),
			NetModeToString(Worker->GetNetMode()),
			Worker->HasAuthority() ? TEXT("true") : TEXT("false"),
			Worker->GetTeamId(),
			Worker->IsGameplaySelectable() ? TEXT("true") : TEXT("false"),
			ActivityToString(Worker->GetWorkerActivityState()),
			Held != nullptr ? *Held->CommandTag.ToString() : TEXT("none"),
			Held != nullptr ? Held->CommandSerial : 0u,
			*GetNameSafe(MineTarget),
			IsValid(Mining) ? *UEnum::GetValueAsString(Mining->GetMiningState()) : TEXT("none"),
			IsValid(Mining) ? *UEnum::GetValueAsString(Mining->GetLastStopReason()) : TEXT("none"),
			IsValid(Cargo) ? Cargo->GetCurrentCargoAmount() : -1.0f,
			IsValid(Cargo) ? Cargo->GetCargoCapacity() : -1.0f,
			IsValid(Cargo) ? Cargo->GetRemainingCapacity() : -1.0f,
			Distance,
			Range,
			bInRange ? TEXT("true") : TEXT("false"),
			Movement != nullptr && Movement->IsMoving() ? TEXT("true") : TEXT("false"),
			Movement != nullptr ? *Movement->GetMoveDestination().ToCompactString() : TEXT("none"),
			Commands != nullptr ? *Commands->GetMineApproachDestination().ToCompactString() : TEXT("none"),
			Commands != nullptr ? Commands->GetMineApproachDesiredNodeDistance() : -1.0f,
			Commands != nullptr ? Commands->GetMineApproachSafetyMarginCm() : -1.0f,
			Commands != nullptr ? Commands->GetMineApproachAttempt() : -1,
			Commands != nullptr ? Commands->GetMinePredictedWorstCaseDistance() : -1.0f,
			Commands != nullptr ? Commands->GetMineLastArrivalDistance() : -1.0f,
			Commands != nullptr ? Commands->GetMineLastArrivalRangeError() : -1.0f,
			Commands != nullptr ? (Commands->GetMineExecutionState() == EGP_MineExecutionState::Approaching ? TEXT("Approaching") : (Commands->GetMineExecutionState() == EGP_MineExecutionState::Active ? TEXT("Active") : TEXT("Idle"))) : TEXT("none"),
			Commands != nullptr ? Commands->GetActiveMineSerial() : 0u,
			Commands != nullptr ? HaulStateToString(Commands->GetHaulExecutionState()) : TEXT("none"),
			Commands != nullptr ? Commands->GetActiveHaulSerial() : 0u,
			Commands != nullptr ? *GetNameSafe(Commands->GetLastHaulDeposit()) : TEXT("none"),
			Commands != nullptr ? *GetNameSafe(Commands->GetHaulMainBase()) : TEXT("none"),
			Commands != nullptr ? Commands->GetLastHaulAcceptedAmount() : -1.0f,
			Commands != nullptr ? Commands->GetLastHaulRejectedAmount() : -1.0f,
			Commands != nullptr ? Commands->GetLastHaulThreatDelta() : -1.0f,
			Commands != nullptr ? Commands->GetHaulDropOffRangeCm() : -1.0f,
			Commands != nullptr ? *Commands->GetHaulApproachDestination().ToCompactString() : TEXT("none"),
			Commands != nullptr ? Commands->GetHaulApproachAttempt() : -1,
			Commands != nullptr && Commands->ShouldReturnToDepositAfterHaul() ? TEXT("true") : TEXT("false"),
			IsValid(Mining) && IsValid(MineTarget) && MineTarget->HasActiveMiningSlot(Worker) ? TEXT("true") : TEXT("false"),
			IsValid(Mining) && IsValid(MineTarget) && MineTarget->IsWaitingForMiningSlot(Worker) ? TEXT("true") : TEXT("false"),
			IsValid(Mining) && Mining->IsMiningTimerActive() ? TEXT("true") : TEXT("false"),
			Worker->IsActorTickEnabled() ? TEXT("true") : TEXT("false"),
			Movement != nullptr && Movement->IsComponentTickEnabled() ? TEXT("true") : TEXT("false"),
			IsValid(Cargo) && Cargo->IsComponentTickEnabled() ? TEXT("true") : TEXT("false"),
			IsValid(Mining) && Mining->IsComponentTickEnabled() ? TEXT("true") : TEXT("false"),
			bValid ? TEXT("true") : TEXT("false"),
			Errors.Num(),
			Warnings.Num(),
			*Worker->GetCapabilityTags().ToStringSimple());
	}

	static void WorkerList(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.List: missing world"));
			return;
		}

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		int32 WorkerCount = 0;
		AGP_Worker* PrimaryWorker = nullptr;
		AGP_Worker* FallbackPlayableWorker = nullptr;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			AGP_Worker* Worker = *It;
			if (!IsValid(Worker))
			{
				continue;
			}
			++WorkerCount;
			if (Worker->GetTeamId() < 1)
			{
				continue;
			}
			if (FallbackPlayableWorker == nullptr)
			{
				FallbackPlayableWorker = Worker;
			}
			// Prefer operator (non-contract) Team1 for hauling readiness validation.
			const bool bContractOwned = Worker->Tags.Contains(GPResourceLoopDiagnostics::TagOwnedByContract);
			if (!bContractOwned && Worker->GetTeamId() == 1)
			{
				PrimaryWorker = Worker;
			}
			else if (PrimaryWorker == nullptr && !bContractOwned)
			{
				PrimaryWorker = Worker;
			}
		}
		if (PrimaryWorker == nullptr)
		{
			PrimaryWorker = FallbackPlayableWorker;
		}
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			AGP_Worker* Worker = *It;
			if (!IsValid(Worker))
			{
				continue;
			}
			UGP_UnitCommandComponent* Commands = Worker->GetUnitCommandComponent();
			UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
			AGP_MainBase* ResolvedBase = (GS != nullptr && Worker->GetTeamId() >= 1)
				? GS->FindMainBaseForTeam(Worker->GetTeamId())
				: nullptr;
			UE_LOG(LogGPWorker, Log,
				TEXT("GP Worker.List Worker: Name=%s TeamId=%d Activity=%s Cargo=%.1f/%.1f Haul=%s MineSerial=%u HaulSerial=%u ResolvedMainBase=%s"),
				*Worker->GetName(),
				Worker->GetTeamId(),
				ActivityToString(Worker->GetWorkerActivityState()),
				IsValid(Cargo) ? Cargo->GetCurrentCargoAmount() : -1.0f,
				IsValid(Cargo) ? Cargo->GetCargoCapacity() : -1.0f,
				Commands != nullptr ? HaulStateToString(Commands->GetHaulExecutionState()) : TEXT("none"),
				Commands != nullptr ? Commands->GetActiveMineSerial() : 0u,
				Commands != nullptr ? Commands->GetActiveHaulSerial() : 0u,
				*GetNameSafe(ResolvedBase));
		}

		int32 NodeCount = 0;
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			AGP_ResourceNode* Node = *It;
			if (!IsValid(Node))
			{
				continue;
			}
			++NodeCount;
			UE_LOG(LogGPWorker, Log,
				TEXT("GP Worker.List ResourceNode: Name=%s Amount=%d/%d MaxMiners=%d Depleted=%s Loc=%s Transient=%s"),
				*Node->GetName(),
				Node->GetCurrentAmount(),
				Node->GetMaxAmount(),
				Node->GetMaxConcurrentMiners(),
				Node->IsDepleted() ? TEXT("true") : TEXT("false"),
				*Node->GetActorLocation().ToCompactString(),
				Node->HasAnyFlags(RF_Transient) ? TEXT("true") : TEXT("false"));
		}

		int32 BaseCount = 0;
		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			AGP_MainBase* Base = *It;
			if (!IsValid(Base))
			{
				continue;
			}
			++BaseCount;
			UGP_StorageComponent* Storage = Base->GetStorageComponent();
			const bool bRegistered = GS != nullptr && GS->FindMainBaseForTeam(Base->GetTeamId()) == Base;
			UE_LOG(LogGPWorker, Log,
				TEXT("GP Worker.List MainBase: Name=%s TeamId=%d DropOffRange=%.1f Stored=%.1f/%.1f Ready=%d Registered=%s"),
				*Base->GetName(),
				Base->GetTeamId(),
				Base->GetDropOffRangeCm(),
				IsValid(Storage) ? Storage->GetTotalStored() : -1.0f,
				IsValid(Storage) ? Storage->GetTotalCapacity() : -1.0f,
				IsValid(Storage) ? Storage->GetReadyCount() : -1,
				bRegistered ? TEXT("true") : TEXT("false"));
		}

		const int32 ValidateTeam = (IsValid(PrimaryWorker) && PrimaryWorker->GetTeamId() >= 1)
			? PrimaryWorker->GetTeamId()
			: 1;
		const GPResourceLoopDiagnostics::FGP_ScenarioValidation Validation =
			GPResourceLoopDiagnostics::ValidateHaulingScenario(World, ValidateTeam, PrimaryWorker);

		UE_LOG(LogGPWorker, Log,
			TEXT("GP Worker.List Summary: Workers=%d ResourceNodes=%d MainBases=%d NetMode=%s"),
			WorkerCount,
			NodeCount,
			BaseCount,
			NetModeToString(World->GetNetMode()));
		UE_LOG(LogGPWorker, Log,
			TEXT("GP Worker.List ScenarioValidation: NavSystemPresent=%s PlayableTeamValid=%s WorkerHasMainBase=%s WorkerHasResourceNode=%s MainBaseRegisteredForTeam=%s WorkerAndBaseSameTeam=%s NodeMineable=%s MainBaseCountForWorkerTeam=%d RegistryUniqueForTeam=%s ResolvedMainBaseMatchesListedBase=%s WorkerProjected=%s NodeApproachProjected=%s BaseDropOffProjected=%s NavWorkerToNode=%s NavNodeToBase=%s NavBaseToNode=%s ReadyForHaulingTest=%s Errors=%d Warnings=%d PathFailure=%s SuggestedCommand=%s"),
			Validation.bNavSystemPresent ? TEXT("true") : TEXT("false"),
			Validation.bPlayableTeamValid ? TEXT("true") : TEXT("false"),
			Validation.bWorkerHasMainBase ? TEXT("true") : TEXT("false"),
			Validation.bWorkerHasResourceNode ? TEXT("true") : TEXT("false"),
			Validation.bMainBaseRegisteredForTeam ? TEXT("true") : TEXT("false"),
			Validation.bWorkerAndBaseSameTeam ? TEXT("true") : TEXT("false"),
			Validation.bNodeMineable ? TEXT("true") : TEXT("false"),
			Validation.MainBaseCountForWorkerTeam,
			Validation.bRegistryUniqueForTeam ? TEXT("true") : TEXT("false"),
			Validation.bResolvedMainBaseMatchesListedBase ? TEXT("true") : TEXT("false"),
			Validation.bWorkerProjected ? TEXT("true") : TEXT("false"),
			Validation.bNodeApproachProjected ? TEXT("true") : TEXT("false"),
			Validation.bBaseDropOffProjected ? TEXT("true") : TEXT("false"),
			Validation.bNavReachableWorkerToNode ? TEXT("true") : TEXT("false"),
			Validation.bNavReachableNodeToBase ? TEXT("true") : TEXT("false"),
			Validation.bNavReachableBaseToNode ? TEXT("true") : TEXT("false"),
			Validation.bReadyForHaulingTest ? TEXT("true") : TEXT("false"),
			Validation.Errors,
			Validation.Warnings,
			*Validation.PathFailureReason,
			*Validation.SuggestedCommand);
	}

	static void WorkerCommandMine(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.CommandMine: missing world or client"));
			return;
		}
		AGP_Worker* Worker = FindWorker(World, Args.Num() > 0 ? Args[0] : FString());
		AGP_ResourceNode* Node = FindNode(World, Args.Num() > 1 ? Args[1] : FString());
		if (!IsValid(Worker) || !IsValid(Node))
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.CommandMine: Worker/Node missing"));
			return;
		}
		IssueMine(Worker, Node);
		UE_LOG(LogGPWorker, Log,
			TEXT("GP Worker.CommandMine: Worker=%s Node=%s Activity=%s"),
			*Worker->GetName(),
			*Node->GetName(),
			ActivityToString(Worker->GetWorkerActivityState()));
	}

	static void WorkerCommandMove(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client || Args.Num() < 4)
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.CommandMove: usage WorkerName X Y Z"));
			return;
		}
		AGP_Worker* Worker = FindWorker(World, Args[0]);
		if (!IsValid(Worker))
		{
			return;
		}
		const FVector Loc(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]));
		IssueMove(Worker, Loc);
	}

	static void WorkerStop(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			return;
		}
		AGP_Worker* Worker = FindWorker(World, Args.Num() > 0 ? Args[0] : FString());
		if (!IsValid(Worker))
		{
			return;
		}
		if (UGP_MiningComponent* Mining = Worker->GetMiningComponent())
		{
			Mining->StopMining(EGP_MiningStopReason::ManualStop);
		}
		if (UGP_MovementComponent* Movement = Worker->GetUnitMovementComponent())
		{
			Movement->StopMove(EGP_MovementStopReason::Manual);
		}
		IssueMove(Worker, Worker->GetActorLocation());
	}
}


void UGP_WorkerContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_WorkerContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_WorkerContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		Abort(TEXT("WorldEndPlay"));
	}
}

void UGP_WorkerContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	bCancelled = false;
	CancelReason = NAME_None;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	MovementWaitTicks = 0;
	MovementWaitStartTime = -1.0;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_WorkerContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPWorker, Log, TEXT("GP Worker.RunContractTest Stage=Start"));
	ScheduleNext();
}

void UGP_WorkerContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalid"));
		return;
	}
	StageTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_WorkerContractTestRunner::AdvanceStage));
}

bool UGP_WorkerContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPWorker, Error, TEXT("GP Worker.RunContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPWorker, Log, TEXT("GP Worker.RunContractTest PASS: %s"), Label);
	return true;
}

void UGP_WorkerContractTestRunner::Abort(const TCHAR* Reason)
{
	if (bFinished)
	{
		return;
	}
	UE_LOG(LogGPWorker, Error, TEXT("GP Worker.RunContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

void UGP_WorkerContractTestRunner::DestroyWeakWorker(TWeakObjectPtr<AGP_Worker>& Weak)
{
	if (AGP_Worker* Worker = Weak.Get())
	{
		if (IsValid(Worker))
		{
			if (UGP_MiningComponent* Mining = Worker->GetMiningComponent())
			{
				Mining->StopMining(EGP_MiningStopReason::ManualStop);
			}
			Worker->Destroy();
		}
	}
	Weak.Reset();
}

void UGP_WorkerContractTestRunner::Finish()
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
	}
	DestroyWeakWorker(PrimaryWorkerWeak);
	for (TWeakObjectPtr<AGP_Worker>& W : FifoWorkersWeak)
	{
		DestroyWeakWorker(W);
	}
	FifoWorkersWeak.Reset();
	if (AGP_ResourceNode* Node = TestNodeWeak.Get())
	{
		if (IsValid(Node))
		{
			Node->Destroy();
		}
	}
	TestNodeWeak.Reset();
	UE_LOG(LogGPWorker, Log, TEXT("GP Worker.RunContractTest: Complete Failures=%d"), Failures);
	GPContractTestCoordinator::Release(ExecutionId, Failures, bCancelled, *CancelReason.ToString());
	RemoveFromRoot();
	GPWorkerDebug::GActiveWorkerContractTest.Reset();
	WorldWeak.Reset();
}

AGP_ResourceNode* UGP_WorkerContractTestRunner::SpawnNode(const FVector& Loc) const
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	return World->SpawnActor<AGP_ResourceNode>(AGP_ResourceNode::StaticClass(), Loc, FRotator::ZeroRotator, Params);
}

void UGP_WorkerContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalidDuringStage"));
		return;
	}

	using namespace GPWorkerDebug;

	switch (StageIndex)
	{
	case 0:
	{
		AGP_ResourceNode* Node = SpawnNode(FVector(-48000.0f, 0.0f, 100.0f));
		if (!Expect(IsValid(Node), TEXT("SpawnTransientNode")))
		{
			Finish();
			return;
		}
		TestNodeWeak = Node;
		if (const UGP_ResourceDefinition* Def = Node->ResolveResourceDefinition(true))
		{
			InteractionRangeCm = Def->InteractionRangeCm;
			Expect(FMath::IsNearlyEqual(Def->AmountPerMiningCycle, 10.0f), TEXT("DefAmount10"));
			Expect(FMath::IsNearlyEqual(Def->MiningCycleDurationSeconds, 1.0f), TEXT("DefDuration1"));
			Expect(FMath::IsNearlyEqual(Def->InteractionRangeCm, 200.0f), TEXT("DefRange200"));
		}
		AGP_Worker* Worker = SpawnWorkerAt(World, Node->GetActorLocation() + FVector(100.0f, 0.0f, 0.0f));
		if (!Expect(IsValid(Worker), TEXT("SpawnWorker")))
		{
			Finish();
			return;
		}
		PrimaryWorkerWeak = Worker;
		Expect(IsValid(Worker->GetCargoComponent()), TEXT("HasCargo"));
		Expect(IsValid(Worker->GetMiningComponent()), TEXT("HasMining"));
		Expect(Worker->GetUnitMovementComponent() != nullptr, TEXT("HasMovement"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCargoCapacity(), 50.0f), TEXT("CargoCap50"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("CargoEmpty"));
		Expect(Worker->GetMiningComponent()->IsComponentTickEnabled() == false, TEXT("MiningTickOff"));
		Expect(Worker->IsActorTickEnabled() == false, TEXT("WorkerActorTickOff"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("PrimaryValid")))
		{
			Finish();
			return;
		}
		IssueMine(Worker, nullptr);
		Expect(Worker->GetMiningComponent()->GetMiningState() == EGP_MiningState::Idle
			|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::Idle, TEXT("NullMineRejected"));
		Worker->GetCargoComponent()->AddCargo(50.0f);
		IssueMine(Worker, Node);
		Expect(Worker->GetMiningComponent()->GetMiningState() != EGP_MiningState::Mining, TEXT("FullCargoRejected"));
		Worker->GetCargoComponent()->ClearCargo();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("ImmediateMineObjects")))
		{
			Finish();
			return;
		}
		const int32 NodeBefore = Node->GetCurrentAmount();
		IssueMine(Worker, Node);
		Expect(Worker->GetMiningComponent()->GetMiningState() == EGP_MiningState::Mining, TEXT("ImmediateMiningState"));
		Expect(Worker->GetMiningComponent()->IsMiningTimerActive(), TEXT("ImmediateTimer"));
		Expect(Node->HasActiveMiningSlot(Worker), TEXT("ImmediateSlot"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("FirstCycleDelay"));
		Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 10.0f), TEXT("OneCyclePlus10"));
		Expect(Node->GetCurrentAmount() == NodeBefore - 10, TEXT("NodeMinus10"));
		IssueMine(Worker, Node);
		Expect(Worker->GetUnitCommandComponent()->GetMineExecutionState() != EGP_MineExecutionState::Approaching
			|| Worker->GetMiningComponent()->IsMining(), TEXT("DuplicateMineNoExtraApproach"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 3:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("MoveMineObjects")))
		{
			Finish();
			return;
		}
		Worker->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		Worker->GetCargoComponent()->ClearCargo();
		const FVector Far = Node->GetActorLocation() + FVector(InteractionRangeCm + 500.0f, 0.0f, 0.0f);
		Worker->SetActorLocation(Far, false, nullptr, ETeleportType::TeleportPhysics);
		IssueMine(Worker, Node);
		Expect(Worker->GetWorkerActivityState() == EGP_WorkerActivityState::MovingToMine
			|| Worker->GetUnitCommandComponent()->IsMineApproachActive(), TEXT("MovingToMine"));
		Expect(!Node->HasActiveMiningSlot(Worker), TEXT("NoSlotWhileMoving"));
		Expect(!Worker->GetMiningComponent()->IsMiningTimerActive(), TEXT("NoTimerWhileMoving"));
		MovementWaitTicks = 0;
		MovementWaitStartTime = -1.0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 4:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (MovementWaitTicks == 0)
		{
			if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("ArrivalObjects")))
			{
				Finish();
				return;
			}
			MovementWaitStartTime = World->GetTimeSeconds();
		}
		else if (!IsValid(Worker) || !IsValid(Node))
		{
			Abort(TEXT("ArrivalObjectsLost"));
			return;
		}

		++MovementWaitTicks;
		if (Worker->GetUnitCommandComponent()->IsMineApproachActive()
			|| (Worker->GetUnitMovementComponent() && Worker->GetUnitMovementComponent()->IsMoving()))
		{
			const double Elapsed = World->GetTimeSeconds() - MovementWaitStartTime;
			if (Elapsed > MovementWaitTimeoutSeconds)
			{
				Expect(false, TEXT("MovementWaitTimeout"));
				Finish();
				return;
			}
			if ((MovementWaitTicks % 60) == 0)
			{
				UE_LOG(LogGPWorker, Log,
					TEXT("GP Worker.RunContractTest Progress: MovementWaitTicks=%d Elapsed=%.2f Dist=%.1f"),
					MovementWaitTicks,
					Elapsed,
					FVector::Dist(Worker->GetActorLocation(), Node->GetActorLocation()));
			}
			ScheduleNext();
			return;
		}
		Expect(Worker->GetMiningComponent()->IsMining() || Worker->GetMiningComponent()->IsWaitingForSlot(),
			TEXT("ArrivedBeganMining"));
		Expect(FVector::Dist(Worker->GetActorLocation(), Node->GetActorLocation()) < InteractionRangeCm,
			TEXT("ArrivedInRange"));
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 5:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("InterruptObjects")))
		{
			Finish();
			return;
		}
		Worker->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		Worker->SetActorLocation(Node->GetActorLocation() + FVector(InteractionRangeCm + 800.0f, 0.0f, 0.0f),
			false, nullptr, ETeleportType::TeleportPhysics);
		IssueMine(Worker, Node);
		const FVector MoveDest = Node->GetActorLocation() + FVector(0.0f, 1000.0f, 0.0f);
		IssueMove(Worker, MoveDest);
		Expect(!Node->HasActiveMiningSlot(Worker) && !Node->IsWaitingForMiningSlot(Worker), TEXT("MoveClearsMineSlot"));
		Expect(!Worker->GetMiningComponent()->IsMiningTimerActive(), TEXT("MoveClearsMineTimer"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 6:
	{
		// FIFO: fresh node + 5 workers in range
		if (AGP_ResourceNode* Old = TestNodeWeak.Get())
		{
			if (IsValid(Old))
			{
				Old->Destroy();
			}
		}
		DestroyWeakWorker(PrimaryWorkerWeak);
		AGP_ResourceNode* SlotNode = SpawnNode(FVector(-49000.0f, 0.0f, 100.0f));
		if (!Expect(IsValid(SlotNode), TEXT("FifoNode")))
		{
			Finish();
			return;
		}
		TestNodeWeak = SlotNode;
		FifoWorkersWeak.Reset();
		for (int32 i = 0; i < 5; ++i)
		{
			AGP_Worker* W = SpawnWorkerAt(World, SlotNode->GetActorLocation() + FVector(80.0f + i * 5.0f, 0.0f, 0.0f));
			if (!IsValid(W))
			{
				Expect(false, TEXT("FifoSpawn5"));
				Finish();
				return;
			}
			FifoWorkersWeak.Add(W);
			IssueMine(W, SlotNode);
		}
		Expect(FifoWorkersWeak.Num() == 5, TEXT("FifoSpawn5"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 7:
	{
		AGP_ResourceNode* SlotNode = TestNodeWeak.Get();
		if (!Expect(IsValid(SlotNode), TEXT("FifoNodeValid")))
		{
			Finish();
			return;
		}
		int32 MiningCount = 0;
		int32 WaitingCount = 0;
		WaitingWorkerWeak.Reset();
		TWeakObjectPtr<AGP_Worker> ActiveToStop;
		for (TWeakObjectPtr<AGP_Worker>& Weak : FifoWorkersWeak)
		{
			AGP_Worker* W = Weak.Get();
			if (!Expect(IsValid(W), TEXT("FifoWorkerValid")))
			{
				Finish();
				return;
			}
			if (W->GetMiningComponent()->IsMining())
			{
				++MiningCount;
				if (!ActiveToStop.IsValid())
				{
					ActiveToStop = W;
				}
			}
			else if (W->GetMiningComponent()->IsWaitingForSlot())
			{
				++WaitingCount;
				WaitingWorkerWeak = W;
			}
		}
		Expect(MiningCount == 4, TEXT("FifoFourMining"));
		Expect(WaitingCount == 1, TEXT("FifoOneWaiting"));
		if (AGP_Worker* StopMe = ActiveToStop.Get())
		{
			StopMe->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		}
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 8:
	{
		AGP_Worker* Waiting = WaitingWorkerWeak.Get();
		if (!Expect(IsValid(Waiting) && Waiting->GetMiningComponent()->IsMining(), TEXT("FifoPromoted")))
		{
			Finish();
			return;
		}
		Expect(Waiting->GetMiningComponent()->IsMiningTimerActive(), TEXT("FifoPromotedTimer"));
		for (TWeakObjectPtr<AGP_Worker>& Weak : FifoWorkersWeak)
		{
			DestroyWeakWorker(Weak);
		}
		FifoWorkersWeak.Reset();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 9:
	{
		AGP_ResourceNode* SlotNode = TestNodeWeak.Get();
		Expect(IsValid(SlotNode) && SlotNode->GetActiveMinerCount() == 0 && SlotNode->GetWaitingMinerCount() == 0,
			TEXT("FifoCleanupEmpty"));
		AGP_Worker* Worker = SpawnWorkerAt(World, SlotNode->GetActorLocation() + FVector(100.0f, 0.0f, 0.0f));
		PrimaryWorkerWeak = Worker;
		if (!Expect(IsValid(Worker), TEXT("CargoFullWorker")))
		{
			Finish();
			return;
		}
		Worker->GetCargoComponent()->ClearCargo();
		IssueMine(Worker, SlotNode);
		for (int32 i = 0; i < 5; ++i)
		{
			Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		}
		Expect(Worker->GetMiningComponent()->GetMiningState() == EGP_MiningState::CargoFull, TEXT("CargoFullState"));
		Expect(!Worker->GetMiningComponent()->IsMiningTimerActive(), TEXT("CargoFullTimerOff"));
		Expect(!SlotNode->HasActiveMiningSlot(Worker), TEXT("CargoFullSlotReleased"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 10:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("DepleteObjects")))
		{
			Finish();
			return;
		}
		Worker->GetCargoComponent()->ClearCargo();
		Worker->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		const int32 Leave = 5;
		const int32 Consume = Node->GetCurrentAmount() - Leave;
		if (Consume > 0)
		{
			Node->ConsumeResource(Consume);
		}
		IssueMine(Worker, Node);
		Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 5.0f), TEXT("DepleteTransfer5"));
		Expect(Worker->GetMiningComponent()->GetMiningState() == EGP_MiningState::DepositDepleted, TEXT("DepositDepletedState"));
		Expect(!Node->HasActiveMiningSlot(Worker), TEXT("DepleteSlotReleased"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 11:
	{
		// EndPlay while mining
		AGP_ResourceNode* Node = SpawnNode(FVector(-50000.0f, 0.0f, 100.0f));
		TestNodeWeak = Node;
		AGP_Worker* Worker = SpawnWorkerAt(World, Node->GetActorLocation() + FVector(100.0f, 0.0f, 0.0f));
		PrimaryWorkerWeak = Worker;
		IssueMine(Worker, Node);
		Expect(Node->HasActiveMiningSlot(Worker), TEXT("EndPlayHadSlot"));
		Worker->Destroy();
		PrimaryWorkerWeak.Reset();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 12:
	{
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		Expect(IsValid(Node) && Node->GetActiveMinerCount() == 0 && Node->GetWaitingMinerCount() == 0,
			TEXT("EndPlayReleasedSlots"));
		if (IsValid(Node))
		{
			Node->Destroy();
		}
		TestNodeWeak.Reset();
		DestroyWeakWorker(PrimaryWorkerWeak);
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 13:
	{
		// Edge: worst-case acceptance boundary + vertical budget + diagonal geometry.
		AGP_ResourceNode* Node = SpawnNode(FVector(-51000.0f, 0.0f, 40.0f));
		TestNodeWeak = Node;
		if (!Expect(IsValid(Node), TEXT("EdgeGeometryNode")))
		{
			Finish();
			return;
		}
		const float Acc = 50.0f;
		const float Safety = 25.0f;
		const float OldDesired = InteractionRangeCm - Acc - 5.0f; // legacy 145
		const float OldWorstFlat = OldDesired + Acc; // 195
		Expect(OldWorstFlat < InteractionRangeCm, TEXT("LegacyFlatWorstUnderRange"));

		AGP_Worker* FlatWorker = SpawnWorkerAt(World, Node->GetActorLocation() + FVector(InteractionRangeCm + 800.0f, 0.0f, 48.0f));
		PrimaryWorkerWeak = FlatWorker;
		if (!Expect(IsValid(FlatWorker), TEXT("EdgeFlatWorker")))
		{
			Finish();
			return;
		}
		IssueMine(FlatWorker, Node);
		UGP_UnitCommandComponent* Cmd = FlatWorker->GetUnitCommandComponent();
		if (!Expect(Cmd != nullptr && Cmd->IsMineApproachActive(), TEXT("EdgeApproachActive")))
		{
			Finish();
			return;
		}
		Expect(Cmd->GetMinePredictedWorstCaseDistance() > 0.0f
			&& Cmd->GetMinePredictedWorstCaseDistance() < InteractionRangeCm,
			TEXT("ApproachWorstCaseWithinRange"));
		Expect(Cmd->GetMineApproachDesiredNodeDistance() > 0.0f
			&& Cmd->GetMineApproachDesiredNodeDistance() <= (InteractionRangeCm - Acc - Safety + 0.1f),
			TEXT("ApproachDesiredUsesSafetyMargin"));
		// Vertical offset: Worker Z differs from Node Z=40.
		Expect(FMath::Abs(FlatWorker->GetActorLocation().Z - Node->GetActorLocation().Z) > 1.0f,
			TEXT("ApproachVerticalBudgetWithinRange_Setup"));
		Expect(Cmd->GetMinePredictedWorstCaseDistance() < InteractionRangeCm,
			TEXT("ApproachVerticalBudgetWithinRange"));
		FlatWorker->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		if (UGP_MovementComponent* Move = FlatWorker->GetUnitMovementComponent())
		{
			Move->StopMove(EGP_MovementStopReason::Manual);
		}
		DestroyWeakWorker(PrimaryWorkerWeak);

		AGP_Worker* DiagWorker = SpawnWorkerAt(
			World,
			Node->GetActorLocation() + FVector(3000.0f, 3000.0f, 48.0f));
		PrimaryWorkerWeak = DiagWorker;
		IssueMine(DiagWorker, Node);
		Cmd = DiagWorker->GetUnitCommandComponent();
		Expect(Cmd != nullptr && Cmd->IsMineApproachActive(), TEXT("DiagonalApproachStarted"));
		Expect(Cmd->GetMinePredictedWorstCaseDistance() < InteractionRangeCm - 10.0f,
			TEXT("DiagonalApproachMarginSafe"));
		MovementWaitTicks = 0;
		MovementWaitStartTime = -1.0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 14:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (MovementWaitTicks == 0)
		{
			if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("DiagonalArrivalObjects")))
			{
				Finish();
				return;
			}
			MovementWaitStartTime = World->GetTimeSeconds();
		}
		++MovementWaitTicks;
		if (IsValid(Worker)
			&& (Worker->GetUnitCommandComponent()->IsMineApproachActive()
				|| (Worker->GetUnitMovementComponent() && Worker->GetUnitMovementComponent()->IsMoving())))
		{
			if ((World->GetTimeSeconds() - MovementWaitStartTime) > MovementWaitTimeoutSeconds)
			{
				Expect(false, TEXT("DiagonalMovementWaitTimeout"));
				Finish();
				return;
			}
			ScheduleNext();
			return;
		}
		if (!Expect(IsValid(Worker) && IsValid(Node)
			&& (Worker->GetMiningComponent()->IsMining() || Worker->GetMiningComponent()->IsWaitingForSlot()),
			TEXT("DiagonalArrivedMining")))
		{
			Finish();
			return;
		}
		const float ArrivedDist = FVector::Dist(Worker->GetActorLocation(), Node->GetActorLocation());
		Expect(ArrivedDist < InteractionRangeCm - 10.0f, TEXT("DiagonalArrivalMarginSafe"));
		Worker->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		DestroyWeakWorker(PrimaryWorkerWeak);
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 15:
	{
		// One-shot corrective approach after forced OOR arrival.
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Node), TEXT("CorrectiveNode")))
		{
			Finish();
			return;
		}
		AGP_Worker* Worker = SpawnWorkerAt(World, Node->GetActorLocation() + FVector(InteractionRangeCm + 600.0f, 0.0f, 0.0f));
		PrimaryWorkerWeak = Worker;
		if (!Expect(IsValid(Worker), TEXT("CorrectiveWorker")))
		{
			Finish();
			return;
		}
		IssueMine(Worker, Node);
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		if (!Expect(Cmd != nullptr && Cmd->IsMineApproachActive(), TEXT("CorrectiveApproachActive")))
		{
			Finish();
			return;
		}
		Expect(!Node->HasActiveMiningSlot(Worker), TEXT("CorrectiveNoSlotBeforeArrival"));
		Expect(!Worker->GetMiningComponent()->IsMiningTimerActive(), TEXT("CorrectiveNoTimerBeforeArrival"));
		Cmd->DebugForceNextMineArrivalOutOfRangeOnce();
		MovementWaitTicks = 0;
		MovementWaitStartTime = -1.0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 16:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (MovementWaitTicks == 0)
		{
			if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("CorrectiveWaitObjects")))
			{
				Finish();
				return;
			}
			MovementWaitStartTime = World->GetTimeSeconds();
		}
		++MovementWaitTicks;
		if (IsValid(Worker)
			&& (Worker->GetUnitCommandComponent()->IsMineApproachActive()
				|| (Worker->GetUnitMovementComponent() && Worker->GetUnitMovementComponent()->IsMoving())))
		{
			if ((World->GetTimeSeconds() - MovementWaitStartTime) > (MovementWaitTimeoutSeconds * 2.0f))
			{
				Expect(false, TEXT("CorrectiveMovementWaitTimeout"));
				Finish();
				return;
			}
			ScheduleNext();
			return;
		}
		if (!Expect(IsValid(Worker) && Worker->GetMiningComponent()->IsMining(), TEXT("CorrectiveBeganMining")))
		{
			Finish();
			return;
		}
		Expect(Worker->GetUnitCommandComponent()->GetMineApproachAttempt() >= 1, TEXT("CorrectiveAttemptUsed"));
		Expect(FVector::Dist(Worker->GetActorLocation(), Node->GetActorLocation()) < InteractionRangeCm,
			TEXT("CorrectiveFinalInRange"));
		DestroyWeakWorker(PrimaryWorkerWeak);
		if (IsValid(Node))
		{
			Node->Destroy();
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

namespace GPWorkerDebug
{
	static void WorkerRunContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.RunContractTest: missing world or client"));
			return;
		}
		if (GActiveWorkerContractTest.IsValid())
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.RunContractTest: rejected — already running"));
			return;
		}
		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(World, TEXT("WorkerContract"), TEXT("Worker"), Token))
		{
			return;
		}
		UGP_WorkerContractTestRunner* Runner = NewObject<UGP_WorkerContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveWorkerContractTest = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GWorkerSpawn(
		TEXT("gp.Worker.SpawnDiagnostic"),
		TEXT("Authority: spawn Worker TeamId (creates full scenario if MainBase missing). Usage: gp.Worker.SpawnDiagnostic [TeamId=1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorkerSpawnDiagnostic));

	static FAutoConsoleCommandWithWorldAndArgs GWorkerInspect(
		TEXT("gp.Worker.Inspect"),
		TEXT("Inspect Worker. Usage: gp.Worker.Inspect [WorkerName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorkerInspect));

	static FAutoConsoleCommandWithWorldAndArgs GWorkerMine(
		TEXT("gp.Worker.CommandMine"),
		TEXT("Authority Mine via ReceiveCommand. Usage: gp.Worker.CommandMine [Worker] [Node]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorkerCommandMine));

	static FAutoConsoleCommandWithWorldAndArgs GWorkerMove(
		TEXT("gp.Worker.CommandMove"),
		TEXT("Authority Move. Usage: gp.Worker.CommandMove WorkerName X Y Z"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorkerCommandMove));

	static FAutoConsoleCommandWithWorldAndArgs GWorkerStop(
		TEXT("gp.Worker.Stop"),
		TEXT("Authority stop mining/movement. Usage: gp.Worker.Stop [WorkerName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorkerStop));

	static FAutoConsoleCommandWithWorldAndArgs GWorkerContract(
		TEXT("gp.Worker.RunContractTest"),
		TEXT("Staged Worker Mine orchestration contract test."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorkerRunContractTest));

	static FAutoConsoleCommandWithWorldAndArgs GWorkerList(
		TEXT("gp.Worker.List"),
		TEXT("List Workers + ResourceNodes + MainBases summary."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorkerList));

	TWeakObjectPtr<UGP_WorkerHaulingContractTestRunner> GActiveHaulingContractTest;

	static void WorkerRunHaulingContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.RunHaulingContractTest: missing world or client"));
			return;
		}
		if (GActiveHaulingContractTest.IsValid())
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.RunHaulingContractTest: rejected — already running"));
			return;
		}
		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(World, TEXT("WorkerHaulingContract"), TEXT("WorkerHauling"), Token))
		{
			return;
		}
		UGP_WorkerHaulingContractTestRunner* Runner = NewObject<UGP_WorkerHaulingContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveHaulingContractTest = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GWorkerHaulingContract(
		TEXT("gp.Worker.RunHaulingContractTest"),
		TEXT("Staged Worker haul/drop-off contract test (GP-S28)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorkerRunHaulingContractTest));

	static void ResourceSpawnDiagnosticScenario(const TArray<FString>& Args, UWorld* World)
	{
		int32 TeamId = 1;
		if (Args.Num() > 0)
		{
			LexTryParseString(TeamId, *Args[0]);
		}
		if (TeamId < 1)
		{
			TeamId = 1;
		}

		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, TeamId, GPContractTestCoordinator::OwnerTagOperator);
		if (!Scenario.bOk)
		{
			UE_LOG(LogGPWorker, Error,
				TEXT("GP Resource.SpawnDiagnosticScenario: FAILED TeamId=%d Error=%s PathFailure=%s CreatedBase=false CreatedWorker=false CreatedNode=false"),
				TeamId,
				*Scenario.Error,
				*Scenario.PathFailureReason);
			return;
		}

		const GPResourceLoopDiagnostics::FGP_ScenarioValidation Validation =
			GPResourceLoopDiagnostics::ValidateHaulingScenario(World, TeamId, Scenario.Worker);
		UE_LOG(LogGPWorker, Log,
			TEXT("GP Resource.SpawnDiagnosticScenario: ReadyForHaulingTest=%s NavWorkerToNode=%s NavNodeToBase=%s NavBaseToNode=%s Errors=%d Warnings=%d SuggestedCommand=%s"),
			Validation.bReadyForHaulingTest ? TEXT("true") : TEXT("false"),
			Validation.bNavReachableWorkerToNode ? TEXT("true") : TEXT("false"),
			Validation.bNavReachableNodeToBase ? TEXT("true") : TEXT("false"),
			Validation.bNavReachableBaseToNode ? TEXT("true") : TEXT("false"),
			Validation.Errors,
			Validation.Warnings,
			*Validation.SuggestedCommand);
	}

	TWeakObjectPtr<UGP_DiagnosticScenarioContractTestRunner> GActiveDiagnosticScenarioContractTest;

	static void ResourceRunDiagnosticScenarioContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Resource.RunDiagnosticScenarioContractTest: missing world or client"));
			return;
		}
		if (GActiveDiagnosticScenarioContractTest.IsValid())
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Resource.RunDiagnosticScenarioContractTest: rejected — already running"));
			return;
		}
		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(World, TEXT("DiagnosticScenarioContract"), TEXT("DiagnosticScenario"), Token))
		{
			return;
		}
		UGP_DiagnosticScenarioContractTestRunner* Runner = NewObject<UGP_DiagnosticScenarioContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveDiagnosticScenarioContractTest = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GResourceSpawnScenario(
		TEXT("gp.Resource.SpawnDiagnosticScenario"),
		TEXT("Authority: spawn coherent MainBase+Worker+ResourceNode scenario. Usage: gp.Resource.SpawnDiagnosticScenario [TeamId=1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceSpawnDiagnosticScenario));

	static FAutoConsoleCommandWithWorldAndArgs GStorageSpawnScenario(
		TEXT("gp.Storage.SpawnDiagnosticScenario"),
		TEXT("Alias of gp.Resource.SpawnDiagnosticScenario."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceSpawnDiagnosticScenario));

	static FAutoConsoleCommandWithWorldAndArgs GResourceDiagContract(
		TEXT("gp.Resource.RunDiagnosticScenarioContractTest"),
		TEXT("Deterministic DiagnosticScenarioSpawn contract test (GP-S28)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceRunDiagnosticScenarioContractTest));

	static void ResourceRunPresentationContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Resource.RunPresentationContractTest: missing world"));
			return;
		}
		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPWorker, Warning, TEXT("GP Resource.RunPresentationContractTest: rejected on client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("PresentationContract"), TEXT("Presentation"), Token))
		{
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (!bOk)
			{
				++Failures;
				UE_LOG(LogGPWorker, Error, TEXT("GP Resource.RunPresentationContractTest FAIL: %s"), Label);
			}
			else
			{
				UE_LOG(LogGPWorker, Log, TEXT("GP Resource.RunPresentationContractTest PASS: %s"), Label);
			}
		};

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;

		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			FVector(0.0f, 0.0f, 100.0f),
			FRotator::ZeroRotator,
			SpawnParams);
		AGP_MainBase* MainBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(500.0f, 0.0f, 100.0f),
			FRotator::ZeroRotator,
			SpawnParams);
		AGP_ResourceNode* Node = World->SpawnActor<AGP_ResourceNode>(
			AGP_ResourceNode::StaticClass(),
			FVector(1000.0f, 0.0f, 100.0f),
			FRotator::ZeroRotator,
			SpawnParams);

		Expect(IsValid(Worker), TEXT("WorkerSpawned"));
		Expect(IsValid(MainBase), TEXT("MainBaseSpawned"));
		Expect(IsValid(Node), TEXT("ResourceNodeSpawned"));

		if (!IsValid(Worker) || !IsValid(MainBase) || !IsValid(Node))
		{
			if (IsValid(Worker))
			{
				Worker->Destroy();
			}
			if (IsValid(MainBase))
			{
				MainBase->Destroy();
			}
			if (IsValid(Node))
			{
				Node->Destroy();
			}
			GPContractTestCoordinator::Release(Token.ExecutionId, Failures + 1, false, TEXT("SpawnFailed"));
			return;
		}

		Worker->SetTeamId(1);
		MainBase->SetTeamId(1);
		MainBase->RefreshMainBaseRegistration();

		Expect(IsValid(Worker->GetPresentationRoot()), TEXT("WorkerPresentationRoot"));
		Expect(IsValid(Worker->GetCargoVisualAnchor()), TEXT("WorkerCargoVisualAnchor"));
		Expect(IsValid(Worker->GetMiningEffectAnchor()), TEXT("WorkerMiningEffectAnchor"));
		Expect(Worker->GetCargoVisualAnchor()->GetAttachParent() == Worker->GetPresentationRoot(),
			TEXT("WorkerCargoAnchorParent"));
		Expect(Worker->GetMiningEffectAnchor()->GetAttachParent() == Worker->GetPresentationRoot(),
			TEXT("WorkerMiningEffectAnchorParent"));
		Expect(Worker->GetPresentationRoot()->GetAttachParent() == Worker->GetCapsuleComponent(),
			TEXT("WorkerPresentationParent"));
		Expect(!Worker->GetPresentationRoot()->CanEverAffectNavigation(), TEXT("WorkerPresentationNoNav"));
		Expect(!Worker->GetMiningEffectAnchor()->CanEverAffectNavigation(), TEXT("MiningEffectAnchorNoNav"));
		Expect(!Worker->PrimaryActorTick.bCanEverTick, TEXT("WorkerNoPermanentTick"));
		Expect(!Worker->GetMiningEffectAnchor()->IsComponentTickEnabled(), TEXT("MiningEffectAnchorNoTick"));

		Expect(IsValid(MainBase->GetPresentationRoot()), TEXT("MainBasePresentationRoot"));
		Expect(IsValid(MainBase->GetDropOffVisualAnchor()), TEXT("MainBaseDropOffAnchor"));
		Expect(MainBase->GetDropOffVisualAnchor()->GetAttachParent() == MainBase->GetPresentationRoot(),
			TEXT("MainBaseDropOffParent"));
		Expect(IsValid(MainBase->GetStorageComponent()), TEXT("MainBaseStorage"));
		Expect(FMath::IsNearlyEqual(MainBase->GetPlanetaryStored(), 0.0f), TEXT("MainBaseStoredZero"));
		Expect(MainBase->GetPlanetaryCapacity() > 0.0f, TEXT("MainBaseCapacityPositive"));

		Expect(IsValid(Node->GetPresentationRoot()), TEXT("NodePresentationRoot"));
		Expect(Node->GetPresentationRoot() == Node->GetRootComponent(), TEXT("NodePresentationIsRoot"));
		Expect(Node->GetMaxAmount() > 0, TEXT("NodeMaxPositive"));
		Expect(FMath::IsNearlyEqual(Node->GetRemainingNormalized(), 1.0f), TEXT("NodeRemainingFull"));
		Expect(!Node->IsDepleted(), TEXT("NodeNotDepleted"));
		Expect(Node->GetUseGeneratedPrototypeVisual(), TEXT("NodeGeneratedVisualDefaultTrue"));

		UGP_CargoVisualStateProbe* Probe = NewObject<UGP_CargoVisualStateProbe>(GetTransientPackage());
		Probe->AddToRoot();
		Worker->OnCargoVisualStateChanged.AddDynamic(
			Probe, &UGP_CargoVisualStateProbe::HandleCargoVisualStateChanged);

		UGP_MiningEffectStateProbe* MiningProbe = NewObject<UGP_MiningEffectStateProbe>(GetTransientPackage());
		MiningProbe->AddToRoot();
		Worker->OnMiningEffectStateChanged.AddDynamic(
			MiningProbe, &UGP_MiningEffectStateProbe::HandleMiningEffectStateChanged);

		UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
		Expect(IsValid(Cargo), TEXT("CargoPresent"));
		Expect(!Cargo->IsComponentTickEnabled(), TEXT("CargoTickDisabled"));

		// BeginPlay already synced before bind — ClearCargo is idempotent when empty.
		Cargo->ClearCargo();
		const int32 EventsAfterClear = Probe->EventCount;
		Expect(!Worker->HasCargoForVisual(), TEXT("InitialEmptyInvisible"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoFillNormalized(), 0.0f), TEXT("InitialFillZero"));

		const float Cap = Cargo->GetCargoCapacity();
		Expect(Cap > KINDA_SMALL_NUMBER, TEXT("CargoCapacityPositive"));

		const float Added10 = Cargo->AddCargo(10.0f);
		Expect(FMath::IsNearlyEqual(Added10, 10.0f), TEXT("AddCargo10"));
		Expect(Worker->HasCargoForVisual(), TEXT("VisibleAfterAdd10"));
		Expect(Probe->bLastVisible, TEXT("EventVisibleAfterAdd10"));
		Expect(FMath::IsNearlyEqual(Probe->LastAmount, 10.0f), TEXT("EventAmount10"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoFillNormalized(), 10.0f / Cap, 1.e-3f), TEXT("FillAfter10"));
		Expect(Probe->EventCount > EventsAfterClear, TEXT("EventFiredOnAdd"));

		Cargo->AddCargo(Cap);
		Expect(Worker->HasCargoForVisual(), TEXT("VisibleWhenFull"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoFillNormalized(), 1.0f), TEXT("FillFull"));
		Expect(Probe->bLastVisible, TEXT("EventVisibleFull"));
		Expect(FMath::IsNearlyEqual(Probe->LastFill, 1.0f), TEXT("EventFillFull"));

		const float BeforePartial = Cargo->GetCurrentCargoAmount();
		Cargo->RemoveCargo(5.0f);
		Expect(Worker->HasCargoForVisual(), TEXT("VisibleAfterPartialRemove"));
		Expect(Cargo->GetCurrentCargoAmount() < BeforePartial, TEXT("AmountDecreased"));
		Expect(Probe->bLastVisible, TEXT("EventVisiblePartial"));

		const int32 EventsBeforeRepeatClear = Probe->EventCount;
		Cargo->ClearCargo();
		Expect(!Worker->HasCargoForVisual(), TEXT("InvisibleAfterClear"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoFillNormalized(), 0.0f), TEXT("FillZeroAfterClear"));
		Expect(!Probe->bLastVisible, TEXT("EventInvisibleAfterClear"));
		Expect(Probe->EventCount > EventsBeforeRepeatClear, TEXT("EventFiredOnClear"));

		Cargo->ClearCargo();
		Expect(!Worker->HasCargoForVisual(), TEXT("StillInvisibleAfterRepeatClear"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoFillNormalized(), 0.0f), TEXT("FillStillZero"));

		// Mining Niagara effect: active only while MiningComponent state == Mining.
		UGP_MiningComponent* Mining = Worker->GetMiningComponent();
		Expect(IsValid(Mining), TEXT("MiningPresent"));
		Expect(Mining->GetMiningState() == EGP_MiningState::Idle, TEXT("MiningInitialIdle"));
		Expect(!MiningProbe->bLastEffectActive, TEXT("MiningEffectInitialInactive"));
		const int32 MiningEventsBeforeBegin = MiningProbe->EventCount;

		Worker->SetActorLocation(Node->GetActorLocation() + FVector(50.0f, 0.0f, 0.0f));
		const EGP_BeginMiningResult BeginResult = Mining->BeginMining(Node);
		Expect(BeginResult == EGP_BeginMiningResult::Started, TEXT("BeginMiningStarted"));
		Expect(Mining->GetMiningState() == EGP_MiningState::Mining, TEXT("MiningStateActive"));
		Expect(MiningProbe->bLastEffectActive, TEXT("MiningEffectActiveTrue"));
		Expect(MiningProbe->LastNewState == EGP_MiningState::Mining, TEXT("MiningEffectNewStateMining"));
		Expect(MiningProbe->EventCount > MiningEventsBeforeBegin, TEXT("MiningEffectEventOnBegin"));

		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		Expect(!MiningProbe->bLastEffectActive, TEXT("MiningEffectInactiveAfterStop"));
		Expect(MiningProbe->LastNewState != EGP_MiningState::Mining, TEXT("MiningEffectLeftMining"));

		// WaitingForSlot must not activate Niagara.
		TArray<AGP_Worker*> SlotHolders;
		for (int32 SlotIndex = 0; SlotIndex < Node->GetMaxConcurrentMiners(); ++SlotIndex)
		{
			AGP_Worker* Holder = World->SpawnActor<AGP_Worker>(
				AGP_Worker::StaticClass(),
				Node->GetActorLocation() + FVector(0.0f, 50.0f * static_cast<float>(SlotIndex + 1), 0.0f),
				FRotator::ZeroRotator,
				SpawnParams);
			if (IsValid(Holder))
			{
				SlotHolders.Add(Holder);
				const EGP_MiningSlotRequestResult SlotResult = Node->RequestMiningSlot(Holder);
				Expect(SlotResult == EGP_MiningSlotRequestResult::Granted
						|| SlotResult == EGP_MiningSlotRequestResult::AlreadyActive,
					TEXT("SlotHolderGranted"));
			}
		}
		const int32 EventsBeforeWait = MiningProbe->EventCount;
		const EGP_BeginMiningResult WaitResult = Mining->BeginMining(Node);
		Expect(WaitResult == EGP_BeginMiningResult::WaitingForSlot, TEXT("BeginMiningWaitingForSlot"));
		Expect(Mining->GetMiningState() == EGP_MiningState::WaitingForSlot, TEXT("MiningStateWaiting"));
		Expect(!MiningProbe->bLastEffectActive, TEXT("WaitingForSlotEffectInactive"));
		Expect(MiningProbe->EventCount > EventsBeforeWait, TEXT("WaitingForSlotEffectEvent"));
		Mining->StopMining(EGP_MiningStopReason::ManualStop);
		for (AGP_Worker* Holder : SlotHolders)
		{
			if (IsValid(Holder))
			{
				Node->ReleaseMiningSlot(Holder);
				Holder->Destroy();
			}
		}

		// SoT remains MiningComponent — no presentation-only replicated flag on Worker.
		Expect(Mining->GetIsReplicated(), TEXT("MiningComponentReplicatedSoT"));
		Expect(!Worker->GetMiningEffectAnchor()->GetIsReplicated(), TEXT("MiningEffectAnchorNotReplicated"));

		// ResourceNode generated prototype visual switch.
		UGP_ResourceNodeVisualComponent* NodeVisual = Node->GetResourceNodeVisualComponent();
		Expect(IsValid(NodeVisual), TEXT("NodeVisualPresent"));
		Expect(NodeVisual->ShouldUseGeneratedPrototypeVisual(), TEXT("GeneratedVisualShouldUse"));
		Expect(NodeVisual->GetGeneratedPartCount() > 0, TEXT("GeneratedPartsPresentDefault"));
		Expect(NodeVisual->AreGeneratedCollisionsDisabled(), TEXT("GeneratedNoCollision"));

		UStaticMeshComponent* AuthoredChild = NewObject<UStaticMeshComponent>(Node, TEXT("AuthoredTestMesh"));
		AuthoredChild->SetupAttachment(Node->GetCollisionBox());
		AuthoredChild->RegisterComponent();
		Expect(IsValid(AuthoredChild), TEXT("AuthoredChildCreated"));
		UBoxComponent* CollisionBefore = Node->GetCollisionBox();
		Expect(IsValid(CollisionBefore), TEXT("CollisionBoxPresent"));

		Node->SetUseGeneratedPrototypeVisual(false);
		Expect(!Node->GetUseGeneratedPrototypeVisual(), TEXT("GeneratedVisualFalse"));
		Expect(!NodeVisual->ShouldUseGeneratedPrototypeVisual(), TEXT("ShouldUseFalse"));
		Expect(NodeVisual->GetGeneratedPartCount() == 0, TEXT("GeneratedPartsCleared"));
		Expect(IsValid(AuthoredChild), TEXT("AuthoredChildSurvivedFalse"));
		Expect(Node->GetCollisionBox() == CollisionBefore, TEXT("CollisionBoxPreservedFalse"));
		Expect(Node->CanAcceptMineCommand(true), TEXT("MineContractPreservedFalse"));

		Node->SetUseGeneratedPrototypeVisual(true);
		Expect(NodeVisual->GetGeneratedPartCount() > 0, TEXT("GeneratedPartsRestored"));
		const int32 PartsAfterRestore = NodeVisual->GetGeneratedPartCount();
		Node->SetUseGeneratedPrototypeVisual(true);
		Expect(NodeVisual->GetGeneratedPartCount() == PartsAfterRestore, TEXT("NoDuplicateGeneratedParts"));
		Node->SetUseGeneratedPrototypeVisual(false);
		Expect(NodeVisual->GetGeneratedPartCount() == 0, TEXT("ToggleFalseClearsOrphans"));
		Node->SetUseGeneratedPrototypeVisual(true);
		Expect(NodeVisual->GetGeneratedPartCount() > 0, TEXT("ToggleTrueRestores"));
		Expect(IsValid(AuthoredChild), TEXT("AuthoredChildSurvivedToggle"));
		Expect(Node->GetCollisionBox() == CollisionBefore, TEXT("CollisionBoxPreservedToggle"));
		if (IsValid(AuthoredChild))
		{
			AuthoredChild->DestroyComponent();
		}

		TArray<FText> WorkerErrors;
		TArray<FText> WorkerWarnings;
		Expect(Worker->ValidateWorkerContract(WorkerErrors, WorkerWarnings), TEXT("WorkerContractValid"));
		bool bHasUnitDefWarning = false;
		for (const FText& Warning : WorkerWarnings)
		{
			if (Warning.ToString().Contains(TEXT("UGP_UnitDefinition"), ESearchCase::IgnoreCase))
			{
				bHasUnitDefWarning = true;
				break;
			}
		}
		Expect(!bHasUnitDefWarning, TEXT("NoUnitDefinitionWarning"));

		TArray<FText> BaseErrors;
		TArray<FText> BaseWarnings;
		Expect(MainBase->ValidateMainBaseContract(BaseErrors, BaseWarnings), TEXT("MainBaseContractValid"));
		bool bHasBuildingDefWarning = false;
		for (const FText& Warning : BaseWarnings)
		{
			if (Warning.ToString().Contains(TEXT("UGP_BuildingDefinition"), ESearchCase::IgnoreCase)
				|| Warning.ToString().Contains(TEXT("BuildingDefinition"), ESearchCase::IgnoreCase))
			{
				bHasBuildingDefWarning = true;
				break;
			}
		}
		Expect(!bHasBuildingDefWarning, TEXT("NoBuildingDefinitionWarning"));

		UGP_StorageComponent* Storage = MainBase->GetStorageComponent();
		Expect(IsValid(Storage), TEXT("StorageForLifecycle"));
		Expect(Storage->GetContainers().Num() == Storage->GetContainerCount(), TEXT("RuntimeContainersInitialized"));
		{
			const AGP_MainBase* BaseCDO = GetDefault<AGP_MainBase>();
			const UGP_StorageComponent* TemplateStorage =
				IsValid(BaseCDO) ? BaseCDO->GetStorageComponent() : nullptr;
			Expect(IsValid(TemplateStorage), TEXT("TemplateStoragePresent"));
			Expect(TemplateStorage->GetContainers().Num() == 0, TEXT("TemplateContainersEmpty"));
			TArray<FText> TemplateErrors;
			TArray<FText> TemplateWarnings;
			Expect(TemplateStorage->ValidateStorageContract(TemplateErrors, TemplateWarnings),
				TEXT("TemplateStorageValid"));
			bool bHasErrArraySize = false;
			for (const FText& Error : TemplateErrors)
			{
				if (Error.ToString().Contains(TEXT("array size"), ESearchCase::IgnoreCase))
				{
					bHasErrArraySize = true;
					break;
				}
			}
			Expect(!bHasErrArraySize, TEXT("TemplateNoErrArraySize"));
		}

		Worker->OnCargoVisualStateChanged.RemoveDynamic(
			Probe, &UGP_CargoVisualStateProbe::HandleCargoVisualStateChanged);
		Worker->OnMiningEffectStateChanged.RemoveDynamic(
			MiningProbe, &UGP_MiningEffectStateProbe::HandleMiningEffectStateChanged);
		Probe->RemoveFromRoot();
		MiningProbe->RemoveFromRoot();
		Worker->Destroy();
		MainBase->Destroy();
		Node->Destroy();

		UE_LOG(LogGPWorker, Log,
			TEXT("GP Resource.RunPresentationContractTest: Complete Failures=%d"),
			Failures);
		GPContractTestCoordinator::Release(Token.ExecutionId, Failures, false, TEXT("None"));
	}

	static FAutoConsoleCommandWithWorldAndArgs GResourcePresentationContract(
		TEXT("gp.Resource.RunPresentationContractTest"),
		TEXT("Authority: GP-S28P1 presentation/cargo visual contract (Worker/MainBase/Node). Transient actors only."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceRunPresentationContractTest));
}

void UGP_WorkerHaulingContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_WorkerHaulingContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_WorkerHaulingContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		Cancel(TEXT("WorldEndPlay"));
	}
}

void UGP_WorkerHaulingContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	bCancelled = false;
	CancelReason = NAME_None;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	MovementWaitTicks = 0;
	MovementWaitStartTime = -1.0;
	StaleHaulSerial = 0;
	ThreatBefore = 0.0f;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_WorkerHaulingContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPWorker, Log, TEXT("GP Worker.RunHaulingContractTest Stage=Start"));
	ScheduleNext();
}

void UGP_WorkerHaulingContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalid"));
		return;
	}
	StageTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_WorkerHaulingContractTestRunner::AdvanceStage));
}

bool UGP_WorkerHaulingContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPWorker, Error, TEXT("GP Worker.RunHaulingContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPWorker, Log, TEXT("GP Worker.RunHaulingContractTest PASS: %s"), Label);
	return true;
}

void UGP_WorkerHaulingContractTestRunner::Abort(const TCHAR* Reason)
{
	if (bFinished)
	{
		return;
	}
	UE_LOG(LogGPWorker, Error, TEXT("GP Worker.RunHaulingContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

void UGP_WorkerHaulingContractTestRunner::Cancel(const TCHAR* Reason)
{
	if (bFinished)
	{
		return;
	}
	bCancelled = true;
	CancelReason = FName(Reason);
	++Failures;
	UE_LOG(LogGPWorker, Warning, TEXT("GP Worker.RunHaulingContractTest CANCELLED: %s"), Reason);
	Finish();
}

void UGP_WorkerHaulingContractTestRunner::DestroyWeakWorker(TWeakObjectPtr<AGP_Worker>& Weak)
{
	if (AGP_Worker* Worker = Weak.Get())
	{
		if (IsValid(Worker))
		{
			if (UGP_MiningComponent* Mining = Worker->GetMiningComponent())
			{
				Mining->StopMining(EGP_MiningStopReason::ManualStop);
			}
			Worker->Destroy();
		}
	}
	Weak.Reset();
}

void UGP_WorkerHaulingContractTestRunner::DestroyWeakMainBase(TWeakObjectPtr<AGP_MainBase>& Weak)
{
	if (AGP_MainBase* Base = Weak.Get())
	{
		if (IsValid(Base))
		{
			Base->Destroy();
		}
	}
	Weak.Reset();
}

void UGP_WorkerHaulingContractTestRunner::Finish()
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
	}
	if (UWorld* World = WorldWeak.Get())
	{
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
	}
	PrimaryWorkerWeak.Reset();
	MainBaseWeak.Reset();
	EnemyBaseWeak.Reset();
	TestNodeWeak.Reset();
	UE_LOG(LogGPWorker, Log, TEXT("GP Worker.RunHaulingContractTest: Complete Failures=%d"), Failures);
	GPContractTestCoordinator::Release(ExecutionId, Failures, bCancelled, *CancelReason.ToString());
	RemoveFromRoot();
	GPWorkerDebug::GActiveHaulingContractTest.Reset();
	WorldWeak.Reset();
}

AGP_ResourceNode* UGP_WorkerHaulingContractTestRunner::SpawnNode(const FVector& Loc) const
{
	AGP_ResourceNode* Node = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
		WorldWeak.Get(),
		Loc,
		OwnerTag);
	if (IsValid(Node))
	{
		Node->Tags.AddUnique(GPResourceLoopDiagnostics::MakeTeamScenarioTag(ContractTeamId));
	}
	return Node;
}

AGP_MainBase* UGP_WorkerHaulingContractTestRunner::SpawnMainBase(const FVector& Loc, int32 TeamId) const
{
	return GPResourceLoopDiagnostics::SpawnMainBaseDeferred(WorldWeak.Get(), Loc, TeamId, OwnerTag);
}

AGP_Worker* UGP_WorkerHaulingContractTestRunner::SpawnWorker(const FVector& Loc, int32 TeamId) const
{
	return GPResourceLoopDiagnostics::SpawnWorkerDeferred(WorldWeak.Get(), Loc, TeamId, OwnerTag);
}

AGP_ResourceNode* UGP_WorkerHaulingContractTestRunner::SpawnNavigableNodeNearScenario(
	AGP_MainBase* Base,
	const AActor* AnchorActor,
	FString& OutFailReason,
	float* OutDistanceToBaseCm) const
{
	OutFailReason.Reset();
	if (OutDistanceToBaseCm != nullptr)
	{
		*OutDistanceToBaseCm = -1.0f;
	}

	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || !IsValid(Base))
	{
		OutFailReason = TEXT("MissingWorldOrBase");
		return nullptr;
	}

	const FVector BaseLoc = Base->GetActorLocation();
	const FVector AnchorLoc = IsValid(AnchorActor) ? AnchorActor->GetActorLocation() : ScenarioNodeLocation;
	const FVector AnchorFallback = AnchorLoc.IsNearlyZero() ? (BaseLoc + FVector(1800.0f, 0.0f, 0.0f)) : AnchorLoc;

	const float MineApproachDist = FMath::Max(25.0f, InteractionRangeCm - 50.0f - 25.0f);
	const float DropOffApproachDist = FMath::Max(25.0f, DropOffRangeCm - 50.0f - 25.0f);

	// Local offsets around the scenario node — keep haul distance ~1.5–3 km-uu of the diagnostic corridor.
	const FVector CandidateOffsets[] = {
		FVector(400.0f, 0.0f, 0.0f),
		FVector(-400.0f, 0.0f, 0.0f),
		FVector(0.0f, 400.0f, 0.0f),
		FVector(0.0f, -400.0f, 0.0f),
		FVector(600.0f, 200.0f, 0.0f),
		FVector(-600.0f, -200.0f, 0.0f),
		FVector(200.0f, 600.0f, 0.0f),
		FVector(-200.0f, -600.0f, 0.0f),
	};

	for (const FVector& Offset : CandidateOffsets)
	{
		const FVector Raw = AnchorFallback + Offset;
		FVector NodeProjected;
		if (!GPResourceLoopDiagnostics::IsNavPointProjected(World, Raw, &NodeProjected, 800.0f, 800.0f))
		{
			continue;
		}

		const float DistToBase = FVector::Dist(NodeProjected, BaseLoc);
		if (DistToBase < 800.0f || DistToBase > 4000.0f)
		{
			continue;
		}

		const FVector BaseToNode = (NodeProjected - BaseLoc).GetSafeNormal2D();
		const FVector NodeApproachRaw = NodeProjected - BaseToNode * MineApproachDist;
		FVector NodeApproach;
		if (!GPResourceLoopDiagnostics::IsNavPointProjected(World, NodeApproachRaw, &NodeApproach, 800.0f, 800.0f))
		{
			continue;
		}

		const FVector BaseDropRaw = BaseLoc + BaseToNode * DropOffApproachDist;
		FVector BaseDropOff;
		if (!GPResourceLoopDiagnostics::IsNavPointProjected(World, BaseDropRaw, &BaseDropOff, 800.0f, 800.0f))
		{
			continue;
		}

		FString PathFail;
		if (!GPResourceLoopDiagnostics::IsNavReachable(World, NodeApproach, BaseDropOff, &PathFail))
		{
			continue;
		}

		AGP_ResourceNode* Node = SpawnNode(NodeProjected);
		if (!IsValid(Node))
		{
			OutFailReason = TEXT("SpawnActorFailed");
			return nullptr;
		}

		// Post-spawn path re-check (approach points from actual actor location).
		const FVector SpawnedLoc = Node->GetActorLocation();
		const FVector PostApproachRaw = SpawnedLoc - (SpawnedLoc - BaseLoc).GetSafeNormal2D() * MineApproachDist;
		FVector PostApproach;
		FVector PostDropOff;
		FString PostFail;
		const bool bApproachOk = GPResourceLoopDiagnostics::IsNavPointProjected(
			World, PostApproachRaw, &PostApproach, 800.0f, 800.0f);
		const bool bDropOk = GPResourceLoopDiagnostics::IsNavPointProjected(
			World, BaseDropRaw, &PostDropOff, 800.0f, 800.0f);
		const bool bPathOk = bApproachOk && bDropOk
			&& GPResourceLoopDiagnostics::IsNavReachable(World, PostApproach, PostDropOff, &PostFail);
		if (!bPathOk)
		{
			Node->Destroy();
			OutFailReason = FString::Printf(TEXT("PostSpawnNavFailed:%s"), *PostFail);
			continue;
		}

		if (OutDistanceToBaseCm != nullptr)
		{
			*OutDistanceToBaseCm = FVector::Dist(SpawnedLoc, BaseLoc);
		}
		return Node;
	}

	OutFailReason = TEXT("NoLocalNavigableNodeCandidate");
	return nullptr;
}

void UGP_WorkerHaulingContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (bFinished || !GPContractTestCoordinator::IsTokenActive(ExecutionId))
	{
		return;
	}
	if (GPContractTestCoordinator::IsWorldTearingDown(World))
	{
		Cancel(TEXT("WorldEndPlay"));
		return;
	}
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalidDuringStage"));
		return;
	}

	using namespace GPWorkerDebug;

	auto WaitHaulOrMove = [&](AGP_Worker* Worker, const TCHAR* TimeoutLabel) -> bool
	{
		if (!IsValid(Worker))
		{
			Expect(false, TEXT("PartialStorageObjectsLost"));
			Finish();
			return true;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		const bool bBusy = (Cmd != nullptr && Cmd->IsHaulActive())
			|| (Worker->GetUnitMovementComponent() && Worker->GetUnitMovementComponent()->IsMoving())
			|| (Cmd != nullptr && Cmd->GetMineExecutionState() == EGP_MineExecutionState::Approaching);
		if (!bBusy)
		{
			return false;
		}
		if (MovementWaitTicks == 0)
		{
			MovementWaitStartTime = World->GetTimeSeconds();
		}
		++MovementWaitTicks;
		if ((World->GetTimeSeconds() - MovementWaitStartTime) > MovementWaitTimeoutSeconds)
		{
			Expect(false, TimeoutLabel);
			Finish();
			return true;
		}
		ScheduleNext();
		return true;
	};

	switch (StageIndex)
	{
	case 0:
	{
		// Navigable layout (same discovery as operator diagnostic) — not hardcoded off-mesh coords.
		// Isolates onto a free playable team when Team 1 is occupied by operator scenario.
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (!Expect(Scenario.bOk && Scenario.bReadyForHaulingTest, TEXT("SpawnNavigableHaulingScenario")))
		{
			Finish();
			return;
		}
		AGP_ResourceNode* Node = Scenario.ResourceNode;
		AGP_MainBase* Base = Scenario.MainBase;
		AGP_Worker* Worker = Scenario.Worker;
		TestNodeWeak = Node;
		MainBaseWeak = Base;
		PrimaryWorkerWeak = Worker;
		ContractTeamId = Scenario.TeamId;
		ScenarioBaseLocation = Base->GetActorLocation();
		ScenarioNodeLocation = Node->GetActorLocation();
		DropOffRangeCm = Base->GetDropOffRangeCm();
		Expect(FMath::IsNearlyEqual(DropOffRangeCm, 400.0f), TEXT("DropOffRange400"));
		if (const UGP_ResourceDefinition* Def = Node->ResolveResourceDefinition(true))
		{
			InteractionRangeCm = Def->InteractionRangeCm;
		}
		Expect(Worker->GetTeamId() == ContractTeamId && Base->GetTeamId() == ContractTeamId, TEXT("TeamMatch"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		// Cargo-full haul + return-to-deposit
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node) && IsValid(Base), TEXT("CargoFullObjects")))
		{
			Finish();
			return;
		}
		Worker->GetCargoComponent()->ClearCargo();
		IssueMine(Worker, Node);
		for (int32 i = 0; i < 5; ++i)
		{
			Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		}
		Expect(Worker->GetCargoComponent()->IsFull(), TEXT("CargoFullBeforeHaul"));
		Expect(Worker->GetUnitCommandComponent()->IsHaulActive()
			|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::ReturningToBase,
			TEXT("HaulStartedOnCargoFull"));
		Expect(Worker->GetUnitCommandComponent()->HasHeldCommand(), TEXT("HeldMineKeptDuringHaul"));
		ThreatBefore = 0.0f;
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			ThreatBefore = GS->GetFerroniteThreatValueForTeam(ContractTeamId);
		}
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (WaitHaulOrMove(Worker, TEXT("CargoFullHaulTimeout")))
		{
			return;
		}
		if (!IsValid(Worker) || !IsValid(Base) || !IsValid(Worker->GetCargoComponent())
			|| !IsValid(Worker->GetMiningComponent()))
		{
			Expect(false, TEXT("PartialStorageObjectsLost"));
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		if (!IsValid(Cmd) || !IsValid(Storage))
		{
			Expect(false, TEXT("PartialStorageObjectsLost"));
			Finish();
			return;
		}
		Expect(IsValid(Storage) && Storage->GetTotalStored() >= 49.0f, TEXT("StorageReceivedCargoFull"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("CargoEmptyAfterDropOff"));
		Expect(Cmd->GetLastHaulAcceptedAmount() >= 49.0f, TEXT("HaulAccepted50"));
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			const float ThreatAfter = GS->GetFerroniteThreatValueForTeam(ContractTeamId);
			Expect(ThreatAfter > ThreatBefore + KINDA_SMALL_NUMBER, TEXT("ThreatIncreasedOnAccepted"));
			Expect(FMath::IsNearlyEqual(Cmd->GetLastHaulThreatDelta(), Cmd->GetLastHaulAcceptedAmount() * Storage->GetThreatPerStoredUnit(), 0.05f),
				TEXT("ThreatDeltaMatchesAccepted"));
		}
		// May already be ReturningToDeposit / mining again.
		Expect(Cmd->GetHaulExecutionState() == EGP_HaulExecutionState::ReturningToDeposit
			|| Cmd->GetMineExecutionState() != EGP_MineExecutionState::Idle
			|| Worker->GetMiningComponent()->IsMining()
			|| Worker->GetMiningComponent()->IsWaitingForSlot(),
			TEXT("ReturnedOrMiningAfterHaul"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 3:
	{
		// Depleted partial haul — no return to deposit
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("DepleteObjects")))
		{
			Finish();
			return;
		}
		Worker->GetMiningComponent()->StopMining(EGP_MiningStopReason::ManualStop);
		Worker->GetCargoComponent()->ClearCargo();
		Worker->SetActorLocation(Node->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		const int32 Leave = 5;
		const int32 Consume = Node->GetCurrentAmount() - Leave;
		if (Consume > 0)
		{
			Node->ConsumeResource(Consume);
		}
		IssueMine(Worker, Node);
		Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 5.0f), TEXT("DepleteCargo5"));
		Expect(Worker->GetUnitCommandComponent()->IsHaulActive()
			|| Worker->GetWorkerActivityState() == EGP_WorkerActivityState::ReturningToBase,
			TEXT("HaulStartedOnDepletedPartial"));
		Expect(!Worker->GetUnitCommandComponent()->ShouldReturnToDepositAfterHaul(), TEXT("NoReturnOnDepleted"));
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 4:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (WaitHaulOrMove(Worker, TEXT("DepleteHaulTimeout")))
		{
			return;
		}
		if (!IsValid(Worker) || !IsValid(Base) || !IsValid(Worker->GetUnitCommandComponent()) || !IsValid(Worker->GetCargoComponent()))
		{
			Expect(false, TEXT("PartialStorageObjectsLost"));
			Finish();
			return;
		}
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("DepleteDropped"));
		Expect(!Worker->GetUnitCommandComponent()->HasHeldCommand(), TEXT("HeldClearedAfterDepleteHaul"));
		Expect(Worker->GetUnitCommandComponent()->GetHaulExecutionState() == EGP_HaulExecutionState::Idle, TEXT("HaulIdleAfterDeplete"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 5:
	{
		// Partial storage overflow LOST — FreshNode must stay in the local navigable corridor.
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_ResourceNode* AnchorNode = TestNodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Base), TEXT("PartialStorageObjects")))
		{
			Finish();
			return;
		}

		FString GeometryFail;
		float DistanceToBase = -1.0f;
		AGP_ResourceNode* FreshNode = SpawnNavigableNodeNearScenario(
			Base,
			IsValid(AnchorNode) ? AnchorNode : nullptr,
			GeometryFail,
			&DistanceToBase);
		if (!IsValid(FreshNode))
		{
			UE_LOG(LogGPWorker, Error,
				TEXT("GP Worker.RunHaulingContractTest PartialStorageGeometry FAIL Reason=%s Base=%s Anchor=%s"),
				*GeometryFail,
				*GetNameSafe(Base),
				*GetNameSafe(AnchorNode));
			Expect(false, TEXT("PartialStorageLocalGeometryFailed"));
			Finish();
			return;
		}

		if (IsValid(AnchorNode) && AnchorNode != FreshNode)
		{
			AnchorNode->Destroy();
		}
		TestNodeWeak = FreshNode;

		const FVector NodeLoc = FreshNode->GetActorLocation();
		const FVector BaseLoc = Base->GetActorLocation();
		FVector NodeProjected;
		FVector DropOffProjected;
		const bool bNodeProjected = GPResourceLoopDiagnostics::IsNavPointProjected(World, NodeLoc, &NodeProjected);
		const FVector DropRaw = BaseLoc + (NodeLoc - BaseLoc).GetSafeNormal2D() * FMath::Max(25.0f, DropOffRangeCm - 75.0f);
		const bool bDropOffProjected = GPResourceLoopDiagnostics::IsNavPointProjected(World, DropRaw, &DropOffProjected);
		FString NavFail;
		const bool bNavNodeToBase = bNodeProjected && bDropOffProjected
			&& GPResourceLoopDiagnostics::IsNavReachable(World, NodeProjected, DropOffProjected, &NavFail);
		const float ExpectedTravelSeconds = DistanceToBase / AssumedTravelSpeedCmPerSec;
		UE_LOG(LogGPWorker, Log,
			TEXT("GP Worker.RunHaulingContractTest PartialStorageGeometry: Node=%s Base=%s Distance=%.1f NodeProjected=%s DropOffProjected=%s NavNodeToBase=%s ExpectedTravelSeconds=%.2f TimeoutSeconds=%.2f"),
			*NodeLoc.ToCompactString(),
			*BaseLoc.ToCompactString(),
			DistanceToBase,
			bNodeProjected ? TEXT("true") : TEXT("false"),
			bDropOffProjected ? TEXT("true") : TEXT("false"),
			bNavNodeToBase ? TEXT("true") : TEXT("false"),
			ExpectedTravelSeconds,
			MovementWaitTimeoutSeconds);
		if (!Expect(bNavNodeToBase, TEXT("PartialStorageNavNodeToBase")))
		{
			Finish();
			return;
		}
		if (!Expect(ExpectedTravelSeconds < MovementWaitTimeoutSeconds * 0.75f, TEXT("PartialStorageTravelWithinTimeoutBudget")))
		{
			Finish();
			return;
		}

		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		if (!IsValid(Storage) || !IsValid(Worker->GetCargoComponent()) || !IsValid(Worker->GetMiningComponent())
			|| !IsValid(Worker->GetUnitCommandComponent()))
		{
			Expect(false, TEXT("PartialStorageObjectsLost"));
			Finish();
			return;
		}
		Storage->RemovePlanetaryFerronite(Storage->GetTotalStored());
		const float Cap = Storage->GetTotalCapacity();
		Storage->AddPlanetaryFerronite(Cap - 20.0f);
		Expect(FMath::IsNearlyEqual(Storage->GetTotalRemaining(), 20.0f, 0.1f), TEXT("StorageRemaining20"));
		Worker->GetCargoComponent()->ClearCargo();
		Worker->SetActorLocation(FreshNode->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		IssueMine(Worker, FreshNode);
		for (int32 i = 0; i < 5; ++i)
		{
			Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		}
		Expect(Worker->GetUnitCommandComponent()->IsHaulActive(), TEXT("PartialStorageHaulActive"));
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 6:
	{
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (WaitHaulOrMove(Worker, TEXT("PartialStorageHaulTimeout")))
		{
			return;
		}
		if (!IsValid(Worker) || !IsValid(Base) || !IsValid(Worker->GetCargoComponent()))
		{
			Expect(false, TEXT("PartialStorageObjectsLost"));
			Finish();
			return;
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		if (!IsValid(Cmd) || !IsValid(Storage))
		{
			Expect(false, TEXT("PartialStorageObjectsLost"));
			Finish();
			return;
		}
		Expect(Cmd->GetLastHaulAcceptedAmount() <= 20.0f + 0.1f, TEXT("PartialAcceptedAtMost20"));
		Expect(Cmd->GetLastHaulRejectedAmount() >= 29.0f, TEXT("PartialRejectedOverflow"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("OverflowLostClearedCargo"));
		Expect(Storage->IsStorageFull(), TEXT("StorageFullAfterPartial"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 7:
	{
		// Interruption by Move during haul
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node) && IsValid(Base), TEXT("InterruptObjects")))
		{
			Finish();
			return;
		}
		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		UGP_CargoComponent* Cargo = Worker->GetCargoComponent();
		UGP_MiningComponent* Mining = Worker->GetMiningComponent();
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		if (!IsValid(Storage) || !IsValid(Cargo) || !IsValid(Mining) || !IsValid(Cmd))
		{
			Expect(false, TEXT("OwnedActorDestroyed"));
			Finish();
			return;
		}
		Storage->RemovePlanetaryFerronite(Storage->GetTotalStored());
		Worker->GetCargoComponent()->ClearCargo();
		Worker->GetCargoComponent()->AddCargo(50.0f);
		Worker->SetActorLocation(Node->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		// Re-open haul via mine + force cycles on a node with amount
		if (Node->GetCurrentAmount() < 50)
		{
			FString ReplaceFail;
			AGP_ResourceNode* NewNode = SpawnNavigableNodeNearScenario(Base, Node, ReplaceFail);
			if (!IsValid(NewNode))
			{
				UE_LOG(LogGPWorker, Error,
					TEXT("GP Worker.RunHaulingContractTest InterruptReplaceNode FAIL Reason=%s"),
					*ReplaceFail);
				Expect(false, TEXT("InterruptLocalGeometryFailed"));
				Finish();
				return;
			}
			if (IsValid(Node))
			{
				Node->Destroy();
			}
			TestNodeWeak = NewNode;
			Node = NewNode;
		}
		Worker->GetCargoComponent()->ClearCargo();
		Worker->SetActorLocation(Node->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		IssueMine(Worker, Node);
		for (int32 i = 0; i < 5; ++i)
		{
			Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		}
		Expect(Worker->GetUnitCommandComponent()->IsHaulActive(), TEXT("InterruptHaulActive"));
		IssueMove(Worker, Worker->GetActorLocation() + FVector(0.0f, 1500.0f, 0.0f));
		Expect(!Worker->GetUnitCommandComponent()->IsHaulActive(), TEXT("MoveClearsHaul"));
		Expect(Worker->GetCargoComponent()->IsFull(), TEXT("InterruptKeepsCargo"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 8:
	{
		// Stale callback ignored: force haul, capture serial, replace command, ensure idle
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node), TEXT("StaleObjects")))
		{
			Finish();
			return;
		}
		if (!IsValid(Worker->GetCargoComponent()) || !IsValid(Worker->GetMiningComponent())
			|| !IsValid(Worker->GetUnitCommandComponent()))
		{
			Expect(false, TEXT("OwnedActorDestroyed"));
			Finish();
			return;
		}
		Worker->GetCargoComponent()->ClearCargo();
		IssueMine(Worker, Node);
		for (int32 i = 0; i < 5 && !Worker->GetCargoComponent()->IsFull(); ++i)
		{
			Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		}
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		StaleHaulSerial = Cmd->GetActiveHaulSerial();
		Expect(StaleHaulSerial != 0, TEXT("StaleSerialCaptured"));
		IssueMove(Worker, Worker->GetActorLocation() + FVector(500.0f, 0.0f, 0.0f));
		Expect(Cmd->GetActiveHaulSerial() == 0, TEXT("StaleSerialClearedAfterReplace"));
		Expect(!Cmd->IsHaulActive(), TEXT("StaleHaulInactive"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 9:
	{
		// Ownership: only enemy-team MainBase registered → haul must fail
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_MainBase* Friendly = MainBaseWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Friendly) && IsValid(Node), TEXT("OwnershipObjects")))
		{
			Finish();
			return;
		}
		if (!IsValid(Worker->GetCargoComponent()) || !IsValid(Worker->GetMiningComponent())
			|| !IsValid(Worker->GetUnitCommandComponent()))
		{
			Expect(false, TEXT("OwnedActorDestroyed"));
			Finish();
			return;
		}
		DestroyWeakMainBase(MainBaseWeak);
		const int32 EnemyTeamId = (ContractTeamId == 8) ? 7 : 8;
		// Enemy base only needs to exist/register for another team — keep it local to the scenario.
		const FVector EnemyLoc = ScenarioBaseLocation + FVector(0.0f, 700.0f, 0.0f);
		AGP_MainBase* Enemy = SpawnMainBase(EnemyLoc, EnemyTeamId);
		EnemyBaseWeak = Enemy;
		Expect(IsValid(Enemy), TEXT("EnemyBaseSpawned"));
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			Expect(GS->FindMainBaseForTeam(ContractTeamId) == nullptr, TEXT("NoContractTeamMainBase"));
		}
		Worker->SetTeamId(ContractTeamId);
		Worker->GetCargoComponent()->ClearCargo();
		Worker->SetActorLocation(Node->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		IssueMine(Worker, Node);
		for (int32 i = 0; i < 5; ++i)
		{
			Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		}
		Expect(!Worker->GetUnitCommandComponent()->IsHaulActive(), TEXT("OwnershipHaulFailedImmediately"));
		Expect(!Worker->GetUnitCommandComponent()->HasHeldCommand(), TEXT("OwnershipClearedHeld"));
		MovementWaitTicks = 0;
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 10:
	{
		// Restore friendly base for lifecycle stage at original scenario base location.
		DestroyWeakMainBase(EnemyBaseWeak);
		FVector RestoreLoc = ScenarioBaseLocation;
		GPResourceLoopDiagnostics::IsNavPointProjected(World, ScenarioBaseLocation, &RestoreLoc, 800.0f, 800.0f);
		MainBaseWeak = SpawnMainBase(RestoreLoc, ContractTeamId);
		Expect(IsValid(MainBaseWeak.Get()), TEXT("RestoreFriendlyBase"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 11:
	{
		// Lifecycle: EndPlay while hauling releases cleanly
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_ResourceNode* Node = TestNodeWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		if (!Expect(IsValid(Worker) && IsValid(Node) && IsValid(Base), TEXT("LifecycleObjects")))
		{
			Finish();
			return;
		}
		Worker->GetCargoComponent()->ClearCargo();
		Worker->SetActorLocation(Node->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		IssueMine(Worker, Node);
		for (int32 i = 0; i < 5; ++i)
		{
			Worker->GetMiningComponent()->DebugForceExecuteMiningCycle();
		}
		Expect(Worker->GetUnitCommandComponent()->IsHaulActive() || Worker->GetCargoComponent()->IsFull(),
			TEXT("LifecycleHaulOrFull"));
		Worker->Destroy();
		PrimaryWorkerWeak.Reset();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 12:
	{
		Expect(!PrimaryWorkerWeak.IsValid(), TEXT("LifecycleWorkerDestroyed"));
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

void UGP_DiagnosticScenarioContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_DiagnosticScenarioContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_DiagnosticScenarioContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		Cancel(TEXT("WorldEndPlay"));
	}
}

void UGP_DiagnosticScenarioContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	bCancelled = false;
	CancelReason = NAME_None;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	ContractTeamId = 1;
	bOperatorTeam1PresentAtStart = false;
	OperatorTeam1MainBaseWeak.Reset();
	RejectedMainBaseWeak.Reset();
	ReplacementMainBaseWeak.Reset();
	MainBaseWeak.Reset();
	WorkerWeak.Reset();
	NodeWeak.Reset();

	if (IsValid(InWorld))
	{
		if (AGP_GameState* GS = InWorld->GetGameState<AGP_GameState>())
		{
			GS->PruneInvalidMainBaseRegistrations();
			if (AGP_MainBase* OpBase = GS->FindMainBaseForTeam(1))
			{
				bOperatorTeam1PresentAtStart =
					GPResourceLoopDiagnostics::ActorHasOwnerTag(OpBase, GPContractTestCoordinator::OwnerTagOperator)
					|| !OpBase->Tags.Contains(GPResourceLoopDiagnostics::TagOwnedByContract);
				OperatorTeam1MainBaseWeak = OpBase;
			}
		}
	}

	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_DiagnosticScenarioContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPWorker, Log,
		TEXT("GP Resource.RunDiagnosticScenarioContractTest Stage=Start OperatorTeam1Present=%s OperatorMainBase=%s"),
		bOperatorTeam1PresentAtStart ? TEXT("true") : TEXT("false"),
		*GetNameSafe(OperatorTeam1MainBaseWeak.Get()));
	ScheduleNext();
}

void UGP_DiagnosticScenarioContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalid"));
		return;
	}
	StageTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_DiagnosticScenarioContractTestRunner::AdvanceStage));
}

bool UGP_DiagnosticScenarioContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPWorker, Error, TEXT("GP Resource.RunDiagnosticScenarioContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPWorker, Log, TEXT("GP Resource.RunDiagnosticScenarioContractTest PASS: %s"), Label);
	return true;
}

void UGP_DiagnosticScenarioContractTestRunner::Abort(const TCHAR* Reason)
{
	if (bFinished)
	{
		return;
	}
	UE_LOG(LogGPWorker, Error, TEXT("GP Resource.RunDiagnosticScenarioContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

void UGP_DiagnosticScenarioContractTestRunner::Cancel(const TCHAR* Reason)
{
	if (bFinished)
	{
		return;
	}
	bCancelled = true;
	CancelReason = FName(Reason);
	++Failures;
	UE_LOG(LogGPWorker, Warning, TEXT("GP Resource.RunDiagnosticScenarioContractTest CANCELLED: %s"), Reason);
	Finish();
}

void UGP_DiagnosticScenarioContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
		GPResourceLoopDiagnostics::CleanupScenarioByOwnerTag(World, OwnerTag);
		if (AGP_MainBase* Rejected = RejectedMainBaseWeak.Get())
		{
			if (IsValid(Rejected))
			{
				Rejected->Destroy();
			}
		}
		if (AGP_MainBase* Replacement = ReplacementMainBaseWeak.Get())
		{
			if (IsValid(Replacement))
			{
				Replacement->Destroy();
			}
		}
	}
	UnbindWorldCleanup();
	UE_LOG(LogGPWorker, Log, TEXT("GP Resource.RunDiagnosticScenarioContractTest: Complete Failures=%d"), Failures);
	GPContractTestCoordinator::Release(ExecutionId, Failures, bCancelled, *CancelReason.ToString());
	RemoveFromRoot();
	GPWorkerDebug::GActiveDiagnosticScenarioContractTest.Reset();
	WorldWeak.Reset();
	MainBaseWeak.Reset();
	RejectedMainBaseWeak.Reset();
	ReplacementMainBaseWeak.Reset();
	WorkerWeak.Reset();
	NodeWeak.Reset();
	OperatorTeam1MainBaseWeak.Reset();
}

void UGP_DiagnosticScenarioContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (bFinished || !GPContractTestCoordinator::IsTokenActive(ExecutionId))
	{
		return;
	}
	if (GPContractTestCoordinator::IsWorldTearingDown(World))
	{
		Cancel(TEXT("WorldEndPlay"));
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
		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1, OwnerTag);
		if (Scenario.Error == TEXT("BlockedByOccupiedPlayableTeams"))
		{
			Expect(false, TEXT("BlockedByOccupiedPlayableTeams"));
			Finish();
			return;
		}
		if (!Expect(Scenario.bOk, TEXT("DiagnosticScenarioSpawnOk")))
		{
			Finish();
			return;
		}

		ContractTeamId = Scenario.TeamId;
		MainBaseWeak = Scenario.MainBase;
		WorkerWeak = Scenario.Worker;
		NodeWeak = Scenario.ResourceNode;

		Expect(Scenario.bReadyForHaulingTest, TEXT("ReadyForHaulingTest"));
		Expect(Scenario.bNavSystemPresent, TEXT("NavigationSystemPresent"));
		Expect(Scenario.bWorkerProjected, TEXT("WorkerProjected"));
		Expect(Scenario.bNodeApproachProjected, TEXT("NodeApproachProjected"));
		Expect(Scenario.bBaseDropOffProjected, TEXT("BaseDropOffProjected"));
		Expect(Scenario.bNavWorkerToNode, TEXT("WorkerToNodePathReachable"));
		Expect(Scenario.bNavNodeToBase, TEXT("NodeToBasePathReachable"));
		Expect(Scenario.bNavBaseToNode, TEXT("BaseToNodePathReachable"));

		Expect(IsValid(Scenario.MainBase) && Scenario.MainBase->GetTeamId() == ContractTeamId, TEXT("MainBaseContractTeam"));
		Expect(IsValid(Scenario.Worker) && Scenario.Worker->GetTeamId() == ContractTeamId, TEXT("WorkerContractTeam"));
		Expect(IsValid(Scenario.ResourceNode) && Scenario.ResourceNode->GetCurrentAmount() > 0, TEXT("NodeMineableAmount"));
		Expect(Scenario.ResourceNode->HasAnyFlags(RF_Transient), TEXT("NodeTransient"));
		Expect(Scenario.MainBase->HasAnyFlags(RF_Transient), TEXT("MainBaseTransient"));
		Expect(Scenario.Worker->HasAnyFlags(RF_Transient), TEXT("WorkerTransient"));
		Expect(GPResourceLoopDiagnostics::ActorHasOwnerTag(Scenario.MainBase, OwnerTag), TEXT("MainBaseOwnerTag"));
		Expect(GPResourceLoopDiagnostics::ActorHasOwnerTag(Scenario.Worker, OwnerTag), TEXT("WorkerOwnerTag"));
		Expect(GPResourceLoopDiagnostics::ActorHasOwnerTag(Scenario.ResourceNode, OwnerTag), TEXT("NodeOwnerTag"));

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (!Expect(IsValid(GS), TEXT("GameStatePresent")))
		{
			Finish();
			return;
		}

		Expect(GS->FindMainBaseForTeam(ContractTeamId) == Scenario.MainBase, TEXT("FirstMainBaseRegistered"));
		Expect(GS->CountRegisteredMainBasesForTeam(ContractTeamId) == 1, TEXT("RegistryCountRemainsOne"));
		Expect(GS->IsMainBaseRegistryUniqueForTeam(ContractTeamId), TEXT("RegistryUniqueForContractTeam"));
		Expect(Scenario.Worker->GetTeamId() != -1 && Scenario.MainBase->GetTeamId() != -1, TEXT("NoUnassignedTeam"));

		const AGP_GameState::EGP_MainBaseRegisterResult Idempotent =
			GS->RegisterMainBase(Scenario.MainBase);
		Expect(Idempotent == AGP_GameState::EGP_MainBaseRegisterResult::AlreadyRegistered, TEXT("SameActorRegistrationIdempotent"));
		Expect(GS->CountRegisteredMainBasesForTeam(ContractTeamId) == 1, TEXT("IdempotentCountStillOne"));

		if (bOperatorTeam1PresentAtStart)
		{
			Expect(ContractTeamId != 1, TEXT("ContractRemappedOffOccupiedTeam1"));
			Expect(OperatorTeam1MainBaseWeak.IsValid() && IsValid(OperatorTeam1MainBaseWeak.Get()), TEXT("OperatorTeam1PreservedAfterContractSpawn"));
			Expect(GS->FindMainBaseForTeam(1) == OperatorTeam1MainBaseWeak.Get(), TEXT("OperatorTeam1RegistryUnchanged"));
		}

		const GPResourceLoopDiagnostics::FGP_ScenarioValidation Validation =
			GPResourceLoopDiagnostics::ValidateHaulingScenario(World, ContractTeamId, Scenario.Worker);
		Expect(Validation.bWorkerHasMainBase, TEXT("WorkerHasMainBase"));
		Expect(Validation.bMainBaseRegisteredForTeam, TEXT("MainBaseRegisteredForTeam"));
		Expect(Validation.MainBaseCountForWorkerTeam == 1, TEXT("ValidationMainBaseCountOne"));
		Expect(Validation.bRegistryUniqueForTeam, TEXT("ValidationRegistryUnique"));
		Expect(Validation.bResolvedMainBaseMatchesListedBase, TEXT("ValidationResolvedMatchesListed"));
		Expect(Validation.bWorkerAndBaseSameTeam, TEXT("WorkerAndBaseSameTeam"));
		Expect(Validation.bNodeMineable, TEXT("NodeMineable"));
		Expect(Validation.bReadyForHaulingTest, TEXT("ValidationReadyForHaulingTest"));
		Expect(Validation.bNavReachableWorkerToNode, TEXT("ValidationWorkerToNode"));
		Expect(Validation.bNavReachableNodeToBase, TEXT("ValidationNodeToBase"));
		Expect(Validation.bNavReachableBaseToNode, TEXT("ValidationBaseToNode"));

		// Team change refresh: reassign then restore must not leave stale registry / disturb Team1 operator.
		const int32 TempTeam = 99;
		Scenario.MainBase->SetTeamId(TempTeam);
		Expect(GS->FindMainBaseForTeam(TempTeam) == Scenario.MainBase, TEXT("RegisteredUnderTempTeam"));
		Expect(GS->FindMainBaseForTeam(ContractTeamId) != Scenario.MainBase, TEXT("NoStaleOnContractTeam"));
		if (bOperatorTeam1PresentAtStart)
		{
			Expect(GS->FindMainBaseForTeam(1) == OperatorTeam1MainBaseWeak.Get(), TEXT("OperatorTeam1StableDuringTempTeam"));
		}
		Scenario.MainBase->SetTeamId(ContractTeamId);
		Expect(Scenario.MainBase->GetTeamId() == ContractTeamId, TEXT("RestoredContractTeam"));
		Expect(GS->FindMainBaseForTeam(TempTeam) == nullptr, TEXT("TempTeamCleared"));
		Expect(GS->FindMainBaseForTeam(ContractTeamId) == Scenario.MainBase, TEXT("RestoredRegistryResolve"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		// Duplicate MainBase rejection stage (production registry invariant).
		AGP_MainBase* BaseA = MainBaseWeak.Get();
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (!Expect(IsValid(BaseA) && IsValid(GS), TEXT("DuplicateStageObjects")))
		{
			Finish();
			return;
		}

		Expect(GS->FindMainBaseForTeam(ContractTeamId) == BaseA, TEXT("ExistingMainBasePreserved"));
		Expect(GS->CountRegisteredMainBasesForTeam(ContractTeamId) == 1, TEXT("RegistryCountRemainsOneBeforeDuplicate"));

		const FVector DupLoc = BaseA->GetActorLocation() + FVector(300.0f, 0.0f, 0.0f);
		AGP_MainBase* BaseB = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(
			World, DupLoc, ContractTeamId, OwnerTag);
		RejectedMainBaseWeak = BaseB;
		if (!Expect(IsValid(BaseB), TEXT("DuplicateCandidateSpawned")))
		{
			Finish();
			return;
		}

		const AGP_GameState::EGP_MainBaseRegisterResult DupResult = GS->RegisterMainBase(BaseB);
		Expect(DupResult == AGP_GameState::EGP_MainBaseRegisterResult::RejectedDuplicate, TEXT("DuplicateMainBaseRejected"));
		Expect(GS->CountRegisteredMainBasesForTeam(ContractTeamId) == 1, TEXT("RegistryCountRemainsOne"));
		Expect(GS->FindMainBaseForTeam(ContractTeamId) == BaseA, TEXT("ExistingMainBasePreserved"));
		Expect(GS->IsMainBaseRegistryUniqueForTeam(ContractTeamId), TEXT("RegistryStillUniqueAfterReject"));

		BaseB->Destroy();
		RejectedMainBaseWeak.Reset();
		Expect(GS->FindMainBaseForTeam(ContractTeamId) == BaseA, TEXT("RejectedBaseCleanupDoesNotRemoveExisting"));
		Expect(GS->CountRegisteredMainBasesForTeam(ContractTeamId) == 1, TEXT("CountOneAfterRejectedCleanup"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		AGP_MainBase* BaseA = MainBaseWeak.Get();
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		if (!Expect(IsValid(GS), TEXT("ReplacementStageGameState")))
		{
			Finish();
			return;
		}

		if (IsValid(BaseA))
		{
			BaseA->Destroy();
		}
		MainBaseWeak.Reset();
		Expect(GS->FindMainBaseForTeam(ContractTeamId) == nullptr, TEXT("DestroyedExistingClearsRegistry"));
		Expect(GS->CountRegisteredMainBasesForTeam(ContractTeamId) == 0, TEXT("RegistryEmptyAfterDestroy"));

		const FVector ReplaceLoc = IsValid(Worker)
			? Worker->GetActorLocation() + FVector(-400.0f, 0.0f, 0.0f)
			: FVector::ZeroVector;
		AGP_MainBase* BaseC = GPResourceLoopDiagnostics::SpawnMainBaseDeferred(
			World, ReplaceLoc, ContractTeamId, OwnerTag);
		ReplacementMainBaseWeak = BaseC;
		if (!Expect(IsValid(BaseC), TEXT("ReplacementBaseSpawned")))
		{
			Finish();
			return;
		}
		Expect(GS->FindMainBaseForTeam(ContractTeamId) == BaseC, TEXT("ReplacementAfterCleanupSucceeds"));
		Expect(GS->CountRegisteredMainBasesForTeam(ContractTeamId) == 1, TEXT("ReplacementCountOne"));

		const AGP_GameState::EGP_MainBaseRegisterResult Again = GS->RegisterMainBase(BaseC);
		Expect(Again == AGP_GameState::EGP_MainBaseRegisterResult::AlreadyRegistered, TEXT("ReplacementIdempotent"));

		GPResourceLoopDiagnostics::DestroyDiagnosticScenarioActors(BaseC, Worker, Node);
		ReplacementMainBaseWeak.Reset();
		WorkerWeak.Reset();
		NodeWeak.Reset();
		Expect(GS->FindMainBaseForTeam(ContractTeamId) == nullptr
			|| !GPResourceLoopDiagnostics::ActorHasOwnerTag(GS->FindMainBaseForTeam(ContractTeamId), OwnerTag),
			TEXT("ContractBaseUnregistered"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 3:
	{
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		Expect(!MainBaseWeak.IsValid() && !WorkerWeak.IsValid() && !NodeWeak.IsValid(), TEXT("ContractActorsDestroyed"));
		if (bOperatorTeam1PresentAtStart)
		{
			Expect(OperatorTeam1MainBaseWeak.IsValid() && IsValid(OperatorTeam1MainBaseWeak.Get()), TEXT("OperatorTeam1PreservedAfterContract"));
			if (IsValid(GS))
			{
				Expect(GS->FindMainBaseForTeam(1) == OperatorTeam1MainBaseWeak.Get(), TEXT("OperatorTeam1RegistryIntact"));
				Expect(GS->CountRegisteredMainBasesForTeam(1) == 1, TEXT("OperatorTeam1CountOne"));
				Expect(GS->IsMainBaseRegistryUniqueForTeam(1), TEXT("OperatorTeam1RegistryUnique"));
			}
		}
		else if (IsValid(GS))
		{
			AGP_MainBase* Remaining = GS->FindMainBaseForTeam(1);
			Expect(Remaining == nullptr || !GPResourceLoopDiagnostics::ActorHasOwnerTag(Remaining, OwnerTag),
				TEXT("NoContractResidueOnTeam1"));
		}
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else
void UGP_WorkerContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}

void UGP_WorkerContractTestRunner::Start(UWorld* InWorld)
{
	(void)InWorld;
}

void UGP_WorkerContractTestRunner::ScheduleNext() {}
void UGP_WorkerContractTestRunner::AdvanceStage() {}
bool UGP_WorkerContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_WorkerContractTestRunner::Abort(const TCHAR* Reason)
{
	(void)Reason;
}
void UGP_WorkerContractTestRunner::Finish()
{
	bFinished = true;
}
void UGP_WorkerContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_WorkerContractTestRunner::UnbindWorldCleanup() {}
void UGP_WorkerContractTestRunner::DestroyWeakWorker(TWeakObjectPtr<AGP_Worker>& Weak)
{
	Weak.Reset();
}
AGP_ResourceNode* UGP_WorkerContractTestRunner::SpawnNode(const FVector& Loc) const
{
	(void)Loc;
	return nullptr;
}

void UGP_WorkerHaulingContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_WorkerHaulingContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_WorkerHaulingContractTestRunner::ScheduleNext() {}
void UGP_WorkerHaulingContractTestRunner::AdvanceStage() {}
bool UGP_WorkerHaulingContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_WorkerHaulingContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_WorkerHaulingContractTestRunner::Cancel(const TCHAR* Reason) { (void)Reason; }
void UGP_WorkerHaulingContractTestRunner::Finish() { bFinished = true; }
void UGP_WorkerHaulingContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_WorkerHaulingContractTestRunner::UnbindWorldCleanup() {}
void UGP_WorkerHaulingContractTestRunner::DestroyWeakWorker(TWeakObjectPtr<AGP_Worker>& Weak) { Weak.Reset(); }
void UGP_WorkerHaulingContractTestRunner::DestroyWeakMainBase(TWeakObjectPtr<AGP_MainBase>& Weak) { Weak.Reset(); }
AGP_ResourceNode* UGP_WorkerHaulingContractTestRunner::SpawnNode(const FVector& Loc) const
{
	(void)Loc;
	return nullptr;
}
AGP_MainBase* UGP_WorkerHaulingContractTestRunner::SpawnMainBase(const FVector& Loc, int32 TeamId) const
{
	(void)Loc;
	(void)TeamId;
	return nullptr;
}
AGP_Worker* UGP_WorkerHaulingContractTestRunner::SpawnWorker(const FVector& Loc, int32 TeamId) const
{
	(void)Loc;
	(void)TeamId;
	return nullptr;
}
AGP_ResourceNode* UGP_WorkerHaulingContractTestRunner::SpawnNavigableNodeNearScenario(
	AGP_MainBase* Base,
	const AActor* AnchorActor,
	FString& OutFailReason,
	float* OutDistanceToBaseCm) const
{
	(void)Base;
	(void)AnchorActor;
	OutFailReason = TEXT("ShippingStub");
	if (OutDistanceToBaseCm != nullptr)
	{
		*OutDistanceToBaseCm = -1.0f;
	}
	return nullptr;
}

void UGP_DiagnosticScenarioContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_DiagnosticScenarioContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_DiagnosticScenarioContractTestRunner::ScheduleNext() {}
void UGP_DiagnosticScenarioContractTestRunner::AdvanceStage() {}
bool UGP_DiagnosticScenarioContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_DiagnosticScenarioContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_DiagnosticScenarioContractTestRunner::Cancel(const TCHAR* Reason) { (void)Reason; }
void UGP_DiagnosticScenarioContractTestRunner::Finish() { bFinished = true; }
void UGP_DiagnosticScenarioContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_DiagnosticScenarioContractTestRunner::UnbindWorldCleanup() {}
#endif
