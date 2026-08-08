// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPSalvageWalker.h"

#if !UE_BUILD_SHIPPING

#include "AttributeSets/GPUnitAttributeSet.h"
#include "Combat/GPCombatPresentationComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Presentation/GPHealthBarComponent.h"
#include "Presentation/GPTeamPresentationComponent.h"
#include "Resources/GPCargoComponent.h"
#include "Resources/GPMiningComponent.h"
#include "TimerManager.h"
#include "Units/GPMobileUnit.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPUnitBase.h"
#include "Units/GPUnitCommandComponent.h"
#include "UObject/Package.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "Visual/GPUnitVisualComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPSalvageWalkerContract, Log, All);

namespace GPSalvageWalkerContractDebug
{
	static TWeakObjectPtr<UGP_SalvageWalkerContractTestRunner> GActiveRunner;

	static void RunSalvageWalkerContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPSalvageWalkerContract, Warning,
				TEXT("GP Combat.RunSalvageWalkerContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("SalvageWalkerContract"), TEXT("SalvageWalker"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_SalvageWalkerContractTestRunner* Runner =
			NewObject<UGP_SalvageWalkerContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GSalvageWalkerContract(
		TEXT("gp.Combat.RunSalvageWalkerContractTest"),
		TEXT("Authority: AGP_SalvageWalker composition + GDD native defaults contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunSalvageWalkerContractTest));
}

void UGP_SalvageWalkerContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_SalvageWalkerContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_SalvageWalkerContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		bCancelled = true;
		CancelReason = TEXT("WorldCleanup");
		Finish();
	}
}

void UGP_SalvageWalkerContractTestRunner::CleanupActors()
{
	if (UnitWeak.IsValid())
	{
		UnitWeak->Destroy();
		UnitWeak.Reset();
	}
}

void UGP_SalvageWalkerContractTestRunner::Finish()
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
	CleanupActors();

	UE_LOG(LogGPSalvageWalkerContract, Log,
		TEXT("GP Combat.RunSalvageWalkerContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("false"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	if (GPSalvageWalkerContractDebug::GActiveRunner.Get() == this)
	{
		GPSalvageWalkerContractDebug::GActiveRunner.Reset();
	}
	RemoveFromRoot();
}

void UGP_SalvageWalkerContractTestRunner::Abort(const TCHAR* Reason)
{
	bCancelled = true;
	CancelReason = Reason;
	UE_LOG(LogGPSalvageWalkerContract, Error, TEXT("GP Combat.RunSalvageWalkerContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_SalvageWalkerContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPSalvageWalkerContract, Error, TEXT("GP Combat.RunSalvageWalkerContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPSalvageWalkerContract, Log, TEXT("GP Combat.RunSalvageWalkerContractTest PASS: %s"), Label);
	return true;
}

void UGP_SalvageWalkerContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_SalvageWalkerContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_SalvageWalkerContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_SalvageWalkerContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPSalvageWalkerContract, Log, TEXT("GP Combat.RunSalvageWalkerContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_SalvageWalkerContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGP_SalvageWalker* Unit = World->SpawnActor<AGP_SalvageWalker>(
			AGP_SalvageWalker::StaticClass(),
			FVector(54000.0f, 54000.0f, 200.0f),
			FRotator::ZeroRotator,
			Params);
		UnitWeak = Unit;

		if (!Expect(IsValid(Unit), TEXT("SpawnSalvageWalker")))
		{
			Finish();
			return;
		}

		Expect(Unit->IsA(AGP_SalvageWalker::StaticClass()), TEXT("IsAGP_SalvageWalker"));
		Expect(Unit->IsA(AGP_Unit::StaticClass()), TEXT("InheritsAGP_Unit"));
		Expect(Unit->IsA(AGP_MobileUnit::StaticClass()), TEXT("InheritsAGP_MobileUnit"));
		Expect(Unit->IsA(AGP_UnitBase::StaticClass()), TEXT("InheritsAGP_UnitBase"));

		TArray<UGP_MovementComponent*> MovementComps;
		Unit->GetComponents<UGP_MovementComponent>(MovementComps);
		Expect(MovementComps.Num() == 1, TEXT("ExactlyOneMovementComponent"));
		Expect(Unit->GetUnitMovementComponent() != nullptr, TEXT("GetUnitMovementComponentNonNull"));
		Expect(Unit->GetUnitMovementComponent() == MovementComps[0], TEXT("MovementGetterMatchesSoleInstance"));

		TArray<UGP_UnitVisualComponent*> VisualComps;
		Unit->GetComponents<UGP_UnitVisualComponent>(VisualComps);
		Expect(VisualComps.Num() == 1, TEXT("ExactlyOneUnitVisualComponent"));
		Expect(Unit->GetUnitVisualComponent() != nullptr, TEXT("GetUnitVisualComponentNonNull"));

		Expect(Unit->GetUnitCommandComponent() != nullptr, TEXT("UnitCommandComponentPresent"));
		Expect(Unit->GetHealthBarComponent() != nullptr, TEXT("HealthBarComponentPresent"));
		Expect(Unit->GetTeamPresentationComponent() != nullptr, TEXT("TeamPresentationComponentPresent"));
		Expect(Unit->GetCombatPresentationComponent() != nullptr, TEXT("CombatPresentationComponentPresent"));

		Expect(Unit->FindComponentByClass<UGP_CargoComponent>() == nullptr, TEXT("NoCargoComponent"));
		Expect(Unit->FindComponentByClass<UGP_MiningComponent>() == nullptr, TEXT("NoMiningComponent"));

		Expect(Unit->IsGameplaySelectable(), TEXT("Selectable"));
		Expect(Unit->IsGameplayInspectable(), TEXT("Inspectable"));
		Expect(Unit->IsSelectionTypeUnit(), TEXT("SelectionTypeUnit"));

		if (UGP_MovementComponent* Movement = Unit->GetUnitMovementComponent())
		{
			Expect(FMath::IsNearlyEqual(Movement->MoveSpeed, 250.0f, 0.01f), TEXT("MoveSpeed250"));
		}
		else
		{
			Expect(false, TEXT("MoveSpeed250"));
		}

		if (UGP_UnitVisualComponent* Visual = Unit->GetUnitVisualComponent())
		{
			Expect(
				Visual->GetVisualSourceMode() == EGP_VisualSourceMode::AuthoredComponents,
				TEXT("VisualSourceModeAuthoredComponents"));
		}
		else
		{
			Expect(false, TEXT("VisualSourceModeAuthoredComponents"));
		}

		// GAS combat attributes initialize in BeginPlay — wait one tick for real lifecycle.
		StageIndex = 1;
		ScheduleNext(0.15f);
		break;
	}
	case 1:
	{
		AGP_SalvageWalker* Unit = UnitWeak.Get();
		if (!Expect(IsValid(Unit), TEXT("UnitAliveAfterBeginPlay")))
		{
			Finish();
			return;
		}

		const UGP_UnitAttributeSet* Attrs = Unit->GetUnitAttributeSet();
		if (!Expect(Attrs != nullptr, TEXT("UnitAttributeSetPresent")))
		{
			Finish();
			return;
		}

		Expect(FMath::IsNearlyEqual(Attrs->GetMaxHealth(), 200.0f, 0.01f), TEXT("MaxHealth200"));
		Expect(FMath::IsNearlyEqual(Attrs->GetHealth(), 200.0f, 0.01f), TEXT("Health200"));
		Expect(FMath::IsNearlyEqual(Attrs->GetDamage(), 20.0f, 0.01f), TEXT("Damage20"));
		Expect(FMath::IsNearlyEqual(Attrs->GetAttackRange(), 600.0f, 0.01f), TEXT("AttackRange600"));
		Expect(FMath::IsNearlyEqual(Attrs->GetAttackCooldown(), 1.0f, 0.01f), TEXT("AttackCooldown1"));

		Finish();
		break;
	}
	default:
		Abort(TEXT("UnexpectedStage"));
		break;
	}
}

#else

void UGP_SalvageWalkerContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_SalvageWalkerContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_SalvageWalkerContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_SalvageWalkerContractTestRunner::AdvanceStage() {}
bool UGP_SalvageWalkerContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_SalvageWalkerContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_SalvageWalkerContractTestRunner::Finish() { bFinished = true; }
void UGP_SalvageWalkerContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_SalvageWalkerContractTestRunner::UnbindWorldCleanup() {}
void UGP_SalvageWalkerContractTestRunner::CleanupActors() {}

#endif // !UE_BUILD_SHIPPING
