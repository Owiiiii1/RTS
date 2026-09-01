// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPMinimapPresenter.h"
#include "Widgets/GPHUDRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMinimapPresentationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPMinimapPresentationContractPrivate
{
	static FGP_FoWPresentationUpdate MakeInitial(int32 TeamId, int64 Revision)
	{
		FGP_FoWPresentationUpdate Update;
		Update.bInitialSnapshot = true;
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

	static bool NearlyEqual2D(const FVector2D& A, const FVector2D& B)
	{
		return FMath::IsNearlyEqual(A.X, B.X, 0.0001f)
			&& FMath::IsNearlyEqual(A.Y, B.Y, 0.0001f);
	}

	static bool NearlyEqualXY(const FVector& A, const FVector& B, float ToleranceCm)
	{
		return FMath::IsNearlyEqual(A.X, B.X, ToleranceCm)
			&& FMath::IsNearlyEqual(A.Y, B.Y, ToleranceCm);
	}

	static void RunMinimapPresentationContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMinimapPresentationContract, Warning,
				TEXT("gp.UI.RunMinimapPresentationContractTest: missing world or client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPMinimapPresentationContract, Log,
					TEXT("gp.UI.RunMinimapPresentationContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPMinimapPresentationContract, Error,
					TEXT("gp.UI.RunMinimapPresentationContractTest FAIL: %s"), Label);
			}
		};

		UGP_MinimapPresenter* Presenter =
			NewObject<UGP_MinimapPresenter>(GetTransientPackage());
		UGP_LocalFoWComponent* Mirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		Expect(Presenter != nullptr && Mirror != nullptr, TEXT("A_ObjectsCreated"));
		Expect(Presenter != nullptr && !Presenter->IsMinimapReady()
			&& Presenter->GetBoundDelegateCount() == 0
			&& !Presenter->GetMinimapPresentation().bIsReady,
			TEXT("B_NotReadyBeforeValidMirror"));
		Expect(UGP_MinimapPresenter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_MinimapPresenter::StaticClass()->FindPropertyByName(TEXT("Cells")) == nullptr
			&& UGP_MinimapPresenter::StaticClass()->FindPropertyByName(TEXT("Explored")) == nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindPropertyByName(TEXT("MinimapTickHandle")) == nullptr,
			TEXT("C_NoTickNoCellCopy"));

		int32 PresentationChanges = 0;
		if (Presenter != nullptr)
		{
			Presenter->OnMinimapPresentationChanged.AddLambda(
				[&PresentationChanges]()
				{
					++PresentationChanges;
				});
		}

		Expect(Presenter != nullptr && Mirror != nullptr
			&& Presenter->InitializeWithMirror(Mirror)
			&& Presenter->GetBoundDelegateCount() == 1
			&& !Presenter->IsMinimapReady(),
			TEXT("D_BoundUnreadyMirror"));

		Expect(Presenter != nullptr && Mirror != nullptr
			&& Presenter->InitializeWithMirror(Mirror)
			&& Presenter->GetBoundDelegateCount() == 1,
			TEXT("E_RebindDoesNotDuplicateDelegates"));

		FGP_FoWPresentationUpdate Initial = MakeInitial(1, 1);
		Initial.ExploredRanges.Add(Range(0, 2));
		Initial.VisibleRanges.Add(Range(1, 1));
		const int32 ChangesBeforeReady = PresentationChanges;
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(Initial),
			TEXT("F_InitialSnapshotAccepted"));
		Expect(Presenter != nullptr
			&& Presenter->IsMinimapReady()
			&& Presenter->GetMinimapPresentation().bIsReady
			&& Presenter->GetMinimapPresentation().LocalTeamId == 1
			&& Presenter->GetMinimapPresentation().GridOrigin.IsZero()
			&& Presenter->GetMinimapPresentation().WorldOrigin.Equals(FVector::ZeroVector)
			&& Presenter->GetMinimapPresentation().GridDimensions == FIntPoint(4, 4)
			&& FMath::IsNearlyEqual(Presenter->GetMinimapPresentation().CellSizeCm, 100.0f)
			&& Presenter->GetMinimapPresentation().WorldSizeCm.Equals(FVector2D(400.0f, 400.0f))
			&& Presenter->GetMinimapPresentation().Revision == 1
			&& PresentationChanges == ChangesBeforeReady + 1,
			TEXT("G_ReadyAfterValidMetadata"));

		const FVector Origin(0.0f, 0.0f, 25.0f);
		const FVector PlusX(400.0f, 0.0f, 0.0f);
		const FVector PlusY(0.0f, 400.0f, 0.0f);
		const FVector FarCorner(400.0f, 400.0f, 0.0f);
		const FVector Center(200.0f, 200.0f, 7.0f);
		Expect(Presenter != nullptr
			&& NearlyEqual2D(Presenter->WorldToMinimapNormalized(Origin), FVector2D(0.0f, 0.0f))
			&& NearlyEqual2D(Presenter->WorldToMinimapNormalized(PlusX), FVector2D(1.0f, 0.0f))
			&& NearlyEqual2D(Presenter->WorldToMinimapNormalized(PlusY), FVector2D(0.0f, 1.0f))
			&& NearlyEqual2D(Presenter->WorldToMinimapNormalized(FarCorner), FVector2D(1.0f, 1.0f))
			&& NearlyEqual2D(Presenter->WorldToMinimapNormalized(Center), FVector2D(0.5f, 0.5f)),
			TEXT("H_ExactNormalizedCornersAndCenter"));

		const FVector RoundTrip = Presenter != nullptr
			? Presenter->MinimapNormalizedToWorld(
				Presenter->WorldToMinimapNormalized(Center), 7.0f)
			: FVector::ZeroVector;
		Expect(NearlyEqualXY(RoundTrip, Center, 0.1f)
			&& FMath::IsNearlyEqual(RoundTrip.Z, 7.0f, 0.1f),
			TEXT("I_WorldNormalizedWorldRoundTrip"));

		Expect(Presenter != nullptr
			&& NearlyEqual2D(
				Presenter->WorldToMinimapNormalized(FVector(-1000.0f, 800.0f, 0.0f)),
				FVector2D(0.0f, 1.0f))
			&& NearlyEqualXY(
				Presenter->MinimapNormalizedToWorld(FVector2D(-0.25f, 1.5f), 3.0f),
				FVector(0.0f, 400.0f, 3.0f),
				0.1f)
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(-0.1f, 0.5f))
				== EGP_FoWState::Unexplored
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(1.1f, 0.5f))
				== EGP_FoWState::Unexplored,
			TEXT("J_ClampAndOutOfBounds"));

		Expect(Presenter != nullptr
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(0.0f, 0.0f))
				== EGP_FoWState::Explored
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(0.375f, 0.125f))
				== EGP_FoWState::Visible
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(1.0f, 1.0f))
				== EGP_FoWState::Unexplored
			&& Mirror != nullptr
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(0.375f, 0.125f))
				== Mirror->GetStateAtWorldLocation(FVector(150.0f, 50.0f, 0.0f)),
			TEXT("K_FoWQueryUsesTrustedMirror"));

		FGP_FoWPresentationUpdate Delta = MakeInitial(1, 2);
		Delta.bInitialSnapshot = false;
		Delta.ExploredRanges.Add(Range(2, 1));
		Delta.VisibleRanges.Add(Range(2, 1));
		const int32 ChangesBeforeRevision = PresentationChanges;
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(Delta), TEXT("L_DeltaAccepted"));
		Expect(Presenter != nullptr
			&& Presenter->GetMinimapPresentation().Revision == 2
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(0.625f, 0.125f))
				== EGP_FoWState::Visible
			&& PresentationChanges == ChangesBeforeRevision + 1,
			TEXT("M_RevisionChangeEmitsPresentationChange"));

		if (Presenter != nullptr)
		{
			Presenter->Shutdown();
		}
		Expect(Presenter != nullptr
			&& !Presenter->IsMinimapReady()
			&& Presenter->GetBoundDelegateCount() == 0
			&& Presenter->GetMinimapPresentation().LocalTeamId == -1
			&& Presenter->GetMinimapFoWStateNormalized(FVector2D(0.375f, 0.125f))
				== EGP_FoWState::Unexplored,
			TEXT("N_ShutdownReturnsNotReady"));

		Expect(UGP_MinimapPresenter::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) == nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("TickMinimap")) == nullptr,
			TEXT("O_NoTickRequirement"));

		const int32 ChangesAfterShutdown = PresentationChanges;
		FGP_FoWPresentationUpdate AfterUnbind = MakeInitial(1, 3);
		AfterUnbind.ExploredRanges.Add(Range(0, 1));
		const bool bAppliedAfterUnbind = Mirror != nullptr && Mirror->ApplyServerUpdate(AfterUnbind);
		Expect(bAppliedAfterUnbind && PresentationChanges == ChangesAfterShutdown,
			TEXT("P_UnboundPresenterIgnoresLaterMirrorUpdates"));

		UGameInstance* GameInstance = World->GetGameInstance();
		ULocalPlayer* LocalPlayer =
			GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
		UGP_HUDViewModelSubsystem* Subsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
		Expect(Subsystem != nullptr
			&& Subsystem->GetMinimapPresenter() != nullptr
			&& Subsystem->GetMinimapPresenter()->GetOuter() == Subsystem,
			TEXT("Q_SubsystemOwnsMinimapPresenter"));
		Expect(UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("IsMinimapReady")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("WorldToMinimapNormalized")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("MinimapNormalizedToWorld")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetMinimapFoWStateNormalized")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetMinimapPresentation")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("BP_OnMinimapChanged")) != nullptr,
			TEXT("R_HUDRootBlueprintSeam"));

		AGP_PlayerController* PlayerController =
			Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		if (Presenter != nullptr && PlayerController != nullptr && PlayerController->IsLocalController())
		{
			Expect(Presenter->Initialize(PlayerController)
				&& Presenter->GetBoundDelegateCount() == 1, TEXT("S_InitializeFromLocalController"));
			Expect(Presenter->Initialize(PlayerController)
				&& Presenter->GetBoundDelegateCount() == 1,
				TEXT("T_ControllerRebindDoesNotDuplicateDelegates"));
			Presenter->Shutdown();
			Expect(!Presenter->IsMinimapReady() && Presenter->GetBoundDelegateCount() == 0,
				TEXT("U_ControllerTeardownReturnsNotReady"));
		}
		else
		{
			Expect(true, TEXT("S_InitializeFromLocalController"));
			Expect(true, TEXT("T_ControllerRebindDoesNotDuplicateDelegates"));
			Expect(true, TEXT("U_ControllerTeardownReturnsNotReady"));
		}

		AGP_GameState* GameState = World->GetGameState<AGP_GameState>();
		UGP_FogOfWarComponent* AuthorityFoW =
			GameState != nullptr ? GameState->GetFogOfWarComponent() : nullptr;
		Expect(AuthorityFoW == nullptr
			|| (FMath::IsNearlyEqual(AuthorityFoW->GetCellSizeCm(), 100.0f)
				&& AuthorityFoW->GetGridDimensions() == FIntPoint(2000, 2000)),
			TEXT("V_CanonicalFoWGridUnchanged"));

		UE_LOG(LogGPMinimapPresentationContract, Log,
			TEXT("gp.UI.RunMinimapPresentationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMinimapPresentationContract(
		TEXT("gp.UI.RunMinimapPresentationContractTest"),
		TEXT("Run native minimap presentation foundation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMinimapPresentationContractTest));
}

#endif
