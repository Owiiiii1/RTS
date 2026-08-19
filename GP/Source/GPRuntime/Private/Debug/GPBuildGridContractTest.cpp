// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPBuildGridContractTest.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPBuildingDefinition.h"
#include "Buildings/GPLogisticsHub.h"
#include "Buildings/GPMainBase.h"
#include "Buildings/Grid/GPBuildGridSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Effects/GPGE_AddOrbital.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Orbital/GPBuildingDropAuthority.h"
#include "Orbital/GPBuildingDropCatalog.h"
#include "Orbital/GPBuildingPlacementGhost.h"
#include "Orbital/GPDropPod.h"
#include "Orbital/GPOrbitalBuildingInventoryComponent.h"
#include "Orbital/GPOrbitalDropDefinition.h"
#include "Player/GPPlayerState.h"
#include "Resources/GPResourceNode.h"
#include "Settings/GPOrbitalDeliverySettings.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPBuildGridContract, Log, All);

namespace GPBuildGridContractDebug
{
	static TWeakObjectPtr<UGP_BuildGridContractTestRunner> GActiveRunner;
	constexpr int32 ContractTeam = 93;

	struct FScopedBoxAuthoring
	{
		UBoxComponent* Box = nullptr;
		FVector Extent = FVector::ZeroVector;
		FVector Location = FVector::ZeroVector;
		FVector Scale = FVector::OneVector;

		explicit FScopedBoxAuthoring(UBoxComponent* InBox)
			: Box(InBox)
		{
			if (Box != nullptr)
			{
				Extent = Box->GetUnscaledBoxExtent();
				Location = Box->GetRelativeLocation();
				Scale = Box->GetRelativeScale3D();
			}
		}

		~FScopedBoxAuthoring()
		{
			if (Box != nullptr)
			{
				Box->SetBoxExtent(Extent);
				Box->SetRelativeLocation(Location);
				Box->SetRelativeScale3D(Scale);
			}
		}

		void DisableUsableBounds()
		{
			if (Box != nullptr)
			{
				Box->SetBoxExtent(FVector(0.0f, 0.0f, 20.0f));
				Box->SetRelativeScale3D(FVector::OneVector);
			}
		}
	};

	static bool VisualHalfMatchesAuthored(const UBoxComponent* Bounds, float Tolerance = 0.5f)
	{
		if (Bounds == nullptr)
		{
			return false;
		}
		const FVector Authored = UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(Bounds);
		const FVector Visual = Bounds->GetScaledBoxExtent();
		return FMath::IsNearlyEqual(Authored.X, FMath::Abs(Visual.X), Tolerance)
			&& FMath::IsNearlyEqual(Authored.Y, FMath::Abs(Visual.Y), Tolerance);
	}

	static bool BoxFollowsActorYaw(const UBoxComponent* Bounds, const AActor* Owner, float YawTolerance = 1.0f)
	{
		if (Bounds == nullptr || Owner == nullptr || Bounds->IsUsingAbsoluteRotation())
		{
			return false;
		}
		return FMath::IsNearlyEqual(
			FRotator::NormalizeAxis(Bounds->GetComponentRotation().Yaw),
			FRotator::NormalizeAxis(Owner->GetActorRotation().Yaw + Bounds->GetRelativeRotation().Yaw),
			YawTolerance);
	}

	static bool CellSetsEqual(TArray<FIntPoint> A, TArray<FIntPoint> B)
	{
		A.Sort([](const FIntPoint& L, const FIntPoint& R) { return L.Y < R.Y || (L.Y == R.Y && L.X < R.X); });
		B.Sort([](const FIntPoint& L, const FIntPoint& R) { return L.Y < R.Y || (L.Y == R.Y && L.X < R.X); });
		return A == B;
	}

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

	static int32 RoundMaxUnits(const AGP_PlayerState* PS)
	{
		if (PS == nullptr || PS->GetPlayerAttributeSet() == nullptr)
		{
			return 0;
		}
		return FMath::RoundToInt(PS->GetPlayerAttributeSet()->GetMaxUnits());
	}

	static bool AllCellsOccupied(UGP_BuildGridSubsystem* Grid, FIntPoint Origin, FIntPoint Size)
	{
		if (Grid == nullptr)
		{
			return false;
		}
		TArray<FIntPoint> Cells;
		Grid->EnumerateFootprintCells(Origin, Size, Cells);
		if (Cells.Num() != Size.X * Size.Y)
		{
			return false;
		}
		for (const FIntPoint& Cell : Cells)
		{
			if (!Grid->IsCellOccupied(Cell))
			{
				return false;
			}
		}
		return true;
	}

	static bool AllCellsFree(UGP_BuildGridSubsystem* Grid, FIntPoint Origin, FIntPoint Size)
	{
		if (Grid == nullptr)
		{
			return false;
		}
		TArray<FIntPoint> Cells;
		Grid->EnumerateFootprintCells(Origin, Size, Cells);
		for (const FIntPoint& Cell : Cells)
		{
			if (Grid->IsCellOccupied(Cell))
			{
				return false;
			}
		}
		return true;
	}

	static void RunBuildGridContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPBuildGridContract, Warning, TEXT("gp.Building.RunBuildGridContractTest: missing world or client"));
			return;
		}
		if (GActiveRunner.IsValid())
		{
			UE_LOG(LogGPBuildGridContract, Warning, TEXT("gp.Building.RunBuildGridContractTest: rejected — already running"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("BuildGridContract"), TEXT("BuildGrid"), Token))
		{
			return;
		}

		UGP_BuildGridContractTestRunner* Runner =
			NewObject<UGP_BuildGridContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveRunner = Runner;
		Runner->SetExecutionToken(Token.ExecutionId, Token.OwnerTag);
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GBuildGridContract(
		TEXT("gp.Building.RunBuildGridContractTest"),
		TEXT("GP-S36G BuildGrid occupancy / snap / reservation contract (A–Y)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunBuildGridContractTest));
}

void UGP_BuildGridContractTestRunner::BeginDestroy()
{
	RestoreSettings();
	CleanupActors();
	UnbindWorldCleanup();
	Super::BeginDestroy();
}

void UGP_BuildGridContractTestRunner::RestoreSettings()
{
	if (!bSettingsMutated)
	{
		return;
	}
	if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
	{
		UGP_BuildingDropCatalog::Get().OverrideDeliveryTiming(
			UGP_BuildingDropCatalog::NativeDeliveryDescentSeconds,
			UGP_BuildingDropCatalog::NativePayloadDeployDelaySeconds);
		Settings->BuildingDropCleanupDelaySeconds = SavedBuildingCleanup;
		Settings->BuildingDropSpawnAltitudeCm = SavedBuildingAltitude;
	}
	bSettingsMutated = false;
}

void UGP_BuildGridContractTestRunner::CleanupActors()
{
	if (UWorld* World = WorldWeak.Get())
	{
		for (TActorIterator<AGP_DropPod> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_BuildingPlacementGhost> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_BuildGridContractStub> It(World); It; ++It)
		{
			It->Destroy();
		}
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			if (It->GetTeamId() == GPBuildGridContractDebug::ContractTeam)
			{
				It->Destroy();
			}
		}
		if (AGP_MainBase* Base = MainBaseWeak.Get())
		{
			Base->Destroy();
		}
		if (AGP_PlayerState* PS = OwnerPSWeak.Get())
		{
			PS->Destroy();
		}
	}
	MainBaseWeak.Reset();
	OwnerPSWeak.Reset();
	LastPodWeak.Reset();
	SkipPodWeak.Reset();
	LiveHubWeak.Reset();
}

void UGP_BuildGridContractTestRunner::Finish()
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
	RestoreSettings();
	CleanupActors();
	UnbindWorldCleanup();
	GPContractTestCoordinator::Release(
		ExecutionId,
		Failures,
		bCancelled,
		bCancelled ? *CancelReason.ToString() : TEXT("None"));
	UE_LOG(LogGPBuildGridContract, Log,
		TEXT("gp.Building.RunBuildGridContractTest: Complete Failures=%d Cancelled=%s"),
		Failures,
		bCancelled ? TEXT("true") : TEXT("false"));
	RemoveFromRoot();
	GPBuildGridContractDebug::GActiveRunner.Reset();
}

void UGP_BuildGridContractTestRunner::Abort(const TCHAR* Reason)
{
	UE_LOG(LogGPBuildGridContract, Error,
		TEXT("gp.Building.RunBuildGridContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

bool UGP_BuildGridContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPBuildGridContract, Error,
			TEXT("gp.Building.RunBuildGridContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPBuildGridContract, Log,
		TEXT("gp.Building.RunBuildGridContractTest PASS: %s"), Label);
	return true;
}

void UGP_BuildGridContractTestRunner::ScheduleNext(float DelaySeconds)
{
	UWorld* World = WorldWeak.Get();
	if (World == nullptr)
	{
		Abort(TEXT("MissingWorldSchedule"));
		return;
	}
	World->GetTimerManager().SetTimer(
		StageTimerHandle,
		FTimerDelegate::CreateUObject(this, &UGP_BuildGridContractTestRunner::AdvanceStage),
		FMath::Max(0.01f, DelaySeconds),
		false);
}

void UGP_BuildGridContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == WorldWeak.Get())
	{
		bCancelled = true;
		CancelReason = TEXT("WorldCleanup");
		Finish();
	}
}

void UGP_BuildGridContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_BuildGridContractTestRunner::Start(UWorld* InWorld)
{
	WorldWeak = InWorld;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_BuildGridContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPBuildGridContract, Log, TEXT("gp.Building.RunBuildGridContractTest Start"));
	StageIndex = 0;
	ScheduleNext(0.1f);
}

void UGP_BuildGridContractTestRunner::AdvanceStage()
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

	UGP_BuildGridSubsystem* Grid = World->GetSubsystem<UGP_BuildGridSubsystem>();
	if (!Expect(Grid != nullptr, TEXT("GridSubsystemPresent")))
	{
		Finish();
		return;
	}

	switch (StageIndex)
	{
	case 0: // A–E conversion / footprint math
	{
		Expect(FMath::IsNearlyEqual(Grid->GetCellSize(), 200.0f), TEXT("A_CellSize200"));
		Expect(Grid->GetGridOriginXY().Equals(FVector2D::ZeroVector), TEXT("A_OriginXYZero"));

		Expect(Grid->WorldToCell(FVector(250.0f, 250.0f, 0.0f)) == FIntPoint(1, 1), TEXT("B_PositiveWorldToCell"));
		Expect(Grid->WorldToCell(FVector(-250.0f, -250.0f, 0.0f)) == FIntPoint(-1, -1), TEXT("B_NegativeWorldToCell"));
		Expect(Grid->WorldToCell(FVector(100.0f, 100.0f, 0.0f)) == FIntPoint(1, 1), TEXT("B_HalfCellPositiveBoundary"));
		Expect(Grid->WorldToCell(FVector(-100.0f, -100.0f, 0.0f)) == FIntPoint(0, 0), TEXT("B_HalfCellNegativeBoundary"));
		Expect(Grid->WorldToCell(FVector(0.0f, 0.0f, 0.0f)) == FIntPoint(0, 0), TEXT("B_ExactOrigin"));
		Expect(Grid->WorldToCell(FVector(-300.0f, 0.0f, 0.0f)) == FIntPoint(-1, 0), TEXT("B_ExactNegativeBoundary"));

		const FIntPoint RoundtripCells[] = {
			FIntPoint(0, 0), FIntPoint(1, -3), FIntPoint(-4, 2), FIntPoint(7, 7)
		};
		bool bRoundtrip = true;
		for (const FIntPoint& Cell : RoundtripCells)
		{
			const FVector WorldLoc = Grid->CellToWorld(Cell, 50.0f);
			if (Grid->WorldToCell(WorldLoc) != Cell)
			{
				bRoundtrip = false;
				break;
			}
		}
		Expect(bRoundtrip, TEXT("C_CellWorldRoundtrip"));

		TArray<FIntPoint> Cells1;
		Grid->EnumerateFootprintCells(FIntPoint(3, 4), FIntPoint(1, 1), Cells1);
		Expect(Cells1.Num() == 1 && Cells1[0] == FIntPoint(3, 4), TEXT("D_Footprint1x1"));
		TArray<FIntPoint> Cells2;
		Grid->EnumerateFootprintCells(FIntPoint(0, 0), FIntPoint(2, 2), Cells2);
		Expect(Cells2.Num() == 4, TEXT("D_Footprint2x2"));
		TArray<FIntPoint> Cells4;
		Grid->EnumerateFootprintCells(FIntPoint(10, 20), FIntPoint(4, 4), Cells4);
		Expect(Cells4.Num() == 16 && Cells4[0] == FIntPoint(10, 20) && Cells4.Last() == FIntPoint(13, 23),
			TEXT("D_Footprint4x4"));
		TArray<FIntPoint> Cells5;
		Grid->EnumerateFootprintCells(FIntPoint(-2, -2), FIntPoint(5, 5), Cells5);
		Expect(Cells5.Num() == 25, TEXT("D_Footprint5x5"));

		const FVector Center4 = Grid->GetFootprintCenterWorld(FIntPoint(0, 0), FIntPoint(4, 4), 0.0f);
		Expect(FMath::IsNearlyEqual(Center4.X, 300.0f) && FMath::IsNearlyEqual(Center4.Y, 300.0f),
			TEXT("E_EvenFootprintCentersBetweenCells"));
		const FVector Center5 = Grid->GetFootprintCenterWorld(FIntPoint(0, 0), FIntPoint(5, 5), 0.0f);
		Expect(FMath::IsNearlyEqual(Center5.X, 400.0f), TEXT("E_OddFootprintCentersOnCell"));

		FVector Min4 = FVector::ZeroVector;
		FVector Max4 = FVector::ZeroVector;
		Grid->GetFootprintWorldAABB(FIntPoint(0, 0), FIntPoint(4, 4), 0.0f, Min4, Max4);
		Expect(FMath::IsNearlyEqual(Max4.X - Min4.X, 800.0f) && FMath::IsNearlyEqual(Max4.Y - Min4.Y, 800.0f),
			TEXT("Preview_4x4Outer800"));
		Expect(Grid->DoFootprintsOverlap(FIntPoint(0, 0), FIntPoint(4, 4), FIntPoint(1, 1), FIntPoint(4, 4)),
			TEXT("Preview_OverlapTrue"));
		Expect(!Grid->DoFootprintsOverlap(FIntPoint(0, 0), FIntPoint(4, 4), FIntPoint(4, 0), FIntPoint(4, 4)),
			TEXT("Preview_AdjacentNoOverlap"));

		Expect(Grid->ConvertAuthoredBoundsToFootprintCells(FVector(100.0f, 100.0f, 20.0f)) == FIntPoint(1, 1),
			TEXT("Bounds_200x200_1x1"));
		Expect(Grid->ConvertAuthoredBoundsToFootprintCells(FVector(200.0f, 200.0f, 20.0f)) == FIntPoint(2, 2),
			TEXT("Bounds_400x400_2x2"));
		Expect(Grid->ConvertAuthoredBoundsToFootprintCells(FVector(275.0f, 190.0f, 20.0f)) == FIntPoint(3, 2),
			TEXT("Bounds_550x380_3x2"));
		Expect(Grid->ConvertAuthoredBoundsToFootprintCells(FVector(0.25f, 0.25f, 20.0f)) == FIntPoint(1, 1),
			TEXT("Bounds_Minimum1x1"));

		UGP_BuildingDefinition* DaFallback = NewObject<UGP_BuildingDefinition>(
			this, FName(TEXT("DA_GP_Building_FootprintFallback")), RF_Transient);
		DaFallback->FootprintCells = FIntPoint(4, 4);

		{
			const AGP_BuildGridContractStub* StubCDO = GetDefault<AGP_BuildGridContractStub>();
			const UBoxComponent* CDOBounds = StubCDO != nullptr ? StubCDO->GetPlacementFootprintBounds() : nullptr;
			const UBoxComponent* CDONav = StubCDO != nullptr ? StubCDO->GetNavigationObstacle() : nullptr;
			Expect(CDOBounds != nullptr && CDOBounds->GetUnscaledBoxExtent().Equals(FVector(100.0f, 100.0f, 20.0f), 0.1f),
				TEXT("Bounds_NativeGenericExtent100"));
			Expect(CDOBounds != nullptr && CDOBounds->bEditableWhenInherited
				&& CDOBounds->IsEditableWhenInherited(),
				TEXT("Authoring_BoundsEditableWhenInherited"));
			Expect(CDONav != nullptr && CDONav->bEditableWhenInherited
				&& CDONav->IsEditableWhenInherited(),
				TEXT("Authoring_NavEditableWhenInherited"));
			const FGP_ResolvedBuildingFootprint NativeGeneric =
				Grid->ResolveBuildingFootprint(AGP_BuildGridContractStub::StaticClass(), nullptr);
			Expect(NativeGeneric.bFromAuthoredBounds && NativeGeneric.SizeCells == FIntPoint(1, 1),
				TEXT("Bounds_NativeGeneric1x1"));
			const FGP_ResolvedBuildingFootprint NativeGenericVsDa =
				Grid->ResolveBuildingFootprint(AGP_BuildGridContractStub::StaticClass(), DaFallback);
			Expect(NativeGenericVsDa.bFromAuthoredBounds && NativeGenericVsDa.SizeCells == FIntPoint(1, 1),
				TEXT("Bounds_NativeGenericOverridesDA"));
		}

		{
			const AGP_LogisticsHub* HubCDO = GetDefault<AGP_LogisticsHub>();
			const UBoxComponent* HubBounds = HubCDO != nullptr ? HubCDO->GetPlacementFootprintBounds() : nullptr;
			Expect(HubBounds != nullptr && HubBounds->GetUnscaledBoxExtent().Equals(FVector(400.0f, 400.0f, 20.0f), 0.1f),
				TEXT("Bounds_NativeHubExtent400"));
			const FGP_ResolvedBuildingFootprint NativeHub =
				Grid->ResolveBuildingFootprint(AGP_LogisticsHub::StaticClass(), nullptr);
			const FGP_ResolvedBuildingFootprint NativeHubVsDa =
				Grid->ResolveBuildingFootprint(AGP_LogisticsHub::StaticClass(), DaFallback);
			Expect(NativeHub.bFromAuthoredBounds && NativeHub.SizeCells == FIntPoint(4, 4),
				TEXT("Bounds_NativeHub4x4"));
			Expect(NativeHubVsDa.SizeCells == NativeHub.SizeCells && NativeHubVsDa.bFromAuthoredBounds,
				TEXT("Preview_ServerNativeHubFootprintMatch"));
		}

		{
			const AGP_MainBase* BaseCDO = GetDefault<AGP_MainBase>();
			const UBoxComponent* BaseBounds = BaseCDO != nullptr ? BaseCDO->GetPlacementFootprintBounds() : nullptr;
			Expect(BaseBounds != nullptr && BaseBounds->GetUnscaledBoxExtent().Equals(FVector(500.0f, 500.0f, 20.0f), 0.1f),
				TEXT("Bounds_NativeMainBaseExtent500"));
			const FGP_ResolvedBuildingFootprint NativeBase =
				Grid->ResolveBuildingFootprint(AGP_MainBase::StaticClass(), nullptr);
			Expect(NativeBase.bFromAuthoredBounds && NativeBase.SizeCells == FIntPoint(5, 5),
				TEXT("Bounds_NativeMainBase5x5"));
			Expect(BaseCDO->GetActorScale3D().Equals(FVector::OneVector, 0.01f),
				TEXT("ScaleTrace_NativeMainBaseActorScale1"));
			const UCapsuleComponent* NativeCapsule = BaseCDO->GetCapsuleComponent();
			Expect(NativeCapsule != nullptr
				&& NativeCapsule->GetRelativeScale3D().Equals(FVector::OneVector, 0.01f),
				TEXT("ScaleTrace_NativeCapsuleRelScale1"));
			Expect(BaseBounds->IsUsingAbsoluteScale()
				&& !BaseBounds->IsUsingAbsoluteRotation()
				&& BaseBounds->GetRelativeScale3D().Equals(FVector::OneVector, 0.01f),
				TEXT("ScaleTrace_NativeBoundsAbsoluteOwnScale1"));
			UE_LOG(
				LogGPBuildGridContract,
				Log,
				TEXT("ScaleTrace native MainBase CDO actor=(%.3f,%.3f,%.3f) capsuleRel=(%.3f,%.3f,%.3f) boundsRel=(%.3f,%.3f,%.3f) boundsComp=(%.3f,%.3f,%.3f) unscaled=(%.1f,%.1f) authoredHalf=(%.1f,%.1f)"),
				BaseCDO->GetActorScale3D().X,
				BaseCDO->GetActorScale3D().Y,
				BaseCDO->GetActorScale3D().Z,
				NativeCapsule != nullptr ? NativeCapsule->GetRelativeScale3D().X : 0.0f,
				NativeCapsule != nullptr ? NativeCapsule->GetRelativeScale3D().Y : 0.0f,
				NativeCapsule != nullptr ? NativeCapsule->GetRelativeScale3D().Z : 0.0f,
				BaseBounds->GetRelativeScale3D().X,
				BaseBounds->GetRelativeScale3D().Y,
				BaseBounds->GetRelativeScale3D().Z,
				BaseBounds->GetComponentScale().X,
				BaseBounds->GetComponentScale().Y,
				BaseBounds->GetComponentScale().Z,
				BaseBounds->GetUnscaledBoxExtent().X,
				BaseBounds->GetUnscaledBoxExtent().Y,
				UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(BaseBounds).X,
				UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(BaseBounds).Y);
		}

		AGP_BuildGridContractStub* MutableStubCDO = GetMutableDefault<AGP_BuildGridContractStub>();
		UBoxComponent* MutableCDOBounds =
			MutableStubCDO != nullptr ? MutableStubCDO->GetPlacementFootprintBounds() : nullptr;
		FGP_ResolvedBuildingFootprint CdoResolved;
		FGP_ResolvedBuildingFootprint CdoScaled;
		FGP_ResolvedBuildingFootprint DaWhenUnusable;
		if (Expect(MutableCDOBounds != nullptr, TEXT("Authoring_MutableCDOBounds")))
		{
			{
				GPBuildGridContractDebug::FScopedBoxAuthoring Restore(MutableCDOBounds);
				MutableCDOBounds->SetBoxExtent(FVector(200.0f, 200.0f, 20.0f));
				MutableCDOBounds->SetRelativeLocation(FVector(100.0f, -30.0f, 0.0f));
				MutableCDOBounds->SetRelativeScale3D(FVector::OneVector);
				CdoResolved = Grid->ResolveBuildingFootprint(AGP_BuildGridContractStub::StaticClass(), DaFallback);
			}
			{
				GPBuildGridContractDebug::FScopedBoxAuthoring Restore(MutableCDOBounds);
				MutableCDOBounds->SetBoxExtent(FVector(400.0f, 400.0f, 20.0f));
				MutableCDOBounds->SetRelativeLocation(FVector(80.0f, 0.0f, 0.0f));
				MutableCDOBounds->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.0f));
				CdoScaled = Grid->ResolveBuildingFootprint(AGP_BuildGridContractStub::StaticClass(), DaFallback);
			}
			{
				GPBuildGridContractDebug::FScopedBoxAuthoring Restore(MutableCDOBounds);
				Restore.DisableUsableBounds();
				DaWhenUnusable = Grid->ResolveBuildingFootprint(AGP_BuildGridContractStub::StaticClass(), DaFallback);
			}
		}
		Expect(CdoResolved.bFromAuthoredBounds && CdoResolved.SizeCells == FIntPoint(2, 2),
			TEXT("Authoring_CDOExtent200Is2x2"));
		Expect(FMath::IsNearlyEqual(CdoResolved.LocalCenterOffsetCm.X, 100.0f)
			&& FMath::IsNearlyEqual(CdoResolved.LocalCenterOffsetCm.Y, -30.0f),
			TEXT("Authoring_CDORelativeLocationOffset"));
		Expect(CdoScaled.bFromAuthoredBounds && CdoScaled.SizeCells == FIntPoint(2, 2),
			TEXT("Authoring_CDOScaleReadsAs2x2"));
		Expect(FMath::IsNearlyEqual(CdoScaled.LocalCenterOffsetCm.X, 80.0f)
			&& FMath::IsNearlyEqual(CdoScaled.LocalCenterOffsetCm.Y, 0.0f),
			TEXT("Authoring_CDOOffsetIndependentOfScale"));
		Expect(DaWhenUnusable.SizeCells == FIntPoint(4, 4) && !DaWhenUnusable.bFromAuthoredBounds,
			TEXT("Bounds_NoUsableBoundsUsesDA"));

		FActorSpawnParameters BoundsParams;
		BoundsParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		BoundsParams.ObjectFlags |= RF_Transient;
		const FTransform BoundsTM(FRotator::ZeroRotator, FVector(-71000.0f, 8500.0f, 100.0f));
		AGP_BuildGridContractStub* BoundsStub = World->SpawnActorDeferred<AGP_BuildGridContractStub>(
			AGP_BuildGridContractStub::StaticClass(),
			BoundsTM,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Expect(IsValid(BoundsStub) && BoundsStub->GetPlacementFootprintBounds() != nullptr,
			TEXT("Bounds_SpawnAuthoredStub")))
		{
			UBoxComponent* Box = BoundsStub->GetPlacementFootprintBounds();
			Expect(Box->IsEditableWhenInherited(), TEXT("Authoring_InstanceEditableWhenInherited"));
			Box->SetBoxExtent(FVector(275.0f, 190.0f, 20.0f));
			Box->SetRelativeLocation(FVector(120.0f, -40.0f, 0.0f));
			BoundsStub->FinishSpawning(BoundsTM);

			const FGP_ResolvedBuildingFootprint Authored = Grid->ResolveActorFootprint(BoundsStub, DaFallback);
			Expect(Authored.bFromAuthoredBounds && Authored.SizeCells == FIntPoint(3, 2),
				TEXT("Bounds_AuthoredOverridesDA"));
			Expect(FMath::IsNearlyEqual(Authored.LocalCenterOffsetCm.X, 120.0f)
				&& FMath::IsNearlyEqual(Authored.LocalCenterOffsetCm.Y, -40.0f),
				TEXT("Bounds_LocalXYOffsetPreserved"));

			const FVector FootprintHint = UGP_BuildGridSubsystem::MakeWorldFootprintCenter(
				BoundsStub->GetActorLocation(),
				BoundsStub->GetActorRotation(),
				Authored.LocalCenterOffsetCm);
			FIntPoint AuthoredOrigin = FIntPoint::ZeroValue;
			FVector AuthoredCenter = FVector::ZeroVector;
			Grid->ResolveSnappedPlacement(FootprintHint, Authored.SizeCells, AuthoredOrigin, AuthoredCenter);
			Expect(BoundsStub->GetGridFootprintSize() == FIntPoint(3, 2), TEXT("Bounds_PreplacedUsesAuthoredSize"));
			Expect(BoundsStub->GetGridOriginCell() == AuthoredOrigin, TEXT("Bounds_PreplacedOriginFromOffset"));
			Expect(GPBuildGridContractDebug::AllCellsOccupied(Grid, AuthoredOrigin, FIntPoint(3, 2)),
				TEXT("Bounds_SpawnedRegistersResolvedCells"));

			const FVector RebuiltActor = Grid->MakeActorLocationFromFootprintCenter(
				FootprintHint, Authored.LocalCenterOffsetCm, BoundsStub->GetActorRotation());
			Expect(FVector::Dist2D(RebuiltActor, BoundsStub->GetActorLocation()) <= 1.0f,
				TEXT("Bounds_ActorPivotPreservedVsFootprint"));

			Box->SetBoxExtent(FVector(400.0f, 400.0f, 20.0f));
			Box->SetRelativeLocation(FVector(50.0f, 25.0f, 0.0f));
			Box->SetRelativeScale3D(FVector::OneVector);
			Expect(Grid->ResolveActorFootprint(BoundsStub, DaFallback).SizeCells == FIntPoint(4, 4),
				TEXT("Bounds_HubBaselineScale1Is4x4"));
			Box->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.0f));
			const FGP_ResolvedBuildingFootprint ScaledHalf = Grid->ResolveActorFootprint(BoundsStub, DaFallback);
			Expect(ScaledHalf.bFromAuthoredBounds && ScaledHalf.SizeCells == FIntPoint(2, 2),
				TEXT("Bounds_HubScaleHalfIs2x2"));
			Expect(FMath::IsNearlyEqual(ScaledHalf.LocalCenterOffsetCm.X, 50.0f)
				&& FMath::IsNearlyEqual(ScaledHalf.LocalCenterOffsetCm.Y, 25.0f),
				TEXT("Bounds_OffsetIndependentOfScale"));
			Box->SetRelativeScale3D(FVector(0.75f, 0.5f, 1.0f));
			Expect(Grid->ResolveActorFootprint(BoundsStub, DaFallback).SizeCells == FIntPoint(3, 2),
				TEXT("Bounds_HubScale075x05Is3x2"));
			Box->SetRelativeScale3D(FVector(1.25f, 0.75f, 1.0f));
			Expect(Grid->ResolveActorFootprint(BoundsStub, DaFallback).SizeCells == FIntPoint(5, 3),
				TEXT("Bounds_HubScale125x075Is5x3"));

			Box->SetBoxExtent(FVector(100.0f, 100.0f, 20.0f));
			Box->SetRelativeScale3D(FVector::OneVector);
			BoundsStub->SetActorScale3D(FVector(2.0f, 2.0f, 2.0f));
			Expect(Grid->ResolveActorFootprint(BoundsStub, DaFallback).SizeCells == FIntPoint(1, 1),
				TEXT("Bounds_ActorScaleDoesNotInflateFootprint"));
			BoundsStub->SetActorScale3D(FVector::OneVector);
			BoundsStub->Destroy();
		}

		{
			FActorSpawnParameters ScaleSpawnParams;
			ScaleSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ScaleSpawnParams.ObjectFlags |= RF_Transient;
			const FTransform ScaleTM(FRotator::ZeroRotator, FVector(-72000.0f, 8700.0f, 100.0f));
			AGP_BuildGridContractStub* ScaleStub = World->SpawnActorDeferred<AGP_BuildGridContractStub>(
				AGP_BuildGridContractStub::StaticClass(),
				ScaleTM,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Expect(IsValid(ScaleStub) && ScaleStub->GetPlacementFootprintBounds() != nullptr,
				TEXT("Bounds_SpawnScaledStub")))
			{
				UBoxComponent* Box = ScaleStub->GetPlacementFootprintBounds();
				Box->SetBoxExtent(FVector(400.0f, 400.0f, 20.0f));
				Box->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.0f));
				ScaleStub->FinishSpawning(ScaleTM);
				Expect(ScaleStub->GetGridFootprintSize() == FIntPoint(2, 2), TEXT("Bounds_SpawnedScaleRegisters2x2"));
				Expect(GPBuildGridContractDebug::AllCellsOccupied(
					Grid, ScaleStub->GetGridOriginCell(), FIntPoint(2, 2)),
					TEXT("Bounds_SpawnedScaleOccupiesResolvedCells"));
				ScaleStub->Destroy();
			}
		}

		{
			FActorSpawnParameters AuthoredBaseParams;
			AuthoredBaseParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AuthoredBaseParams.ObjectFlags |= RF_Transient;
			const FTransform AuthoredTM(FRotator::ZeroRotator, FVector(-75000.0f, 11000.0f, 100.0f));
			AGP_MainBase* AuthoredBase = World->SpawnActorDeferred<AGP_MainBase>(
				AGP_MainBase::StaticClass(),
				AuthoredTM,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Expect(IsValid(AuthoredBase) && AuthoredBase->GetPlacementFootprintBounds() != nullptr,
				TEXT("Preplaced_SpawnAuthoredMainBase")))
			{
				UBoxComponent* Box = AuthoredBase->GetPlacementFootprintBounds();
				Box->SetBoxExtent(FVector(700.0f, 600.0f, 20.0f));
				Box->SetRelativeScale3D(FVector::OneVector);
				Box->SetRelativeLocation(FVector::ZeroVector);
				AuthoredBase->FinishSpawning(AuthoredTM);

				const FGP_ResolvedBuildingFootprint AuthoredResolved = Grid->ResolveActorFootprint(AuthoredBase, nullptr);
				Expect(AuthoredResolved.SizeCells == FIntPoint(7, 6) && AuthoredBase->GetGridFootprintSize() == FIntPoint(7, 6),
					TEXT("Preplaced_MainBaseAuthoredRegisters7x6"));
				Expect(!AuthoredBase->GetBuildGridOccupancyDebugString().IsEmpty(), TEXT("Preplaced_OccupancyDebugString"));
				Expect(GPBuildGridContractDebug::AllCellsOccupied(
					Grid, AuthoredBase->GetGridOriginCell(), FIntPoint(7, 6)),
					TEXT("Preplaced_All42CellsOccupied"));

				const FIntPoint AuthoredOrigin = AuthoredBase->GetGridOriginCell();
				Expect(GPBuildGridContractDebug::AllCellsFree(
					Grid, FIntPoint(AuthoredOrigin.X + 7, AuthoredOrigin.Y), FIntPoint(1, 6)),
					TEXT("Preplaced_AdjacentOutsideRemainsFree"));

				EGP_GridRejectReason OverlapReason = EGP_GridRejectReason::Free;
				Expect(!Grid->CanPlaceFootprint(
					FIntPoint(AuthoredOrigin.X + 6, AuthoredOrigin.Y - 3),
					FIntPoint(4, 4),
					OverlapReason,
					nullptr)
					&& OverlapReason == EGP_GridRejectReason::CellOccupied,
					TEXT("Preplaced_HubOverlapEdgeRejected"));

				EGP_GridRejectReason AdjacentReason = EGP_GridRejectReason::CellOccupied;
				Expect(Grid->CanPlaceFootprint(
					FIntPoint(AuthoredOrigin.X + 7, AuthoredOrigin.Y),
					FIntPoint(4, 4),
					AdjacentReason,
					nullptr)
					&& AdjacentReason == EGP_GridRejectReason::Free,
					TEXT("Preplaced_HubBesideAuthoredValid"));
				AuthoredBase->Destroy();
			}

			const FTransform ScaleBaseTM(FRotator::ZeroRotator, FVector(-76000.0f, 11200.0f, 100.0f));
			AGP_MainBase* ScaleBase = World->SpawnActorDeferred<AGP_MainBase>(
				AGP_MainBase::StaticClass(),
				ScaleBaseTM,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Expect(IsValid(ScaleBase) && ScaleBase->GetPlacementFootprintBounds() != nullptr,
				TEXT("Preplaced_SpawnScaledMainBase")))
			{
				UBoxComponent* Box = ScaleBase->GetPlacementFootprintBounds();
				Box->SetBoxExtent(FVector(500.0f, 500.0f, 20.0f));
				Box->SetRelativeScale3D(FVector(1.4f, 1.2f, 1.0f));
				Box->SetRelativeLocation(FVector::ZeroVector);
				ScaleBase->FinishSpawning(ScaleBaseTM);
				Expect(ScaleBase->GetGridFootprintSize() == FIntPoint(7, 6),
					TEXT("Preplaced_MainBaseScaleRegisters7x6"));
				ScaleBase->Destroy();
			}

			const FTransform OffsetBaseTM(FRotator::ZeroRotator, FVector(-77000.0f, 11400.0f, 100.0f));
			AGP_MainBase* OffsetBase = World->SpawnActorDeferred<AGP_MainBase>(
				AGP_MainBase::StaticClass(),
				OffsetBaseTM,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Expect(IsValid(OffsetBase) && OffsetBase->GetPlacementFootprintBounds() != nullptr,
				TEXT("Preplaced_SpawnOffsetMainBase")))
			{
				UBoxComponent* Box = OffsetBase->GetPlacementFootprintBounds();
				Box->SetBoxExtent(FVector(500.0f, 500.0f, 20.0f));
				Box->SetRelativeScale3D(FVector::OneVector);
				Box->SetRelativeLocation(FVector(200.0f, -100.0f, 0.0f));
				OffsetBase->FinishSpawning(OffsetBaseTM);

				const FGP_ResolvedBuildingFootprint OffsetResolved = Grid->ResolveActorFootprint(OffsetBase, nullptr);
				Expect(FMath::IsNearlyEqual(OffsetResolved.LocalCenterOffsetCm.X, 200.0f)
					&& FMath::IsNearlyEqual(OffsetResolved.LocalCenterOffsetCm.Y, -100.0f),
					TEXT("Preplaced_MainBaseOffsetPreserved"));
				FIntPoint ExpectedOrigin = FIntPoint::ZeroValue;
				FVector ExpectedCenter = FVector::ZeroVector;
				Grid->ResolveSnappedPlacement(
					UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(
						OffsetBase->GetPlacementFootprintBounds()),
					FIntPoint(5, 5),
					ExpectedOrigin,
					ExpectedCenter);
				Expect(OffsetBase->GetGridOriginCell() == ExpectedOrigin
					&& OffsetBase->GetGridFootprintSize() == FIntPoint(5, 5),
					TEXT("Preplaced_MainBaseOffsetShiftsOrigin"));
				OffsetBase->Destroy();
			}

			AGP_MainBase* MutableMainCDO = GetMutableDefault<AGP_MainBase>();
			UBoxComponent* MutableMainBounds =
				MutableMainCDO != nullptr ? MutableMainCDO->GetPlacementFootprintBounds() : nullptr;
			const FTransform StaleTM(FRotator::ZeroRotator, FVector(-78000.0f, 11600.0f, 100.0f));
			AGP_MainBase* StaleBase = World->SpawnActorDeferred<AGP_MainBase>(
				AGP_MainBase::StaticClass(),
				StaleTM,
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Expect(IsValid(StaleBase) && StaleBase->GetPlacementFootprintBounds() != nullptr
				&& MutableMainBounds != nullptr,
				TEXT("Preplaced_SpawnStaleNativeSnapshot")))
			{
				GPBuildGridContractDebug::FScopedBoxAuthoring RestoreMainCDO(MutableMainBounds);
				MutableMainBounds->SetBoxExtent(FVector(700.0f, 600.0f, 20.0f));
				MutableMainBounds->SetRelativeScale3D(FVector::OneVector);
				MutableMainBounds->SetRelativeLocation(FVector::ZeroVector);

				UBoxComponent* InstanceBox = StaleBase->GetPlacementFootprintBounds();
				InstanceBox->SetBoxExtent(FVector(500.0f, 500.0f, 20.0f));
				InstanceBox->SetRelativeScale3D(FVector::OneVector);
				InstanceBox->SetRelativeLocation(FVector::ZeroVector);
				Expect(UGP_BuildGridSubsystem::LooksLikeNativeDefaultPlacementBounds(
					AGP_MainBase::StaticClass(), InstanceBox),
					TEXT("Preplaced_InstanceLooksNativeDefault"));
				const FGP_ResolvedBuildingFootprint BeforeSync =
					Grid->ResolveActorFootprint(StaleBase, nullptr);
				Expect(BeforeSync.bFromAuthoredBounds && BeforeSync.SizeCells == FIntPoint(5, 5),
					TEXT("Live_StaleInstanceResolvesFromLiveNotCDO"));
				StaleBase->ApplyClassDesignToLivePlacementFootprintBounds();
				Expect(InstanceBox->GetUnscaledBoxExtent().Equals(FVector(700.0f, 600.0f, 20.0f), 0.1f),
					TEXT("Live_SyncCopiesDesignExtent"));
				StaleBase->FinishSpawning(StaleTM);
				Expect(StaleBase->GetGridFootprintSize() == FIntPoint(7, 6),
					TEXT("Live_AfterSyncRegistersDesignSize"));
				StaleBase->Destroy();
			}
			else if (IsValid(StaleBase))
			{
				StaleBase->Destroy();
			}

			{
				AGP_MainBase* DesignCDO = GetMutableDefault<AGP_MainBase>();
				UBoxComponent* DesignBounds =
					DesignCDO != nullptr ? DesignCDO->GetPlacementFootprintBounds() : nullptr;
				const FTransform LiveTM(FRotator::ZeroRotator, FVector(-84000.0f, 13000.0f, 100.0f));
				AGP_MainBase* LiveBase = World->SpawnActorDeferred<AGP_MainBase>(
					AGP_MainBase::StaticClass(),
					LiveTM,
					nullptr,
					nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (Expect(IsValid(LiveBase) && LiveBase->GetPlacementFootprintBounds() != nullptr
					&& DesignBounds != nullptr,
					TEXT("Live_SpawnDesignSyncMainBase")))
				{
					GPBuildGridContractDebug::FScopedBoxAuthoring RestoreDesign(DesignBounds);
					DesignBounds->SetBoxExtent(FVector(1000.0f, 800.0f, 20.0f));
					DesignBounds->SetRelativeScale3D(FVector::OneVector);
					DesignBounds->SetRelativeLocation(FVector(600.0f, 0.0f, 0.0f));

					UBoxComponent* LiveBox = LiveBase->GetPlacementFootprintBounds();
					LiveBox->SetBoxExtent(FVector(200.0f, 200.0f, 20.0f));
					LiveBox->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.0f));
					LiveBox->SetRelativeLocation(FVector(133.5f, 0.0f, 0.0f));
					Expect(Grid->ResolveActorFootprint(LiveBase, nullptr).SizeCells == FIntPoint(1, 1),
						TEXT("Live_StaleSnapshotResolvesFromLive"));

					LiveBase->ApplyClassDesignToLivePlacementFootprintBounds();
					const FVector LiveHalf = UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(LiveBox);
					Expect(LiveHalf.Equals(FVector(1000.0f, 800.0f, 20.0f), 0.5f)
						&& LiveBox->GetRelativeLocation().Equals(FVector(600.0f, 0.0f, 0.0f), 0.1f),
						TEXT("Live_SyncMatchesDesignExtentAndOffset"));

					LiveBase->FinishSpawning(LiveTM);
					const FGP_ResolvedBuildingFootprint AfterSync = Grid->ResolveActorFootprint(LiveBase, nullptr);
					Expect(AfterSync.bFromAuthoredBounds && AfterSync.SizeCells == FIntPoint(10, 8),
						TEXT("Live_ResolveUsesLiveComponent"));
					Expect(LiveBase->GetGridFootprintSize() == FIntPoint(10, 8),
						TEXT("Live_Registers10x8AroundVisibleBox"));

					const FVector LiveCenter = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(LiveBox);
					FIntPoint SnapOrigin = FIntPoint::ZeroValue;
					FVector SnapCenter = FVector::ZeroVector;
					Grid->ResolveSnappedPlacement(LiveCenter, FIntPoint(10, 8), SnapOrigin, SnapCenter);
					Expect(LiveBase->GetGridOriginCell() == SnapOrigin, TEXT("Live_OriginFromLiveCenter"));
					Expect(FVector::Dist2D(LiveCenter, LiveTM.GetLocation() + FVector(600.0f, 0.0f, 0.0f)) <= 1.0f,
						TEXT("Live_WorldCenterMatchesDesignOffset"));
					Expect(GPBuildGridContractDebug::AllCellsOccupied(
						Grid, LiveBase->GetGridOriginCell(), FIntPoint(10, 8)),
						TEXT("Live_All10x8Occupied"));

					DesignBounds->SetBoxExtent(FVector(100.0f, 100.0f, 20.0f));
					Expect(Grid->ResolveActorFootprint(LiveBase, nullptr).SizeCells == FIntPoint(10, 8),
						TEXT("Live_HiddenCDOChangeDoesNotBypassLive"));
					LiveBase->Destroy();
				}
				else if (IsValid(LiveBase))
				{
					LiveBase->Destroy();
				}

				const FTransform YawLiveTM(FRotator(0.0f, 90.0f, 0.0f), FVector(-85000.0f, 13200.0f, 100.0f));
				AGP_MainBase* YawLive = World->SpawnActorDeferred<AGP_MainBase>(
					AGP_MainBase::StaticClass(),
					YawLiveTM,
					nullptr,
					nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (Expect(IsValid(YawLive) && YawLive->GetPlacementFootprintBounds() != nullptr,
					TEXT("Live_SpawnYaw90Offset")))
				{
					UBoxComponent* Box = YawLive->GetPlacementFootprintBounds();
					Box->SetBoxExtent(FVector(500.0f, 500.0f, 20.0f));
					Box->SetRelativeScale3D(FVector::OneVector);
					Box->SetRelativeLocation(FVector(600.0f, 0.0f, 0.0f));
					YawLive->FinishSpawning(YawLiveTM);
					const FVector LiveCenter = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(
						YawLive->GetPlacementFootprintBounds());
					Expect(FVector::Dist2D(LiveCenter, YawLiveTM.GetLocation() + FVector(0.0f, 600.0f, 0.0f)) <= 1.0f,
						TEXT("Live_Yaw90CenterFollowsVisibleBox"));
					TArray<FIntPoint> YawLiveCells;
					TArray<FIntPoint> YawLiveResolved;
					Expect(Grid->GetOccupantCells(YawLive->GetGridOccupantId(), YawLiveCells)
						&& Grid->ResolveOccupiedCellsFromBounds(YawLive->GetPlacementFootprintBounds(), YawLiveResolved)
						&& GPBuildGridContractDebug::CellSetsEqual(YawLiveCells, YawLiveResolved),
						TEXT("Live_Yaw90OccupiedCenterMatchesLiveBox"));
					YawLive->Destroy();
				}
			}

			const FVector2D Offset400X(400.0f, 0.0f);
			Expect(UGP_BuildGridSubsystem::TransformFootprintLocalOffsetToWorld(
				Offset400X, FRotator::ZeroRotator).Equals(FVector(400.0f, 0.0f, 0.0f), 0.5f),
				TEXT("Offset_Yaw0LocalXToWorldX"));
			Expect(UGP_BuildGridSubsystem::TransformFootprintLocalOffsetToWorld(
				Offset400X, FRotator(0.0f, 90.0f, 0.0f)).Equals(FVector(0.0f, 400.0f, 0.0f), 0.5f),
				TEXT("Offset_Yaw90LocalXToWorldY"));
			Expect(UGP_BuildGridSubsystem::TransformFootprintLocalOffsetToWorld(
				Offset400X, FRotator(0.0f, 180.0f, 0.0f)).Equals(FVector(-400.0f, 0.0f, 0.0f), 0.5f),
				TEXT("Offset_Yaw180LocalXToWorldNegX"));

			auto SpawnOffsetMainBase = [&](const FVector& Loc, const FRotator& Rot, const FVector& ActorScale,
				const TCHAR* SpawnLabel) -> AGP_MainBase*
			{
				const FTransform TM(Rot, Loc);
				AGP_MainBase* Base = World->SpawnActorDeferred<AGP_MainBase>(
					AGP_MainBase::StaticClass(),
					TM,
					nullptr,
					nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Expect(IsValid(Base) && Base->GetPlacementFootprintBounds() != nullptr, SpawnLabel))
				{
					return nullptr;
				}
				UBoxComponent* Box = Base->GetPlacementFootprintBounds();
				Box->SetBoxExtent(FVector(500.0f, 500.0f, 20.0f));
				Box->SetRelativeScale3D(FVector::OneVector);
				Box->SetRelativeLocation(FVector(400.0f, 0.0f, 0.0f));
				Base->SetActorScale3D(ActorScale);
				Base->FinishSpawning(TM);
				return Base;
			};

			auto ExpectShiftedOccupancy = [&](AGP_MainBase* Base, const FVector& ExpectedWorldOffset, const TCHAR* LabelPrefix)
			{
				if (!IsValid(Base))
				{
					return;
				}
				const FVector WorldOffset = UGP_BuildGridSubsystem::TransformFootprintLocalOffsetToWorld(
					FVector2D(400.0f, 0.0f), Base->GetActorRotation());
				Expect(WorldOffset.Equals(ExpectedWorldOffset, 0.5f),
					*FString::Printf(TEXT("%s_WorldOffset"), LabelPrefix));
				const FVector WorldCenter = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(
					Base->GetPlacementFootprintBounds());
				TArray<FIntPoint> Occupied;
				TArray<FIntPoint> Resolved;
				Expect(Grid->GetOccupantCells(Base->GetGridOccupantId(), Occupied) && Occupied.Num() > 0,
					*FString::Printf(TEXT("%s_HasOccupiedCells"), LabelPrefix));
				Expect(Grid->ResolveOccupiedCellsFromBounds(Base->GetPlacementFootprintBounds(), Resolved)
					&& GPBuildGridContractDebug::CellSetsEqual(Occupied, Resolved),
					*FString::Printf(TEXT("%s_OccupiedFollowsLiveBounds"), LabelPrefix));
				Expect(FVector::Dist2D(WorldCenter, Base->GetPlacementFootprintBounds()->GetComponentLocation()) <= 1.0f,
					*FString::Printf(TEXT("%s_CenterIsLiveComponent"), LabelPrefix));

				FIntPoint OccupiedOrigin = FIntPoint::ZeroValue;
				FIntPoint OccupiedSize = FIntPoint::ZeroValue;
				UGP_BuildGridSubsystem::MakeOccupiedCellsAabb(Occupied, OccupiedOrigin, OccupiedSize);
				const FIntPoint Probe = Occupied[Occupied.Num() / 2];
				EGP_GridRejectReason OverlapReason = EGP_GridRejectReason::Free;
				Expect(!Grid->CanPlaceFootprint(Probe, FIntPoint(4, 4), OverlapReason, nullptr)
					&& OverlapReason == EGP_GridRejectReason::CellOccupied,
					*FString::Printf(TEXT("%s_HubOverlapEdgeRejected"), LabelPrefix));

				EGP_GridRejectReason AdjacentReason = EGP_GridRejectReason::CellOccupied;
				Expect(Grid->CanPlaceFootprint(
					FIntPoint(OccupiedOrigin.X + OccupiedSize.X + 1, OccupiedOrigin.Y),
					FIntPoint(4, 4),
					AdjacentReason,
					nullptr)
					&& AdjacentReason == EGP_GridRejectReason::Free,
					*FString::Printf(TEXT("%s_HubAdjacentValid"), LabelPrefix));
			};

			if (AGP_MainBase* Yaw0 = SpawnOffsetMainBase(
				FVector(-80000.0f, 12000.0f, 100.0f),
				FRotator::ZeroRotator,
				FVector::OneVector,
				TEXT("Offset_SpawnYaw0")))
			{
				ExpectShiftedOccupancy(Yaw0, FVector(400.0f, 0.0f, 0.0f), TEXT("Offset_Yaw0"));
				Yaw0->Destroy();
			}
			if (AGP_MainBase* Yaw90 = SpawnOffsetMainBase(
				FVector(-81000.0f, 12200.0f, 100.0f),
				FRotator(0.0f, 90.0f, 0.0f),
				FVector::OneVector,
				TEXT("Offset_SpawnYaw90")))
			{
				ExpectShiftedOccupancy(Yaw90, FVector(0.0f, 400.0f, 0.0f), TEXT("Offset_Yaw90"));
				Yaw90->Destroy();
			}
			if (AGP_MainBase* Yaw180 = SpawnOffsetMainBase(
				FVector(-82000.0f, 12400.0f, 100.0f),
				FRotator(0.0f, 180.0f, 0.0f),
				FVector::OneVector,
				TEXT("Offset_SpawnYaw180")))
			{
				ExpectShiftedOccupancy(Yaw180, FVector(-400.0f, 0.0f, 0.0f), TEXT("Offset_Yaw180"));
				Yaw180->Destroy();
			}
			if (AGP_MainBase* Scaled = SpawnOffsetMainBase(
				FVector(-83000.0f, 12600.0f, 100.0f),
				FRotator::ZeroRotator,
				FVector(2.0f, 2.0f, 2.0f),
				TEXT("Offset_SpawnActorScale")))
			{
				Expect(Scaled->GetGridFootprintSize() == FIntPoint(5, 5), TEXT("Offset_ActorScaleDoesNotInflateSize"));
				const FVector ScaledWorldOffset = UGP_BuildGridSubsystem::TransformFootprintLocalOffsetToWorld(
					FVector2D(400.0f, 0.0f), Scaled->GetActorRotation());
				Expect(ScaledWorldOffset.Equals(FVector(400.0f, 0.0f, 0.0f), 0.5f),
					TEXT("Offset_ActorScaleDoesNotMultiplyOffset"));
				ExpectShiftedOccupancy(Scaled, FVector(400.0f, 0.0f, 0.0f), TEXT("Offset_ActorScale"));
				Scaled->Destroy();
			}

			auto SpawnParentScaleMainBase = [&](const FVector& Loc, const FRotator& Rot, const FVector& ActorScale,
				const FVector& Extent, const FVector& OwnScale, const FVector& RelLoc, const TCHAR* SpawnLabel) -> AGP_MainBase*
			{
				const FTransform TM(Rot, Loc);
				AGP_MainBase* Base = World->SpawnActorDeferred<AGP_MainBase>(
					AGP_MainBase::StaticClass(),
					TM,
					nullptr,
					nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Expect(IsValid(Base) && Base->GetPlacementFootprintBounds() != nullptr, SpawnLabel))
				{
					return nullptr;
				}
				UBoxComponent* Box = Base->GetPlacementFootprintBounds();
				Box->SetBoxExtent(Extent);
				Box->SetRelativeScale3D(OwnScale);
				Box->SetRelativeLocation(RelLoc);
				Base->SetActorScale3D(ActorScale);
				Base->FinishSpawning(TM);
				return Base;
			};

			if (AGP_MainBase* ParentX3 = SpawnParentScaleMainBase(
				FVector(-86000.0f, 13400.0f, 100.0f),
				FRotator::ZeroRotator,
				FVector(3.0f, 1.0f, 1.0f),
				FVector(500.0f, 500.0f, 20.0f),
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("ScaleIso_SpawnParentX3Own1")))
			{
				UBoxComponent* Box = ParentX3->GetPlacementFootprintBounds();
				const FVector Authored = UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(Box);
				Expect(Box->IsUsingAbsoluteScale() && !Box->IsUsingAbsoluteRotation(),
					TEXT("ScaleIso_A_AbsoluteScale"));
				Expect(GPBuildGridContractDebug::BoxFollowsActorYaw(Box, ParentX3), TEXT("ScaleIso_A_FollowsActorYaw"));
				Expect(Authored.Equals(FVector(500.0f, 500.0f, 20.0f), 0.5f), TEXT("ScaleIso_A_Authored500"));
				Expect(ParentX3->GetGridFootprintSize() == FIntPoint(5, 5)
					&& Grid->ResolveActorFootprint(ParentX3, nullptr).SizeCells == FIntPoint(5, 5),
					TEXT("ScaleIso_A_BuildGrid5x5Not15x5"));
				Expect(Box->GetComponentScale().Equals(FVector::OneVector, 0.05f),
					TEXT("ScaleIso_A_WorldScaleIgnoresParentX3"));
				Expect(GPBuildGridContractDebug::VisualHalfMatchesAuthored(Box),
					TEXT("ScaleIso_D_VisualMatchesAuthored_A"));
				const FVector LiveCenter = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(Box);
				Expect(FVector::Dist2D(LiveCenter, ParentX3->GetActorLocation()) <= 1.0f,
					TEXT("ScaleIso_E_ZeroOffsetCenterUnchanged"));
				Expect(GPBuildGridContractDebug::AllCellsOccupied(
					Grid, ParentX3->GetGridOriginCell(), FIntPoint(5, 5)),
					TEXT("ScaleIso_A_Occupies5x5"));
				ParentX3->Destroy();
			}

			if (AGP_MainBase* ParentX3Own2 = SpawnParentScaleMainBase(
				FVector(-87000.0f, 13600.0f, 100.0f),
				FRotator::ZeroRotator,
				FVector(3.0f, 1.0f, 1.0f),
				FVector(500.0f, 500.0f, 20.0f),
				FVector(2.0f, 1.0f, 1.0f),
				FVector::ZeroVector,
				TEXT("ScaleIso_SpawnParentX3Own2")))
			{
				UBoxComponent* Box = ParentX3Own2->GetPlacementFootprintBounds();
				const FVector Authored = UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(Box);
				Expect(Authored.Equals(FVector(1000.0f, 500.0f, 20.0f), 0.5f), TEXT("ScaleIso_B_Authored1000x500"));
				Expect(ParentX3Own2->GetGridFootprintSize() == FIntPoint(10, 5),
					TEXT("ScaleIso_B_BuildGrid10x5"));
				Expect(Box->GetComponentScale().Equals(FVector(2.0f, 1.0f, 1.0f), 0.05f),
					TEXT("ScaleIso_B_OwnScalePreserved"));
				Expect(GPBuildGridContractDebug::VisualHalfMatchesAuthored(Box),
					TEXT("ScaleIso_D_VisualMatchesAuthored_B"));
				ParentX3Own2->Destroy();
			}

			if (AGP_MainBase* ParentNonUniform = SpawnParentScaleMainBase(
				FVector(-88000.0f, 13800.0f, 100.0f),
				FRotator::ZeroRotator,
				FVector(0.5f, 2.0f, 1.0f),
				FVector(500.0f, 500.0f, 20.0f),
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("ScaleIso_SpawnParent05x2")))
			{
				Expect(ParentNonUniform->GetGridFootprintSize() == FIntPoint(5, 5),
					TEXT("ScaleIso_C_Still5x5"));
				Expect(GPBuildGridContractDebug::VisualHalfMatchesAuthored(
					ParentNonUniform->GetPlacementFootprintBounds()),
					TEXT("ScaleIso_D_VisualMatchesAuthored_C"));
				ParentNonUniform->Destroy();
			}

			if (AGP_MainBase* Scale1 = SpawnParentScaleMainBase(
				FVector(-90000.0f, 14200.0f, 100.0f),
				FRotator::ZeroRotator,
				FVector::OneVector,
				FVector(500.0f, 500.0f, 20.0f),
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("ScaleIso_SpawnActorScale1")))
			{
				Expect(Scale1->GetGridFootprintSize() == FIntPoint(5, 5), TEXT("ScaleIso_I_ActorScale1Still5x5"));
				Expect(GPBuildGridContractDebug::VisualHalfMatchesAuthored(Scale1->GetPlacementFootprintBounds()),
					TEXT("ScaleIso_I_VisualMatchesAuthored"));
				Scale1->Destroy();
			}

			if (AGP_MainBase* HugeWide = SpawnParentScaleMainBase(
				FVector(-89000.0f, 14000.0f, 100.0f),
				FRotator::ZeroRotator,
				FVector(3.0f, 1.0f, 1.0f),
				FVector(1000.0f, 800.0f, 20.0f),
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("ScaleIso_SpawnHuge10x8")))
			{
				Expect(HugeWide->GetGridFootprintSize() == FIntPoint(10, 8),
					TEXT("ScaleIso_G_Huge10x8Not30x8"));
				Expect(GPBuildGridContractDebug::AllCellsOccupied(
					Grid, HugeWide->GetGridOriginCell(), FIntPoint(10, 8)),
					TEXT("ScaleIso_G_All10x8Occupied"));
				HugeWide->Destroy();
			}

			if (AGP_MainBase* YawOffset = SpawnParentScaleMainBase(
				FVector(-91000.0f, 14400.0f, 100.0f),
				FRotator(0.0f, 90.0f, 0.0f),
				FVector(3.0f, 1.0f, 1.0f),
				FVector(500.0f, 500.0f, 20.0f),
				FVector::OneVector,
				FVector(600.0f, 0.0f, 0.0f),
				TEXT("ScaleIso_SpawnYaw90Offset")))
			{
				UBoxComponent* Box = YawOffset->GetPlacementFootprintBounds();
				const FVector LiveCenter = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(Box);
				Expect(FVector::Dist2D(LiveCenter, Box->GetComponentLocation()) <= 0.1f,
					TEXT("ScaleIso_E_LiveCenterIsComponentLocation"));
				Expect(GPBuildGridContractDebug::BoxFollowsActorYaw(Box, YawOffset),
					TEXT("ScaleIso_F_BoxFollowsActorYaw90"));
				TArray<FIntPoint> YawOffCells;
				TArray<FIntPoint> YawOffResolved;
				Expect(Grid->GetOccupantCells(YawOffset->GetGridOccupantId(), YawOffCells)
					&& Grid->ResolveOccupiedCellsFromBounds(Box, YawOffResolved)
					&& GPBuildGridContractDebug::CellSetsEqual(YawOffCells, YawOffResolved),
					TEXT("ScaleIso_F_YawOffsetOccupancyFollowsLiveCenter"));
				YawOffset->Destroy();
			}

			auto SpawnRect10x4 = [&](const FVector& Loc, const FRotator& Rot, const FVector& ActorScale,
				const FVector& OwnScale, const FVector& RelLoc, const TCHAR* SpawnLabel) -> AGP_MainBase*
			{
				return SpawnParentScaleMainBase(
					Loc,
					Rot,
					ActorScale,
					FVector(1000.0f, 400.0f, 20.0f),
					OwnScale,
					RelLoc,
					SpawnLabel);
			};

			auto ExpectRectFollowsActor = [&](AGP_MainBase* Base, const TCHAR* Prefix)
			{
				if (!IsValid(Base) || Base->GetPlacementFootprintBounds() == nullptr)
				{
					return;
				}
				UBoxComponent* Box = Base->GetPlacementFootprintBounds();
				const FVector Authored = UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(Box);
				Expect(Authored.Equals(FVector(1000.0f, 400.0f, 20.0f), 0.5f),
					*FString::Printf(TEXT("%s_Authored1000x400"), Prefix));
				Expect(Grid->ResolveActorFootprint(Base, nullptr).SizeCells == FIntPoint(10, 4),
					*FString::Printf(TEXT("%s_AuthoredSizeStill10x4"), Prefix));
				Expect(GPBuildGridContractDebug::BoxFollowsActorYaw(Box, Base),
					*FString::Printf(TEXT("%s_FollowsActorYaw"), Prefix));
				Expect(Box->IsUsingAbsoluteScale() && !Box->IsUsingAbsoluteRotation(),
					*FString::Printf(TEXT("%s_ScaleOnlyIsolation"), Prefix));
				TArray<FIntPoint> ResolvedCells;
				Expect(Grid->ResolveOccupiedCellsFromBounds(Box, ResolvedCells) && ResolvedCells.Num() > 0,
					*FString::Printf(TEXT("%s_ResolveOccupied"), Prefix));
				TArray<FIntPoint> RegisteredCells;
				Expect(Grid->GetOccupantCells(Base->GetGridOccupantId(), RegisteredCells)
					&& GPBuildGridContractDebug::CellSetsEqual(ResolvedCells, RegisteredCells),
					*FString::Printf(TEXT("%s_RegisteredMatchesResolved"), Prefix));
			};

			if (AGP_MainBase* Rect0 = SpawnRect10x4(
				FVector(-92000.0f, 14600.0f, 100.0f),
				FRotator::ZeroRotator,
				FVector::OneVector,
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("Axis_SpawnYaw0")))
			{
				ExpectRectFollowsActor(Rect0, TEXT("Orient_A_Yaw0"));
				TArray<FIntPoint> Yaw0Cells;
				TArray<FIntPoint> OldRect;
				Grid->ResolveOccupiedCellsFromBounds(Rect0->GetPlacementFootprintBounds(), Yaw0Cells);
				FIntPoint OldOrigin = FIntPoint::ZeroValue;
				FVector OldCenter = FVector::ZeroVector;
				Grid->ResolveSnappedPlacement(
					UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(Rect0->GetPlacementFootprintBounds()),
					FIntPoint(10, 4),
					OldOrigin,
					OldCenter);
				Grid->EnumerateFootprintCells(OldOrigin, FIntPoint(10, 4), OldRect);
				Expect(GPBuildGridContractDebug::CellSetsEqual(Yaw0Cells, OldRect),
					TEXT("Orient_A_Yaw0MatchesOld10x4"));
				Rect0->Destroy();
			}
			if (AGP_MainBase* Rect90 = SpawnRect10x4(
				FVector(-93000.0f, 14800.0f, 100.0f),
				FRotator(0.0f, 90.0f, 0.0f),
				FVector::OneVector,
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("Axis_SpawnYaw90")))
			{
				ExpectRectFollowsActor(Rect90, TEXT("Orient_B_Yaw90"));
				TArray<FIntPoint> Yaw90Cells;
				Grid->ResolveOccupiedCellsFromBounds(Rect90->GetPlacementFootprintBounds(), Yaw90Cells);
				FIntPoint AabbOrigin = FIntPoint::ZeroValue;
				FIntPoint AabbSize = FIntPoint::ZeroValue;
				UGP_BuildGridSubsystem::MakeOccupiedCellsAabb(Yaw90Cells, AabbOrigin, AabbSize);
				Expect(AabbSize.Y > AabbSize.X,
					TEXT("Orient_B_Yaw90Covers800x2000Not10x4"));
				TArray<FIntPoint> OldRect90;
				FIntPoint OldOrigin90 = FIntPoint::ZeroValue;
				FVector OldCenter90 = FVector::ZeroVector;
				Grid->ResolveSnappedPlacement(
					UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(Rect90->GetPlacementFootprintBounds()),
					FIntPoint(10, 4),
					OldOrigin90,
					OldCenter90);
				Grid->EnumerateFootprintCells(OldOrigin90, FIntPoint(10, 4), OldRect90);
				Expect(!GPBuildGridContractDebug::CellSetsEqual(Yaw90Cells, OldRect90),
					TEXT("Orient_B_Yaw90IsNotUnswappedRectangle"));
				Rect90->Destroy();
			}
			if (AGP_MainBase* Rect180 = SpawnRect10x4(
				FVector(-94000.0f, 15000.0f, 100.0f),
				FRotator(0.0f, 180.0f, 0.0f),
				FVector::OneVector,
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("Axis_SpawnYaw180")))
			{
				ExpectRectFollowsActor(Rect180, TEXT("Orient_C_Yaw180"));
				Rect180->Destroy();
			}
			if (AGP_MainBase* Rect270 = SpawnRect10x4(
				FVector(-95000.0f, 15200.0f, 100.0f),
				FRotator(0.0f, 270.0f, 0.0f),
				FVector::OneVector,
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("Axis_SpawnYaw270")))
			{
				ExpectRectFollowsActor(Rect270, TEXT("Orient_C_Yaw270"));
				TArray<FIntPoint> Yaw270Cells;
				Grid->ResolveOccupiedCellsFromBounds(Rect270->GetPlacementFootprintBounds(), Yaw270Cells);
				FIntPoint Aabb270Origin = FIntPoint::ZeroValue;
				FIntPoint Aabb270Size = FIntPoint::ZeroValue;
				UGP_BuildGridSubsystem::MakeOccupiedCellsAabb(Yaw270Cells, Aabb270Origin, Aabb270Size);
				Expect(Aabb270Size.Y > Aabb270Size.X, TEXT("Orient_C_Yaw270OrientedTall"));
				Rect270->Destroy();
			}

			if (AGP_MainBase* Rect917 = SpawnRect10x4(
				FVector(-101000.0f, 16400.0f, 100.0f),
				FRotator(0.0f, 91.7f, 0.0f),
				FVector::OneVector,
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("Orient_SpawnYaw917")))
			{
				UBoxComponent* Box = Rect917->GetPlacementFootprintBounds();
				Expect(GPBuildGridContractDebug::BoxFollowsActorYaw(Box, Rect917, 0.5f),
					TEXT("Orient_C_Yaw917FootprintYawMatchesActor"));
				TArray<FIntPoint> Cells917;
				Grid->ResolveOccupiedCellsFromBounds(Box, Cells917);
				FIntPoint Aabb917Origin = FIntPoint::ZeroValue;
				FIntPoint Aabb917Size = FIntPoint::ZeroValue;
				UGP_BuildGridSubsystem::MakeOccupiedCellsAabb(Cells917, Aabb917Origin, Aabb917Size);
				Expect(Cells917.Num() > 10 && Aabb917Size.Y > Aabb917Size.X,
					TEXT("Orient_C_Yaw917FollowsOrientedRect"));
				Rect917->Destroy();
			}
			if (AGP_MainBase* Rect45 = SpawnRect10x4(
				FVector(-102000.0f, 16600.0f, 100.0f),
				FRotator(0.0f, 45.0f, 0.0f),
				FVector::OneVector,
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("Orient_SpawnYaw45")))
			{
				TArray<FIntPoint> Cells45;
				Grid->ResolveOccupiedCellsFromBounds(Rect45->GetPlacementFootprintBounds(), Cells45);
				FIntPoint Aabb45Origin = FIntPoint::ZeroValue;
				FIntPoint Aabb45Size = FIntPoint::ZeroValue;
				UGP_BuildGridSubsystem::MakeOccupiedCellsAabb(Cells45, Aabb45Origin, Aabb45Size);
				Expect(Cells45.Num() > 16 && Aabb45Size.X > 4 && Aabb45Size.Y > 4,
					TEXT("Orient_D_Yaw45StairStepCoverage"));
				TArray<FIntPoint> Rect45Registered;
				Expect(Grid->GetOccupantCells(Rect45->GetGridOccupantId(), Rect45Registered)
					&& GPBuildGridContractDebug::CellSetsEqual(Cells45, Rect45Registered),
					TEXT("Orient_D_Yaw45RegisteredMatches"));
				Rect45->Destroy();
			}
			{
				const FTransform Rot0TM(FRotator::ZeroRotator, FVector(-103000.0f, 16800.0f, 100.0f));
				AGP_MainBase* RotSeam = World->SpawnActorDeferred<AGP_MainBase>(
					AGP_MainBase::StaticClass(),
					Rot0TM,
					nullptr,
					nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (Expect(IsValid(RotSeam) && RotSeam->GetPlacementFootprintBounds() != nullptr,
					TEXT("Orient_E_SpawnReconstruct")))
				{
					UBoxComponent* Box = RotSeam->GetPlacementFootprintBounds();
					Box->SetBoxExtent(FVector(1000.0f, 400.0f, 20.0f));
					Box->SetRelativeScale3D(FVector::OneVector);
					Box->SetRelativeLocation(FVector::ZeroVector);
					RotSeam->FinishSpawning(Rot0TM);
					TArray<FIntPoint> BeforeRot;
					Grid->ResolveOccupiedCellsFromBounds(Box, BeforeRot);
					RotSeam->SetActorRotation(FRotator(0.0f, 45.0f, 0.0f));
					Box->UpdateComponentToWorld();
					TArray<FIntPoint> AfterRot;
					Grid->ResolveOccupiedCellsFromBounds(Box, AfterRot);
					Expect(BeforeRot.Num() > 0 && AfterRot.Num() > 0
						&& !GPBuildGridContractDebug::CellSetsEqual(BeforeRot, AfterRot),
						TEXT("Orient_E_CellsChangeWithOrientation"));
					RotSeam->Destroy();
				}
				else if (IsValid(RotSeam))
				{
					RotSeam->Destroy();
				}
			}

			if (AGP_MainBase* Off0 = SpawnRect10x4(
				FVector(-96000.0f, 15400.0f, 100.0f),
				FRotator::ZeroRotator,
				FVector::OneVector,
				FVector::OneVector,
				FVector(400.0f, 0.0f, 0.0f),
				TEXT("Axis_SpawnOffsetYaw0")))
			{
				UBoxComponent* Box = Off0->GetPlacementFootprintBounds();
				const FVector LiveCenter = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(Box);
				const FVector Intended = UGP_BuildGridSubsystem::MakeWorldFootprintCenter(
					Off0->GetActorLocation(), Off0->GetActorRotation(), FVector2D(400.0f, 0.0f));
				Expect(FVector::Dist2D(LiveCenter, Off0->GetActorLocation() + FVector(400.0f, 0.0f, 0.0f)) <= 1.0f,
					TEXT("Axis_D_Yaw0OffsetPlus400WorldX"));
				Expect(FVector::Dist2D(LiveCenter, Intended) <= 1.0f
					&& FVector::Dist2D(LiveCenter, Box->GetComponentLocation()) <= 1.0f,
					TEXT("Axis_D_VisualCenterMatchesIntendedYaw0"));
				Expect(GPBuildGridContractDebug::BoxFollowsActorYaw(Box, Off0), TEXT("Orient_F_Yaw0Follows"));
				Expect(Off0->GetGridFootprintSize() == FIntPoint(10, 4), TEXT("Orient_F_Yaw0Still10x4"));
				Off0->Destroy();
			}
			if (AGP_MainBase* Off90 = SpawnRect10x4(
				FVector(-97000.0f, 15600.0f, 100.0f),
				FRotator(0.0f, 90.0f, 0.0f),
				FVector::OneVector,
				FVector::OneVector,
				FVector(400.0f, 0.0f, 0.0f),
				TEXT("Axis_SpawnOffsetYaw90")))
			{
				UBoxComponent* Box = Off90->GetPlacementFootprintBounds();
				const FVector LiveCenter = UGP_BuildGridSubsystem::GetLivePlacementFootprintCenterWorld(Box);
				const FVector Intended = UGP_BuildGridSubsystem::MakeWorldFootprintCenter(
					Off90->GetActorLocation(), Off90->GetActorRotation(), FVector2D(400.0f, 0.0f));
				Expect(FVector::Dist2D(LiveCenter, Off90->GetActorLocation() + FVector(0.0f, 400.0f, 0.0f)) <= 1.0f,
					TEXT("Axis_D_Yaw90OffsetPlus400WorldY"));
				Expect(FVector::Dist2D(LiveCenter, Intended) <= 1.0f
					&& FVector::Dist2D(LiveCenter, Box->GetComponentLocation()) <= 1.0f,
					TEXT("Axis_D_VisualCenterMatchesIntendedYaw90"));
				Expect(GPBuildGridContractDebug::BoxFollowsActorYaw(Box, Off90), TEXT("Orient_F_Yaw90Follows"));
				TArray<FIntPoint> Off90Cells;
				Expect(Grid->GetOccupantCells(Off90->GetGridOccupantId(), Off90Cells) && Off90Cells.Num() > 0,
					TEXT("Orient_F_Yaw90OccupiedFollowsCenter"));
				Off90->Destroy();
			}

			if (AGP_MainBase* RectScaled = SpawnRect10x4(
				FVector(-98000.0f, 15800.0f, 100.0f),
				FRotator(0.0f, 90.0f, 0.0f),
				FVector(2.352f, 2.352f, 2.352f),
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("Axis_SpawnParent2352")))
			{
				ExpectRectFollowsActor(RectScaled, TEXT("Orient_G_ParentScale2352"));
				Expect(RectScaled->GetPlacementFootprintBounds()->GetComponentScale().Equals(FVector::OneVector, 0.05f),
					TEXT("Orient_G_WorldScaleIgnoresParent"));
				RectScaled->Destroy();
			}
			if (AGP_MainBase* RectOwnScale = SpawnParentScaleMainBase(
				FVector(-99000.0f, 16000.0f, 100.0f),
				FRotator(0.0f, 90.0f, 0.0f),
				FVector(2.352f, 2.352f, 2.352f),
				FVector(500.0f, 400.0f, 20.0f),
				FVector(2.0f, 1.0f, 1.0f),
				FVector::ZeroVector,
				TEXT("Axis_SpawnOwnScale2x1")))
			{
				UBoxComponent* Box = RectOwnScale->GetPlacementFootprintBounds();
				const FVector Authored = UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(Box);
				Expect(Authored.Equals(FVector(1000.0f, 400.0f, 20.0f), 0.5f), TEXT("Axis_G_OwnScale1000x400"));
				TArray<FIntPoint> OwnScaleCells;
				Expect(Grid->ResolveOccupiedCellsFromBounds(Box, OwnScaleCells) && OwnScaleCells.Num() > 0,
					TEXT("Orient_H_OwnScaleOccupied"));
				Expect(GPBuildGridContractDebug::BoxFollowsActorYaw(Box, RectOwnScale), TEXT("Orient_H_FollowsYaw"));
				RectOwnScale->Destroy();
			}

			if (AGP_MainBase* EdgeBase = SpawnRect10x4(
				FVector(-100000.0f, 16200.0f, 100.0f),
				FRotator(0.0f, 90.0f, 0.0f),
				FVector::OneVector,
				FVector::OneVector,
				FVector::ZeroVector,
				TEXT("Axis_SpawnEdge10x4")))
			{
				ExpectRectFollowsActor(EdgeBase, TEXT("Orient_I_Rect"));
				TArray<FIntPoint> EdgeCells;
				Expect(Grid->GetOccupantCells(EdgeBase->GetGridOccupantId(), EdgeCells) && EdgeCells.Num() > 0,
					TEXT("Orient_I_HasOccupiedCells"));
				const FIntPoint ProbeOccupied = EdgeCells[EdgeCells.Num() / 2];
				EGP_GridRejectReason HitOccupied = EGP_GridRejectReason::Free;
				Expect(!Grid->CanPlaceFootprint(ProbeOccupied, FIntPoint(1, 1), HitOccupied)
					&& HitOccupied == EGP_GridRejectReason::CellOccupied,
					TEXT("Orient_I_IntersectingCellOccupied"));
				FIntPoint EdgeAabbOrigin = FIntPoint::ZeroValue;
				FIntPoint EdgeAabbSize = FIntPoint::ZeroValue;
				UGP_BuildGridSubsystem::MakeOccupiedCellsAabb(EdgeCells, EdgeAabbOrigin, EdgeAabbSize);
				const FIntPoint FarFree(EdgeAabbOrigin.X + EdgeAabbSize.X + 2, EdgeAabbOrigin.Y + EdgeAabbSize.Y + 2);
				EGP_GridRejectReason OutsideReason = EGP_GridRejectReason::CellOccupied;
				Expect(Grid->CanPlaceFootprint(FarFree, FIntPoint(1, 1), OutsideReason)
					&& OutsideReason == EGP_GridRejectReason::Free,
					TEXT("Orient_J_OutsideOrientedBoxFree"));
				EGP_GridRejectReason HubOverlap = EGP_GridRejectReason::Free;
				Expect(!Grid->CanPlaceFootprint(ProbeOccupied, FIntPoint(4, 4), HubOverlap)
					&& HubOverlap == EGP_GridRejectReason::CellOccupied,
					TEXT("Orient_K_HubOverlapRotatedOccupied"));
				const FGuid EdgeId = EdgeBase->GetGridOccupantId();
				EdgeBase->Destroy();
				Expect(!Grid->IsCellOccupied(ProbeOccupied), TEXT("Orient_L_DestroyReleasesOrientedCells"));
				TArray<FIntPoint> AfterDestroy;
				Expect(!Grid->GetOccupantCells(EdgeId, AfterDestroy), TEXT("Orient_L_OccupantCellsCleared"));
			}

			if (UClass* BpMainClass = LoadClass<AGP_MainBase>(
				nullptr,
				TEXT("/Game/GrimProtocol/Blueprint/Buildings/BP_GP_MainBase.BP_GP_MainBase_C")))
			{
				if (const AGP_MainBase* BpCDO = BpMainClass->GetDefaultObject<AGP_MainBase>())
				{
					const UBoxComponent* BpBounds = BpCDO->GetPlacementFootprintBounds();
					const UCapsuleComponent* BpCapsule = BpCDO->GetCapsuleComponent();
					const FVector BpActorScale = BpCDO->GetActorScale3D();
					const FVector BpCapsuleRel = BpCapsule != nullptr ? BpCapsule->GetRelativeScale3D() : FVector::ZeroVector;
					const FVector BpCapsuleWorld = BpCapsule != nullptr ? BpCapsule->GetComponentScale() : FVector::ZeroVector;
					const FVector BpBoundsRel = BpBounds != nullptr ? BpBounds->GetRelativeScale3D() : FVector::ZeroVector;
					const FVector BpBoundsComp = BpBounds != nullptr ? BpBounds->GetComponentScale() : FVector::ZeroVector;
					const FVector BpUnscaled = BpBounds != nullptr ? BpBounds->GetUnscaledBoxExtent() : FVector::ZeroVector;
					const FVector BpAuthored = UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(BpBounds);
					UE_LOG(
						LogGPBuildGridContract,
						Log,
						TEXT("ScaleTrace BP_GP_MainBase CDO actor=(%.3f,%.3f,%.3f) capsuleRel=(%.3f,%.3f,%.3f) capsuleWorld=(%.3f,%.3f,%.3f) boundsRel=(%.3f,%.3f,%.3f) boundsComp=(%.3f,%.3f,%.3f) unscaled=(%.1f,%.1f) authoredHalf=(%.1f,%.1f) absScale=%d"),
						BpActorScale.X,
						BpActorScale.Y,
						BpActorScale.Z,
						BpCapsuleRel.X,
						BpCapsuleRel.Y,
						BpCapsuleRel.Z,
						BpCapsuleWorld.X,
						BpCapsuleWorld.Y,
						BpCapsuleWorld.Z,
						BpBoundsRel.X,
						BpBoundsRel.Y,
						BpBoundsRel.Z,
						BpBoundsComp.X,
						BpBoundsComp.Y,
						BpBoundsComp.Z,
						BpUnscaled.X,
						BpUnscaled.Y,
						BpAuthored.X,
						BpAuthored.Y,
						BpBounds != nullptr && BpBounds->IsUsingAbsoluteScale() ? 1 : 0);
					if (BpBounds != nullptr)
					{
						Expect(BpBounds->IsUsingAbsoluteScale() && !BpBounds->IsUsingAbsoluteRotation(),
							TEXT("ScaleIso_BpCdoAbsoluteScale"));
					}
				}
			}

			for (TActorIterator<AGP_MainBase> It(World); It; ++It)
			{
				AGP_MainBase* LevelBase = *It;
				if (!IsValid(LevelBase) || !LevelBase->IsNetStartupActor())
				{
					continue;
				}
				const UBoxComponent* LevelBounds = LevelBase->GetPlacementFootprintBounds();
				const UCapsuleComponent* LevelCapsule = LevelBase->GetCapsuleComponent();
				UE_LOG(
					LogGPBuildGridContract,
					Log,
					TEXT("ScaleTrace level MainBase=%s actor=(%.3f,%.3f,%.3f) capsuleRel=(%.3f,%.3f,%.3f) capsuleWorld=(%.3f,%.3f,%.3f) boundsRel=(%.3f,%.3f,%.3f) boundsComp=(%.3f,%.3f,%.3f) unscaled=(%.1f,%.1f) authoredHalf=(%.1f,%.1f) registered=%dx%d absScale=%d"),
					*LevelBase->GetName(),
					LevelBase->GetActorScale3D().X,
					LevelBase->GetActorScale3D().Y,
					LevelBase->GetActorScale3D().Z,
					LevelCapsule != nullptr ? LevelCapsule->GetRelativeScale3D().X : 0.0f,
					LevelCapsule != nullptr ? LevelCapsule->GetRelativeScale3D().Y : 0.0f,
					LevelCapsule != nullptr ? LevelCapsule->GetRelativeScale3D().Z : 0.0f,
					LevelCapsule != nullptr ? LevelCapsule->GetComponentScale().X : 0.0f,
					LevelCapsule != nullptr ? LevelCapsule->GetComponentScale().Y : 0.0f,
					LevelCapsule != nullptr ? LevelCapsule->GetComponentScale().Z : 0.0f,
					LevelBounds != nullptr ? LevelBounds->GetRelativeScale3D().X : 0.0f,
					LevelBounds != nullptr ? LevelBounds->GetRelativeScale3D().Y : 0.0f,
					LevelBounds != nullptr ? LevelBounds->GetRelativeScale3D().Z : 0.0f,
					LevelBounds != nullptr ? LevelBounds->GetComponentScale().X : 0.0f,
					LevelBounds != nullptr ? LevelBounds->GetComponentScale().Y : 0.0f,
					LevelBounds != nullptr ? LevelBounds->GetComponentScale().Z : 0.0f,
					LevelBounds != nullptr ? LevelBounds->GetUnscaledBoxExtent().X : 0.0f,
					LevelBounds != nullptr ? LevelBounds->GetUnscaledBoxExtent().Y : 0.0f,
					UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(LevelBounds).X,
					UGP_BuildGridSubsystem::GetAuthoredPlacementHalfExtentCm(LevelBounds).Y,
					LevelBase->GetGridFootprintSize().X,
					LevelBase->GetGridFootprintSize().Y,
					LevelBounds != nullptr && LevelBounds->IsUsingAbsoluteScale() ? 1 : 0);
				if (LevelBounds != nullptr)
				{
					Expect(LevelBounds->IsUsingAbsoluteScale() && !LevelBounds->IsUsingAbsoluteRotation(),
						TEXT("ScaleIso_LevelMainBaseAbsoluteScale"));
					Expect(GPBuildGridContractDebug::BoxFollowsActorYaw(LevelBounds, LevelBase, 2.0f),
						TEXT("Orient_C_LevelMainBaseFollowsYaw"));
					Expect(GPBuildGridContractDebug::VisualHalfMatchesAuthored(LevelBounds, 1.0f),
						TEXT("ScaleIso_LevelMainBaseVisualMatchesAuthored"));
					TArray<FIntPoint> LevelResolved;
					TArray<FIntPoint> LevelRegistered;
					Expect(Grid->ResolveOccupiedCellsFromBounds(LevelBounds, LevelResolved)
						&& Grid->GetOccupantCells(LevelBase->GetGridOccupantId(), LevelRegistered)
						&& GPBuildGridContractDebug::CellSetsEqual(LevelResolved, LevelRegistered),
						TEXT("Orient_LevelOccupiedFollowsOrientedBox"));
				}
			}
		}

		FActorSpawnParameters AttachParams;
		AttachParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AttachParams.ObjectFlags |= RF_Transient;
		const FTransform AttachTM(FRotator::ZeroRotator, FVector(-73000.0f, 9500.0f, 100.0f));
		AGP_MainBase* DeferredBase = World->SpawnActorDeferred<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			AttachTM,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Expect(IsValid(DeferredBase)
			&& DeferredBase->GetPlacementFootprintBounds() != nullptr
			&& DeferredBase->GetNavigationObstacle() != nullptr,
			TEXT("Attach_SpawnDeferredMainBase")))
		{
			UBoxComponent* Bounds = DeferredBase->GetPlacementFootprintBounds();
			UBoxComponent* Nav = DeferredBase->GetNavigationObstacle();
			USceneComponent* CtorRoot = DeferredBase->GetRootComponent();
			Expect(Nav != nullptr && CtorRoot != nullptr && Nav->GetAttachParent() == CtorRoot,
				TEXT("Attach_PreRegistrationPath"));
			Bounds->SetRelativeLocation(FVector(80.0f, -55.0f, 12.0f));
			Bounds->SetBoxExtent(FVector(200.0f, 200.0f, 20.0f));
			DeferredBase->FinishSpawning(AttachTM);

			USceneComponent* Root = DeferredBase->GetRootComponent();
			Expect(Bounds->IsRegistered(), TEXT("Attach_BoundsRegisteredAfterSpawn"));
			Expect(Root != nullptr && Bounds->GetAttachParent() == Root, TEXT("Attach_BoundsParentIsRoot"));
			Expect(DeferredBase->GetNavigationObstacle()->IsRegistered()
				&& DeferredBase->GetNavigationObstacle()->GetAttachParent() == Root,
				TEXT("Attach_NavObstacleParentIsRoot"));
			Expect(Bounds->GetRelativeLocation().Equals(FVector(80.0f, -55.0f, 12.0f), 0.1f),
				TEXT("Attach_RelativeLocationSurvives"));
			Expect(Bounds->GetUnscaledBoxExtent().Equals(FVector(200.0f, 200.0f, 20.0f), 0.1f),
				TEXT("Attach_BoxExtentSurvives"));

			DeferredBase->AttachDeferredSceneComponentsToRoot();
			DeferredBase->AttachDeferredSceneComponentsToRoot();
			Expect(Bounds->GetAttachParent() == Root
				&& Bounds->GetRelativeLocation().Equals(FVector(80.0f, -55.0f, 12.0f), 0.1f)
				&& Bounds->GetUnscaledBoxExtent().Equals(FVector(200.0f, 200.0f, 20.0f), 0.1f),
				TEXT("Attach_HelperIdempotent"));
			DeferredBase->Destroy();
		}

		AGP_MainBase* RegisteredBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			AttachTM.GetLocation(),
			AttachTM.Rotator(),
			AttachParams);
		if (Expect(IsValid(RegisteredBase)
			&& RegisteredBase->GetPlacementFootprintBounds() != nullptr,
			TEXT("Attach_SpawnRegisteredMainBase")))
		{
			UBoxComponent* Bounds = RegisteredBase->GetPlacementFootprintBounds();
			USceneComponent* Root = RegisteredBase->GetRootComponent();
			Expect(Bounds->IsRegistered() && Root != nullptr && Bounds->GetAttachParent() == Root,
				TEXT("Attach_RegisteredPathBoundsOnRoot"));
			Expect(RegisteredBase->GetNavigationObstacle() != nullptr
				&& RegisteredBase->GetNavigationObstacle()->GetAttachParent() == Root,
				TEXT("Attach_RegisteredPathNavOnRoot"));
			Bounds->SetRelativeLocation(FVector(40.0f, 25.0f, 8.0f));
			Bounds->SetBoxExtent(FVector(150.0f, 175.0f, 20.0f));
			RegisteredBase->AttachDeferredSceneComponentsToRoot();
			Expect(Bounds->GetRelativeLocation().Equals(FVector(40.0f, 25.0f, 8.0f), 0.1f)
				&& Bounds->GetUnscaledBoxExtent().Equals(FVector(150.0f, 175.0f, 20.0f), 0.1f)
				&& Bounds->GetAttachParent() == Root,
				TEXT("Attach_RegisteredKeepRelativeTransform"));
			RegisteredBase->Destroy();
		}

		Expect(FCString::Strcmp(
			GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(true, EGP_BuildingDropRejectReason::None),
			TEXT("VALID")) == 0, TEXT("Preview_LabelValid"));
		Expect(FCString::Strcmp(
			GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(false, EGP_BuildingDropRejectReason::GridOccupied),
			TEXT("BLOCKED: OCCUPIED")) == 0, TEXT("Preview_LabelOccupied"));
		Expect(FCString::Strcmp(
			GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(false, EGP_BuildingDropRejectReason::OutOfDeployRadius),
			TEXT("BLOCKED: OUT OF RANGE")) == 0, TEXT("Preview_LabelOutOfRange"));
		Expect(FCString::Strcmp(
			GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(false, EGP_BuildingDropRejectReason::NotNavigable),
			TEXT("BLOCKED: NOT NAVIGABLE")) == 0, TEXT("Preview_LabelNotNavigable"));
		Expect(FCString::Strcmp(
			GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(false, EGP_BuildingDropRejectReason::PlacementOverlap),
			TEXT("BLOCKED: WORLD")) == 0, TEXT("Preview_LabelWorld"));
		Expect(FCString::Strcmp(
			GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(false, EGP_BuildingDropRejectReason::InvalidType),
			TEXT("BLOCKED")) == 0, TEXT("Preview_LabelFallback"));

		FActorSpawnParameters GhostParams;
		GhostParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GhostParams.ObjectFlags |= RF_Transient;
		AGP_BuildingPlacementGhost* Ghost = World->SpawnActor<AGP_BuildingPlacementGhost>(
			AGP_BuildingPlacementGhost::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			GhostParams);
		if (Expect(IsValid(Ghost), TEXT("Preview_SpawnGhost")))
		{
			Ghost->UpdateGridPreview(
				Grid,
				FIntPoint(0, 0),
				FIntPoint(4, 4),
				0.0f,
				true,
				EGP_BuildingDropRejectReason::None);
			Expect(FMath::IsNearlyEqual(Ghost->GetPreviewOuterExtentXY().X, 800.0f)
				&& FMath::IsNearlyEqual(Ghost->GetPreviewOuterExtentXY().Y, 800.0f),
				TEXT("Preview_GhostOuter800"));
			Expect(Ghost->GetPreviewCellCount() == 16, TEXT("Preview_GhostCellCount16"));
			Expect(Ghost->GetPreviewGridLineCount() == 0, TEXT("Preview_NoGridLines"));
			Expect(Ghost->GetPreviewLineWorldCount() == 0, TEXT("Preview_NoLineWorlds"));
			Expect(Ghost->GetPreviewStatusLabel() == TEXT("VALID"), TEXT("Preview_GhostValidLabel"));
			Expect(Ghost->HasActiveGridPreview(), TEXT("Preview_GhostActive"));
			Expect(Ghost->IsGhostFillHidden(), TEXT("Preview_GhostFillHidden"));
			Ghost->SetBuildingGhostClass(AGP_LogisticsHub::StaticClass());
			Ghost->UpdateGridPreview(
				Grid,
				FIntPoint(0, 0),
				FIntPoint(4, 4),
				0.0f,
				true,
				EGP_BuildingDropRejectReason::None);
			Expect(Ghost->IsBuildingGhostVisible(), TEXT("Preview_BuildingGhostShownWhenValid"));
			{
				AGP_LogisticsHub* HubCDO = GetMutableDefault<AGP_LogisticsHub>();
				UBoxComponent* HubBox = HubCDO != nullptr ? HubCDO->GetPlacementFootprintBounds() : nullptr;
				GPBuildGridContractDebug::FScopedBoxAuthoring RestoreHub(HubBox);
				if (HubBox != nullptr)
				{
					HubBox->SetRelativeScale3D(FVector(2.0f, 2.0f, 1.0f));
				}
				Ghost->SetBuildingGhostClass(nullptr);
				Ghost->SetBuildingGhostClass(AGP_LogisticsHub::StaticClass());
				TArray<UStaticMeshComponent*> GhostMeshes;
				Ghost->GetComponents<UStaticMeshComponent>(GhostMeshes);
				bool bFootprintScaleLeakedOntoGhost = false;
				for (const UStaticMeshComponent* Mesh : GhostMeshes)
				{
					if (Mesh != nullptr && Mesh->GetRelativeScale3D().Equals(FVector(2.0f, 2.0f, 1.0f), 0.01f))
					{
						bFootprintScaleLeakedOntoGhost = true;
						break;
					}
				}
				Expect(!bFootprintScaleLeakedOntoGhost, TEXT("Preview_GhostMeshScaleIndependentOfFootprint"));
			}
			Ghost->UpdateGridPreview(
				Grid,
				FIntPoint(0, 0),
				FIntPoint(2, 2),
				0.0f,
				true,
				EGP_BuildingDropRejectReason::None);
			Expect(FMath::IsNearlyEqual(Ghost->GetPreviewOuterExtentXY().X, 400.0f)
				&& FMath::IsNearlyEqual(Ghost->GetPreviewOuterExtentXY().Y, 400.0f)
				&& Ghost->GetPreviewCellCount() == 4,
				TEXT("Preview_Scaled2x2Outer400"));
			Ghost->ClearGridPreview();
			Expect(!Ghost->HasActiveGridPreview() && Ghost->GetPreviewGridLineCount() == 0
				&& Ghost->GetPreviewStatusLabel().IsEmpty()
				&& Ghost->GetPreviewLineWorldCount() == 0, TEXT("Preview_CancelClearsState"));

			const FVector Requested(2000.0f, 1200.0f, 50.0f);
			FIntPoint OffsetOrigin = FIntPoint::ZeroValue;
			FVector OffsetSnapped = FVector::ZeroVector;
			Expect(Grid->ResolveSnappedPlacement(Requested, FIntPoint(4, 4), OffsetOrigin, OffsetSnapped),
				TEXT("Preview_OffsetSnap"));
			Ghost->SetActorLocation(OffsetSnapped);
			Ghost->UpdateGridPreview(
				Grid,
				OffsetOrigin,
				FIntPoint(4, 4),
				OffsetSnapped.Z,
				true,
				EGP_BuildingDropRejectReason::None);

			FVector ExpectedMin = FVector::ZeroVector;
			FVector ExpectedMax = FVector::ZeroVector;
			Grid->GetFootprintWorldAABB(OffsetOrigin, FIntPoint(4, 4), OffsetSnapped.Z, ExpectedMin, ExpectedMax);
			Expect(Ghost->GetPreviewLineWorldCount() == 0 && Ghost->GetPreviewGridLineCount() == 0,
				TEXT("Preview_OffsetHasNoBorders"));
			Expect(Ghost->GetPreviewFillWorldMin().Equals(ExpectedMin, 1.0f)
				&& Ghost->GetPreviewFillWorldMax().Equals(ExpectedMax, 1.0f),
				TEXT("Preview_OffsetFillMatchesAABB"));

			const FVector FillCenter = 0.5f * (Ghost->GetPreviewFillWorldMin() + Ghost->GetPreviewFillWorldMax());
			Expect(FVector::Dist2D(FillCenter, OffsetSnapped) < 1.0f, TEXT("Preview_OffsetFillCenteredOnSnap"));
			Expect(FVector2D(FillCenter.X, FillCenter.Y).Size() > 500.0f, TEXT("Preview_OffsetFillNotAtOrigin"));
			Expect(FMath::Abs(OffsetSnapped.X - 2000.0f) < 400.0f
				&& FMath::Abs(OffsetSnapped.Y - 1200.0f) < 400.0f,
				TEXT("Preview_OffsetNearRequested"));

			Ghost->ClearGridPreview();
			Expect(!Ghost->HasActiveGridPreview() && Ghost->GetPreviewLineWorldCount() == 0,
				TEXT("Preview_OffsetCancelClears"));
			Ghost->Destroy();
		}

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 1: // F–L occupancy
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		const FTransform FarTM(FRotator::ZeroRotator, FVector(-70000.0f, 9000.0f, 100.0f));
		AGP_BuildGridContractStub* Stub = World->SpawnActorDeferred<AGP_BuildGridContractStub>(
			AGP_BuildGridContractStub::StaticClass(),
			FarTM,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Expect(IsValid(Stub), TEXT("F_SpawnStub")))
		{
			Finish();
			return;
		}
		Stub->ConfigureGridPlacement(FIntPoint(80, 80), FIntPoint(1, 1));
		Stub->FinishSpawning(FarTM);

		const FIntPoint Origin(90, 90);
		const FIntPoint Size4(4, 4);
		EGP_GridRejectReason Reason = EGP_GridRejectReason::CellOccupied;
		Expect(Grid->CanPlaceFootprint(Origin, Size4, Reason) && Reason == EGP_GridRejectReason::Free,
			TEXT("F_Empty4x4Accepted"));

		Expect(Grid->RegisterFootprint(Stub, Origin, Size4, Stub->GetGridOccupantId()), TEXT("G_Register"));
		Expect(GPBuildGridContractDebug::AllCellsOccupied(Grid, Origin, Size4), TEXT("G_All16Occupied"));
		Expect(Grid->GetActorAtCell(Origin) == Stub, TEXT("G_ActorAtOrigin"));

		EGP_GridRejectReason OverlapReason = EGP_GridRejectReason::Free;
		Expect(!Grid->CanPlaceFootprint(FIntPoint(91, 91), Size4, OverlapReason), TEXT("H_OverlapRejected"));
		Expect(OverlapReason == EGP_GridRejectReason::CellOccupied, TEXT("H_OverlapReason"));

		EGP_GridRejectReason AdjacentReason = EGP_GridRejectReason::CellOccupied;
		Expect(Grid->CanPlaceFootprint(FIntPoint(94, 90), Size4, AdjacentReason)
			&& AdjacentReason == EGP_GridRejectReason::Free, TEXT("I_AdjacentAccepted"));

		Expect(Grid->RegisterFootprint(Stub, Origin, Size4, Stub->GetGridOccupantId()), TEXT("K_DuplicateRegisterSafe"));
		Grid->UnregisterFootprint(Stub);
		Grid->UnregisterFootprint(Stub);
		Expect(GPBuildGridContractDebug::AllCellsFree(Grid, Origin, Size4), TEXT("J_UnregisterFrees"));

		Expect(Grid->RegisterFootprint(Stub, Origin, Size4, Stub->GetGridOccupantId()), TEXT("L_Reregister"));
		const FIntPoint HeldOrigin = Origin;
		Stub->Destroy();
		Grid->SweepStaleOccupants();
		Expect(GPBuildGridContractDebug::AllCellsFree(Grid, HeldOrigin, Size4), TEXT("L_DestroyedReleasesCells"));

		EGP_GridRejectReason InvalidReason = EGP_GridRejectReason::Free;
		Expect(!Grid->CanPlaceFootprint(Origin, FIntPoint(0, 4), InvalidReason), TEXT("R_InvalidFootprintQuery"));
		Expect(InvalidReason == EGP_GridRejectReason::InvalidFootprint, TEXT("R_InvalidFootprintReason"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 2: // world setup + M overlapping reservations
	{
		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			SavedBuildingCleanup = Settings->BuildingDropCleanupDelaySeconds;
			SavedBuildingAltitude = Settings->BuildingDropSpawnAltitudeCm;
			Settings->BuildingDropCleanupDelaySeconds = 0.05f;
			Settings->BuildingDropSpawnAltitudeCm = 400.0f;
			UGP_BuildingDropCatalog::Get().OverrideDeliveryTiming(0.45f, 0.35f);
			bSettingsMutated = true;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-46000.0f, 8000.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		MainBaseWeak = Base;
		if (!Expect(IsValid(Base), TEXT("Q_SpawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(GPBuildGridContractDebug::ContractTeam);
		Expect(Base->GetGridFootprintSize() == FIntPoint(5, 5), TEXT("Q_MainBaseFallback5x5"));
		Expect(GPBuildGridContractDebug::AllCellsOccupied(Grid, Base->GetGridOriginCell(), FIntPoint(5, 5)),
			TEXT("Q_MainBaseOccupiesGrid"));
		Expect(Base->GetPlacementFootprintBounds() != nullptr
			&& Base->GetPlacementFootprintBounds()->IsRegistered()
			&& Base->GetPlacementFootprintBounds()->GetAttachParent() == Base->GetRootComponent(),
			TEXT("Attach_LiveMainBaseBoundsOnRoot"));
		Expect(Base->GetNavigationObstacle() != nullptr
			&& Base->GetNavigationObstacle()->GetAttachParent() == Base->GetRootComponent(),
			TEXT("Attach_LiveMainBaseNavOnRoot"));

		ValidDeployLocation = Base->GetActorLocation() + FVector(1400.0f, 0.0f, 0.0f);
		const FVector Unsnapped = ValidDeployLocation + FVector(47.0f, -83.0f, 0.0f);
		Grid->ResolveSnappedPlacement(Unsnapped, FIntPoint(4, 4), FirstHubOrigin, SnappedExpected);
		AdjacentDeployLocation = Grid->GetFootprintCenterWorld(
			FIntPoint(FirstHubOrigin.X + 4, FirstHubOrigin.Y), FIntPoint(4, 4), ValidDeployLocation.Z);

		AGP_PlayerState* OwnerPS = GPBuildGridContractDebug::SpawnTeamPlayerState(
			World, GS, GPBuildGridContractDebug::ContractTeam);
		OwnerPSWeak = OwnerPS;
		if (!Expect(IsValid(OwnerPS), TEXT("SpawnOwnerPS")))
		{
			Finish();
			return;
		}

		GPBuildGridContractDebug::GrantOrbital(OwnerPS, 1000.0f);
		UGP_OrbitalDropDefinition* HubDrop = UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop();
		if (!Expect(IsValid(HubDrop), TEXT("HubDropPresent")))
		{
			Finish();
			return;
		}

		GPBuildingDropAuthority::FPurchaseResult Buy1 =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, HubDrop);
		GPBuildingDropAuthority::FPurchaseResult Buy2 =
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, HubDrop);
		Expect(Buy1.bAccepted && Buy2.bAccepted, TEXT("M_PurchaseTwo"));
		Expect(OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(HubDrop) == 2, TEXT("M_Ready2"));

		GPBuildingDropAuthority::FPlacementPreview ClientPreview;
		GPBuildingDropAuthority::EvaluateLocalPlacementPreview(
			World,
			OwnerPS,
			HubDrop,
			FTransform(FRotator::ZeroRotator, Unsnapped),
			ClientPreview);

		GPBuildingDropAuthority::FDeployResult First =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World, OwnerPS, HubDrop, FTransform(FRotator::ZeroRotator, Unsnapped));
		Expect(First.bAccepted, TEXT("M_FirstReservation"));
		Expect(First.OriginCell == FirstHubOrigin, TEXT("P_ServerSnappedOrigin"));
		Expect(FVector::Dist2D(First.SnappedLocation, SnappedExpected) <= 1.0f, TEXT("P_ServerSnappedLocation"));
		Expect(ClientPreview.OriginCell == First.OriginCell
			&& ClientPreview.FootprintSize == First.FootprintSize, TEXT("Preview_ClientServerFootprintMatch"));
		Expect(FVector::Dist2D(ClientPreview.SnappedGround, First.SnappedLocation) <= 1.0f
			&& FVector::Dist2D(ClientPreview.SnappedActorLocation, First.SnappedActorLocation) <= 1.0f,
			TEXT("Preview_ClientServerActorTransformMatch"));
		LastPodWeak = First.SpawnedPod;
		Expect(Grid->IsReservationActive(First.ReservationId), TEXT("M_ReservationActive"));

		GPBuildingDropAuthority::FDeployResult Overlap =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World, OwnerPS, HubDrop, FTransform(FRotator::ZeroRotator, ValidDeployLocation));
		Expect(!Overlap.bAccepted, TEXT("M_SecondOverlapRejected"));
		Expect(Overlap.RejectReason == EGP_BuildingDropRejectReason::GridOccupied, TEXT("M_OverlapReason"));
		Expect(OwnerPS->GetOrbitalBuildingInventoryComponent()->GetReadyCount(HubDrop) == 1, TEXT("M_ReadyPreserved"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 3: // Q MainBase block + R invalid footprint + S radius + T invalid does not consume
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		AGP_MainBase* Base = MainBaseWeak.Get();
		UGP_OrbitalDropDefinition* HubDrop = UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop();
		if (!Expect(IsValid(OwnerPS) && IsValid(Base) && IsValid(HubDrop), TEXT("Q_ActorsAlive")))
		{
			Finish();
			return;
		}

		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		const int32 ReadyBefore = Inventory->GetReadyCount(HubDrop);
		const float OrbitalBefore = OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite();
		GPBuildingDropAuthority::FDeployResult OnBase =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World, OwnerPS, HubDrop, FTransform(FRotator::ZeroRotator, Base->GetActorLocation()));
		Expect(!OnBase.bAccepted, TEXT("Q_MainBaseBlocks"));
		Expect(OnBase.RejectReason == EGP_BuildingDropRejectReason::GridOccupied, TEXT("Q_MainBaseReason"));
		Expect(Inventory->GetReadyCount(HubDrop) == ReadyBefore, TEXT("T_ReadyUnchangedOnGridReject"));
		Expect(FMath::IsNearlyEqual(OwnerPS->GetPlayerAttributeSet()->GetOrbitalFerronite(), OrbitalBefore, 0.05f),
			TEXT("T_NoSecondOrbitalSpend"));

		GPBuildingDropAuthority::FDeployResult Far =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World,
				OwnerPS,
				HubDrop,
				FTransform(FRotator::ZeroRotator, Base->GetActorLocation() + FVector(99999.0f, 0.0f, 0.0f)));
		Expect(!Far.bAccepted && Far.RejectReason == EGP_BuildingDropRejectReason::OutOfDeployRadius,
			TEXT("S_DeployRadiusEnforced"));
		Expect(Inventory->GetReadyCount(HubDrop) == ReadyBefore, TEXT("S_ReadyPreservedOnRadius"));

		GPBuildingDropAuthority::FPlacementPreview OccupiedPreview;
		GPBuildingDropAuthority::EvaluateLocalPlacementPreview(
			World,
			OwnerPS,
			HubDrop,
			FTransform(FRotator::ZeroRotator, Base->GetActorLocation()),
			OccupiedPreview);
		Expect(!OccupiedPreview.bValid
			&& OccupiedPreview.RejectReason == EGP_BuildingDropRejectReason::GridOccupied,
			TEXT("Preview_LocalOccupiedReason"));
		Expect(FCString::Strcmp(
			GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(OccupiedPreview.bValid, OccupiedPreview.RejectReason),
			TEXT("BLOCKED: OCCUPIED")) == 0, TEXT("Preview_LocalOccupiedLabel"));

		const FIntPoint MixedOrigin(
			Base->GetGridOriginCell().X + 3,
			Base->GetGridOriginCell().Y);
		const FVector MixedCenter = Grid->GetFootprintCenterWorld(
			MixedOrigin, FIntPoint(4, 4), Base->GetActorLocation().Z);
		GPBuildingDropAuthority::FPlacementPreview MixedPreview;
		GPBuildingDropAuthority::EvaluateLocalPlacementPreview(
			World,
			OwnerPS,
			HubDrop,
			FTransform(FRotator::ZeroRotator, MixedCenter),
			MixedPreview);
		int32 MixedOccupied = 0;
		int32 MixedFree = 0;
		for (const EGP_PlacementPreviewCellState State : MixedPreview.CellStates)
		{
			if (State == EGP_PlacementPreviewCellState::Occupied)
			{
				++MixedOccupied;
			}
			else if (State == EGP_PlacementPreviewCellState::Free)
			{
				++MixedFree;
			}
		}
		Expect(MixedPreview.CellStates.Num() == 16, TEXT("Preview_MixedCellCount16"));
		Expect(MixedOccupied > 0 && MixedFree > 0, TEXT("Preview_MixedValidityNotMonolithic"));
		Expect(!MixedPreview.bValid
			&& MixedPreview.RejectReason == EGP_BuildingDropRejectReason::GridOccupied,
			TEXT("Preview_MixedOccupiedReason"));

		FActorSpawnParameters MixedGhostParams;
		MixedGhostParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MixedGhostParams.ObjectFlags |= RF_Transient;
		AGP_BuildingPlacementGhost* MixedGhost = World->SpawnActor<AGP_BuildingPlacementGhost>(
			AGP_BuildingPlacementGhost::StaticClass(),
			MixedCenter,
			FRotator::ZeroRotator,
			MixedGhostParams);
		if (Expect(IsValid(MixedGhost), TEXT("Preview_MixedGhostSpawn")))
		{
			MixedGhost->SetBuildingGhostClass(AGP_LogisticsHub::StaticClass());
			MixedGhost->UpdateGridPreview(
				Grid,
				MixedPreview.OriginCell,
				MixedPreview.FootprintSize,
				MixedPreview.SnappedGround.Z,
				MixedPreview.bValid,
				MixedPreview.RejectReason,
				&MixedPreview.CellStates);
			Expect(MixedGhost->GetPreviewInvalidCellCount() == MixedOccupied, TEXT("Preview_MixedInvalidCellCount"));
			Expect(MixedGhost->GetPreviewGridLineCount() == 0 && MixedGhost->GetPreviewLineWorldCount() == 0,
				TEXT("Preview_MixedNoBorders"));
			Expect(!MixedGhost->IsBuildingGhostVisible(), TEXT("Preview_BuildingGhostHiddenWhenInvalid"));
			Expect(MixedGhost->GetPreviewStatusLabel() == TEXT("BLOCKED: OCCUPIED"), TEXT("Preview_MixedStatusText"));
			MixedGhost->ClearGridPreview();
			Expect(!MixedGhost->HasActiveGridPreview() && MixedGhost->GetPreviewInvalidCellCount() == 0,
				TEXT("Preview_MixedCancelClears"));
			MixedGhost->Destroy();
		}

		const FVector GroundProbe( -72000.0f, 11000.0f, 450.0f);
		UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		FActorSpawnParameters GroundParams;
		GroundParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GroundParams.ObjectFlags |= RF_Transient;
		auto ConfigureWorldStaticCube = [CubeMesh](AStaticMeshActor* Actor, const FVector& Scale)
		{
			if (!IsValid(Actor) || Actor->GetStaticMeshComponent() == nullptr || CubeMesh == nullptr)
			{
				return;
			}
			UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(CubeMesh);
			Actor->SetActorScale3D(Scale);
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Mesh->SetCollisionObjectType(ECC_WorldStatic);
			Mesh->SetCollisionResponseToAllChannels(ECR_Block);
			Mesh->SetGenerateOverlapEvents(false);
			Mesh->UpdateBounds();
		};

		AStaticMeshActor* Floor = nullptr;
		if (Expect(CubeMesh != nullptr, TEXT("Preview_GroundCubeMesh")))
		{
			Floor = World->SpawnActor<AStaticMeshActor>(
				AStaticMeshActor::StaticClass(),
				FVector(GroundProbe.X, GroundProbe.Y, 50.0f),
				FRotator::ZeroRotator,
				GroundParams);
		}
		if (Expect(IsValid(Floor) && Floor->GetStaticMeshComponent() != nullptr, TEXT("Preview_SpawnGroundSlab")))
		{
			ConfigureWorldStaticCube(Floor, FVector(8.0f, 8.0f, 0.2f));
		}

		AStaticMeshActor* ElevatedProp = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(),
			FVector(GroundProbe.X, GroundProbe.Y, 420.0f),
			FRotator::ZeroRotator,
			GroundParams);
		if (Expect(IsValid(ElevatedProp), TEXT("Preview_SpawnElevatedProp")))
		{
			ConfigureWorldStaticCube(ElevatedProp, FVector(1.5f, 1.5f, 1.5f));
		}

		AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(),
			FVector(GroundProbe.X, GroundProbe.Y, 400.0f),
			FRotator::ZeroRotator,
			GroundParams);
		if (Expect(IsValid(Wall), TEXT("Preview_SpawnBoundaryWall")))
		{
			ConfigureWorldStaticCube(Wall, FVector(0.25f, 4.0f, 8.0f));
		}

		AGP_ResourceNode* Resource = World->SpawnActor<AGP_ResourceNode>(
			AGP_ResourceNode::StaticClass(),
			FVector(GroundProbe.X, GroundProbe.Y, 280.0f),
			FRotator::ZeroRotator,
			GroundParams);
		AStaticMeshActor* ResourceGeo = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(),
			FVector(GroundProbe.X, GroundProbe.Y, 260.0f),
			FRotator::ZeroRotator,
			GroundParams);
		if (Expect(IsValid(ResourceGeo), TEXT("Preview_SpawnResourceGeometry")))
		{
			ConfigureWorldStaticCube(ResourceGeo, FVector(2.0f, 2.0f, 2.0f));
		}

		AGP_BuildGridContractStub* HighStub = World->SpawnActorDeferred<AGP_BuildGridContractStub>(
			AGP_BuildGridContractStub::StaticClass(),
			FTransform(FRotator::ZeroRotator, GroundProbe),
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Expect(IsValid(HighStub), TEXT("Preview_SpawnElevatedStub")))
		{
			HighStub->ConfigureGridPlacement(FIntPoint(-360, 55), FIntPoint(1, 1));
			HighStub->FinishSpawning(FTransform(FRotator::ZeroRotator, GroundProbe));
		}
		const float ResolvedGroundZ = GPBuildingDropAuthority::ResolvePreviewGroundZ(World, GroundProbe);
		Expect(ResolvedGroundZ < 150.0f, TEXT("Preview_GroundZIgnoresBuildingSurface"));
		Expect(ResolvedGroundZ < 150.0f, TEXT("Preview_GroundZStaysTerrain"));
		Expect(ResolvedGroundZ < 150.0f, TEXT("Preview_GroundZIgnoresElevatedStatic"));
		Expect(ResolvedGroundZ < 150.0f, TEXT("Preview_GroundZIgnoresBoundaryWall"));
		Expect(ResolvedGroundZ < 150.0f, TEXT("Preview_GroundZIgnoresResourceGeometry"));
		if (IsValid(HighStub))
		{
			HighStub->Destroy();
		}
		if (IsValid(ElevatedProp))
		{
			ElevatedProp->Destroy();
		}
		if (IsValid(Wall))
		{
			Wall->Destroy();
		}
		if (IsValid(Resource))
		{
			Resource->Destroy();
		}
		if (IsValid(ResourceGeo))
		{
			ResourceGeo->Destroy();
		}
		if (IsValid(Floor))
		{
			Floor->Destroy();
		}

		AStaticMeshActor* WorldBlocker = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(),
			AdjacentDeployLocation + FVector(0.0f, 0.0f, 80.0f),
			FRotator::ZeroRotator,
			GroundParams);
		if (Expect(IsValid(WorldBlocker), TEXT("Preview_SpawnWorldBlocker")))
		{
			ConfigureWorldStaticCube(WorldBlocker, FVector(6.0f, 6.0f, 2.0f));
		}
		GPBuildingDropAuthority::FPlacementPreview WorldPreview;
		GPBuildingDropAuthority::EvaluateLocalPlacementPreview(
			World,
			OwnerPS,
			HubDrop,
			FTransform(FRotator::ZeroRotator, AdjacentDeployLocation),
			WorldPreview);
		Expect(!WorldPreview.bValid, TEXT("Preview_WorldBlockedStillInvalid"));
		Expect(WorldPreview.RejectReason == EGP_BuildingDropRejectReason::PlacementOverlap
			|| WorldPreview.RejectReason == EGP_BuildingDropRejectReason::NotNavigable,
			TEXT("Preview_EnvironmentOverlapRejected"));
		if (IsValid(WorldBlocker))
		{
			WorldBlocker->Destroy();
		}

		GPBuildingDropAuthority::FPlacementPreview RangePreview;
		GPBuildingDropAuthority::EvaluateLocalPlacementPreview(
			World,
			OwnerPS,
			HubDrop,
			FTransform(FRotator::ZeroRotator, Base->GetActorLocation() + FVector(99999.0f, 0.0f, 0.0f)),
			RangePreview);
		Expect(!RangePreview.bValid
			&& RangePreview.RejectReason == EGP_BuildingDropRejectReason::OutOfDeployRadius,
			TEXT("Preview_LocalOutOfRangeReason"));
		Expect(FCString::Strcmp(
			GPBuildingDropAuthority::GetPlacementPreviewStatusLabel(RangePreview.bValid, RangePreview.RejectReason),
			TEXT("BLOCKED: OUT OF RANGE")) == 0, TEXT("Preview_LocalOutOfRangeLabel"));

		UGP_BuildingDefinition* BadBuilding = NewObject<UGP_BuildingDefinition>(
			this, FName(TEXT("DA_GP_Building_InvalidFootprint")), RF_Transient);
		BadBuilding->DisplayName = NSLOCTEXT("GPBuildGrid", "Bad", "Invalid Footprint");
		BadBuilding->FootprintCells = FIntPoint(0, 4);
		BadBuilding->SpawnedClass = AGP_BuildGridContractStub::StaticClass();
		UGP_BuildingDropCatalog::Get().RegisterBuildingDefinition(BadBuilding);
		UGP_OrbitalDropDefinition* BadDrop = NewObject<UGP_OrbitalDropDefinition>(
			this, FName(TEXT("DA_GP_OrbitalDrop_InvalidFootprint")), RF_Transient);
		BadDrop->Cost = 10.0f;
		BadDrop->BuildingDefinition = BadBuilding;
		UGP_BuildingDropCatalog::Get().RegisterDropDefinition(BadDrop);
		InvalidFootprintDropWeak = BadDrop;
		Expect(Inventory->AuthorityAddReady(BadDrop, 1), TEXT("R_AddInvalidReady"));
		AGP_BuildGridContractStub* InvalidStubCDO = GetMutableDefault<AGP_BuildGridContractStub>();
		UBoxComponent* InvalidStubBounds =
			InvalidStubCDO != nullptr ? InvalidStubCDO->GetPlacementFootprintBounds() : nullptr;
		GPBuildGridContractDebug::FScopedBoxAuthoring RestoreInvalidStub(InvalidStubBounds);
		RestoreInvalidStub.DisableUsableBounds();
		GPBuildingDropAuthority::FDeployResult InvalidFp =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World, OwnerPS, BadDrop, FTransform(FRotator::ZeroRotator, ValidDeployLocation));
		Expect(!InvalidFp.bAccepted, TEXT("R_InvalidFootprintReject"));
		Expect(InvalidFp.RejectReason == EGP_BuildingDropRejectReason::InvalidFootprint, TEXT("R_InvalidFootprintDeployReason"));
		Expect(Inventory->GetReadyCount(BadDrop) == 1, TEXT("R_InvalidDoesNotConsumeReady"));

		++StageIndex;
		ScheduleNext(0.05f);
		break;
	}
	case 4: // N failed payload releases reservation
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalDropDefinition* HubDrop = UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop();
		if (!Expect(IsValid(OwnerPS) && IsValid(HubDrop), TEXT("N_SetupAlive")))
		{
			Finish();
			return;
		}

		if (AGP_DropPod* FirstPod = LastPodWeak.Get())
		{
			FirstPod->Destroy();
		}
		Grid->SweepStaleOccupants();
		Expect(GPBuildGridContractDebug::AllCellsFree(Grid, FirstHubOrigin, FIntPoint(4, 4)),
			TEXT("N_DestroyedPodReleasesReservation"));

		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		if (Inventory->GetReadyCount(HubDrop) < 1)
		{
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, HubDrop);
		}

		GPBuildingDropAuthority::FDeployResult SkipDeploy =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World, OwnerPS, HubDrop, FTransform(FRotator::ZeroRotator, AdjacentDeployLocation));
		Expect(SkipDeploy.bAccepted, TEXT("N_SkipDeployAccepted"));
		SkipPodWeak = SkipDeploy.SpawnedPod;
		if (AGP_DropPod* SkipPod = SkipDeploy.SpawnedPod.Get())
		{
			SkipPod->DebugForceSkipPayloadSpawn();
		}

		++StageIndex;
		ScheduleNext(1.0f);
		break;
	}
	case 5: // N complete + U/V/W/X successful hub
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		UGP_OrbitalDropDefinition* HubDrop = UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop();
		if (!Expect(IsValid(OwnerPS) && IsValid(HubDrop), TEXT("O_SetupAlive")))
		{
			Finish();
			return;
		}

		FIntPoint SkipOrigin;
		FVector SkipSnapped;
		Grid->ResolveSnappedPlacement(AdjacentDeployLocation, FIntPoint(4, 4), SkipOrigin, SkipSnapped);
		Expect(GPBuildGridContractDebug::AllCellsFree(Grid, SkipOrigin, FIntPoint(4, 4)),
			TEXT("N_FailedDeliveryReleasedReservation"));

		if (UGP_OrbitalDeliverySettings* Settings = GetMutableDefault<UGP_OrbitalDeliverySettings>())
		{
			Settings->BuildingDropCleanupDelaySeconds = 0.05f;
			UGP_BuildingDropCatalog::Get().OverrideDeliveryTiming(0.08f, 0.0f);
		}

		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		while (Inventory->GetReadyCount(HubDrop) < 1)
		{
			if (!GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, HubDrop).bAccepted)
			{
				GPBuildGridContractDebug::GrantOrbital(OwnerPS, 200.0f);
				GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, HubDrop);
				break;
			}
		}
		MaxUnitsBeforeHub = GPBuildGridContractDebug::RoundMaxUnits(OwnerPS);
		const int32 ReadyBefore = Inventory->GetReadyCount(HubDrop);
		GPBuildingDropAuthority::FDeployResult HubDeploy =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World, OwnerPS, HubDrop, FTransform(FRotator::ZeroRotator, ValidDeployLocation));
		Expect(HubDeploy.bAccepted, TEXT("U_AcceptedDeploy"));
		Expect(Inventory->GetReadyCount(HubDrop) == ReadyBefore - 1, TEXT("U_ConsumeReadyOnce"));
		LastPodWeak = HubDeploy.SpawnedPod;
		FirstHubOrigin = HubDeploy.OriginCell;
		SnappedExpected = HubDeploy.SnappedLocation;

		++StageIndex;
		ScheduleNext(0.35f);
		break;
	}
	case 6: // payload grid state / +5 / nav obstacle / adjacent / Y
	{
		AGP_PlayerState* OwnerPS = OwnerPSWeak.Get();
		if (!Expect(IsValid(OwnerPS), TEXT("V_OwnerAlive")))
		{
			Finish();
			return;
		}

		AGP_LogisticsHub* Hub = nullptr;
		int32 HubCount = 0;
		for (TActorIterator<AGP_LogisticsHub> It(World); It; ++It)
		{
			if (It->GetTeamId() == GPBuildGridContractDebug::ContractTeam)
			{
				++HubCount;
				Hub = *It;
			}
		}
		Expect(HubCount == 1 && IsValid(Hub), TEXT("O_PayloadConvertedToBuilding"));
		LiveHubWeak = Hub;
		if (IsValid(Hub))
		{
			Expect(Hub->GetGridFootprintSize() == FIntPoint(4, 4), TEXT("V_HubFootprint4x4"));
			Expect(Hub->GetGridOriginCell() == FirstHubOrigin, TEXT("V_HubOriginCell"));
			Expect(Hub->GetPlacementFootprintBounds() != nullptr, TEXT("Bounds_ComponentPresentOnHub"));
			Expect(GPBuildGridContractDebug::AllCellsOccupied(Grid, Hub->GetGridOriginCell(), FIntPoint(4, 4)),
				TEXT("O_BuildingOccupiesFootprint"));
			Expect(FVector::Dist2D(Hub->GetActorLocation(), SnappedExpected) <= 8.0f, TEXT("P_LandingUsesSnappedXY"));
			Expect(Hub->GetNavigationObstacle() != nullptr
				&& Hub->GetNavigationObstacle()->bDynamicObstacle, TEXT("X_NavigationObstaclePresent"));
		}
		Expect(GPBuildGridContractDebug::RoundMaxUnits(OwnerPS) == MaxUnitsBeforeHub + 5, TEXT("W_Plus5Granted"));

		UGP_OrbitalDropDefinition* HubDrop = UGP_BuildingDropCatalog::Get().GetLegacyLogisticsHubDrop();
		UGP_OrbitalBuildingInventoryComponent* Inventory = OwnerPS->GetOrbitalBuildingInventoryComponent();
		if (Inventory->GetReadyCount(HubDrop) < 1)
		{
			GPBuildingDropAuthority::AuthorityPurchaseBuilding(World, OwnerPS, HubDrop);
		}
		GPBuildingDropAuthority::FDeployResult Adjacent =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World, OwnerPS, HubDrop, FTransform(FRotator::ZeroRotator, AdjacentDeployLocation));
		Expect(Adjacent.bAccepted, TEXT("I_AdjacentDeployAccepted"));

		const UObject* WallClass = StaticFindObject(
			UClass::StaticClass(), nullptr, TEXT("/Script/GPRuntime.GP_Wall"));
		Expect(WallClass == nullptr, TEXT("Y_NoWallClass"));
		const UObject* FoWClass = StaticFindObject(
			UClass::StaticClass(), nullptr, TEXT("/Script/GPRuntime.GP_FogOfWarSubsystem"));
		Expect(FoWClass == nullptr, TEXT("Y_FoWPlacementDeferred"));

		Finish();
		break;
	}
	default:
		Finish();
		break;
	}
}

#else

void UGP_BuildGridContractTestRunner::BeginDestroy()
{
	Super::BeginDestroy();
}
void UGP_BuildGridContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_BuildGridContractTestRunner::ScheduleNext(float DelaySeconds) { (void)DelaySeconds; }
void UGP_BuildGridContractTestRunner::AdvanceStage() {}
bool UGP_BuildGridContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return true;
}
void UGP_BuildGridContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_BuildGridContractTestRunner::Finish() { bFinished = true; }
void UGP_BuildGridContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_BuildGridContractTestRunner::UnbindWorldCleanup() {}
void UGP_BuildGridContractTestRunner::CleanupActors() {}
void UGP_BuildGridContractTestRunner::RestoreSettings() {}

#endif
