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
#include "Settings/GPOrbitalDeliverySettings.h"
#include "TimerManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPBuildGridContract, Log, All);

namespace GPBuildGridContractDebug
{
	static TWeakObjectPtr<UGP_BuildGridContractTestRunner> GActiveRunner;
	constexpr int32 ContractTeam = 93;

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
		Settings->BuildingDropPayloadDeployDelaySeconds = SavedBuildingDeployDelay;
		Settings->BuildingDropDescentDurationSeconds = SavedBuildingDescent;
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
			Expect(Ghost->GetPreviewGridLineCount() >= 4, TEXT("Preview_GhostHasOuterLines"));
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
			const float LineZ = OffsetSnapped.Z + 24.0f;
			FVector BorderStart = FVector::ZeroVector;
			FVector BorderEnd = FVector::ZeroVector;
			Expect(Ghost->GetPreviewLineWorldSegment(0, BorderStart, BorderEnd), TEXT("Preview_OffsetHasBorder"));
			Expect(FMath::IsNearlyEqual(BorderStart.X, ExpectedMin.X)
				&& FMath::IsNearlyEqual(BorderStart.Y, ExpectedMin.Y)
				&& FMath::IsNearlyEqual(BorderEnd.X, ExpectedMax.X)
				&& FMath::IsNearlyEqual(BorderEnd.Y, ExpectedMin.Y)
				&& FMath::IsNearlyEqual(BorderStart.Z, LineZ),
				TEXT("Preview_OffsetBorderMatchesAABB"));

			bool bAllOnFootprint = Ghost->GetPreviewLineWorldCount() >= 4;
			bool bAnyNearOrigin = false;
			for (int32 LineIdx = 0; LineIdx < Ghost->GetPreviewLineWorldCount(); ++LineIdx)
			{
				FVector Start = FVector::ZeroVector;
				FVector End = FVector::ZeroVector;
				if (!Ghost->GetPreviewLineWorldSegment(LineIdx, Start, End))
				{
					bAllOnFootprint = false;
					break;
				}
				const float StartXY = FVector2D(Start.X, Start.Y).Size();
				const float EndXY = FVector2D(End.X, End.Y).Size();
				if (StartXY < 500.0f || EndXY < 500.0f)
				{
					bAnyNearOrigin = true;
				}
				if (Start.X < ExpectedMin.X - 1.0f || Start.X > ExpectedMax.X + 1.0f
					|| Start.Y < ExpectedMin.Y - 1.0f || Start.Y > ExpectedMax.Y + 1.0f
					|| End.X < ExpectedMin.X - 1.0f || End.X > ExpectedMax.X + 1.0f
					|| End.Y < ExpectedMin.Y - 1.0f || End.Y > ExpectedMax.Y + 1.0f)
				{
					bAllOnFootprint = false;
				}
			}
			Expect(bAllOnFootprint && !bAnyNearOrigin, TEXT("Preview_OffsetLinesFollowFootprint"));
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
			SavedBuildingDescent = Settings->BuildingDropDescentDurationSeconds;
			SavedBuildingCleanup = Settings->BuildingDropCleanupDelaySeconds;
			SavedBuildingAltitude = Settings->BuildingDropSpawnAltitudeCm;
			SavedBuildingDeployDelay = Settings->BuildingDropPayloadDeployDelaySeconds;
			Settings->BuildingDropDescentDurationSeconds = 0.45f;
			Settings->BuildingDropCleanupDelaySeconds = 0.05f;
			Settings->BuildingDropSpawnAltitudeCm = 400.0f;
			Settings->BuildingDropPayloadDeployDelaySeconds = 0.35f;
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

		GPBuildingDropAuthority::FDeployResult First =
			GPBuildingDropAuthority::AuthorityDeployBuilding(
				World, OwnerPS, HubDrop, FTransform(FRotator::ZeroRotator, Unsnapped));
		Expect(First.bAccepted, TEXT("M_FirstReservation"));
		Expect(First.OriginCell == FirstHubOrigin, TEXT("P_ServerSnappedOrigin"));
		Expect(FVector::Dist2D(First.SnappedLocation, SnappedExpected) <= 1.0f, TEXT("P_ServerSnappedLocation"));
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
			UStaticMeshComponent* FloorMesh = Floor->GetStaticMeshComponent();
			FloorMesh->SetMobility(EComponentMobility::Movable);
			FloorMesh->SetStaticMesh(CubeMesh);
			Floor->SetActorScale3D(FVector(8.0f, 8.0f, 0.2f));
			FloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			FloorMesh->SetCollisionObjectType(ECC_WorldStatic);
			FloorMesh->SetCollisionResponseToAllChannels(ECR_Block);
			FloorMesh->SetGenerateOverlapEvents(false);
			FloorMesh->UpdateBounds();
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
		const float ResolvedGroundZ = GPBuildingDropAuthority::ResolvePreviewGroundZ(World, GroundProbe, HighStub);
		Expect(ResolvedGroundZ < 150.0f, TEXT("Preview_GroundZIgnoresBuildingSurface"));
		if (IsValid(HighStub))
		{
			HighStub->Destroy();
		}
		if (IsValid(Floor))
		{
			Floor->Destroy();
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
			Settings->BuildingDropDescentDurationSeconds = 0.08f;
			Settings->BuildingDropPayloadDeployDelaySeconds = 0.0f;
			Settings->BuildingDropCleanupDelaySeconds = 0.05f;
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
