// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttributeSets/GPPlayerAttributeSet.h"
#include "CommonUserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "UI/GPTEMP_S28P_PlanetaryFerroniteHUD.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPMatchViewModel.h"
#include "ViewModels/GPResourceViewModel.h"
#include "ViewModels/GPResourceViewModelAdapter.h"
#include "Widgets/GPHUDRootWidget.h"
#include "Widgets/GPUserWidgetBase.h"

#include <limits>

DEFINE_LOG_CATEGORY_STATIC(LogGPProductionHUDFoundationContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPProductionHUDFoundationContractPrivate
{
	static void RunProductionHUDFoundationContractTest(
		const TArray<FString>& Args,
		UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPProductionHUDFoundationContract, Warning,
				TEXT("gp.UI.RunProductionHUDFoundationContractTest: missing world"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPProductionHUDFoundationContract, Log,
					TEXT("gp.UI.RunProductionHUDFoundationContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPProductionHUDFoundationContract, Error,
					TEXT("gp.UI.RunProductionHUDFoundationContractTest FAIL: %s"), Label);
			}
		};

		UGP_ResourceViewModel* ResourceVM =
			NewObject<UGP_ResourceViewModel>(GetTransientPackage());
		UGP_MatchViewModel* MatchVM =
			NewObject<UGP_MatchViewModel>(GetTransientPackage());
		Expect(ResourceVM != nullptr && MatchVM != nullptr, TEXT("A_ViewModelsCreated"));

		if (ResourceVM != nullptr)
		{
			const auto& Descriptor = ResourceVM->GetFieldNotificationDescriptor();
			Expect(Descriptor.GetField(ResourceVM->GetClass(), TEXT("OrbitalFerronite")).IsValid()
				&& Descriptor.GetField(ResourceVM->GetClass(), TEXT("FerroniteScore")).IsValid()
				&& Descriptor.GetField(ResourceVM->GetClass(), TEXT("CurrentUnits")).IsValid()
				&& Descriptor.GetField(ResourceVM->GetClass(), TEXT("MaxUnits")).IsValid()
				&& Descriptor.GetField(ResourceVM->GetClass(), TEXT("OpponentFerroniteScore")).IsValid()
				&& Descriptor.GetField(ResourceVM->GetClass(), TEXT("PlanetFerronite")).IsValid(),
				TEXT("B_ResourceFieldsAreFieldNotify"));
			ResourceVM->SetOrbitalFerronite(125.0f);
			ResourceVM->SetFerroniteScore(450.0f);
			ResourceVM->SetCurrentUnits(7.0f);
			ResourceVM->SetMaxUnits(12.0f);
			ResourceVM->SetOpponentFerroniteScore(300.0f);
			ResourceVM->SetPlanetFerronite(250.0f);
			Expect(FMath::IsNearlyEqual(ResourceVM->OrbitalFerronite, 125.0f)
				&& FMath::IsNearlyEqual(ResourceVM->FerroniteScore, 450.0f)
				&& FMath::IsNearlyEqual(ResourceVM->CurrentUnits, 7.0f)
				&& FMath::IsNearlyEqual(ResourceVM->MaxUnits, 12.0f)
				&& FMath::IsNearlyEqual(ResourceVM->OpponentFerroniteScore, 300.0f)
				&& FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, 250.0f),
				TEXT("C_ResourceSettersMapValues"));
			ResourceVM->SetPlanetFerronite(std::numeric_limits<float>::quiet_NaN());
			Expect(FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, 0.0f),
				TEXT("C2_PlanetFerroniteNonFiniteClampsToZero"));
		}

		if (MatchVM != nullptr)
		{
			const auto& Descriptor = MatchVM->GetFieldNotificationDescriptor();
			Expect(Descriptor.GetField(MatchVM->GetClass(), TEXT("MatchTimeRemaining")).IsValid()
				&& Descriptor.GetField(MatchVM->GetClass(), TEXT("MatchStateTag")).IsValid()
				&& Descriptor.GetField(MatchVM->GetClass(), TEXT("FerroniteThreatValue")).IsValid()
				&& Descriptor.GetField(MatchVM->GetClass(), TEXT("FerroniteThreatNormalized")).IsValid()
				&& Descriptor.GetField(MatchVM->GetClass(), TEXT("WinnerTeamId")).IsValid()
				&& Descriptor.GetField(MatchVM->GetClass(), TEXT("WinReasonTag")).IsValid()
				&& Descriptor.GetField(MatchVM->GetClass(), TEXT("MatchDuration")).IsValid()
				&& Descriptor.GetField(MatchVM->GetClass(), TEXT("bMatchFinished")).IsValid(),
				TEXT("D_MatchFieldsAreFieldNotify"));
			MatchVM->SetMatchTimeRemaining(99.0f);
			MatchVM->SetFerroniteThreatValue(42.0f);
			MatchVM->SetFerroniteThreatNormalized(1.5f);
			MatchVM->SetWinnerTeamId(2);
			MatchVM->SetMatchDuration(301.0f);
			MatchVM->SetMatchFinished(true);
			Expect(FMath::IsNearlyEqual(MatchVM->MatchTimeRemaining, 99.0f)
				&& FMath::IsNearlyEqual(MatchVM->FerroniteThreatValue, 42.0f)
				&& FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, 1.0f)
				&& MatchVM->WinnerTeamId == 2
				&& FMath::IsNearlyEqual(MatchVM->MatchDuration, 301.0f)
				&& MatchVM->bMatchFinished,
				TEXT("E_MatchSettersMapValues"));
			MatchVM->SetFerroniteThreatNormalized(-0.25f);
			Expect(FMath::IsNearlyEqual(MatchVM->FerroniteThreatNormalized, 0.0f),
				TEXT("E2_ThreatNormalizedSetterClampsToUnitInterval"));
		}

		UGP_ResourceViewModelAdapter* Adapter =
			NewObject<UGP_ResourceViewModelAdapter>(GetTransientPackage());
		if (Adapter != nullptr && ResourceVM != nullptr)
		{
			ResourceVM->SetFerroniteScore(0.0f);
			ResourceVM->SetOpponentFerroniteScore(0.0f);
			Adapter->InitializeForContract(ResourceVM);
			Adapter->ApplyOwnAttributeForContract(
				UGP_PlayerAttributeSet::GetFerroniteScoreAttribute(), 777.0f);
			Adapter->ApplyOpponentScoreForContract(333.0f);
			Expect(FMath::IsNearlyEqual(ResourceVM->FerroniteScore, 777.0f)
				&& FMath::IsNearlyEqual(ResourceVM->OpponentFerroniteScore, 333.0f)
				&& FMath::IsNearlyEqual(ResourceVM->PlanetFerronite, 0.0f),
				TEXT("F_AdapterSeparatesOwnAndOpponentScore"));
			Adapter->Shutdown();
			Adapter->Shutdown();
			Expect(Adapter->GetBoundDelegateCount() == 0, TEXT("G_AdapterTeardownIsIdempotent"));
		}

		UGameInstance* GameInstance = World->GetGameInstance();
		ULocalPlayer* LocalPlayer =
			GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
		UGP_HUDViewModelSubsystem* Subsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
		UGP_HUDViewModelSubsystem* SameSubsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
		Expect(Subsystem != nullptr && Subsystem == SameSubsystem
			&& Subsystem->GetResourceViewModel() != nullptr
			&& Subsystem->GetMatchViewModel() != nullptr,
			TEXT("H_OneLocalSubsystemOwnsOneVMSet"));

		if (Subsystem != nullptr)
		{
			UGP_ResourceViewModel* OwnedResources = Subsystem->GetResourceViewModel();
			UGP_MatchViewModel* OwnedMatch = Subsystem->GetMatchViewModel();
			const int32 ResourceDelegateCount = Subsystem->GetResourceDelegateCount();
			const int32 MatchDelegateCount = Subsystem->GetMatchDelegateCount();
			Subsystem->PlayerControllerChanged(
				LocalPlayer != nullptr ? LocalPlayer->GetPlayerController(World) : nullptr);
			Expect(Subsystem->GetResourceViewModel() == OwnedResources
				&& Subsystem->GetMatchViewModel() == OwnedMatch
				&& Subsystem->GetResourceDelegateCount() == ResourceDelegateCount
				&& Subsystem->GetMatchDelegateCount() == MatchDelegateCount,
				TEXT("I_RebindKeepsVMIdentityAndNoDuplicateDelegates"));
		}

		Expect(UGP_UserWidgetBase::StaticClass()->IsChildOf(UCommonUserWidget::StaticClass())
			&& UGP_HUDRootWidget::StaticClass()->IsChildOf(UGP_UserWidgetBase::StaticClass()),
			TEXT("J_ProjectCommonUIWidgetBases"));
		Expect(UGP_TEMP_S28P_PlanetaryFerroniteHUD::StaticClass() != nullptr
			&& !UGP_HUDRootWidget::StaticClass()->IsChildOf(
				UGP_TEMP_S28P_PlanetaryFerroniteHUD::StaticClass()),
			TEXT("K_TEMP_HUDPreservedAndNotReplaced"));
		Expect(UGP_HUDRootWidget::StaticClass()->FindFunctionByName(TEXT("GetAbilitySystemComponent")) == nullptr
			&& UGP_UserWidgetBase::StaticClass()->FindFunctionByName(TEXT("GetPlayerState")) == nullptr,
			TEXT("L_WidgetBasesExposeNoGameplayQueryContract"));

		UE_LOG(LogGPProductionHUDFoundationContract, Log,
			TEXT("gp.UI.RunProductionHUDFoundationContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GProductionHUDFoundationContract(
		TEXT("gp.UI.RunProductionHUDFoundationContractTest"),
		TEXT("Run production HUD CommonUI/MVVM foundation contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunProductionHUDFoundationContractTest));
}

#endif
