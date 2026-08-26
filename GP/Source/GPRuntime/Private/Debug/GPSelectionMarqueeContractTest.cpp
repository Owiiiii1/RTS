// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPMainBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Units/GPUnitBase.h"
#include "Units/GPWorker.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPSelectionMarqueeContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPSelectionMarqueeContractPrivate
{
	static void NeutralizeAllUnits(UWorld* World)
	{
		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (IsValid(Unit))
			{
				Unit->SetTeamId(-1);
			}
		}
	}

	static void PrepareLocalView(APlayerController* PlayerController)
	{
		if (!IsValid(PlayerController))
		{
			return;
		}

		if (APawn* Pawn = PlayerController->GetPawn())
		{
			PlayerController->SetViewTarget(Pawn);
		}

		if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			CameraManager->SetGameCameraCutThisFrame();
			CameraManager->UpdateCamera(0.0f);
		}

		ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		UGameViewportClient* ViewportClient =
			LocalPlayer != nullptr ? LocalPlayer->ViewportClient : nullptr;
		int32 SizeX = 0;
		int32 SizeY = 0;
		PlayerController->GetViewportSize(SizeX, SizeY);
		if ((SizeX <= 1 || SizeY <= 1) && ViewportClient != nullptr && ViewportClient->Viewport != nullptr)
		{
			ViewportClient->Viewport->SetInitialSize(FIntPoint(1280, 720));
		}
	}

	static FVector ResolveGround(APlayerController* PlayerController)
	{
		PrepareLocalView(PlayerController);

		FVector ViewLoc = FVector::ZeroVector;
		FRotator ViewRot = FRotator::ZeroRotator;
		if (IsValid(PlayerController))
		{
			PlayerController->GetPlayerViewPoint(ViewLoc, ViewRot);
		}

		FVector Ground = ViewLoc;
		const FVector Look = ViewRot.Vector();
		if (FMath::Abs(Look.Z) > 0.05f)
		{
			const float T = (100.0f - ViewLoc.Z) / Look.Z;
			if (T > 50.0f)
			{
				Ground = ViewLoc + Look * T;
			}
		}
		else if (APawn* Pawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr)
		{
			Ground = Pawn->GetActorLocation();
		}

		Ground.Z = 100.0f;
		return Ground;
	}

	static AGP_Worker* SpawnWorker(UWorld* World, const FVector& Location, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (Worker != nullptr)
		{
			Worker->SetTeamId(TeamId);
		}
		return Worker;
	}

	static AGP_MainBase* SpawnMainBase(UWorld* World, const FVector& Location, int32 TeamId)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* MainBase = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (MainBase != nullptr)
		{
			MainBase->SetTeamId(TeamId);
		}
		return MainBase;
	}

	static bool ProjectActor(APlayerController* PlayerController, const AActor* Actor, FVector2D& OutScreen)
	{
		if (!IsValid(PlayerController) || !IsValid(Actor))
		{
			return false;
		}
		return PlayerController->ProjectWorldLocationToScreen(Actor->GetActorLocation(), OutScreen, false);
	}

	static bool MakeCoveringRect(
		APlayerController* PlayerController,
		const TArray<AActor*>& Actors,
		FVector2D& OutMin,
		FVector2D& OutMax)
	{
		TArray<FVector2D> Points;
		Points.Reserve(Actors.Num());
		for (AActor* Actor : Actors)
		{
			FVector2D Screen = FVector2D::ZeroVector;
			if (!ProjectActor(PlayerController, Actor, Screen))
			{
				return false;
			}
			Points.Add(Screen);
		}

		OutMin = Points[0];
		OutMax = Points[0];
		for (const FVector2D& Point : Points)
		{
			OutMin.X = FMath::Min(OutMin.X, Point.X);
			OutMin.Y = FMath::Min(OutMin.Y, Point.Y);
			OutMax.X = FMath::Max(OutMax.X, Point.X);
			OutMax.Y = FMath::Max(OutMax.Y, Point.Y);
		}
		OutMin -= FVector2D(48.0f, 48.0f);
		OutMax += FVector2D(48.0f, 48.0f);
		return OutMax.X > OutMin.X && OutMax.Y > OutMin.Y;
	}

	static bool SelectionContains(const UGP_SelectionComponent* Selection, const AGP_UnitBase* Unit)
	{
		return Selection != nullptr && Selection->IsUnitSelected(Unit);
	}

	static void RunSelectionMarqueeContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPSelectionMarqueeContract, Warning,
				TEXT("gp.Selection.RunMarqueeUnitsOnlyContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (bOk)
			{
				UE_LOG(LogGPSelectionMarqueeContract, Log,
					TEXT("gp.Selection.RunMarqueeUnitsOnlyContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPSelectionMarqueeContract, Error,
					TEXT("gp.Selection.RunMarqueeUnitsOnlyContractTest FAIL: %s"), Label);
			}
		};

		NeutralizeAllUnits(World);
		FlushAsyncLoading();

		AGP_PlayerController* PlayerController =
			Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		UGP_SelectionComponent* Selection =
			PlayerController != nullptr ? PlayerController->GetSelectionComponent() : nullptr;
		Expect(IsValid(PlayerController) && PlayerController->IsLocalController() && IsValid(Selection),
			TEXT("A1_LocalSelectionPresent"));
		if (!IsValid(PlayerController) || !IsValid(Selection))
		{
			UE_LOG(LogGPSelectionMarqueeContract, Log,
				TEXT("gp.Selection.RunMarqueeUnitsOnlyContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		int32 LocalTeamId = 1;
		if (const AGP_PlayerState* PlayerState = PlayerController->GetPlayerState<AGP_PlayerState>())
		{
			LocalTeamId = PlayerState->GetTeamId();
		}
		if (LocalTeamId < 1)
		{
			LocalTeamId = 1;
		}

		const FVector Ground = ResolveGround(PlayerController);
		AGP_Worker* WorkerA = SpawnWorker(World, Ground + FVector(180.0f, 0.0f, 0.0f), LocalTeamId);
		AGP_Worker* WorkerB = SpawnWorker(World, Ground + FVector(-180.0f, 0.0f, 0.0f), LocalTeamId);
		AGP_MainBase* MainBase = SpawnMainBase(World, Ground + FVector(0.0f, 220.0f, 0.0f), LocalTeamId);
		FlushAsyncLoading();
		Expect(IsValid(WorkerA) && IsValid(WorkerB) && IsValid(MainBase)
			&& WorkerA->IsSelectionTypeUnit() && !WorkerA->IsSelectionTypeBuilding()
			&& MainBase->IsSelectionTypeBuilding() && !MainBase->IsSelectionTypeUnit(),
			TEXT("A2_SpawnedTypedActors"));
		if (!IsValid(WorkerA) || !IsValid(WorkerB) || !IsValid(MainBase))
		{
			UE_LOG(LogGPSelectionMarqueeContract, Log,
				TEXT("gp.Selection.RunMarqueeUnitsOnlyContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		auto DestroySpawned = [&]()
		{
			Selection->ClearAllSelectionState();
			if (IsValid(WorkerA)) { WorkerA->Destroy(); }
			if (IsValid(WorkerB)) { WorkerB->Destroy(); }
			if (IsValid(MainBase)) { MainBase->Destroy(); }
		};

		TArray<AActor*> MixedActors;
		MixedActors.Add(WorkerA);
		MixedActors.Add(WorkerB);
		MixedActors.Add(MainBase);
		FVector2D MixedMin = FVector2D::ZeroVector;
		FVector2D MixedMax = FVector2D::ZeroVector;
		PrepareLocalView(PlayerController);
		const bool bMixedRect = MakeCoveringRect(PlayerController, MixedActors, MixedMin, MixedMax);
		if (!bMixedRect)
		{
			int32 SizeX = 0;
			int32 SizeY = 0;
			PlayerController->GetViewportSize(SizeX, SizeY);
			FVector2D ScreenA = FVector2D::ZeroVector;
			FVector2D ScreenB = FVector2D::ZeroVector;
			FVector2D ScreenBase = FVector2D::ZeroVector;
			UE_LOG(LogGPSelectionMarqueeContract, Error,
				TEXT("A3 projection debug Viewport=%dx%d WorkerA=%d WorkerB=%d MainBase=%d"),
				SizeX, SizeY,
				ProjectActor(PlayerController, WorkerA, ScreenA) ? 1 : 0,
				ProjectActor(PlayerController, WorkerB, ScreenB) ? 1 : 0,
				ProjectActor(PlayerController, MainBase, ScreenBase) ? 1 : 0);
		}
		Expect(bMixedRect, TEXT("A3_ProjectedMixedMarqueeRect"));
		if (!bMixedRect)
		{
			DestroySpawned();
			UE_LOG(LogGPSelectionMarqueeContract, Log,
				TEXT("gp.Selection.RunMarqueeUnitsOnlyContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		Selection->ClearAllSelectionState();
		PlayerController->ApplyMarqueeSelectionForContract(MixedMin, MixedMax);
		Expect(Selection->GetSelectionCount() == 2
			&& SelectionContains(Selection, WorkerA)
			&& SelectionContains(Selection, WorkerB)
			&& !SelectionContains(Selection, MainBase),
			TEXT("B_MarqueeUnitsPlusMainBaseSelectsTwoUnitsOnly"));

		WorkerA->SetTeamId(-1);
		WorkerB->SetTeamId(-1);

		TArray<AActor*> BuildingOnly;
		BuildingOnly.Add(MainBase);
		FVector2D BuildingMin = FVector2D::ZeroVector;
		FVector2D BuildingMax = FVector2D::ZeroVector;
		const bool bBuildingRect = MakeCoveringRect(PlayerController, BuildingOnly, BuildingMin, BuildingMax);
		Expect(bBuildingRect, TEXT("C0_ProjectedBuildingMarqueeRect"));
		Selection->ClearAllSelectionState();
		if (bBuildingRect)
		{
			PlayerController->ApplyMarqueeSelectionForContract(BuildingMin, BuildingMax);
		}
		Expect(Selection->GetSelectionCount() == 0
			&& !SelectionContains(Selection, MainBase),
			TEXT("C_MarqueeBuildingOnlyLeavesSelectionEmpty"));

		FVector2D MainBaseScreen = FVector2D::ZeroVector;
		PrepareLocalView(PlayerController);
		const bool bProjectedClick = ProjectActor(PlayerController, MainBase, MainBaseScreen);
		Expect(bProjectedClick, TEXT("D0_ProjectedMainBaseClick"));
		Selection->ClearAllSelectionState();
		if (bProjectedClick)
		{
			PlayerController->ProcessSelectionClickForContract(MainBaseScreen);
		}
		Expect(Selection->GetSelectionCount() == 1
			&& SelectionContains(Selection, MainBase)
			&& !SelectionContains(Selection, WorkerA)
			&& !SelectionContains(Selection, WorkerB),
			TEXT("D_ClickFriendlyMainBaseStillSelects"));

		DestroySpawned();
		UE_LOG(LogGPSelectionMarqueeContract, Log,
			TEXT("gp.Selection.RunMarqueeUnitsOnlyContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GSelectionMarqueeContract(
		TEXT("gp.Selection.RunMarqueeUnitsOnlyContractTest"),
		TEXT("Run marquee units-only selection contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunSelectionMarqueeContractTest));
}

#endif
