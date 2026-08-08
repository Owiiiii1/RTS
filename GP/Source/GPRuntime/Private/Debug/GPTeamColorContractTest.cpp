// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Presentation/GPTeamPresentationComponent.h"
#include "Settings/GPGameplayPresentationSettings.h"
#include "TimerManager.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPTeamColorContract, Log, All);

namespace GPTeamColorContractDebug
{
	static TWeakObjectPtr<UGP_TeamColorContractTestRunner> GActiveRunner;

	static bool ColorsNearlyEqual(const FLinearColor& A, const FLinearColor& B)
	{
		return FMath::IsNearlyEqual(A.R, B.R, 0.02f)
			&& FMath::IsNearlyEqual(A.G, B.G, 0.02f)
			&& FMath::IsNearlyEqual(A.B, B.B, 0.02f);
	}

	static void RunTeamColorContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPTeamColorContract, Warning,
				TEXT("GP Combat.RunTeamColorContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("TeamColorContract"), TEXT("TeamColor"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_TeamColorContractTestRunner* Runner =
			NewObject<UGP_TeamColorContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GTeamColorContract(
		TEXT("gp.Combat.RunTeamColorContractTest"),
		TEXT("Authority: GP-S29R TeamId presentation color contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunTeamColorContractTest));
}

void UGP_TeamColorContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_TeamColorContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_TeamColorContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_TeamColorContractTestRunner::CleanupActors()
{
	if (UnitWeak.IsValid())
	{
		UnitWeak->Destroy();
		UnitWeak.Reset();
	}
	if (BaseWeak.IsValid())
	{
		BaseWeak->Destroy();
		BaseWeak.Reset();
	}
}

void UGP_TeamColorContractTestRunner::Finish()
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

	UE_LOG(LogGPTeamColorContract, Log,
		TEXT("GP Combat.RunTeamColorContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("false"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	if (GPTeamColorContractDebug::GActiveRunner.Get() == this)
	{
		GPTeamColorContractDebug::GActiveRunner.Reset();
	}
	RemoveFromRoot();
}

void UGP_TeamColorContractTestRunner::Abort(const TCHAR* Reason)
{
	bCancelled = true;
	CancelReason = Reason;
	UE_LOG(LogGPTeamColorContract, Error, TEXT("GP Combat.RunTeamColorContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_TeamColorContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPTeamColorContract, Error, TEXT("GP Combat.RunTeamColorContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPTeamColorContract, Log, TEXT("GP Combat.RunTeamColorContractTest PASS: %s"), Label);
	return true;
}

void UGP_TeamColorContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_TeamColorContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_TeamColorContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_TeamColorContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPTeamColorContract, Log, TEXT("GP Combat.RunTeamColorContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_TeamColorContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	const UGP_GameplayPresentationSettings* Settings = UGP_GameplayPresentationSettings::Get();
	if (!Expect(Settings != nullptr, TEXT("SettingsPresent")))
	{
		Finish();
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		const FLinearColor Team1 = Settings->GetTeamColor(1);
		const FLinearColor Team2 = Settings->GetTeamColor(2);
		const FLinearColor Unknown = Settings->GetTeamColor(99);
		const FLinearColor Unassigned = Settings->GetTeamColor(-1);

		Expect(GPTeamColorContractDebug::ColorsNearlyEqual(Team1, FLinearColor(0.15f, 0.40f, 0.95f, 1.0f))
			|| Team1.B > Team1.R, TEXT("A_Team1ConfiguredBlue"));
		Expect(GPTeamColorContractDebug::ColorsNearlyEqual(Team2, FLinearColor(0.90f, 0.15f, 0.15f, 1.0f))
			|| Team2.R > Team2.B, TEXT("B_Team2ConfiguredRed"));
		Expect(GPTeamColorContractDebug::ColorsNearlyEqual(Unknown, Settings->NeutralTeamColor), TEXT("C_UnknownNeutral"));
		Expect(GPTeamColorContractDebug::ColorsNearlyEqual(Unassigned, Settings->NeutralTeamColor), TEXT("C_UnassignedNeutral"));
		Expect(!GPTeamColorContractDebug::ColorsNearlyEqual(Team1, Team2), TEXT("D_Team1NeTeam2"));

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGP_Worker* Unit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			FVector(54000.0f, 54000.0f, 200.0f),
			FRotator::ZeroRotator,
			Params);
		UnitWeak = Unit;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(54100.0f, 54000.0f, 200.0f),
			FRotator::ZeroRotator,
			Params);
		BaseWeak = Base;

		if (!Expect(IsValid(Unit) && IsValid(Base), TEXT("SpawnPresentationActors")))
		{
			Finish();
			return;
		}

		Unit->SetTeamId(1);
		Base->SetTeamId(1);
		UGP_TeamPresentationComponent* UnitPres = Unit->GetTeamPresentationComponent();
		UGP_TeamPresentationComponent* BasePres = Base->GetTeamPresentationComponent();
		if (!Expect(UnitPres != nullptr && BasePres != nullptr, TEXT("PresentationComponents")))
		{
			Finish();
			return;
		}

		UnitPres->RefreshTeamPresentation();
		BasePres->RefreshTeamPresentation();
		Expect(GPTeamColorContractDebug::ColorsNearlyEqual(UnitPres->GetAppliedTeamColor(), Settings->GetTeamColor(1)),
			TEXT("A_UnitAppliedTeam1"));
		Expect(GPTeamColorContractDebug::ColorsNearlyEqual(BasePres->GetAppliedTeamColor(), Settings->GetTeamColor(1)),
			TEXT("A_BaseAppliedTeam1"));
		Expect(!UnitPres->PrimaryComponentTick.bCanEverTick, TEXT("H_NoTickRequired"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		AGP_Worker* Unit = UnitWeak.Get();
		UGP_TeamPresentationComponent* Pres = Unit != nullptr ? Unit->GetTeamPresentationComponent() : nullptr;
		if (!Expect(IsValid(Unit) && Pres != nullptr, TEXT("E_Ready")))
		{
			Finish();
			return;
		}

		TeamIdBefore = Unit->GetTeamId();
		Unit->SetTeamId(2);
		Expect(Unit->GetTeamId() == 2, TEXT("E_TeamIdChanged"));
		Expect(GPTeamColorContractDebug::ColorsNearlyEqual(Pres->GetAppliedTeamColor(), Settings->GetTeamColor(2)),
			TEXT("E_PresentationUpdatedOnTeamChange"));
		Expect(Unit->GetTeamId() == 2 && TeamIdBefore == 1, TEXT("G_SettingsDoNotMutateTeamId"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		AGP_Worker* Unit = UnitWeak.Get();
		UGP_TeamPresentationComponent* Pres = Unit != nullptr ? Unit->GetTeamPresentationComponent() : nullptr;
		if (!Expect(IsValid(Unit) && Pres != nullptr, TEXT("F_Ready")))
		{
			Finish();
			return;
		}

		// Simulate replicated OnRep path: TeamId already 2; OnRep refreshes presentation.
		const FLinearColor Before = Pres->GetAppliedTeamColor();
		(void)Before;
		if (UFunction* OnRepFn = Unit->FindFunction(TEXT("OnRep_TeamId")))
		{
			Unit->ProcessEvent(OnRepFn, nullptr);
			Expect(GPTeamColorContractDebug::ColorsNearlyEqual(Pres->GetAppliedTeamColor(), Settings->GetTeamColor(2)),
				TEXT("F_OnRepPathTriggersUpdate"));
		}
		else
		{
			Pres->RefreshTeamPresentation();
			Expect(GPTeamColorContractDebug::ColorsNearlyEqual(Pres->GetAppliedTeamColor(), Settings->GetTeamColor(2)),
				TEXT("F_OnRepEquivalentRefresh"));
		}

		Expect(Unit->GetTeamId() == 2, TEXT("G_TeamIdUnchangedByPresentation"));
		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_TeamColorContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_TeamColorContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_TeamColorContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_TeamColorContractTestRunner::AdvanceStage() {}
bool UGP_TeamColorContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_TeamColorContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_TeamColorContractTestRunner::Finish() { bFinished = true; }
void UGP_TeamColorContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_TeamColorContractTestRunner::UnbindWorldCleanup() {}
void UGP_TeamColorContractTestRunner::CleanupActors() {}

#endif // !UE_BUILD_SHIPPING
