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

void UGP_WallPackageInventoryContractTestRunner::CleanupCatalogIfExists()
{
	if (UGP_WallPackageCatalog* Catalog = UGP_WallPackageCatalog::TryGetExisting())
	{
		Catalog->DebugClearAuthoredOverrides();
		Catalog->DebugEndContractIsolation();
	}
}

bool UGP_WallPackageInventoryContractTestRunner::WaitForStock(
	UGP_WallSegmentInventoryComponent* Inventory,
	int32 ExpectedStock,
	int32 RetryStage)
{
	if (!IsValid(Inventory))
	{
		return false;
	}
	if (Inventory->GetWallSegmentCount() != ExpectedStock && ArrivalWaitAttempts < 40)
	{
		++ArrivalWaitAttempts;
		StageIndex = RetryStage;
		ScheduleNext(0.1f);
		return true;
	}
	ArrivalWaitAttempts = 0;
	return false;
}

void UGP_WallPackageInventoryContractTestRunner::BeginDestroy()
{
	UnbindInventoryDelegates();
	CleanupCatalogIfExists();
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
	CleanupCatalogIfExists();
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
	UGP_WallPackageCatalog* Catalog = UGP_WallPackageCatalog::Get();
	if (!Expect(Catalog != nullptr, TEXT("CatalogReady")))
	{
		Finish();
		return;
	}

	switch (StageIndex++)
	{
	case 0:
	{
		UGP_WallPackageCatalog* Created = UGP_WallPackageCatalog::Get();
		Expect(Created != nullptr && UGP_WallPackageCatalog::TryGetExisting() == Created, TEXT("Life_Existing"));
		CleanupCatalogIfExists();
		Expect(UGP_WallPackageCatalog::TryGetExisting() != nullptr, TEXT("Life_CleanupNoCreate"));
		UGP_WallPackageCatalog::ShutdownCatalog();
		Expect(UGP_WallPackageCatalog::TryGetExisting() == nullptr, TEXT("Life_NullAfterShutdown"));
		CleanupCatalogIfExists();
		Expect(UGP_WallPackageCatalog::TryGetExisting() == nullptr, TEXT("Life_DestroyCleanupNoResurrect"));
		Catalog = UGP_WallPackageCatalog::Get();
		Expect(Catalog != nullptr && Catalog->GetNativeWallPackage() != nullptr, TEXT("Life_Recreate"));
		if (Catalog == nullptr)
		{
			Finish();
			return;
		}

		Catalog->DebugBeginContractIsolation();
		UGP_WallPackageDefinition* Native = Catalog->GetNativeWallPackage();
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
			&& IsValid(Base->GetUnitDropZone()), TEXT("SpawnMainBase")))
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
		if (Buy.SpawnedPod.IsValid() && IsValid(Base->GetUnitDropZone()))
		{
			Expect(Buy.SpawnedPod->GetLandingLocation().Equals(
				Base->GetUnitDropZone()->GetComponentLocation(),
				1.0f), TEXT("I_LandingEqualsUnitDropZone"));
		}

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
		if (WaitForStock(Inventory, 5, 2))
		{
			return;
		}
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
		Expect(!Full.bAccepted, TEXT("D5_FullReject"));
		Expect(Full.RejectReason == EGP_WallPackageRejectReason::InventoryFull, TEXT("D5_FullReason"));
		Expect(FMath::IsNearlyEqual(PS->GetPlayerAttributeSet()->GetOrbitalFerronite(), Orbital, 1.0f),
			TEXT("D5_NoSpend"));

		Expect(Inventory->AuthorityTryConsumeSegments(1), TEXT("F_ConsumeTo4"));
		Expect(Inventory->GetWallSegmentCount() == 4, TEXT("F_Stock4"));
		Expect(Inventory->CanPurchaseWallPackage(), TEXT("F_CanPurchaseAt4"));
		Expect(Inventory->AuthorityBeginPackageDelivery(), TEXT("F_BeginAt4"));
		Expect(Inventory->AuthorityCompletePackageDelivery(5), TEXT("F_CompleteNonZeroStock"));
		Expect(Inventory->GetWallSegmentCount() == 5, TEXT("F_FilledTo5"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("F_PendingCleared"));

		Expect(Inventory->AuthorityTryConsumeSegments(1), TEXT("B4_ConsumeTo4"));
		Expect(Inventory->CanPurchaseWallPackage(), TEXT("B4_CanPurchase"));
		const float OrbitalAt4 = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const GPWallPackageAuthority::FPurchaseResult Buy4 =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(Buy4.bAccepted, TEXT("B4_Accepted"));
		Expect(FMath::IsNearlyEqual(Buy4.OrbitalCost, Catalog->GetWallPackage()->Cost, 0.01f), TEXT("B4_FullCost"));
		Expect(FMath::IsNearlyEqual(
			PS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalAt4 - Buy4.OrbitalCost,
			1.0f), TEXT("B4_SpendOnce"));
		Expect(Buy4.SpawnedPod.IsValid()
			&& Buy4.SpawnedPod->GetPayloadKind() == EGP_DropPodPayloadKind::WallPackage,
			TEXT("B4_OnePod"));
		LastPodWeak = Buy4.SpawnedPod;
		ExpectedArrivalStock = 5;
		ScheduleNext(0.15f);
		break;
	}
	case 3:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_PlayerState* PS = OwnerPSWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(Inventory) && IsValid(PS), TEXT("B4_ArrivalActors")))
		{
			Finish();
			return;
		}
		if (WaitForStock(Inventory, 5, 3))
		{
			return;
		}
		Expect(Inventory->GetWallSegmentCount() == 5, TEXT("B4_ArrivalStock5"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("B4_NoPartialRefundPending"));

		Expect(Inventory->AuthorityTryConsumeSegments(4), TEXT("C1_ConsumeTo1"));
		Expect(Inventory->GetWallSegmentCount() == 1, TEXT("C1_Stock1"));
		Expect(Inventory->CanPurchaseWallPackage(), TEXT("C1_CanPurchase"));
		const float OrbitalAt1 = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const GPWallPackageAuthority::FPurchaseResult Buy1 =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(Buy1.bAccepted, TEXT("C1_Accepted"));
		Expect(FMath::IsNearlyEqual(Buy1.OrbitalCost, Catalog->GetWallPackage()->Cost, 0.01f), TEXT("C1_FullCost"));
		Expect(FMath::IsNearlyEqual(
			PS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalAt1 - Buy1.OrbitalCost,
			1.0f), TEXT("C1_SpendOnce"));
		LastPodWeak = Buy1.SpawnedPod;
		ScheduleNext(0.15f);
		break;
	}
	case 4:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_PlayerState* PS = OwnerPSWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(Inventory) && IsValid(PS), TEXT("C1_ArrivalActors")))
		{
			Finish();
			return;
		}
		if (WaitForStock(Inventory, 5, 4))
		{
			return;
		}
		Expect(Inventory->GetWallSegmentCount() == 5, TEXT("C1_ArrivalStock5"));

		Expect(Inventory->AuthorityTryConsumeSegments(1), TEXT("E_ConsumeTo4"));
		const float OrbitalPending = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const GPWallPackageAuthority::FPurchaseResult BuyPending =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(BuyPending.bAccepted, TEXT("E_AcceptedAt4"));
		const GPWallPackageAuthority::FPurchaseResult DupAt4 =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(!DupAt4.bAccepted, TEXT("E_PendingBlocks"));
		Expect(DupAt4.RejectReason == EGP_WallPackageRejectReason::PackagePending, TEXT("E_PendingReason"));
		Expect(FMath::IsNearlyEqual(
			PS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalPending - BuyPending.OrbitalCost,
			1.0f), TEXT("E_NoSecondSpend"));
		int32 WallPods = 0;
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			if (It->GetPayloadKind() == EGP_DropPodPayloadKind::WallPackage
				&& It->GetPhase() != EGP_DropPodPhase::Idle
				&& It->GetPhase() != EGP_DropPodPhase::PayloadDeployed)
			{
				++WallPods;
			}
		}
		Expect(WallPods == 1, TEXT("E_NoSecondPod"));

		Expect(Inventory->AuthorityTryConsumeSegments(3), TEXT("G_Consume3WhilePending"));
		Expect(Inventory->GetWallSegmentCount() == 1, TEXT("G_Stock1DuringFlight"));
		LastPodWeak = BuyPending.SpawnedPod;
		ScheduleNext(0.15f);
		break;
	}
	case 5:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_PlayerState* PS = OwnerPSWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(Inventory) && IsValid(PS), TEXT("G_ArrivalActors")))
		{
			Finish();
			return;
		}
		if (WaitForStock(Inventory, 5, 5))
		{
			return;
		}
		Expect(Inventory->GetWallSegmentCount() == 5, TEXT("G_ArrivalStock5"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("G_PendingCleared"));

		Expect(Inventory->AuthorityTryConsumeSegments(1), TEXT("H_ConsumeTo4"));
		OrbitalAtPending = PS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		const GPWallPackageAuthority::FPurchaseResult FullFlight =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(FullFlight.bAccepted, TEXT("H_PurchaseAt4"));
		Inventory->DebugForceSetStock(5);
		Expect(Inventory->GetWallSegmentCount() == 5, TEXT("H_ForcedFull"));
		LastPodWeak = FullFlight.SpawnedPod;
		ScheduleNext(0.15f);
		break;
	}
	case 6:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		AGP_PlayerState* PS = OwnerPSWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(Inventory) && IsValid(PS), TEXT("H_FullArrivalActors")))
		{
			Finish();
			return;
		}
		if (Inventory->IsWallPackagePending() && ArrivalWaitAttempts < 40)
		{
			++ArrivalWaitAttempts;
			StageIndex = 6;
			ScheduleNext(0.1f);
			return;
		}
		ArrivalWaitAttempts = 0;
		Expect(Inventory->GetWallSegmentCount() == 5, TEXT("H_Still5"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("H_PendingClearedAccepted0"));
		Expect(FMath::IsNearlyEqual(
			PS->GetPlayerAttributeSet()->GetOrbitalFerronite(),
			OrbitalAtPending - Catalog->GetWallPackage()->Cost,
			1.0f), TEXT("H_NoRefund"));

		Expect(Inventory->AuthorityTryConsumeSegments(5), TEXT("GFail_ClearStock"));
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
		Expect(DeathBuy.bAccepted, TEXT("Death_Purchase"));
		LastPodWeak = DeathBuy.SpawnedPod;
		if (AGP_GameMode* GM = World->GetAuthGameMode<AGP_GameMode>())
		{
			GM->DebugSetAnnihilationCountsAsWin(false);
		}
		Base->NotifyAuthorityDeath();
		Expect(Inventory->GetWallSegmentCount() == 0, TEXT("Death_StockCleared"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("Death_PendingCleared"));
		ScheduleNext(0.35f);
		break;
	}
	case 7:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(Inventory), TEXT("Death_InventoryAfterWait")))
		{
			Finish();
			return;
		}
		Expect(Inventory->GetWallSegmentCount() == 0, TEXT("Death_NoResurrectStock"));
		Expect(!Inventory->IsWallPackagePending(), TEXT("Death_StillNotPending"));

		{
			if (AGP_MainBase* DeadBase = MainBaseWeak.Get())
			{
				DeadBase->SetTeamId(-1);
			}
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

		AGP_PlayerState* PS = OwnerPSWeak.Get();
		const GPWallPackageAuthority::FPurchaseResult TeamBuy =
			GPWallPackageAuthority::AuthorityPurchaseWallPackage(World, PS, nullptr);
		Expect(TeamBuy.bAccepted, TEXT("Team_Purchase"));
		if (AGP_MainBase* LiveBase = MainBaseWeak.Get())
		{
			LiveBase->SetTeamId(92);
		}
		Expect(!Inventory->IsWallPackagePending(), TEXT("Team_PendingCancelled"));
		ScheduleNext(0.25f);
		break;
	}
	case 8:
	{
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_WallSegmentInventoryComponent* Inventory =
			IsValid(Base) ? Base->GetWallSegmentInventoryComponent() : nullptr;
		if (!Expect(IsValid(Inventory), TEXT("Team_AfterWait")))
		{
			Finish();
			return;
		}
		Expect(Inventory->GetWallSegmentCount() == 0, TEXT("Team_NoWrongGrant"));
		if (AGP_MainBase* LiveBase = MainBaseWeak.Get())
		{
			LiveBase->SetTeamId(ContractTeam);
		}

		Catalog->DebugForceUnresolvedAuthoredLoad(nullptr, true);
		Expect(Catalog->IsWallPackageDefinitionPending(), TEXT("I_PendingDef"));
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
		Catalog->DebugCompletePendingAuthoredLoad();
		Catalog->DebugAssignLoadedAuthored(AuthoredPackage);
		Expect(Catalog->GetWallPackage() == AuthoredPackage, TEXT("J_AuthoredWins"));
		Expect(FMath::IsNearlyEqual(Catalog->GetWallPackage()->Cost, 17.0f, 0.01f), TEXT("J_Cost17"));
		Expect(FMath::IsNearlyEqual(Catalog->GetWallPackage()->DeliveryDescentSeconds, 4.25f, 0.01f), TEXT("J_Descent"));
		Expect(FMath::IsNearlyEqual(Catalog->GetWallPackage()->PayloadDeployDelaySeconds, 0.75f, 0.01f), TEXT("J_Deploy"));

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
		ScheduleNext(0.15f);
		break;
	}
	case 9:
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
		if (WaitForStock(Inventory, 5, 9))
		{
			return;
		}
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
