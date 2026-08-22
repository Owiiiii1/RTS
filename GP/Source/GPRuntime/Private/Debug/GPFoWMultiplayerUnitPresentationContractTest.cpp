// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Combat/GPCombatPresentationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/IConsoleManager.h"
#include "Presentation/GPHealthBarComponent.h"
#include "Presentation/GPLocalFoWUnitPresentationSubsystem.h"
#include "Units/GPWorker.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFoWMultiplayerUnitPresentationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPFoWMultiplayerUnitPresentationContractPrivate
{
	static FGP_FoWCellRange Range(int32 StartIndex, int32 NumCells)
	{
		FGP_FoWCellRange Result;
		Result.StartIndex = StartIndex;
		Result.NumCells = NumCells;
		return Result;
	}

	static FGP_FoWPresentationUpdate Initial(int32 TeamId, int64 Revision)
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

	static FVector CellLocation(int32 X, int32 Y)
	{
		return FVector(
			(static_cast<double>(X) + 0.5) * 100.0,
			(static_cast<double>(Y) + 0.5) * 100.0,
			200.0);
	}

	static void RunMultiplayerUnitPresentationContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPFoWMultiplayerUnitPresentationContract, Warning,
				TEXT("gp.FoW.RunMultiplayerUnitPresentationContractTest: missing authority test world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPFoWMultiplayerUnitPresentationContract, Log,
					TEXT("gp.FoW.RunMultiplayerUnitPresentationContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPFoWMultiplayerUnitPresentationContract, Error,
					TEXT("gp.FoW.RunMultiplayerUnitPresentationContractTest FAIL: %s"), Label);
			}
		};

		UGP_LocalFoWComponent* Team1VisibleMirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		UGP_LocalFoWComponent* Team1HiddenMirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		UGP_LocalFoWComponent* Team2Mirror =
			NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		FGP_FoWPresentationUpdate Team1VisibleInitial = Initial(1, 1);
		Team1VisibleInitial.VisibleRanges.Add(Range(1, 1));
		FGP_FoWPresentationUpdate Team1HiddenInitial = Initial(1, 1);
		FGP_FoWPresentationUpdate Team2Initial = Initial(2, 1);
		Expect(Team1VisibleMirror != nullptr && Team1HiddenMirror != nullptr && Team2Mirror != nullptr
			&& Team1VisibleMirror->ApplyServerUpdate(Team1VisibleInitial)
			&& Team1HiddenMirror->ApplyServerUpdate(Team1HiddenInitial)
			&& Team2Mirror->ApplyServerUpdate(Team2Initial)
			&& Team1VisibleMirror->IsVisible(CellLocation(1, 0))
			&& !Team1HiddenMirror->IsVisible(CellLocation(1, 0)),
			TEXT("A1_LocalMirrorsDivergeAtSameWorldCell"));

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		AGP_Worker* OwnUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), CellLocation(2, 0), FRotator::ZeroRotator, SpawnParams);
		AGP_Worker* EnemyUnit = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(), CellLocation(1, 0), FRotator::ZeroRotator, SpawnParams);
		if (OwnUnit != nullptr)
		{
			OwnUnit->SetTeamId(1);
		}
		if (EnemyUnit != nullptr)
		{
			EnemyUnit->SetTeamId(2);
		}
		Expect(IsValid(OwnUnit) && IsValid(EnemyUnit), TEXT("A2_SpawnedOwnAndEnemyUnits"));
		if (!IsValid(OwnUnit) || !IsValid(EnemyUnit))
		{
			UE_LOG(LogGPFoWMultiplayerUnitPresentationContract, Log,
				TEXT("gp.FoW.RunMultiplayerUnitPresentationContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		UStaticMeshComponent* AuthoredChildMesh = NewObject<UStaticMeshComponent>(EnemyUnit);
		AuthoredChildMesh->SetupAttachment(EnemyUnit->GetRootComponent());
		AuthoredChildMesh->RegisterComponent();
		AuthoredChildMesh->SetHiddenInGame(false, false);

		Expect(UGP_LocalFoWUnitPresentationSubsystem::ShouldPresentUnitForLocalPlayer(
				EnemyUnit, 1, Team1VisibleMirror)
			&& !UGP_LocalFoWUnitPresentationSubsystem::ShouldPresentUnitForLocalPlayer(
				EnemyUnit, 1, Team1HiddenMirror)
			&& UGP_LocalFoWUnitPresentationSubsystem::ShouldPresentUnitForLocalPlayer(
				EnemyUnit, 2, Team2Mirror),
			TEXT("A_ClientDecisionsDivergeForSameEnemy"));

		Expect(UGP_LocalFoWUnitPresentationSubsystem::ShouldPresentUnitForLocalPlayer(
				OwnUnit, 1, Team1HiddenMirror),
			TEXT("D_OwnTeamRemainsVisible"));

		const bool bEnemyCollisionBefore = EnemyUnit->GetActorEnableCollision();
		const bool bEnemyReplicatesBefore = EnemyUnit->GetIsReplicated();
		const bool bEnemyReplicateMovementBefore = EnemyUnit->IsReplicatingMovement();
		const int32 EnemyTeamBefore = EnemyUnit->GetTeamId();

		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			OwnUnit, 1, Team1HiddenMirror);
		Expect(OwnUnit->IsLocalFoWPresentationVisible()
			&& !OwnUnit->IsHidden()
			&& OwnUnit->GetLocalFoWGatedPrimitiveCount() == 0,
			TEXT("D2_OwnTeamApplyLeavesActorUnhidden"));

		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			EnemyUnit, 1, Team1VisibleMirror);
		Expect(EnemyUnit->IsLocalFoWPresentationVisible()
			&& !EnemyUnit->IsHidden()
			&& !AuthoredChildMesh->bHiddenInGame,
			TEXT("A3_Team1VisibleKeepsLocalVisualsShown"));

		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			EnemyUnit, 1, Team1HiddenMirror);
		Expect(!EnemyUnit->IsLocalFoWPresentationVisible()
			&& !EnemyUnit->IsHidden()
			&& EnemyUnit->GetLocalFoWGatedPrimitiveCount() > 0
			&& AuthoredChildMesh->bHiddenInGame
			&& UGP_LocalFoWUnitPresentationSubsystem::ShouldPresentUnitForLocalPlayer(
				EnemyUnit, 2, Team2Mirror),
			TEXT("B_ClientPresentationDoesNotSetActorHidden"));

		Expect(EnemyUnit->GetActorEnableCollision() == bEnemyCollisionBefore
			&& EnemyUnit->GetIsReplicated() == bEnemyReplicatesBefore
			&& EnemyUnit->IsReplicatingMovement() == bEnemyReplicateMovementBefore
			&& EnemyUnit->GetTeamId() == EnemyTeamBefore,
			TEXT("C_LocalVisualGateDoesNotChangeGameplayOrNetworkState"));

		UGP_HealthBarComponent* EnemyHealthBar = EnemyUnit->GetHealthBarComponent();
		UGP_CombatPresentationComponent* EnemyCombat = EnemyUnit->GetCombatPresentationComponent();
		if (UGP_AbilitySystemComponent* EnemyASC = EnemyUnit->GetGPAbilitySystemComponent())
		{
			EnemyASC->SetNumericAttributeBase(
				UGP_UnitAttributeSet::GetHealthAttribute(), 40.0f);
		}
		if (EnemyHealthBar != nullptr)
		{
			EnemyHealthBar->RefreshHealthBarFromAttributes();
		}
		Expect(EnemyHealthBar != nullptr
			&& EnemyCombat != nullptr
			&& EnemyHealthBar->DoesHealthPolicyAllowVisibility()
			&& !EnemyHealthBar->IsFoWPresentationAllowed()
			&& !EnemyHealthBar->IsComposedHealthBarVisible()
			&& !EnemyCombat->IsLocalPresentationAllowed(),
			TEXT("E_EnemyHealthBarAndCombatFollowLocalFoW"));

		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			EnemyUnit, 1, Team1VisibleMirror);
		Expect(EnemyUnit->IsLocalFoWPresentationVisible()
			&& !EnemyUnit->IsHidden()
			&& EnemyUnit->GetLocalFoWGatedPrimitiveCount() == 0
			&& !AuthoredChildMesh->bHiddenInGame
			&& EnemyHealthBar->IsFoWPresentationAllowed()
			&& EnemyCombat->IsLocalPresentationAllowed(),
			TEXT("F1_HiddenToVisibleRestoresLocalVisuals"));

		UGP_LocalFoWUnitPresentationSubsystem::ApplyUnitPresentationForLocalPlayer(
			EnemyUnit, 1, Team1HiddenMirror);
		Expect(!EnemyUnit->IsLocalFoWPresentationVisible()
			&& !EnemyUnit->IsHidden()
			&& AuthoredChildMesh->bHiddenInGame
			&& !EnemyHealthBar->IsFoWPresentationAllowed()
			&& !EnemyCombat->IsLocalPresentationAllowed(),
			TEXT("F_VisibleToHiddenIsStableWithoutActorHidden"));

		Expect(EnemyUnit->GetActorEnableCollision() == bEnemyCollisionBefore
			&& EnemyUnit->GetIsReplicated() == bEnemyReplicatesBefore
			&& EnemyUnit->IsReplicatingMovement() == bEnemyReplicateMovementBefore
			&& !EnemyUnit->IsHidden(),
			TEXT("G_NoCollisionReplicationOrActorHiddenChanges"));

		OwnUnit->Destroy();
		EnemyUnit->Destroy();

		UE_LOG(LogGPFoWMultiplayerUnitPresentationContract, Log,
			TEXT("gp.FoW.RunMultiplayerUnitPresentationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMultiplayerUnitPresentationContract(
		TEXT("gp.FoW.RunMultiplayerUnitPresentationContractTest"),
		TEXT("Run listen-server/multiplayer local FoW unit presentation regression."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMultiplayerUnitPresentationContractTest));
}

#endif
