// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#include "Buildings/GPMainBase.h"
#include "Components/CapsuleComponent.h"
#include "Command/GPUnitCommand.h"
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
		FString OptionalNodeName;
		if (Args.Num() > 0)
		{
			int32 ParsedTeam = 1;
			if (LexTryParseString(ParsedTeam, *Args[0]) && ParsedTeam >= 1)
			{
				TeamId = ParsedTeam;
			}
			else
			{
				OptionalNodeName = Args[0];
			}
		}

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		AGP_MainBase* ExistingBase = GS != nullptr ? GS->FindMainBaseForTeam(TeamId) : nullptr;
		if (!IsValid(ExistingBase))
		{
			const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
				GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, TeamId);
			if (!Scenario.bOk)
			{
				UE_LOG(LogGPWorker, Error,
					TEXT("GP Worker.SpawnDiagnostic: MainBase missing for TeamId=%d — full scenario failed (%s). Use gp.Resource.SpawnDiagnosticScenario %d"),
					TeamId,
					*Scenario.Error,
					TeamId);
				return;
			}
			UE_LOG(LogGPWorker, Log,
				TEXT("GP Worker.SpawnDiagnostic: CreatedFullScenario Worker=%s TeamId=%d Node=%s MainBase=%s WorkerLoc=%s NodeLoc=%s BaseLoc=%s"),
				*GetNameSafe(Scenario.Worker),
				Scenario.Worker != nullptr ? Scenario.Worker->GetTeamId() : -1,
				*GetNameSafe(Scenario.ResourceNode),
				*GetNameSafe(Scenario.MainBase),
				Scenario.Worker != nullptr ? *Scenario.Worker->GetActorLocation().ToCompactString() : TEXT("n/a"),
				Scenario.ResourceNode != nullptr ? *Scenario.ResourceNode->GetActorLocation().ToCompactString() : TEXT("n/a"),
				Scenario.MainBase != nullptr ? *Scenario.MainBase->GetActorLocation().ToCompactString() : TEXT("n/a"));
			return;
		}

		AGP_ResourceNode* Node = !OptionalNodeName.IsEmpty() ? FindNode(World, OptionalNodeName) : nullptr;
		if (!IsValid(Node))
		{
			Node = GPResourceLoopDiagnostics::SpawnResourceNodeTransient(
				World,
				GPResourceLoopDiagnostics::GetDiagnosticResourceNodeLocation());
		}
		if (!IsValid(Node))
		{
			UE_LOG(LogGPWorker, Error,
				TEXT("GP Worker.SpawnDiagnostic: failed to create ResourceNode. Use gp.Resource.SpawnDiagnosticScenario %d"),
				TeamId);
			return;
		}

		AGP_Worker* Worker = GPResourceLoopDiagnostics::SpawnWorkerDeferred(
			World,
			Node->GetActorLocation() + FVector(150.0f, 0.0f, 0.0f),
			TeamId);
		if (!IsValid(Worker) || Worker->GetTeamId() != TeamId)
		{
			UE_LOG(LogGPWorker, Error, TEXT("GP Worker.SpawnDiagnostic: Worker spawn/team failed TeamId=%d"), TeamId);
			return;
		}

		UE_LOG(LogGPWorker, Log,
			TEXT("GP Worker.SpawnDiagnostic: Worker=%s TeamId=%d Node=%s MainBase=%s WorkerLoc=%s NodeLoc=%s BaseLoc=%s"),
			*Worker->GetName(),
			Worker->GetTeamId(),
			*Node->GetName(),
			*GetNameSafe(ExistingBase),
			*Worker->GetActorLocation().ToCompactString(),
			*Node->GetActorLocation().ToCompactString(),
			*ExistingBase->GetActorLocation().ToCompactString());
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
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			AGP_Worker* Worker = *It;
			if (!IsValid(Worker))
			{
				continue;
			}
			++WorkerCount;
			if (PrimaryWorker == nullptr || Worker->GetTeamId() >= 1)
			{
				PrimaryWorker = Worker;
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
			TEXT("GP Worker.List ScenarioValidation: PlayableTeamValid=%s WorkerHasMainBase=%s WorkerHasResourceNode=%s MainBaseRegisteredForTeam=%s WorkerAndBaseSameTeam=%s NodeMineable=%s NavReachableWorkerToNode=%s NavReachableNodeToBase=%s ReadyForHaulingTest=%s Errors=%d Warnings=%d"),
			Validation.bPlayableTeamValid ? TEXT("true") : TEXT("false"),
			Validation.bWorkerHasMainBase ? TEXT("true") : TEXT("false"),
			Validation.bWorkerHasResourceNode ? TEXT("true") : TEXT("false"),
			Validation.bMainBaseRegisteredForTeam ? TEXT("true") : TEXT("false"),
			Validation.bWorkerAndBaseSameTeam ? TEXT("true") : TEXT("false"),
			Validation.bNodeMineable ? TEXT("true") : TEXT("false"),
			Validation.bNavReachableWorkerToNode ? TEXT("true") : TEXT("false"),
			Validation.bNavReachableNodeToBase ? TEXT("true") : TEXT("false"),
			Validation.bReadyForHaulingTest ? TEXT("true") : TEXT("false"),
			Validation.Errors,
			Validation.Warnings);
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
		UGP_WorkerHaulingContractTestRunner* Runner = NewObject<UGP_WorkerHaulingContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveHaulingContractTest = Runner;
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
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, TeamId);
		if (!Scenario.bOk)
		{
			UE_LOG(LogGPWorker, Error,
				TEXT("GP Resource.SpawnDiagnosticScenario: FAILED TeamId=%d Error=%s"),
				TeamId,
				*Scenario.Error);
			return;
		}

		const GPResourceLoopDiagnostics::FGP_ScenarioValidation Validation =
			GPResourceLoopDiagnostics::ValidateHaulingScenario(World, TeamId, Scenario.Worker);
		UE_LOG(LogGPWorker, Log,
			TEXT("GP Resource.SpawnDiagnosticScenario: ReadyForHaulingTest=%s Errors=%d Warnings=%d"),
			Validation.bReadyForHaulingTest ? TEXT("true") : TEXT("false"),
			Validation.Errors,
			Validation.Warnings);
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
		UGP_DiagnosticScenarioContractTestRunner* Runner = NewObject<UGP_DiagnosticScenarioContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveDiagnosticScenarioContractTest = Runner;
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
		Finish();
	}
}

void UGP_WorkerHaulingContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
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
	DestroyWeakWorker(PrimaryWorkerWeak);
	DestroyWeakMainBase(MainBaseWeak);
	DestroyWeakMainBase(EnemyBaseWeak);
	if (AGP_ResourceNode* Node = TestNodeWeak.Get())
	{
		if (IsValid(Node))
		{
			Node->Destroy();
		}
	}
	TestNodeWeak.Reset();
	UE_LOG(LogGPWorker, Log, TEXT("GP Worker.RunHaulingContractTest: Complete Failures=%d"), Failures);
	RemoveFromRoot();
	GPWorkerDebug::GActiveHaulingContractTest.Reset();
	WorldWeak.Reset();
}

AGP_ResourceNode* UGP_WorkerHaulingContractTestRunner::SpawnNode(const FVector& Loc) const
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

AGP_MainBase* UGP_WorkerHaulingContractTestRunner::SpawnMainBase(const FVector& Loc, int32 TeamId) const
{
	return GPResourceLoopDiagnostics::SpawnMainBaseDeferred(WorldWeak.Get(), Loc, TeamId);
}

AGP_Worker* UGP_WorkerHaulingContractTestRunner::SpawnWorker(const FVector& Loc, int32 TeamId) const
{
	return GPResourceLoopDiagnostics::SpawnWorkerDeferred(WorldWeak.Get(), Loc, TeamId);
}

void UGP_WorkerHaulingContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
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
			return false;
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
		AGP_ResourceNode* Node = SpawnNode(FVector(-52000.0f, 0.0f, 100.0f));
		AGP_MainBase* Base = SpawnMainBase(FVector(-52400.0f, 0.0f, 100.0f), 1);
		if (!Expect(IsValid(Node) && IsValid(Base), TEXT("SpawnNodeAndMainBase")))
		{
			Finish();
			return;
		}
		TestNodeWeak = Node;
		MainBaseWeak = Base;
		DropOffRangeCm = Base->GetDropOffRangeCm();
		Expect(FMath::IsNearlyEqual(DropOffRangeCm, 400.0f), TEXT("DropOffRange400"));
		if (const UGP_ResourceDefinition* Def = Node->ResolveResourceDefinition(true))
		{
			InteractionRangeCm = Def->InteractionRangeCm;
		}
		AGP_Worker* Worker = SpawnWorker(Node->GetActorLocation() + FVector(100.0f, 0.0f, 0.0f), 1);
		PrimaryWorkerWeak = Worker;
		if (!Expect(IsValid(Worker), TEXT("SpawnWorkerTeam1")))
		{
			Finish();
			return;
		}
		Expect(Worker->GetTeamId() == 1 && Base->GetTeamId() == 1, TEXT("TeamMatch"));
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
			ThreatBefore = GS->GetFerroniteThreatValueForTeam(1);
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
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		Expect(IsValid(Storage) && Storage->GetTotalStored() >= 49.0f, TEXT("StorageReceivedCargoFull"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("CargoEmptyAfterDropOff"));
		Expect(Cmd->GetLastHaulAcceptedAmount() >= 49.0f, TEXT("HaulAccepted50"));
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			const float ThreatAfter = GS->GetFerroniteThreatValueForTeam(1);
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
		if (WaitHaulOrMove(Worker, TEXT("DepleteHaulTimeout")))
		{
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
		// Partial storage overflow LOST
		AGP_Worker* Worker = PrimaryWorkerWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_ResourceNode* FreshNode = SpawnNode(FVector(-53000.0f, 200.0f, 100.0f));
		if (AGP_ResourceNode* Old = TestNodeWeak.Get())
		{
			if (IsValid(Old))
			{
				Old->Destroy();
			}
		}
		TestNodeWeak = FreshNode;
		if (!Expect(IsValid(Worker) && IsValid(Base) && IsValid(FreshNode), TEXT("PartialStorageObjects")))
		{
			Finish();
			return;
		}
		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		Storage->RemovePlanetaryFerronite(Storage->GetTotalStored());
		const float Cap = Storage->GetTotalCapacity();
		Storage->AddPlanetaryFerronite(Cap - 20.0f);
		Expect(FMath::IsNearlyEqual(Storage->GetTotalRemaining(), 20.0f, 0.1f), TEXT("StorageRemaining20"));
		Worker->GetCargoComponent()->ClearCargo();
		Worker->GetCargoComponent()->AddCargo(50.0f);
		Worker->SetActorLocation(Base->GetActorLocation() + FVector(DropOffRangeCm + 600.0f, 0.0f, 0.0f),
			false, nullptr, ETeleportType::TeleportPhysics);
		IssueMine(Worker, FreshNode);
		// Force haul path: mining terminal CargoFull while held.
		// Cargo already full — IssueMine may be rejected unless we start haul via mining terminal.
		// Simulate by filling through mining at node in range after clearing a bit.
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
		UGP_UnitCommandComponent* Cmd = Worker->GetUnitCommandComponent();
		Expect(Cmd->GetLastHaulAcceptedAmount() <= 20.0f + 0.1f, TEXT("PartialAcceptedAtMost20"));
		Expect(Cmd->GetLastHaulRejectedAmount() >= 29.0f, TEXT("PartialRejectedOverflow"));
		Expect(FMath::IsNearlyEqual(Worker->GetCargoComponent()->GetCurrentCargoAmount(), 0.0f), TEXT("OverflowLostClearedCargo"));
		Expect(Base->GetStorageComponent()->IsStorageFull(), TEXT("StorageFullAfterPartial"));
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
		Base->GetStorageComponent()->RemovePlanetaryFerronite(Base->GetStorageComponent()->GetTotalStored());
		Worker->GetCargoComponent()->ClearCargo();
		Worker->GetCargoComponent()->AddCargo(50.0f);
		Worker->SetActorLocation(Node->GetActorLocation() + FVector(80.0f, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		// Re-open haul via mine + force cycles on a node with amount
		if (Node->GetCurrentAmount() < 50)
		{
			// spawn replacement with stock
			AGP_ResourceNode* NewNode = SpawnNode(FVector(-53500.0f, 0.0f, 100.0f));
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
		DestroyWeakMainBase(MainBaseWeak);
		AGP_MainBase* Enemy = SpawnMainBase(FVector(-54000.0f, 0.0f, 100.0f), 2);
		EnemyBaseWeak = Enemy;
		Expect(IsValid(Enemy), TEXT("EnemyBaseSpawned"));
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			Expect(GS->FindMainBaseForTeam(1) == nullptr, TEXT("NoTeam1MainBase"));
		}
		Worker->SetTeamId(1);
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
		// Restore friendly base for lifecycle stage
		DestroyWeakMainBase(EnemyBaseWeak);
		MainBaseWeak = SpawnMainBase(FVector(-52400.0f, 0.0f, 100.0f), 1);
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
		Finish();
	}
}

void UGP_DiagnosticScenarioContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_DiagnosticScenarioContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPWorker, Log, TEXT("GP Resource.RunDiagnosticScenarioContractTest Stage=Start"));
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
	UE_LOG(LogGPWorker, Error, TEXT("GP Resource.RunDiagnosticScenarioContractTest ABORT: %s"), Reason);
	++Failures;
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
	}
	UnbindWorldCleanup();
	UE_LOG(LogGPWorker, Log, TEXT("GP Resource.RunDiagnosticScenarioContractTest: Complete Failures=%d"), Failures);
	RemoveFromRoot();
	GPWorkerDebug::GActiveDiagnosticScenarioContractTest.Reset();
	WorldWeak.Reset();
	MainBaseWeak.Reset();
	WorkerWeak.Reset();
	NodeWeak.Reset();
}

void UGP_DiagnosticScenarioContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalidDuringStage"));
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		// Deterministic cleanup of prior Team1 diagnostic leftovers.
		TArray<AGP_MainBase*> BasesToDestroy;
		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			if (IsValid(*It) && ((*It)->GetTeamId() == 1 || (*It)->HasAnyFlags(RF_Transient)))
			{
				BasesToDestroy.Add(*It);
			}
		}
		for (AGP_MainBase* Base : BasesToDestroy)
		{
			Base->Destroy();
		}
		TArray<AGP_Worker*> WorkersToDestroy;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (IsValid(*It) && ((*It)->GetTeamId() == 1 || (*It)->HasAnyFlags(RF_Transient)))
			{
				WorkersToDestroy.Add(*It);
			}
		}
		for (AGP_Worker* Worker : WorkersToDestroy)
		{
			Worker->Destroy();
		}
		TArray<AGP_ResourceNode*> NodesToDestroy;
		const FVector PreferredNode = GPResourceLoopDiagnostics::GetDiagnosticResourceNodeLocation();
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->HasAnyFlags(RF_Transient)
				&& FVector::DistSquared((*It)->GetActorLocation(), PreferredNode) <= FMath::Square(600.0f))
			{
				NodesToDestroy.Add(*It);
			}
		}
		for (AGP_ResourceNode* Node : NodesToDestroy)
		{
			Node->Destroy();
		}

		const GPResourceLoopDiagnostics::FGP_DiagnosticScenarioActors Scenario =
			GPResourceLoopDiagnostics::SpawnDiagnosticScenario(World, 1);
		if (!Expect(Scenario.bOk, TEXT("DiagnosticScenarioSpawnOk")))
		{
			Finish();
			return;
		}
		MainBaseWeak = Scenario.MainBase;
		WorkerWeak = Scenario.Worker;
		NodeWeak = Scenario.ResourceNode;

		Expect(IsValid(Scenario.MainBase) && Scenario.MainBase->GetTeamId() == 1, TEXT("MainBaseTeam1"));
		Expect(IsValid(Scenario.Worker) && Scenario.Worker->GetTeamId() == 1, TEXT("WorkerTeam1"));
		Expect(IsValid(Scenario.ResourceNode) && Scenario.ResourceNode->GetCurrentAmount() > 0, TEXT("NodeMineableAmount"));
		Expect(Scenario.ResourceNode->HasAnyFlags(RF_Transient), TEXT("NodeTransient"));
		Expect(Scenario.MainBase->HasAnyFlags(RF_Transient), TEXT("MainBaseTransient"));
		Expect(Scenario.Worker->HasAnyFlags(RF_Transient), TEXT("WorkerTransient"));

		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		Expect(IsValid(GS) && GS->FindMainBaseForTeam(1) == Scenario.MainBase, TEXT("RegistryResolvesMainBase"));
		Expect(Scenario.Worker->GetTeamId() != -1 && Scenario.MainBase->GetTeamId() != -1, TEXT("NoUnassignedTeam"));

		int32 BaseCountTeam1 = 0;
		int32 WorkerCountTeam1 = 0;
		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->GetTeamId() == 1)
			{
				++BaseCountTeam1;
			}
		}
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			if (IsValid(*It) && (*It)->GetTeamId() == 1)
			{
				++WorkerCountTeam1;
			}
		}
		Expect(BaseCountTeam1 == 1, TEXT("ExactlyOneMainBaseTeam1"));
		Expect(WorkerCountTeam1 >= 1, TEXT("AtLeastOneWorkerTeam1"));

		const GPResourceLoopDiagnostics::FGP_ScenarioValidation Validation =
			GPResourceLoopDiagnostics::ValidateHaulingScenario(World, 1, Scenario.Worker);
		Expect(Validation.bWorkerHasMainBase, TEXT("WorkerHasMainBase"));
		Expect(Validation.bMainBaseRegisteredForTeam, TEXT("MainBaseRegisteredForTeam"));
		Expect(Validation.bWorkerAndBaseSameTeam, TEXT("WorkerAndBaseSameTeam"));
		Expect(Validation.bNodeMineable, TEXT("NodeMineable"));

		// Team change refresh: reassign then restore must not leave stale registry.
		Scenario.MainBase->SetTeamId(2);
		Expect(GS->FindMainBaseForTeam(1) == nullptr, TEXT("NoStaleTeam1AfterReassign"));
		Expect(GS->FindMainBaseForTeam(2) == Scenario.MainBase, TEXT("RegisteredUnderTeam2"));
		Scenario.MainBase->SetTeamId(1);
		Expect(GS->FindMainBaseForTeam(1) == Scenario.MainBase, TEXT("ReRegisteredTeam1"));
		Expect(GS->FindMainBaseForTeam(2) == nullptr, TEXT("NoStaleTeam2"));

		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_Worker* Worker = WorkerWeak.Get();
		AGP_ResourceNode* Node = NodeWeak.Get();
		GPResourceLoopDiagnostics::DestroyDiagnosticScenarioActors(Base, Worker, Node);
		MainBaseWeak.Reset();
		WorkerWeak.Reset();
		NodeWeak.Reset();
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		AGP_GameState* GS = World->GetGameState<AGP_GameState>();
		Expect(IsValid(GS) && GS->FindMainBaseForTeam(1) == nullptr, TEXT("RegistryEmptyAfterCleanup"));
		Expect(!MainBaseWeak.IsValid() && !WorkerWeak.IsValid(), TEXT("ActorsDestroyed"));
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
void UGP_DiagnosticScenarioContractTestRunner::Finish() { bFinished = true; }
void UGP_DiagnosticScenarioContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_DiagnosticScenarioContractTestRunner::UnbindWorldCleanup() {}
#endif
