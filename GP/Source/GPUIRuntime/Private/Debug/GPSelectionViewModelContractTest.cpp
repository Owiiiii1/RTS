// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystem/GPAbilitySystemComponent.h"
#include "AttributeSets/GPUnitAttributeSet.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Player/GPSelectionComponent.h"
#include "Resources/GPCargoComponent.h"
#include "Units/GPUnitBase.h"
#include "Units/GPUnitDefinition.h"
#include "Units/GPWorker.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPSelectionViewModel.h"
#include "ViewModels/GPSelectionViewModelAdapter.h"
#include "Widgets/GPHUDRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPSelectionViewModelContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPSelectionViewModelContractPrivate
{
	static AGP_Worker* SpawnContractWorker(UWorld* World, const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGP_Worker* Worker = World->SpawnActor<AGP_Worker>(
			AGP_Worker::StaticClass(),
			Location,
			FRotator::ZeroRotator,
			Params);
		if (Worker != nullptr)
		{
			Worker->SetTeamId(1);
		}
		return Worker;
	}

	static void NeutralizeAuthoredCombat(UWorld* World)
	{
		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (IsValid(Unit) && !Unit->IsA<AGP_Worker>())
			{
				Unit->SetTeamId(-1);
			}
		}
	}

	static bool SetUnitHealth(AGP_UnitBase* Unit, float Health)
	{
		UGP_AbilitySystemComponent* ASC = Unit != nullptr ? Unit->GetGPAbilitySystemComponent() : nullptr;
		if (ASC == nullptr)
		{
			return false;
		}
		ASC->SetNumericAttributeBase(UGP_UnitAttributeSet::GetHealthAttribute(), Health);
		return true;
	}

	static void RunSelectionViewModelContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPSelectionViewModelContract, Warning,
				TEXT("gp.UI.RunSelectionViewModelContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (bOk)
			{
				UE_LOG(LogGPSelectionViewModelContract, Log,
					TEXT("gp.UI.RunSelectionViewModelContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPSelectionViewModelContract, Error,
					TEXT("gp.UI.RunSelectionViewModelContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_SelectionViewModel::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_SelectionViewModelAdapter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_HUDViewModelSubsystem::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr,
			TEXT("A0_NoTickOnSelectionPresentationPath"));

		NeutralizeAuthoredCombat(World);
		FlushAsyncLoading();

		UGameInstance* GameInstance = World->GetGameInstance();
		ULocalPlayer* LocalPlayer =
			GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
		AGP_PlayerController* PlayerController =
			LocalPlayer != nullptr
				? Cast<AGP_PlayerController>(LocalPlayer->GetPlayerController(World))
				: Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		UGP_HUDViewModelSubsystem* Subsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
		UGP_SelectionComponent* Selection =
			PlayerController != nullptr ? PlayerController->GetSelectionComponent() : nullptr;
		UGP_SelectionViewModel* VM =
			Subsystem != nullptr ? Subsystem->GetSelectionViewModel() : nullptr;
		Expect(IsValid(PlayerController) && IsValid(Selection) && IsValid(Subsystem) && IsValid(VM)
			&& VM->GetOuter() == Subsystem,
			TEXT("A1_LocalSelectionAndSubsystemPresent"));
		if (!IsValid(Selection) || !IsValid(VM) || Subsystem == nullptr)
		{
			UE_LOG(LogGPSelectionViewModelContract, Log,
				TEXT("gp.UI.RunSelectionViewModelContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		Selection->ClearAllSelectionState();
		Expect(VM->Mode == EGP_SelectionPresentationMode::None && VM->SelectionCount == 0,
			TEXT("A_EmptySelectionIsNone"));

		AGP_Worker* UnitA = SpawnContractWorker(World, FVector(54000.0f, 54000.0f, 200.0f));
		AGP_Worker* UnitB = SpawnContractWorker(World, FVector(54100.0f, 54000.0f, 200.0f));
		FlushAsyncLoading();
		Expect(IsValid(UnitA) && IsValid(UnitB), TEXT("A_SpawnedWorkers"));
		if (!IsValid(UnitA) || !IsValid(UnitB))
		{
			UE_LOG(LogGPSelectionViewModelContract, Log,
				TEXT("gp.UI.RunSelectionViewModelContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		Selection->ReplaceSelectionWithUnit(UnitA);
		Expect(Selection->GetSelectionCount() == 1, TEXT("A_SelectionComponentCount1"));
		Expect(VM->Mode == EGP_SelectionPresentationMode::Single && VM->SelectionCount == 1,
			TEXT("A_SingleModeCount1"));
		if (const UGP_UnitDefinition* DefA = UnitA->ResolveLoadedUnitDefinition())
		{
			if (!DefA->DisplayName.IsEmpty())
			{
				Expect(VM->DisplayName.EqualTo(DefA->DisplayName), TEXT("A_DisplayNameFromDefinition"));
			}
			Expect(FMath::IsNearlyEqual(VM->Damage, DefA->Damage, 0.01f), TEXT("A_DamageFromDefinition"));
			Expect(FMath::IsNearlyEqual(VM->Armor, DefA->Armor, 0.01f), TEXT("A_ArmorFromDefinition"));
			Expect(FMath::IsNearlyEqual(VM->MoveSpeed, DefA->MoveSpeedCmPerSecond, 0.01f),
				TEXT("A_MoveSpeedFromDefinition"));
		}
		else
		{
			Expect(true, TEXT("A_DefinitionNotResident_SafeFallback"));
		}
		const UGP_UnitAttributeSet* AttrsA = UnitA->GetUnitAttributeSet();
		Expect(AttrsA != nullptr && FMath::IsNearlyEqual(VM->CurrentHealth, AttrsA->GetHealth(), 0.05f),
			TEXT("A_RuntimeHealthFromGAS"));
		Expect(VM->bIsUnit && !VM->bIsBuilding && !VM->bIsInspectPresentation,
			TEXT("A_SingleWorkerIdentity"));

		if (UGP_CargoComponent* CargoA = UnitA->GetCargoComponent())
		{
			const float CargoBefore = VM->CargoAmount;
			CargoA->AddCargo(12.0f);
			Expect(VM->bHasCargo
				&& VM->CargoAmount + KINDA_SMALL_NUMBER >= CargoBefore
				&& FMath::IsNearlyEqual(VM->CargoAmount, CargoA->GetCurrentCargoAmount(), 0.05f),
				TEXT("A_CargoPushUpdatesSingleVM"));
		}
		else
		{
			Expect(false, TEXT("A_WorkerCargoComponentPresent"));
		}

		Expect(SetUnitHealth(UnitA, 40.0f), TEXT("B_SetHealth"));
		Expect(FMath::IsNearlyEqual(VM->CurrentHealth, 40.0f, 0.05f),
			TEXT("B_HealthPushUpdatesVMWithoutPolling"));
		Expect(VM->MaxHealth > KINDA_SMALL_NUMBER
			&& FMath::IsNearlyEqual(VM->HealthNormalized, 40.0f / VM->MaxHealth, 0.02f),
			TEXT("B_HealthNormalizedFromRuntimeVitals"));

		Selection->AddUnitToSelection(UnitB);
		Expect(VM->Mode == EGP_SelectionPresentationMode::Group && VM->SelectionCount == 2
			&& VM->GetGroupRows().Num() == 2
			&& VM->GetGroupRows()[0].Index == 0
			&& VM->GetGroupRows()[1].Index == 1,
			TEXT("C_GroupModeStableOrder"));
		Expect(FMath::IsNearlyEqual(VM->GetGroupRows()[0].CurrentHealth, 40.0f, 0.05f),
			TEXT("C_GroupRow0KeepsDamagedHealth"));
		const float Row0Before = VM->GetGroupRows()[0].CurrentHealth;

		Expect(SetUnitHealth(UnitB, 55.0f), TEXT("D_SetGroupHealth"));
		Expect(FMath::IsNearlyEqual(VM->GetGroupRows()[1].CurrentHealth, 55.0f, 0.05f)
			&& FMath::IsNearlyEqual(VM->GetGroupRows()[0].CurrentHealth, Row0Before, 0.05f),
			TEXT("D_OnlyDamagedGroupRowChanges"));

		const int32 BoundWhileGrouped = Subsystem->GetSelectionDelegateCount();
		Selection->ClearSelection();
		Expect(VM->Mode == EGP_SelectionPresentationMode::None && VM->SelectionCount == 0
			&& VM->GetGroupRows().Num() == 0
			&& Subsystem->GetSelectionDelegateCount() < BoundWhileGrouped,
			TEXT("E_ClearReturnsNoneAndDropsUnitBindings"));
		Expect(SetUnitHealth(UnitA, 25.0f), TEXT("E_StaleHealthMutation"));
		Expect(VM->Mode == EGP_SelectionPresentationMode::None
			&& FMath::IsNearlyEqual(VM->CurrentHealth, 0.0f),
			TEXT("E_StaleActorHealthDoesNotMutateVM"));

		Selection->ReplaceSelectionWithUnit(UnitA);
		Expect(VM->Mode == EGP_SelectionPresentationMode::Single, TEXT("F_SelectA"));
		Selection->ReplaceSelectionWithUnit(UnitB);
		Expect(VM->Mode == EGP_SelectionPresentationMode::Single && VM->SelectionCount == 1,
			TEXT("F_SelectBReplacesA"));
		const float PresentedHealth = VM->CurrentHealth;
		Expect(SetUnitHealth(UnitA, 11.0f), TEXT("F_MutateUnselectedA"));
		Expect(FMath::IsNearlyEqual(VM->CurrentHealth, PresentedHealth, 0.05f)
			&& !FMath::IsNearlyEqual(VM->CurrentHealth, 11.0f),
			TEXT("F_UnselectedANoLongerOwnsPresentation"));

		Selection->ClearSelection();
		Selection->SetInspectedTarget(UnitA);
		Expect(VM->Mode == EGP_SelectionPresentationMode::Single
			&& VM->bIsInspectPresentation
			&& VM->SelectionCount == 1,
			TEXT("G_InspectFallbackSingle"));
		Selection->ReplaceSelectionWithUnit(UnitB);
		Expect(VM->Mode == EGP_SelectionPresentationMode::Single
			&& !VM->bIsInspectPresentation
			&& VM->SelectionCount == 1,
			TEXT("G_SelectionOverridesInspect"));

		Expect(UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetSelectionGroupRows")) != nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("BP_OnSelectionPresentationChanged")) != nullptr,
			TEXT("H_HUDRootGroupPresenterSeamExists"));

		Selection->ClearAllSelectionState();
		if (IsValid(UnitA))
		{
			UnitA->Destroy();
		}
		if (IsValid(UnitB))
		{
			UnitB->Destroy();
		}

		UE_LOG(LogGPSelectionViewModelContract, Log,
			TEXT("gp.UI.RunSelectionViewModelContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GSelectionViewModelContract(
		TEXT("gp.UI.RunSelectionViewModelContractTest"),
		TEXT("Run production HUD Selection ViewModel push-adapter contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunSelectionViewModelContractTest));
}

#endif
