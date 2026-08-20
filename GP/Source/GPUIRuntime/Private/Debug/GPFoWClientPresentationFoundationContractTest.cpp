// Copyright Epic Games, Inc. All Rights Reserved.

#include "FogOfWar/GPLocalFoWComponent.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "HAL/IConsoleManager.h"
#include "ViewModels/GPFoWViewModel.h"
#include "ViewModels/GPFoWViewModelAdapter.h"
#include "Widgets/GPActivatableWidgetBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFoWClientPresentationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPFoWClientPresentationContractPrivate
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

	static FVector CellLocation(int32 X, int32 Y)
	{
		return FVector((static_cast<float>(X) + 0.5f) * 100.0f,
			(static_cast<float>(Y) + 0.5f) * 100.0f,
			0.0f);
	}

	static void RunClientPresentationFoundationContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPFoWClientPresentationContract, Warning,
				TEXT("gp.FoW.RunClientPresentationFoundationContractTest: missing world or client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPFoWClientPresentationContract, Log,
					TEXT("gp.FoW.RunClientPresentationFoundationContractTest PASS: %s"),
					Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPFoWClientPresentationContract, Error,
					TEXT("gp.FoW.RunClientPresentationFoundationContractTest FAIL: %s"),
					Label);
			}
		};

		UGP_LocalFoWComponent* Team1Mirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		UGP_FoWViewModelAdapter* Adapter =
			NewObject<UGP_FoWViewModelAdapter>(GetTransientPackage());
		AGP_GameState* GameState = World->GetGameState<AGP_GameState>();
		UGP_FogOfWarComponent* AuthorityFoW =
			GameState != nullptr ? GameState->GetFogOfWarComponent() : nullptr;
		const EGP_FoWState AuthorityStateBefore = AuthorityFoW != nullptr
			? AuthorityFoW->GetStateForTeamAtWorldLocation(1, CellLocation(0, 0))
			: EGP_FoWState::Unexplored;
		Expect(Team1Mirror != nullptr && !Team1Mirror->IsReady(), TEXT("A_MirrorStartsNotReady"));
		Expect(Adapter != nullptr && Adapter->InitializeWithMirror(Team1Mirror),
			TEXT("B_AdapterBindsWithoutTick"));

		UGP_FoWViewModel* ViewModel = Adapter != nullptr ? Adapter->GetViewModel() : nullptr;
		Expect(ViewModel != nullptr && !ViewModel->bIsReady, TEXT("C_ViewModelStartsNotReady"));

		FGP_FoWPresentationUpdate Initial = MakeInitial(1, 1);
		Initial.ExploredRanges.Add(Range(0, 2));
		Initial.VisibleRanges.Add(Range(1, 1));
		Expect(Team1Mirror->ApplyServerUpdate(Initial), TEXT("D_InitialSnapshotAccepted"));
		Expect(Team1Mirror->IsReady()
			&& Team1Mirror->GetLocalTeamId() == 1
			&& Team1Mirror->GetRevision() == 1,
			TEXT("E_ReadyTeamRevision"));
		Expect(Team1Mirror->GetGridOriginWorldXY().IsZero()
			&& Team1Mirror->GetGridDimensions() == FIntPoint(4, 4)
			&& FMath::IsNearlyEqual(Team1Mirror->GetCellSizeCm(), 100.0f),
			TEXT("F_MetadataMatches"));
		Expect(Team1Mirror->GetStateAtWorldLocation(CellLocation(1, 0)) == EGP_FoWState::Visible,
			TEXT("G_VisibleArrives"));
		Expect(Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0)) == EGP_FoWState::Explored,
			TEXT("H_ExploredArrives"));
		Expect(Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0)) == EGP_FoWState::Unexplored,
			TEXT("I_DefaultUnexplored"));
		Expect(Team1Mirror->AllowsLocalPlacementPreview(CellLocation(1, 0))
			&& !Team1Mirror->AllowsLocalPlacementPreview(CellLocation(0, 0))
			&& !Team1Mirror->AllowsLocalPlacementPreview(CellLocation(2, 0)),
			TEXT("I2_LocalPlacementVisibleOnly"));
		Expect(AuthorityFoW == nullptr
			|| AuthorityFoW->GetStateForTeamAtWorldLocation(1, CellLocation(0, 0)) == AuthorityStateBefore,
			TEXT("I3_LocalMirrorCannotMutateAuthority"));
		Expect(AuthorityFoW == nullptr
			|| (FMath::IsNearlyEqual(AuthorityFoW->GetCellSizeCm(), 50.0f)
				&& AuthorityFoW->GetGridDimensions() == FIntPoint(4000, 4000)
				&& FMath::IsNearlyEqual(AuthorityFoW->GetUpdateIntervalSeconds(), 0.1f)),
			TEXT("I4_CanonicalGameplayGridIs50cmTenHz4000"));
		Expect(ViewModel != nullptr
			&& ViewModel->bIsReady
			&& ViewModel->LocalTeamId == 1
			&& ViewModel->GridDimensions == FIntPoint(4, 4)
			&& FMath::IsNearlyEqual(ViewModel->CellSizeCm, 100.0f)
			&& ViewModel->Revision == 1,
			TEXT("J_ViewModelInitialMetadata"));

		FGP_FoWPresentationUpdate Delta = MakeInitial(1, 2);
		Delta.bInitialSnapshot = false;
		Delta.ExploredRanges.Add(Range(2, 1));
		Delta.VisibleRanges.Add(Range(2, 1));
		Expect(Team1Mirror->ApplyServerUpdate(Delta), TEXT("K_DeltaAccepted"));
		Expect(Team1Mirror->GetStateAtWorldLocation(CellLocation(1, 0)) == EGP_FoWState::Explored,
			TEXT("L_VisibleRemovalDowngradesToExplored"));
		Expect(Team1Mirror->GetStateAtWorldLocation(CellLocation(2, 0)) == EGP_FoWState::Visible,
			TEXT("M_CurrentVisibleReplacement"));
		Expect(Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0)) == EGP_FoWState::Explored,
			TEXT("N_ExploredPersists"));
		Expect(Team1Mirror->AllowsLocalPlacementPreview(CellLocation(2, 0))
			&& !Team1Mirror->AllowsLocalPlacementPreview(CellLocation(1, 0)),
			TEXT("N2_LocalPlacementTracksCurrentVisible"));
		Expect(ViewModel != nullptr
			&& ViewModel->Revision == 2
			&& ViewModel->GetStateAtWorldLocation(CellLocation(2, 0)) == EGP_FoWState::Visible,
			TEXT("O_ViewModelReceivesMirrorUpdate"));

		Expect(!Team1Mirror->ApplyServerUpdate(Initial) && Team1Mirror->GetRevision() == 2,
			TEXT("P_StaleRevisionRejected"));
		Expect(!Team1Mirror->ApplyServerUpdate(Delta) && Team1Mirror->GetRevision() == 2,
			TEXT("Q_DuplicateRevisionRejected"));

		FGP_FoWPresentationUpdate Invalid = MakeInitial(1, 3);
		Invalid.bInitialSnapshot = false;
		Invalid.ExploredRanges.Add(Range(15, 2));
		Expect(!Team1Mirror->ApplyServerUpdate(Invalid) && Team1Mirror->GetRevision() == 2,
			TEXT("R_InvalidRangeRejectedAtomically"));

		UGP_LocalFoWComponent* Team2Mirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		FGP_FoWPresentationUpdate Team2Initial = MakeInitial(2, 7);
		Team2Initial.ExploredRanges.Add(Range(0, 1));
		Team2Initial.VisibleRanges.Add(Range(0, 1));
		Expect(Team2Mirror != nullptr && Team2Mirror->ApplyServerUpdate(Team2Initial),
			TEXT("S_SecondOwningTeamSnapshot"));
		Expect(Team1Mirror->GetLocalTeamId() == 1
			&& Team2Mirror->GetLocalTeamId() == 2
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0)) == EGP_FoWState::Explored
			&& Team2Mirror->GetStateAtWorldLocation(CellLocation(0, 0)) == EGP_FoWState::Visible,
			TEXT("T_PerClientTeamIsolationAtSameCoordinate"));

		Team1Mirror->ResetPresentation();
		Expect(!Team1Mirror->IsReady()
			&& Team1Mirror->GetLocalTeamId() == -1
			&& Team1Mirror->GetStateAtWorldLocation(CellLocation(0, 0)) == EGP_FoWState::Unexplored,
			TEXT("U_TeamResetClearsPreviousState"));
		Expect(Team1Mirror->ApplyServerUpdate(Team2Initial)
			&& Team1Mirror->GetLocalTeamId() == 2
			&& Team1Mirror->GetRevision() == 7,
			TEXT("V_ReconnectFullSnapshotReinitializes"));

		Expect(UGP_LocalFoWComponent::StaticClass()->FindFunctionByName(
				TEXT("GetStateForTeamAtWorldLocation")) == nullptr,
			TEXT("W_NoArbitraryTeamQuerySurface"));
		Expect(Team1Mirror != nullptr && !Team1Mirror->GetIsReplicated(),
			TEXT("X_NoRawMirrorReplication"));
		Expect(UGP_ActivatableWidgetBase::StaticClass()->IsChildOf(UCommonActivatableWidget::StaticClass()),
			TEXT("Y_CommonUIProjectBase"));
		Expect(ViewModel != nullptr && ViewModel->IsA<UMVVMViewModelBase>(),
			TEXT("Z_ProductionMVVMBase"));

		if (Adapter != nullptr)
		{
			Adapter->Shutdown();
		}

		UE_LOG(LogGPFoWClientPresentationContract, Log,
			TEXT("gp.FoW.RunClientPresentationFoundationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GClientPresentationContract(
		TEXT("gp.FoW.RunClientPresentationFoundationContractTest"),
		TEXT("Run trusted local FoW mirror and MVVM presentation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunClientPresentationFoundationContractTest));
}

#endif
