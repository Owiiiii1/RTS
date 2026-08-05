// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#include "Components/CapsuleComponent.h"
#include "Command/GPUnitCommand.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceNode.h"
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

EGP_WorkerActivityState AGP_Worker::GetWorkerActivityState() const
{
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

	if (const UGP_UnitCommandComponent* Commands = GetUnitCommandComponent())
	{
		if (Commands->GetMineExecutionState() == EGP_MineExecutionState::Approaching)
		{
			return EGP_WorkerActivityState::MovingToMine;
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

	OutWarnings.Add(NSLOCTEXT("GPWorker", "WarnNoUnitDefinitionAsset",
		"No UGP_UnitDefinition Worker asset in project (known limitation)."));

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
		case EGP_WorkerActivityState::CommandFailed: return TEXT("CommandFailed");
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

		const FString NodeName = Args.Num() > 0 ? Args[0] : FString();
		AGP_ResourceNode* Node = FindNode(World, NodeName);
		const FVector Location = IsValid(Node)
			? Node->GetActorLocation() + FVector(100.0f, 0.0f, 0.0f)
			: FVector(-47000.0f, 0.0f, 100.0f);

		AGP_Worker* Worker = SpawnWorkerAt(World, Location);
		UE_LOG(LogGPWorker, Log,
			TEXT("GP Worker.SpawnDiagnostic: Worker=%s Node=%s Loc=%s"),
			*GetNameSafe(Worker),
			*GetNameSafe(Node),
			*Location.ToCompactString());
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
			TEXT("GP Worker.Inspect: Owner=%s Path=%s Class=%s Role=%s NetMode=%s HasAuthority=%s TeamId=%d Selectable=%s Activity=%s HeldTag=%s HeldSerial=%u PendingMineNode=%s MiningState=%s MiningStop=%s Cargo=%.1f/%.1f Remaining=%.1f Distance=%.1f InteractionRangeCm=%.1f InRange=%s Moving=%s MoveDest=%s ApproachDestination=%s ApproachDesiredNodeDistance=%.1f ApproachSafetyMargin=%.1f ApproachAttempt=%d PredictedWorstCaseDistance=%.1f LastArrivalDistance=%.1f LastArrivalRangeError=%.1f MineExec=%s ActiveMineSerial=%u HasActiveSlot=%s WaitingSlot=%s MiningTimer=%s WorkerTick=%s MoveTick=%s CargoTick=%s MiningTick=%s ValidationOk=%s Errors=%d Warnings=%d Caps=%s"),
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
		Finish();
	}
}

void UGP_WorkerContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
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
		if (GEngine)
		{
			GEngine->Exec(World, TEXT("gp.Cargo.RunContractTest"));
		}
		Expect(true, TEXT("CargoRegressionInvoked"));
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
		UGP_WorkerContractTestRunner* Runner = NewObject<UGP_WorkerContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveWorkerContractTest = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GWorkerSpawn(
		TEXT("gp.Worker.SpawnDiagnostic"),
		TEXT("Authority: spawn transient AGP_Worker near ResourceNode."),
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
#endif
