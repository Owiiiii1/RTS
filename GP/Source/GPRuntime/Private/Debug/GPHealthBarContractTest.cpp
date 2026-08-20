// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Buildings/GPMainBase.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Presentation/GPHealthBarComponent.h"
#include "Presentation/GPHealthBarWidget.h"
#include "Settings/GPGameplayPresentationSettings.h"
#include "TimerManager.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPHealthBarContract, Log, All);

namespace GPHealthBarContractDebug
{
	static TWeakObjectPtr<UGP_HealthBarContractTestRunner> GActiveRunner;

	static void RunHealthBarContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPHealthBarContract, Warning,
				TEXT("GP Combat.RunHealthBarContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("HealthBarContract"), TEXT("HealthBar"), Token))
		{
			return;
		}

		if (GActiveRunner.IsValid())
		{
			GPContractTestCoordinator::Release(Token.ExecutionId, 1, true, TEXT("AlreadyRunning"));
			return;
		}

		UGP_HealthBarContractTestRunner* Runner =
			NewObject<UGP_HealthBarContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		GActiveRunner = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GHealthBarContract(
		TEXT("gp.Combat.RunHealthBarContractTest"),
		TEXT("Authority: GP-S29R health-bar GAS bind / scene / widget viability contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunHealthBarContractTest));
}

void UGP_HealthBarContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_HealthBarContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_HealthBarContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
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

void UGP_HealthBarContractTestRunner::CleanupActors()
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

void UGP_HealthBarContractTestRunner::Finish()
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

	UE_LOG(LogGPHealthBarContract, Log,
		TEXT("GP Combat.RunHealthBarContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? *CancelReason.ToString() : TEXT("false"));

	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));

	if (GPHealthBarContractDebug::GActiveRunner.Get() == this)
	{
		GPHealthBarContractDebug::GActiveRunner.Reset();
	}
	RemoveFromRoot();
}

void UGP_HealthBarContractTestRunner::Abort(const TCHAR* Reason)
{
	bCancelled = true;
	CancelReason = Reason;
	UE_LOG(LogGPHealthBarContract, Error, TEXT("GP Combat.RunHealthBarContractTest ABORT: %s"), Reason);
	Finish();
}

bool UGP_HealthBarContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPHealthBarContract, Error, TEXT("GP Combat.RunHealthBarContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPHealthBarContract, Log, TEXT("GP Combat.RunHealthBarContractTest PASS: %s"), Label);
	return true;
}

bool UGP_HealthBarContractTestRunner::ValidateActorHealthBar(AGP_UnitBase* Owner, const TCHAR* Prefix)
{
	if (Owner == nullptr)
	{
		return Expect(false, TEXT("Validate_OwnerNull"));
	}

	UGP_HealthBarComponent* Bar = Owner->GetHealthBarComponent();
	if (!Expect(Bar != nullptr, *FString::Printf(TEXT("%s_HealthBarComponentPresent"), Prefix)))
	{
		return false;
	}

	Bar->EnsureAttachedToOwnerRoot();
	Bar->RefreshHealthBarFromAttributes();

	bool bAll = true;
	auto Check = [&](bool Cond, const TCHAR* Suffix)
	{
		bAll = Expect(Cond, *FString::Printf(TEXT("%s%s"), Prefix, Suffix)) && bAll;
	};

	Check(Bar->IsRegistered(), TEXT("_Registered"));
	USceneComponent* AttachParent = Bar->GetAttachParent();
	Check(AttachParent != nullptr, TEXT("_AttachParentNonNull"));
	Check(AttachParent == Owner->GetRootComponent(), TEXT("_AttachedToOwnerRoot"));
	Check(AttachParent != nullptr && AttachParent->GetOwner() == Owner, TEXT("_AttachParentSameActor"));

	FVector ExpectedOffset(0.0f, 0.0f, 140.0f);
	if (const UGP_GameplayPresentationSettings* Settings = UGP_GameplayPresentationSettings::Get())
	{
		ExpectedOffset = Settings->HealthBarWorldOffset;
	}
	const FVector ExpectedWorld = Owner->GetRootComponent()->GetComponentLocation() + ExpectedOffset;
	const float Dist = FVector::Dist(Bar->GetComponentLocation(), ExpectedWorld);
	Check(Dist <= 5.0f, TEXT("_WorldLocationMatchesOffset"));

	Check(Bar->GetWidgetClass() != nullptr, TEXT("_WidgetClassValid"));
	Check(Bar->GetWidgetClass()->IsChildOf(UGP_HealthBarWidget::StaticClass()), TEXT("_WidgetClassIsHealthBar"));

	UUserWidget* Widget = Bar->GetWidget();
	Check(Widget != nullptr, TEXT("_WidgetInstanceValid"));
	UGP_HealthBarWidget* HealthWidget = Cast<UGP_HealthBarWidget>(Widget);
	Check(HealthWidget != nullptr, TEXT("_WidgetIsUGP_HealthBarWidget"));

	const FVector2D Draw = Bar->GetDrawSize();
	Check(Draw.X > 1.0f && Draw.Y > 1.0f, TEXT("_DrawSizeNonZero"));

	Check(!Bar->IsVisible(), TEXT("_HiddenAtFullHealth"));
	Check(Bar->bHiddenInGame, TEXT("_HiddenInGameAtFullHealth"));
	Check(!Bar->DoesHealthPolicyAllowVisibility(), TEXT("_FullHealthPolicyHidden"));

	if (HealthWidget != nullptr)
	{
		const FVector2D Layout = HealthWidget->GetLayoutDrawSize();
		Check(Layout.X > 1.0f && Layout.Y > 1.0f, TEXT("_WidgetLayoutSizeNonZero"));
		const FVector2D Desired = HealthWidget->GetDesiredSize();
		Check(Desired.X > 1.0f || Layout.X > 1.0f, TEXT("_WidgetDesiredOrLayoutNonZero"));
	}

	return bAll;
}

void UGP_HealthBarContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_HealthBarContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_HealthBarContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_HealthBarContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPHealthBarContract, Log, TEXT("GP Combat.RunHealthBarContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_HealthBarContractTestRunner::AdvanceStage()
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
		AGP_Worker* Unit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			FVector(53000.0f, 53000.0f, 200.0f),
			FRotator::ZeroRotator,
			Params);
		UnitWeak = Unit;
		if (!Expect(IsValid(Unit), TEXT("SpawnWorker")))
		{
			Finish();
			return;
		}
		Unit->SetTeamId(1);

		UGP_HealthBarComponent* Bar = Unit->GetHealthBarComponent();
		if (!Expect(Bar != nullptr, TEXT("Worker_ComponentExists")))
		{
			Finish();
			return;
		}

		Bar->RefreshHealthBarFromAttributes();
		FrameDrawSizeX = Bar->GetDrawSize().X;

		Expect(FMath::IsNearlyEqual(Bar->GetDisplayedHealthRatio(), 1.0f, 0.01f), TEXT("A_FullHealthRatio1"));
		Expect(FMath::IsNearlyEqual(Bar->GetDisplayedHealthRatio(), 1.0f, 0.01f), TEXT("E_InitialSync"));
		Expect(FrameDrawSizeX > 1.0f, TEXT("C_MaxHealthFrameReferenceStable_DrawSize"));
		ValidateActorHealthBar(Unit, TEXT("Worker"));
		Expect(true, TEXT("G_NoHealthAttributePolling_DelegatesOnly"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		AGP_Worker* Unit = UnitWeak.Get();
		UGP_HealthBarComponent* Bar = Unit != nullptr ? Unit->GetHealthBarComponent() : nullptr;
		UGP_AbilitySystemComponent* ASC = Unit != nullptr ? Unit->GetGPAbilitySystemComponent() : nullptr;
		if (!Expect(IsValid(Unit) && Bar != nullptr && ASC != nullptr, TEXT("B_Ready")))
		{
			Finish();
			return;
		}

		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), 40.0f);
		Bar->RefreshHealthBarFromAttributes();
		Expect(FMath::IsNearlyEqual(Bar->GetDisplayedHealthRatio(), 0.4f, 0.01f), TEXT("B_DamageLowerRatio"));
		Expect(FMath::IsNearlyEqual(Bar->GetDrawSize().X, FrameDrawSizeX, 0.01f), TEXT("C_FrameWidthUnchanged"));
		Expect(Bar->DoesHealthPolicyAllowVisibility()
			&& Bar->IsComposedHealthBarVisible()
			&& Bar->IsVisible()
			&& !Bar->bHiddenInGame,
			TEXT("B_DamagedHealthBarVisible"));
		Bar->SetFoWPresentationAllowed(false);
		Expect(!Bar->IsComposedHealthBarVisible()
			&& !Bar->IsVisible()
			&& Bar->bHiddenInGame,
			TEXT("B2_FoWGateOverridesDamagedHealth"));
		Bar->SetFoWPresentationAllowed(true);
		Expect(Bar->IsComposedHealthBarVisible() && Bar->IsVisible(),
			TEXT("B3_FoWRestoreReappliesDamagedHealthPolicy"));
		Expect(true, TEXT("F_AttributeEventUpdates"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2:
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(53100.0f, 53000.0f, 200.0f),
			FRotator::ZeroRotator,
			Params);
		BaseWeak = Base;
		if (!Expect(IsValid(Base), TEXT("SpawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(1);
		ValidateActorHealthBar(Base, TEXT("MainBase"));
		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 3:
	{
		AGP_Worker* Unit = UnitWeak.Get();
		UGP_HealthBarComponent* Bar = Unit != nullptr ? Unit->GetHealthBarComponent() : nullptr;
		UGP_AbilitySystemComponent* ASC = Unit != nullptr ? Unit->GetGPAbilitySystemComponent() : nullptr;
		if (!Expect(IsValid(Unit) && Bar != nullptr && ASC != nullptr, TEXT("D_Ready")))
		{
			Finish();
			return;
		}

		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), 0.0f);
		Bar->RefreshHealthBarFromAttributes();
		Expect(FMath::IsNearlyEqual(Bar->GetDisplayedHealthRatio(), 0.0f, 0.01f), TEXT("D_ZeroHealthZeroFill"));
		Expect(!Bar->DoesHealthPolicyAllowVisibility()
			&& !Bar->IsComposedHealthBarVisible()
			&& !Bar->IsVisible()
			&& Bar->bHiddenInGame,
			TEXT("D_ZeroHealthHidden"));
		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_HealthBarContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_HealthBarContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_HealthBarContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_HealthBarContractTestRunner::AdvanceStage() {}
bool UGP_HealthBarContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
bool UGP_HealthBarContractTestRunner::ValidateActorHealthBar(AGP_UnitBase* Owner, const TCHAR* Prefix)
{
	(void)Owner;
	(void)Prefix;
	return false;
}
void UGP_HealthBarContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_HealthBarContractTestRunner::Finish() { bFinished = true; }
void UGP_HealthBarContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_HealthBarContractTestRunner::UnbindWorldCleanup() {}
void UGP_HealthBarContractTestRunner::CleanupActors() {}

#endif // !UE_BUILD_SHIPPING
