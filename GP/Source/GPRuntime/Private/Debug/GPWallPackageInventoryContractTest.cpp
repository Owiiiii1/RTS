// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPWallPackageInventoryContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPMainBase.h"
#include "Buildings/GPWallSegmentInventoryComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameMode.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Orbital/GPUnitDropAuthority.h"
#include "Orbital/GPUnitDropManifest.h"
#include "Orbital/GPWallPackageAuthority.h"
#include "Orbital/GPWallPackageCatalog.h"
#include "Orbital/GPWallPackageDefinition.h"
#include "Player/GPPlayerState.h"
#include "Tags/GPGameplayTags.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPWallPackageInventory, Log, All);

namespace GPWallPackageInventoryDebug
{
	static TWeakObjectPtr<UGP_WallPackageInventoryContractTestRunner> GActiveRunner;

	static AGP_PlayerState* SpawnTeamPlayerState(UWorld* World, AGameStateBase* GameState, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_PlayerState* PS = World->SpawnActor<AGP_PlayerState>(
			AGP_PlayerState::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!IsValid(PS) || GameState == nullptr)
		{
			return nullptr;
		}
		PS->SetTeamId(TeamId);
		GameState->AddPlayerState(PS);
		if (UGP_AbilitySystemComponent* ASC = PS->GetGPAbilitySystemComponent())
		{
			ASC->InitAbilityActorInfo(PS, PS);
		}
		return PS;
	}

	static void GrantOrbital(AGP_PlayerState* PS, float Amount)
	{
		if (!IsValid(PS) || Amount <= 0.0f)
		{
			return;
		}
		UGP_AbilitySystemComponent* ASC = PS->GetGPAbilitySystemComponent();
		if (ASC == nullptr)
		{
			return;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(PS);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UGP_GE_AddOrbital::StaticClass(), 1.0f, Context);
		if (!Spec.IsValid())
		{
			return;
		}
		Spec.Data->SetSetByCallerMagnitude(UGP_GE_AddOrbital::GetMagnitudeDataName(), Amount);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	static void RunWallPackageInventoryContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPWallPackageInventory, Warning, TEXT("gp.Orbital.RunWallPackageInventoryContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPWallPackageInventory, Warning, TEXT("gp.Orbital.RunWallPackageInventoryContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("WallPackageInventoryContract"), TEXT("WallPackageInventory"), Token))
		{
			return;
		}

		UGP_WallPackageInventoryContractTestRunner* Runner =
			NewObject<UGP_WallPackageInventoryContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GWallPackageInventoryContract(
		TEXT("gp.Orbital.RunWallPackageInventoryContractTest"),
		TEXT("GP-S42A Wall Package purchase + MainBase inventory contract (A–N)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunWallPackageInventoryContractTest));
}

void UGP_WallPackageInventoryContractTestRunner::BeginDestroy()
{
	UnbindInventoryDelegates();
	UGP_WallPackageCatalog::Get().DebugClearAuthoredOverrides();
	UGP_WallPackageCatalog::Get().DebugEndContractIsolation();
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_WallPackageInventoryContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_WallPackageInventoryContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		bCancelled = true;
		CancelReason = FName(TEXT("WorldCleanup"));
		Finish();
	}
}

void UGP_WallPackageInventoryContractTestRunner::CleanupActors()
{
	if (UWorld* World = WorldWeak.Get())
	{
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_PlayerState> It(World); It; ++It)
		{
			if (*It == OwnerPSWeak.Get())
			{
				It->Destroy();
			}
		}
		if (AGP_MainBase* Base = MainBaseWeak.Get())
		{
			Base->Destroy();
		}
	}
	MainBaseWeak.Reset();
	OwnerPSWeak.Reset();
	LastPodWeak.Reset();
}

void UGP_WallPackageInventoryContractTestRunner::BindInventoryDelegates()
{
	UnbindInventoryDelegates();
	AGP_MainBase* Base = MainBaseWeak.Get();
	UGP_WallSegmentInventoryComponent* Inventory =
		IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
	if (!IsValid(Inventory))
	{
		return;
	}
	Inventory->OnWallInventoryChanged.AddDynamic(this, &UGP_WallPackageInventoryContractTestRunner::HandleStockChanged);
	Inventory->OnWallPackagePendingChanged.AddDynamic(
		this,
		&UGP_WallPackageInventoryContractTestRunner::HandlePendingChanged);
}

void UGP_WallPackageInventoryContractTestRunner::UnbindInventoryDelegates()
{
	AGP_MainBase* Base = MainBaseWeak.Get();
	UGP_WallSegmentInventoryComponent* Inventory =
		IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
	if (IsValid(Inventory))
	{
		Inventory->OnWallInventoryChanged.RemoveDynamic(
			this, &UGP_WallPackageInventoryContractTestRunner::HandleStockChanged);
		Inventory->OnWallPackagePendingChanged.RemoveDynamic(
			this, &UGP_WallPackageInventoryContractTestRunner::HandlePendingChanged);
	}
}

void UGP_WallPackageInventoryContractTestRunner::HandleStockChanged(int32 NewCount)
{
	++StockBroadcasts;
	LastBroadcastStock = NewCount;
}

void UGP_WallPackageInventoryContractTestRunner::HandlePendingChanged(bool bPending)
{
	++PendingBroadcasts;
	bLastBroadcastPending = bPending;
}

void UGP_WallPackageInventoryContractTestRunner::Finish()
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
	UnbindInventoryDelegates();
	UGP_WallPackageCatalog::Get().DebugClearAuthoredOverrides();
	UGP_WallPackageCatalog::Get().DebugEndContractIsolation();
	CleanupActors();
	UnbindWorldCleanup();
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPWallPackageInventory, Log,
		TEXT("gp.Orbital.RunWallPackageInventoryContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPWallPackageInventoryDebug::GActiveRunner.Reset();
}

void UGP_WallPackageInventoryContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPWallPackageInventory, Error,
		TEXT("gp.Orbital.RunWallPackageInventoryContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_WallPackageInventoryContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPWallPackageInventory, Error,
			TEXT("gp.Orbital.RunWallPackageInventoryContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPWallPackageInventory, Log,
		TEXT("gp.Orbital.RunWallPackageInventoryContractTest PASS: %s"), Label);
	return true;
}

void UGP_WallPackageInventoryContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World) || bFinished)
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_WallPackageInventoryContractTestRunner::AdvanceStage),
		DelaySeconds,
		false);
}

void UGP_WallPackageInventoryContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_WallPackageInventoryContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPWallPackageInventory, Log, TEXT("gp.Orbital.RunWallPackageInventoryContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_WallPackageInventoryContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorld"));
		return;
	}

	AGP_GameState* GS = World->GetGameState<AGP_GameState>();
	if (!Expect(IsValid(GS), TEXT("GameStatePresent")))
	{
		Finish();
		return;
	}

	constexpr int32 ContractTeam = 91;
	UGP_WallPackageCatalog& Catalog = UGP_WallPackageCatalog::Get();

	switch (StageIndex++)
	{
	case 0:
	{
		Catalog.DebugBeginContractIsolation();
		UGP_WallPackageDefinition* Native = Catalog.GetNativeWallPackage();
		const FGPGameplayTags& Tags = FGPGameplayTags::Get();
		Expect(IsValid(Native), TEXT("A_NativeExists"));
		Expect(IsValid(Native)
			&& Native->GetPrimaryAssetId().PrimaryAssetType
				== FPrimaryAssetType(UGP_WallPackageDefinition::PrimaryAssetTypeName()),
			TEXT("A_PrimaryAssetType"));
		Expect(IsValid(Native) && Native->SegmentCount == 5, TEXT("A_SegmentCount5"));
		Expect(IsValid(Native) && FMath::IsFinite(Native->Cost) && Native->Cost >= 0.0f, TEXT("A_CostNonNeg"));
		Expect(IsValid(Native)
			&& FMath::IsFinite(Native->DeliveryDescentSeconds)
			&& Native->DeliveryDescentSeconds >= 0.0f, TEXT("A_DescentNonNeg"));
		Expect(IsValid(Native)
			&& Tags.Drop_Type_WallPackage.IsValid()
			&& Native->DropTags.HasTagExact(Tags.Drop_Type_WallPackage), TEXT("A_WallPackageTag"));
		Expect(IsValid(Native)
			&& Native->IsValidForDelivery(UGP_WallSegmentInventoryComponent::DefaultCapacity),
			TEXT("A_ValidForDelivery"));

		Native->DeliveryDescentSeconds = 0.2f;
		Native->PayloadDeployDelaySeconds = 0.05f;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-52000.0f, 2500.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base) && IsValid(Base->GetWallSegmentInventoryComponent())
			&& IsValid(Base->GetWallPackageDropZone()), TEXT("SpawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(ContractTeam);

		AGP_PlayerState* OwnerPS = GPWallPackageInventoryDebug::SpawnTeamPlayerState(World, GS, ContractTeam);
		OwnerPSWeak = OwnerPS;
		if (!Expect(IsValid(OwnerPS), TEXT("SpawnOwnerPS")))
		{
			Finish();
			return;
		}

		BindInventoryDelegates();
		UGP_WallSegmentInventoryComponent* Inventory = Base->GetWallSegmentInventoryComponent();
		Expect(Inventory->GetWallSegmentCount() == 0, TEXT("B_Stock0"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("B_PendingFalse"));
		Expect(Inventory->CanPurchaseWallPackage(), TEXT("B_CanPurchase"));
		Expect(!Inventory->CanBuildWall(), TEXT("B_CannotBuild"));

		GPWallPackageInventoryDebug::GrantOrbital(OwnerPS, 1000.0f);
		ScheduleNext(0.05f);
		break;
	}
	case 1:
	{
		AGP_PlayerState* PS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(PS) && IsValid(Inventory), TEXT("C_Actors")))
		{
			Finish();
			return;
		}

		OrbitalBefore = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const int32 ReadyBefore = PS->GetOrbitalBuildingInventoryComponent()->GetReadyEntries().Num();
		const GPWallPackageAuthority::FPurchaseResult Buy =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(Buy.bAccepted, TEXT("C_Accepted"));
		Expect(Buy.bPending, TEXT("C_Pending"));
		Expect(Inventory->IsWallPackagePending(), TEXT("C_InventoryPending"));
		Expect(Inventory->GetWallSegmentCount() == 0, TEXT("C_NoStockYet"));
		Expect(!Inventory->CanPurchaseWallPackage(), TEXT("C_CannotRepurchase"));
		Expect(FMath::IsNearlyEqual(
			PS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalBefore - Buy.OrbitalCost,
			1.0f), TEXT("C_SpendOnce"));
		Expect(PS->GetOrbitalBuildingInventoryComponent()->GetReadyEntries().Num() == ReadyBefore,
			TEXT("C_NoReadyEntry"));
		Expect(Buy.SpawnedPod.IsValid()
			&& Buy.SpawnedPod->GetPayloadKind() == EGP_DropPodPayloadKind::WallPackage,
			TEXT("C_WallPackagePod"));
		LastPodWeak = Buy.SpawnedPod;

		int32 WallPods = 0;
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			if (It->GetPayloadKind() == EGP_DropPodPayloadKind::WallPackage)
			{
				++WallPods;
			}
		}
		Expect(WallPods == 1, TEXT("C_OnePod"));

		const GPWallPackageAuthority::FPurchaseResult Dup =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(!Dup.bAccepted, TEXT("F_PendingBlocks"));
		Expect(Dup.RejectReason == EGP_WallPackageRejectReason::PackagePending, TEXT("F_PendingReason"));
		Expect(FMath::IsNearlyEqual(
			PS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalBefore - Buy.OrbitalCost,
			1.0f), TEXT("F_NoSecondSpend"));
		int32 WallPodsAfter = 0;
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			if (It->GetPayloadKind() == EGP_DropPodPayloadKind::WallPackage)
			{
				++WallPodsAfter;
			}
		}
		Expect(WallPodsAfter == 1, TEXT("F_NoSecondPod"));
		ArrivalWaitAttempts = 0;
		ScheduleNext(0.15f);
		break;
	}
	case 2:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(Inventory), TEXT("D_Inventory")))
		{
			Finish();
			return;
		}
		if (Inventory->GetWallSegmentCount() != 5 && ArrivalWaitAttempts < 40)
		{
			++ArrivalWaitAttempts;
			StageIndex = 2;
			ScheduleNext(0.1f);
			return;
		}
		ArrivalWaitAttempts = 0;
		Expect(Inventory->GetWallSegmentCount() == 5, TEXT("D_Stock5"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("D_PendingCleared"));
		Expect(Inventory->CanBuildWall(), TEXT("D_CanBuild"));
		Expect(!Inventory->CanPurchaseWallPackage(), TEXT("D_CannotBuyFull"));
		Expect(LastBroadcastStock == 5, TEXT("K_StockBroadcast5"));
		Expect(PendingBroadcasts >= 2, TEXT("K_PendingOnAndOff"));

		AGP_PlayerState* PS = OwnerPSWeak.Get();
		const float Orbital = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const GPWallPackageAuthority::FPurchaseResult Full =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(!Full.bAccepted, TEXT("E_FullReject"));
		Expect(Full.RejectReason == EGP_WallPackageRejectReason::InventoryFull, TEXT("E_FullReason"));
		Expect(FMath::IsNearlyEqual(PS->GetPlayerAttributeSet()->GetOrbitalFerronite(), Orbital, 1.0f),
			TEXT("E_NoSpend"));
		int32 WallPods = 0;
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			if (It->GetPayloadKind() == EGP_DropPodPayloadKind::WallPackage && It->GetPhase() != EGP_DropPodPhase::Idle)
			{
				++WallPods;
			}
		}
		(void)WallPods;

		Expect(!Inventory->AuthorityCompletePackageDelivery(6), TEXT("L_OverCapacityRejected"));
		Expect(Inventory->GetWallSegmentCount() == 5, TEXT("L_Still5"));
		Expect(Inventory->AuthorityTryConsumeSegments(5), TEXT("L_ConsumeSetup"));
		Expect(Inventory->GetWallSegmentCount() == 0, TEXT("L_ConsumedTo0"));

		if (Inventory->CanPurchaseWallPackage())
		{
			Expect(Inventory->AuthorityBeginPackageDelivery(), TEXT("L_BeginPending"));
		}
		if (Inventory->IsWallPackagePending())
		{
			Expect(!Inventory->AuthorityCompletePackageDelivery(6), TEXT("L_Complete6Fails"));
			Expect(Inventory->GetWallSegmentCount() == 0, TEXT("L_NoGrantOnBadComplete"));
			Expect(!Inventory->IsWallPackagePending(), TEXT("L_PendingClearedOnBadComplete"));
		}

		const float OrbitalFailBefore = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		GPWallPackageAuthority::DebugForceNextPodSpawnFailure(true);
		const GPWallPackageAuthority::FPurchaseResult FailSpawn =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(!FailSpawn.bAccepted, TEXT("G_SpawnFailReject"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("G_NoStuckPending"));
		Expect(Inventory->GetWallSegmentCount() == 0, TEXT("G_NoStock"));
		Expect(FMath::IsNearlyEqual(
			PS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalFailBefore,
			1.0f), TEXT("G_NoNetSpend"));

		GPWallPackageInventoryDebug::GrantOrbital(PS, 500.0f);
		const GPWallPackageAuthority::FPurchaseResult DeathBuy =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(DeathBuy.bAccepted, TEXT("H_PurchaseForDeath"));
		LastPodWeak = DeathBuy.SpawnedPod;
		if (AGP_GameMode* GM = World->GetAuthGameMode<AGP_GameMode>())
		{
			GM->DebugSetAnnihilationCountsAsWin(false);
		}
		Base->NotifyAuthorityDeath();
		Expect(Inventory->GetWallSegmentCount() == 0, TEXT("H_StockClearedOnDeath"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("H_PendingClearedOnDeath"));
		ArrivalWaitAttempts = 0;
		ScheduleNext(0.35f);
		break;
	}
	case 3:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(Inventory), TEXT("H_InventoryAfterWait")))
		{
			Finish();
			return;
		}
		Expect(Inventory->GetWallSegmentCount() == 0, TEXT("H_NoResurrectStock"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("H_StillNotPending"));

		if (GS->FindMainBaseForTeam(ContractTeam) == nullptr)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			AGP_MainBase* Replacement = World->SpawnActor<AGP_MainBase>(
				AGP_MainBase::StaticClass(),
				FVector(-52000.0f, 2800.0f, 100.0f),
				FRotator::ZeroRotator,
				Params);
			if (IsValid(Replacement))
			{
				Replacement->SetTeamId(ContractTeam);
				UnbindInventoryDelegates();
				MainBaseWeak = Replacement;
				BindInventoryDelegates();
				Inventory = Replacement->GetWallSegmentInventoryComponent();
			}
		}

		Catalog.DebugForceUnresolvedAuthoredLoad(nullptr, true);
		Expect(Catalog.IsWallPackageDefinitionPending(), TEXT("I_PendingDef"));
		AGP_PlayerState* PS = OwnerPSWeak.Get();
		const float OrbitalPending = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const GPWallPackageAuthority::FPurchaseResult NotReady =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(!NotReady.bAccepted, TEXT("I_RejectPendingDef"));
		Expect(NotReady.RejectReason == EGP_WallPackageRejectReason::DefinitionNotReady, TEXT("I_NotReadyReason"));
		Expect(FMath::IsNearlyEqual(PS->GetPlayerAttributeSet()->GetOrbitalFerronite(), OrbitalPending, 1.0f),
			TEXT("I_NoSpendWhilePending"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("I_NoMutation"));

		AuthoredPackage = NewObject<UGP_WallPackageDefinition>(GetTransientPackage(), FName(TEXT("DA_GP_WallPackage_Authored")));
		AuthoredPackage->DisplayName = FText::FromString(TEXT("Authored Wall Package"));
		AuthoredPackage->Cost = 17.0f;
		AuthoredPackage->SegmentCount = 5;
		AuthoredPackage->DeliveryDescentSeconds = 4.25f;
		AuthoredPackage->PayloadDeployDelaySeconds = 0.75f;
		const FGPGameplayTags& Tags = FGPGameplayTags::Get();
		if (Tags.Drop_Type_WallPackage.IsValid())
		{
			AuthoredPackage->DropTags.AddTag(Tags.Drop_Type_WallPackage);
		}
		Catalog.DebugCompletePendingAuthoredLoad();
		Catalog.DebugAssignLoadedAuthored(AuthoredPackage);
		Expect(Catalog.GetWallPackage() == AuthoredPackage, TEXT("J_AuthoredWins"));
		Expect(FMath::IsNearlyEqual(Catalog.GetWallPackage()->Cost, 17.0f, 0.01f), TEXT("J_Cost17"));
		Expect(FMath::IsNearlyEqual(Catalog.GetWallPackage()->DeliveryDescentSeconds, 4.25f, 0.01f), TEXT("J_Descent"));
		Expect(FMath::IsNearlyEqual(Catalog.GetWallPackage()->PayloadDeployDelaySeconds, 0.75f, 0.01f), TEXT("J_Deploy"));

		AuthoredPackage->DeliveryDescentSeconds = 0.2f;
		AuthoredPackage->PayloadDeployDelaySeconds = 0.05f;
		const float OrbitalAuth = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const GPWallPackageAuthority::FPurchaseResult AuthBuy =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(AuthBuy.bAccepted, TEXT("J_AuthoredPurchase"));
		Expect(FMath::IsNearlyEqual(AuthBuy.OrbitalCost, 17.0f, 0.01f), TEXT("J_Spent17"));
		Expect(FMath::IsNearlyEqual(
			PS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalAuth - 17.0f,
			1.0f), TEXT("J_SpendMatchesAuthored"));
		LastPodWeak = AuthBuy.SpawnedPod;
		ArrivalWaitAttempts = 0;
		ScheduleNext(0.15f);
		break;
	}
	case 4:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_PlayerState* PS = OwnerPSWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(Inventory) && IsValid(PS), TEXT("J_ArrivalActors")))
		{
			Finish();
			return;
		}
		if (Inventory->GetWallSegmentCount() != 5 && ArrivalWaitAttempts < 40)
		{
			++ArrivalWaitAttempts;
			StageIndex = 4;
			ScheduleNext(0.1f);
			return;
		}
		ArrivalWaitAttempts = 0;
		Expect(Inventory->GetWallSegmentCount() == 5, TEXT("J_ArrivalStock5"));

		const int32 ReadyCount = PS->GetOrbitalBuildingInventoryComponent()->GetReadyEntries().Num();
		Expect(ReadyCount == 0, TEXT("M_ReadyUntouched"));

		FGP_UnitDropManifest Manifest;
		Manifest.WorkerCount = 1;
		GPWallPackageInventoryDebug::GrantOrbital(PS, 100.0f);
		const GPUnitDropAuthority::FEvalResult UnitDrop =
			GPUnitDropAuthority::AuthorityRequestUnitDrop(World, PS, Manifest);
		if (UnitDrop.bAccepted && UnitDrop.SpawnedPod.IsValid())
		{
			Expect(UnitDrop.SpawnedPod->GetPayloadKind() == EGP_DropPodPayloadKind::Unit, TEXT("N_UnitPayloadUnchanged"));
		}
		else
		{
			Expect(true, TEXT("N_UnitDropSkippedPreconditions"));
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_DropPod* BuildingPod = World->SpawnActor<AGP_DropPod>(
			AGP_DropPod::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (IsValid(BuildingPod))
		{
			BuildingPod->AuthorityInitBuildingDrop(
				PS,
				ContractTeam,
				FPrimaryAssetId(),
				nullptr,
				FVector(-51000.0f, 2500.0f, 100.0f),
				FRotator::ZeroRotator,
				0.2f,
				400.0f,
				0.0f,
				0.05f,
				FIntPoint::ZeroValue,
				FIntPoint(1, 1),
				FGuid());
			Expect(BuildingPod->GetPayloadKind() == EGP_DropPodPayloadKind::Building, TEXT("N_BuildingPayloadUnchanged"));
			BuildingPod->Destroy();
		}

		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#endif
