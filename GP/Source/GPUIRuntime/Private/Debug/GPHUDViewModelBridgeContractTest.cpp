// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "View/MVVMView.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPMatchViewModel.h"
#include "ViewModels/GPResourceViewModel.h"
#include "Widgets/GPFoWWorldOverlayWidget.h"
#include "Widgets/GPHUDRootWidget.h"
#include "Widgets/GPHUDViewModelBridge.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPHUDViewModelBridgeContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPHUDViewModelBridgeContractPrivate
{
	static void RunHUDViewModelBridgeContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPHUDViewModelBridgeContract, Warning,
				TEXT("gp.UI.RunHUDViewModelBridgeContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPHUDViewModelBridgeContract, Log,
					TEXT("gp.UI.RunHUDViewModelBridgeContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPHUDViewModelBridgeContract, Error,
					TEXT("gp.UI.RunHUDViewModelBridgeContractTest FAIL: %s"), Label);
			}
		};

		Expect(FGP_HUDViewModelBridge::ResourceViewModelSlotName == FName(TEXT("GP_ResourceViewModel"))
			&& FGP_HUDViewModelBridge::MatchViewModelSlotName == FName(TEXT("GP_MatchViewModel")),
			TEXT("A_SlotNamesMatchAuthoredManualEntries"));

		Expect(UGP_HUDRootWidget::StaticClass()->FindPropertyByName(TEXT("ResourceViewModel")) == nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindPropertyByName(TEXT("MatchViewModel")) == nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetAbilitySystemComponent")) == nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetPlayerState")) == nullptr,
			TEXT("B_HUDRootDoesNotOwnOrQueryGameplayVMs"));

		UGameInstance* GameInstance = World->GetGameInstance();
		ULocalPlayer* LocalPlayer =
			GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
		UGP_HUDViewModelSubsystem* Subsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
		Expect(Subsystem != nullptr
			&& Subsystem->GetResourceViewModel() != nullptr
			&& Subsystem->GetMatchViewModel() != nullptr
			&& Subsystem->GetResourceViewModel()->GetOuter() == Subsystem
			&& Subsystem->GetMatchViewModel()->GetOuter() == Subsystem,
			TEXT("C_SubsystemOwnsResourceAndMatchVMs"));

		UGP_ResourceViewModel* OwnedResource =
			Subsystem != nullptr ? Subsystem->GetResourceViewModel() : nullptr;
		UGP_MatchViewModel* OwnedMatch =
			Subsystem != nullptr ? Subsystem->GetMatchViewModel() : nullptr;

		const FGP_HUDViewModelBridgeResult MissingWidget =
			FGP_HUDViewModelBridge::AssignOwnedViewModels(nullptr, Subsystem);
		Expect(!MissingWidget.bHadView
			&& MissingWidget.bHadSubsystem
			&& !MissingWidget.bResourceAssigned
			&& !MissingWidget.bMatchAssigned
			&& Subsystem != nullptr
			&& Subsystem->GetResourceViewModel() == OwnedResource
			&& Subsystem->GetMatchViewModel() == OwnedMatch,
			TEXT("D_MissingWidgetFailsSafelyWithoutCreatingVMs"));

		const FGP_HUDViewModelBridgeResult MissingSubsystem =
			FGP_HUDViewModelBridge::AssignOwnedViewModels(nullptr, nullptr);
		Expect(!MissingSubsystem.bHadView
			&& !MissingSubsystem.bHadSubsystem
			&& !MissingSubsystem.bResourceAssigned
			&& !MissingSubsystem.bMatchAssigned,
			TEXT("E_MissingSubsystemFailsSafely"));

		UGP_FoWWorldOverlayWidget* BareWidget = NewObject<UGP_FoWWorldOverlayWidget>(GetTransientPackage());
		const FGP_HUDViewModelBridgeResult MissingView =
			FGP_HUDViewModelBridge::AssignOwnedViewModels(BareWidget, Subsystem);
		Expect(!MissingView.bHadView
			&& MissingView.bHadSubsystem
			&& !MissingView.bResourceAssigned
			&& !MissingView.bMatchAssigned
			&& Subsystem != nullptr
			&& Subsystem->GetResourceViewModel() == OwnedResource
			&& Subsystem->GetMatchViewModel() == OwnedMatch
			&& OwnedResource != nullptr
			&& OwnedResource->GetOuter() == Subsystem,
			TEXT("F_MissingMVVMViewFailsSafelyWithoutCreatingVMs"));

		UMVVMView* UnconstructedView = NewObject<UMVVMView>(BareWidget);
		const FGP_HUDViewModelBridgeResult MissingSlots =
			FGP_HUDViewModelBridge::AssignOwnedViewModelsToView(UnconstructedView, Subsystem);
		Expect(MissingSlots.bHadView
			&& MissingSlots.bHadSubsystem
			&& !MissingSlots.bResourceAssigned
			&& !MissingSlots.bMatchAssigned
			&& Subsystem != nullptr
			&& Subsystem->GetResourceViewModel() == OwnedResource
			&& Subsystem->GetMatchViewModel() == OwnedMatch
			&& OwnedMatch != nullptr
			&& OwnedMatch->GetOuter() == Subsystem,
			TEXT("G_UnconstructedViewSetViewModelFailsSafelyUsingExistingInstances"));

		const FGP_HUDViewModelBridgeResult MissingViewPtr =
			FGP_HUDViewModelBridge::AssignOwnedViewModelsToView(nullptr, Subsystem);
		Expect(!MissingViewPtr.bHadView
			&& !MissingViewPtr.bResourceAssigned
			&& Subsystem != nullptr
			&& Subsystem->GetResourceViewModel() == OwnedResource,
			TEXT("H_NullViewHelperFailsSafely"));

		Expect(UGP_HUDRootWidget::StaticClass()->FindPropertyByName(TEXT("BridgeRetryTimer")) == nullptr
			&& UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetAbilitySystemComponent")) == nullptr,
			TEXT("I_NoRetryTimerAndNoGameplayQueryOnHUDRoot"));

		UE_LOG(LogGPHUDViewModelBridgeContract, Log,
			TEXT("gp.UI.RunHUDViewModelBridgeContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
		UE_LOG(LogGPHUDViewModelBridgeContract, Log,
			TEXT("gp.UI.RunHUDViewModelBridgeContractTest NOTE: full authored Manual-slot assignment requires WBP_GP_HUD; this contract covers fail-safe helper + ownership. Operator validates live WBP injection."));
	}

	static FAutoConsoleCommandWithWorldAndArgs GHUDViewModelBridgeContract(
		TEXT("gp.UI.RunHUDViewModelBridgeContractTest"),
		TEXT("Run production HUD ViewModel MVVM bridge contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunHUDViewModelBridgeContractTest));
}

#endif
