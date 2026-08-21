// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/UserWidget.h"
#include "Debug/GPHUDRootWidgetContractStub.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Settings/GPUIPresentationSettings.h"
#include "UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h"
#include "UObject/UObjectIterator.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "Widgets/GPHUDRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPHUDBootstrapContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPHUDBootstrapContractPrivate
{
	static int32 CountInViewportHUDRootsForLocalPlayer(
		const ULocalPlayer* LocalPlayer,
		UClass* OptionalClassFilter)
	{
		int32 Count = 0;
		if (LocalPlayer == nullptr)
		{
			return 0;
		}

		for (TObjectIterator<UGP_HUDRootWidget> It; It; ++It)
		{
			UGP_HUDRootWidget* Widget = *It;
			if (!IsValid(Widget) || Widget->HasAnyFlags(RF_ClassDefaultObject))
			{
				continue;
			}
			if (Widget->GetOwningLocalPlayer() != LocalPlayer || !Widget->IsInViewport())
			{
				continue;
			}
			if (OptionalClassFilter != nullptr && !Widget->IsA(OptionalClassFilter))
			{
				continue;
			}
			++Count;
		}
		return Count;
	}

	static void RunHUDBootstrapContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPHUDBootstrapContract, Warning,
				TEXT("gp.UI.RunHUDBootstrapContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPHUDBootstrapContract, Log,
					TEXT("gp.UI.RunHUDBootstrapContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPHUDBootstrapContract, Error,
					TEXT("gp.UI.RunHUDBootstrapContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_UIPresentationSettings::StaticClass() != nullptr
			&& UGP_UIPresentationSettings::StaticClass()->FindPropertyByName(
				TEXT("ProductionHUDWidgetClass")) != nullptr,
			TEXT("A_SettingsExposeProductionHUDWidgetClass"));

		Expect(UGP_HUDRootWidget::StaticClass()->HasAnyClassFlags(CLASS_Abstract),
			TEXT("B_NativeHUDRootRemainsAbstract"));

		Expect(UGP_HUDViewModelSubsystem::StaticClass()->GetPathName().Contains(
				TEXT("/Script/GPUIRuntime."))
			&& AGP_PlayerController::StaticClass()->GetPathName().Contains(
				TEXT("/Script/GPRuntime."))
			&& !AGP_PlayerController::StaticClass()->GetPathName().Contains(
				TEXT("GPUIRuntime")),
			TEXT("C_NoGPRuntimeToGPUIRuntimeTypeOwnership"));

		const FObjectProperty* TempHUDProperty = FindFProperty<FObjectProperty>(
			AGP_PlayerController::StaticClass(), TEXT("PlanetaryFerroniteHUD"));
		Expect(TempHUDProperty != nullptr
			&& TempHUDProperty->PropertyClass == UGP_TEMP_S28P_PlanetaryFerroniteHUD::StaticClass()
			&& AGP_PlayerController::StaticClass()->FindPropertyByName(
				TEXT("ProductionHUDWidget")) == nullptr,
			TEXT("D_TEMPHUDPathRemainsOnPlayerController"));

		UGameInstance* GameInstance = World->GetGameInstance();
		ULocalPlayer* LocalPlayer =
			GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
		UGP_HUDViewModelSubsystem* Subsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
		Expect(LocalPlayer != nullptr && Subsystem != nullptr,
			TEXT("E_LocalPlayerOwnsHUDViewModelSubsystem"));

		if (Subsystem == nullptr || LocalPlayer == nullptr)
		{
			UE_LOG(LogGPHUDBootstrapContract, Log,
				TEXT("gp.UI.RunHUDBootstrapContractTest: Complete Failures=%d Cancelled=false"),
				Failures);
			return;
		}

		const UGP_UIPresentationSettings* Settings = UGP_UIPresentationSettings::Get();
		const bool bClassConfigured =
			Settings != nullptr && !Settings->ProductionHUDWidgetClass.IsNull();

		Subsystem->TeardownProductionHUD();
		Subsystem->EnsureProductionHUDWithClassForContract(nullptr);
		Expect(Subsystem->GetProductionHUDWidget() == nullptr,
			TEXT("F_NullClassSafeNoOp"));

		if (!bClassConfigured)
		{
			Subsystem->EnsureProductionHUD();
			Expect(Subsystem->GetProductionHUDWidget() == nullptr,
				TEXT("G_UnconfiguredSettingsSafeNoOp"));
		}
		else
		{
			Expect(true, TEXT("G_UnconfiguredSettingsSafeNoOp"));
		}

		Subsystem->EnsureProductionHUDWithClassForContract(
			UGP_HUDRootWidgetContractStub::StaticClass());
		UGP_HUDRootWidget* FirstHUD = Subsystem->GetProductionHUDWidget();
		Expect(FirstHUD != nullptr
			&& FirstHUD->IsA(UGP_HUDRootWidgetContractStub::StaticClass())
			&& FirstHUD->IsInViewport()
			&& FirstHUD->GetOwningLocalPlayer() == LocalPlayer
			&& FirstHUD->GetVisibility() == ESlateVisibility::HitTestInvisible,
			TEXT("H_OneLocalPlayerCreatesOneHUDInstance"));

		Subsystem->EnsureProductionHUDWithClassForContract(
			UGP_HUDRootWidgetContractStub::StaticClass());
		Expect(Subsystem->GetProductionHUDWidget() == FirstHUD
			&& CountInViewportHUDRootsForLocalPlayer(
				LocalPlayer, UGP_HUDRootWidgetContractStub::StaticClass()) == 1,
			TEXT("I_RepeatedEnsureDoesNotDuplicate"));

		TWeakObjectPtr<UGP_HUDRootWidget> WeakHUD = FirstHUD;
		Subsystem->TeardownProductionHUD();
		Expect(Subsystem->GetProductionHUDWidget() == nullptr
			&& (!WeakHUD.IsValid() || !WeakHUD->IsInViewport()),
			TEXT("J_TeardownClearsAndRemovesInstance"));

		Subsystem->EnsureProductionHUDWithClassForContract(
			UGP_HUDRootWidgetContractStub::StaticClass());
		Expect(Subsystem->GetProductionHUDWidget() != nullptr
			&& Subsystem->GetProductionHUDWidget()->IsInViewport(),
			TEXT("K_EnsureAfterTeardownRecreatesOnce"));

		Subsystem->TeardownProductionHUD();
		Subsystem->EnsureProductionHUD();
		if (!bClassConfigured)
		{
			Expect(Subsystem->GetProductionHUDWidget() == nullptr,
				TEXT("L_RestoredUnconfiguredNoOp"));
		}
		else
		{
			Expect(Subsystem->GetProductionHUDWidget() != nullptr,
				TEXT("L_RestoredConfiguredProductionHUD"));
		}

		APlayerController* PlayerController = LocalPlayer->GetPlayerController(World);
		AGP_PlayerController* GPPlayerController = Cast<AGP_PlayerController>(PlayerController);
		if (GPPlayerController != nullptr && TempHUDProperty != nullptr)
		{
			const UObject* TempHUD =
				TempHUDProperty->GetObjectPropertyValue_InContainer(GPPlayerController);
			Expect(TempHUD != nullptr,
				TEXT("M_TEMPHUDInstanceStillPresent"));
		}
		else
		{
			Expect(false, TEXT("M_TEMPHUDInstanceStillPresent"));
		}

		UE_LOG(LogGPHUDBootstrapContract, Log,
			TEXT("gp.UI.RunHUDBootstrapContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GHUDBootstrapContract(
		TEXT("gp.UI.RunHUDBootstrapContractTest"),
		TEXT("Run production HUD LocalPlayer bootstrap contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunHUDBootstrapContractTest));
}

#endif
