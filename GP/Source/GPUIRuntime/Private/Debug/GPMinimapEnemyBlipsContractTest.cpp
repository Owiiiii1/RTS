// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPMainBase.h"
#include "Engine/World.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/IConsoleManager.h"
#include "Presentation/GPLocalFoWUnitPresentationSubsystem.h"
#include "Settings/GPGameplayPresentationSettings.h"
#include "Units/GPWorker.h"
#include "ViewModels/GPMinimapPresenter.h"
#include "Widgets/GPMinimapWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMinimapEnemyBlipsContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPMinimapEnemyBlipsContractPrivate
{
	static FGP_FoWPresentationUpdate MakeGrid(int32 TeamId, int64 Revision, bool bInitial)
	{
		FGP_FoWPresentationUpdate Update;
		Update.bInitialSnapshot = bInitial;
		Update.TeamId = TeamId;
		Update.Revision = Revision;
		Update.GridOriginWorldXY = FVector2D::ZeroVector;
		Update.GridDimensions = FIntPoint(4, 4);
		Update.CellSizeCm = 100.0f;
		return Update;
	}

	static FGP_FoWCellRange Range(int32 Start, int32 Count)
	{
		FGP_FoWCellRange Result;
		Result.StartIndex = Start;
		Result.NumCells = Count;
		return Result;
	}

	static int32 WorldToCellIndex(const FVector& WorldLocation)
	{
		const int32 X = FMath::FloorToInt(WorldLocation.X / 100.0f);
		const int32 Y = FMath::FloorToInt(WorldLocation.Y / 100.0f);
		return Y * 4 + X;
	}

	static bool ColorsEqual(const FLinearColor& A, const FLinearColor& B)
	{
		return FMath::IsNearlyEqual(A.R, B.R, 0.001f)
			&& FMath::IsNearlyEqual(A.G, B.G, 0.001f)
			&& FMath::IsNearlyEqual(A.B, B.B, 0.001f)
			&& FMath::IsNearlyEqual(A.A, B.A, 0.001f);
	}

	static void RunMinimapEnemyBlipsContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMinimapEnemyBlipsContract, Warning,
				TEXT("gp.UI.RunMinimapEnemyBlipsContractTest: missing world or client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPMinimapEnemyBlipsContract, Log,
					TEXT("gp.UI.RunMinimapEnemyBlipsContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPMinimapEnemyBlipsContract, Error,
					TEXT("gp.UI.RunMinimapEnemyBlipsContractTest FAIL: %s"), Label);
			}
		};

		const UGP_GameplayPresentationSettings* Settings = UGP_GameplayPresentationSettings::Get();
		const FLinearColor Team1Color =
			Settings != nullptr ? Settings->GetTeamColor(1) : FLinearColor::White;
		const FLinearColor Team2Color =
			Settings != nullptr ? Settings->GetTeamColor(2) : FLinearColor::White;
		const FLinearColor NeutralColor =
			Settings != nullptr ? Settings->NeutralTeamColor : FLinearColor::White;
		const FLinearColor LegacyCyan(0.25f, 0.95f, 1.0f, 1.0f);
		const FLinearColor LegacyYellow(0.95f, 0.95f, 0.35f, 1.0f);

		Expect(Settings != nullptr
			&& !ColorsEqual(Team1Color, Team2Color)
			&& ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(1), Team1Color)
			&& ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(2), Team2Color)
			&& ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(-1), NeutralColor)
			&& ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(99), NeutralColor)
			&& !ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(1), LegacyCyan)
			&& !ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(1), LegacyYellow)
			&& !ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(2), LegacyCyan)
			&& !ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(2), LegacyYellow)
			&& UGP_MinimapWidget::GetUnitBlipHalfExtentPx()
				< UGP_MinimapWidget::GetBuildingBlipHalfExtentPx(),
			TEXT("A_CanonicalTeamColorAndSizeNotHardcodedTypeColors"));

		UGP_MinimapPresenter* Presenter = NewObject<UGP_MinimapPresenter>(GetTransientPackage());
		UGP_LocalFoWComponent* Mirror = NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		UGP_MinimapWidget* Widget = NewObject<UGP_MinimapWidget>(GetTransientPackage());
		Expect(Presenter != nullptr && Mirror != nullptr && Widget != nullptr
			&& Presenter->InitializeWithMirror(Mirror),
			TEXT("B_ObjectsCreated"));

		FGP_FoWPresentationUpdate Initial = MakeGrid(1, 1, true);
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(Initial), TEXT("C_FoWSnapshotAccepted"));

		const FBox DisplayedBounds(
			FVector(50.0f, 80.0f, -10.0f),
			FVector(250.0f, 280.0f, 10.0f));
		if (Presenter != nullptr)
		{
			Presenter->ContractApplyDisplayedWorldBounds(DisplayedBounds);
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		const FVector FriendlyUnitLoc(150.0f, 180.0f, 0.0f);
		const FVector FriendlyBuildingLoc(100.0f, 100.0f, 0.0f);
		const FVector EnemyUnitLoc(160.0f, 190.0f, 0.0f);
		const FVector EnemyBuildingLoc(220.0f, 220.0f, 0.0f);
		const FVector UnassignedLoc(140.0f, 140.0f, 0.0f);
		const FVector OutsideEnemyLoc(10.0f, 10.0f, 0.0f);

		AGP_Worker* FriendlyUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), FriendlyUnitLoc, FRotator::ZeroRotator, SpawnParams);
		AGP_MainBase* FriendlyBuilding = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), FriendlyBuildingLoc, FRotator::ZeroRotator, SpawnParams);
		AGP_Worker* EnemyUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), EnemyUnitLoc, FRotator::ZeroRotator, SpawnParams);
		AGP_MainBase* EnemyBuilding = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), EnemyBuildingLoc, FRotator::ZeroRotator, SpawnParams);
		AGP_Worker* UnassignedUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), UnassignedLoc, FRotator::ZeroRotator, SpawnParams);
		AGP_Worker* OutsideEnemy = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), OutsideEnemyLoc, FRotator::ZeroRotator, SpawnParams);

		if (FriendlyUnit != nullptr) { FriendlyUnit->SetTeamId(1); }
		if (FriendlyBuilding != nullptr) { FriendlyBuilding->SetTeamId(1); }
		if (EnemyUnit != nullptr) { EnemyUnit->SetTeamId(2); }
		if (EnemyBuilding != nullptr) { EnemyBuilding->SetTeamId(2); }
		if (UnassignedUnit != nullptr) { UnassignedUnit->SetTeamId(0); }
		if (OutsideEnemy != nullptr) { OutsideEnemy->SetTeamId(2); }

		if (Presenter != nullptr)
		{
			Presenter->ContractBindUnitRegistry(World);
			Presenter->ContractRebuildFriendlyBlips();
		}

		const FGP_MinimapBlip* FriendlyUnitBlip =
			Presenter != nullptr ? Presenter->ContractFindBlipForActor(FriendlyUnit) : nullptr;
		const FGP_MinimapBlip* FriendlyBuildingBlip =
			Presenter != nullptr ? Presenter->ContractFindBlipForActor(FriendlyBuilding) : nullptr;
		Expect(FriendlyUnitBlip != nullptr
			&& FriendlyBuildingBlip != nullptr
			&& FriendlyUnitBlip->TeamId == 1
			&& FriendlyBuildingBlip->TeamId == 1
			&& FriendlyUnitBlip->Kind == EGP_MinimapBlipKind::Unit
			&& FriendlyBuildingBlip->Kind == EGP_MinimapBlipKind::Building
			&& ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(FriendlyUnitBlip->TeamId), Team1Color)
			&& ColorsEqual(
				UGP_MinimapWidget::ResolveBlipColor(FriendlyUnitBlip->TeamId),
				UGP_MinimapWidget::ResolveBlipColor(FriendlyBuildingBlip->TeamId)),
			TEXT("D_FriendlyUnitAndBuildingShareTeamColorDifferByKind"));

		Expect(Presenter != nullptr
			&& Presenter->ContractFindBlipForActor(EnemyUnit) == nullptr
			&& Presenter->ContractFindBlipForActor(EnemyBuilding) == nullptr
			&& Presenter->ContractFindBlipForActor(UnassignedUnit) == nullptr
			&& Presenter->ContractFindBlipForActor(OutsideEnemy) == nullptr,
			TEXT("E_EnemyUnexploredAndInvalidTeamHaveNoBlip"));

		FGP_FoWPresentationUpdate EnemyVisible = MakeGrid(1, 2, false);
		EnemyVisible.VisibleRanges.Add(Range(WorldToCellIndex(EnemyUnitLoc), 1));
		EnemyVisible.ExploredRanges.Add(Range(WorldToCellIndex(EnemyUnitLoc), 1));
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(EnemyVisible)
			&& Mirror->IsVisible(EnemyUnitLoc)
			&& Mirror->GetStateAtWorldLocation(EnemyBuildingLoc) == EGP_FoWState::Unexplored,
			TEXT("F_EnemyUnitCellVisibleBuildingUnexplored"));
		if (Presenter != nullptr)
		{
			Presenter->ContractRebuildFriendlyBlips();
		}
		Expect(Presenter != nullptr
			&& Presenter->ContractFindBlipForActor(EnemyUnit) != nullptr
			&& Presenter->ContractFindBlipForActor(EnemyUnit)->TeamId == 2
			&& Presenter->ContractFindBlipForActor(EnemyUnit)->Kind == EGP_MinimapBlipKind::Unit
			&& ColorsEqual(
				UGP_MinimapWidget::ResolveBlipColor(
					Presenter->ContractFindBlipForActor(EnemyUnit)->TeamId),
				Team2Color)
			&& Presenter->ContractFindBlipForActor(EnemyBuilding) == nullptr
			&& Presenter->ContractFindBlipForActor(FriendlyUnit) != nullptr,
			TEXT("G_VisibleEnemyUnitBlipUnexploredEnemyBuildingAbsent"));

		FGP_FoWPresentationUpdate EnemyExplored = MakeGrid(1, 3, false);
		EnemyExplored.ExploredRanges.Add(Range(WorldToCellIndex(EnemyUnitLoc), 1));
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(EnemyExplored)
			&& Mirror->GetStateAtWorldLocation(EnemyUnitLoc) == EGP_FoWState::Explored,
			TEXT("H_EnemyUnitNowExplored"));
		if (Presenter != nullptr)
		{
			Presenter->ContractRebuildFriendlyBlips();
		}
		Expect(Presenter != nullptr
			&& Presenter->ContractFindBlipForActor(EnemyUnit) == nullptr
			&& Presenter->ContractFindBlipForActor(FriendlyUnit) != nullptr
			&& Presenter->ContractFindBlipForActor(FriendlyBuilding) != nullptr,
			TEXT("I_ExploredRemovesEnemyFriendlyRemain"));

		FGP_FoWPresentationUpdate RestoreVisible = MakeGrid(1, 4, false);
		RestoreVisible.VisibleRanges.Add(Range(WorldToCellIndex(EnemyUnitLoc), 1));
		RestoreVisible.VisibleRanges.Add(Range(WorldToCellIndex(EnemyBuildingLoc), 1));
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(RestoreVisible)
			&& Mirror->IsVisible(EnemyUnitLoc)
			&& Mirror->IsVisible(EnemyBuildingLoc),
			TEXT("J_EnemyUnitAndBuildingVisible"));
		if (Presenter != nullptr)
		{
			Presenter->ContractRebuildFriendlyBlips();
		}
		const FGP_MinimapBlip* RestoredEnemyUnit =
			Presenter != nullptr ? Presenter->ContractFindBlipForActor(EnemyUnit) : nullptr;
		const FGP_MinimapBlip* VisibleEnemyBuilding =
			Presenter != nullptr ? Presenter->ContractFindBlipForActor(EnemyBuilding) : nullptr;
		Expect(RestoredEnemyUnit != nullptr
			&& VisibleEnemyBuilding != nullptr
			&& VisibleEnemyBuilding->Kind == EGP_MinimapBlipKind::Building
			&& RestoredEnemyUnit->TeamId == 2
			&& VisibleEnemyBuilding->TeamId == 2
			&& ColorsEqual(
				UGP_MinimapWidget::ResolveBlipColor(RestoredEnemyUnit->TeamId),
				UGP_MinimapWidget::ResolveBlipColor(VisibleEnemyBuilding->TeamId))
			&& Presenter->ContractFindBlipForActor(OutsideEnemy) == nullptr,
			TEXT("K_VisibleRestoresEnemyAndBuildingSameTeamColorOutsideOmitted"));

		if (Widget != nullptr && Presenter != nullptr)
		{
			Widget->ContractBindPresenter(Presenter);
		}
		bool bSawTeam1Unit = false;
		bool bSawTeam1Building = false;
		bool bSawTeam2 = false;
		bool bTeam1ColorsMatch = true;
		if (Widget != nullptr)
		{
			for (int32 Index = 0; Index < Widget->GetBlipDrawCount(); ++Index)
			{
				const int32 DrawTeamId = Widget->ContractGetBlipDrawTeamId(Index);
				const bool bBuilding = Widget->ContractGetBlipDrawIsBuilding(Index);
				if (DrawTeamId == 1)
				{
					bTeam1ColorsMatch = bTeam1ColorsMatch
						&& ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(DrawTeamId), Team1Color);
					if (bBuilding)
					{
						bSawTeam1Building = true;
					}
					else
					{
						bSawTeam1Unit = true;
					}
				}
				else if (DrawTeamId == 2)
				{
					bSawTeam2 = true;
					bTeam1ColorsMatch = bTeam1ColorsMatch
						&& ColorsEqual(UGP_MinimapWidget::ResolveBlipColor(DrawTeamId), Team2Color);
				}
			}
		}
		Expect(Widget != nullptr
			&& Widget->GetBlipDrawCount() >= 4
			&& bSawTeam1Unit
			&& bSawTeam1Building
			&& bSawTeam2
			&& bTeam1ColorsMatch
			&& UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr,
			TEXT("L_WidgetDrawUsesTeamColorAndNoTick"));

		if (Presenter != nullptr)
		{
			Presenter->Shutdown();
		}
		if (Widget != nullptr)
		{
			Widget->ContractUnbindPresenter();
		}
		if (FriendlyUnit != nullptr) { FriendlyUnit->Destroy(); }
		if (FriendlyBuilding != nullptr) { FriendlyBuilding->Destroy(); }
		if (EnemyUnit != nullptr) { EnemyUnit->Destroy(); }
		if (EnemyBuilding != nullptr) { EnemyBuilding->Destroy(); }
		if (UnassignedUnit != nullptr) { UnassignedUnit->Destroy(); }
		if (OutsideEnemy != nullptr) { OutsideEnemy->Destroy(); }

		UE_LOG(LogGPMinimapEnemyBlipsContract, Log,
			TEXT("gp.UI.RunMinimapEnemyBlipsContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMinimapEnemyBlipsContract(
		TEXT("gp.UI.RunMinimapEnemyBlipsContractTest"),
		TEXT("Run native minimap team-color and enemy FoW-gated blip contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMinimapEnemyBlipsContractTest));
}

#endif
