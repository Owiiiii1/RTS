// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "FogOfWar/GPFogOfWarComponent.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/IConsoleManager.h"
#include "Settings/GPUIPresentationSettings.h"
#include "UObject/UObjectIterator.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPMinimapPresenter.h"
#include "Widgets/GPMinimapWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMinimapSurfaceContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPMinimapSurfaceContractPrivate
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

	static bool HasLoadSynchronousCall(const UFunction* Function)
	{
		return Function != nullptr && Function->GetName().Contains(TEXT("LoadSynchronous"));
	}

	static void RunMinimapSurfaceContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMinimapSurfaceContract, Warning,
				TEXT("gp.UI.RunMinimapSurfaceContractTest: missing world or client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPMinimapSurfaceContract, Log,
					TEXT("gp.UI.RunMinimapSurfaceContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPMinimapSurfaceContract, Error,
					TEXT("gp.UI.RunMinimapSurfaceContractTest FAIL: %s"), Label);
			}
		};

		Expect(UGP_UIPresentationSettings::StaticClass()->FindPropertyByName(
				TEXT("MinimapBackgroundTexture")) != nullptr
			&& UGP_UIPresentationSettings::StaticClass()->FindPropertyByName(
				TEXT("MinimapFoWPresentationResolution")) != nullptr,
			TEXT("A_SettingsOwnBackgroundAndResolution"));

		UGP_MinimapWidget* Widget = NewObject<UGP_MinimapWidget>(GetTransientPackage());
		Expect(Widget != nullptr, TEXT("B_WidgetCreated"));
		if (Widget != nullptr)
		{
			Widget->SynchronizeProperties();
		}

		Expect(Widget != nullptr
			&& Widget->IsUsingFallbackBackground()
			&& !Widget->HasResidentBackgroundTexture()
			&& Widget->GetBoundPresenterListenerCount() == 0
			&& Widget->GetFoWPresentationSampleCount() == Widget->GetFoWPresentationResolution()
				* Widget->GetFoWPresentationResolution()
			&& Widget->GetFoWPresentationResolution() == 128
			&& Widget->GetFoWPresentationSample(0, 0) == EGP_FoWState::Unexplored
			&& Widget->GetFoWPresentationSample(64, 64) == EGP_FoWState::Unexplored,
			TEXT("C_SafeWithoutPresenterUsesFallbackAndUnexplored"));

		Expect(UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("ReceiveTick")) == nullptr
			&& UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("NativeTick")) == nullptr,
			TEXT("D_NoTickRequired"));

		bool bFoundSyncLoadOnWidget = false;
		for (TFieldIterator<UFunction> It(UGP_MinimapWidget::StaticClass()); It; ++It)
		{
			bFoundSyncLoadOnWidget = bFoundSyncLoadOnWidget || HasLoadSynchronousCall(*It);
		}
		Expect(!bFoundSyncLoadOnWidget
			&& Widget != nullptr
			&& !Widget->DebugDidCallLoadSynchronous(),
			TEXT("E_NoSynchronousTextureLoadOnWidget"));

		if (Widget != nullptr)
		{
			Widget->ContractRequestBackgroundLoad();
			Widget->ContractRequestBackgroundLoadForPath(
				FSoftObjectPath(TEXT("/Game/GPMinimapSurfaceContract/T_Missing.T_Missing")));
		}
		Expect(Widget != nullptr
			&& !Widget->DebugDidCallLoadSynchronous()
			&& Widget->IsUsingFallbackBackground()
			&& Widget->DebugDidRequestAsyncBackgroundLoad(),
			TEXT("F_UnresolvedBackgroundPathRequestsAsyncAndKeepsFallback"));
		if (Widget != nullptr)
		{
			Widget->ContractRequestBackgroundLoad();
		}

		UGP_MinimapPresenter* Presenter = NewObject<UGP_MinimapPresenter>(GetTransientPackage());
		UGP_LocalFoWComponent* Mirror = NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		Expect(Presenter != nullptr && Mirror != nullptr && Presenter->InitializeWithMirror(Mirror),
			TEXT("G_PresenterMirrorBound"));

		FGP_FoWPresentationUpdate Initial = MakeInitial(1, 1);
		Initial.ExploredRanges.Add(Range(0, 2));
		Initial.VisibleRanges.Add(Range(1, 1));
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(Initial), TEXT("H_InitialFoWSnapshot"));

		if (Widget != nullptr)
		{
			Widget->ContractBindPresenter(Presenter);
			Widget->ContractBindPresenter(Presenter);
		}
		Expect(Widget != nullptr
			&& Widget->GetBoundPresenterListenerCount() == 1
			&& Presenter != nullptr
			&& Presenter->GetBoundDelegateCount() == 1,
			TEXT("I_RebindDoesNotDuplicateDelegates"));

		const int32 Resolution = Widget != nullptr ? Widget->GetFoWPresentationResolution() : 0;
		Expect(Widget != nullptr
			&& Resolution == 128
			&& Widget->GetFoWPresentationSampleCount() == 128 * 128
			&& Widget->GetFoWPresentationSampleCount() <= 256 * 256
			&& Widget->GetConsumedFoWRevision() == 1,
			TEXT("J_BoundedFoWResolutionAndRevision"));

		const FVector2D RoundTripPresenter(0.375f, 0.125f);
		const FVector2D SurfaceUV =
			UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(RoundTripPresenter);
		const FVector2D RoundTripBack =
			UGP_MinimapWidget::SurfaceUVToPresenterNormalized(SurfaceUV);
		const int32 MappedSurfaceX = FMath::Clamp(
			FMath::FloorToInt(SurfaceUV.X * static_cast<float>(Resolution)),
			0,
			Resolution - 1);
		const int32 MappedSurfaceY = FMath::Clamp(
			FMath::FloorToInt(SurfaceUV.Y * static_cast<float>(Resolution)),
			0,
			Resolution - 1);
		Expect(RoundTripBack.Equals(RoundTripPresenter, 0.0001f)
			&& Widget != nullptr
			&& Presenter != nullptr
			&& Widget->GetFoWPresentationSample(MappedSurfaceX, MappedSurfaceY)
				== EGP_FoWState::Visible
			&& Widget->GetFoWPresentationSample(MappedSurfaceX, MappedSurfaceY)
				== Presenter->GetMinimapFoWStateNormalized(RoundTripPresenter),
			TEXT("K_FoWUsesSharedSurfaceTransform"));

		Expect(UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(FVector2D(0.0f, 0.0f))
				.Equals(FVector2D(1.0f, 1.0f), 0.0001f)
			&& UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(FVector2D(1.0f, 1.0f))
				.Equals(FVector2D(0.0f, 0.0f), 0.0001f)
			&& UGP_MinimapWidget::SurfaceUVToPresenterNormalized(FVector2D(0.0f, 0.0f))
				.Equals(FVector2D(1.0f, 1.0f), 0.0001f),
			TEXT("M_ScreenXYAreOneMinusPresenterXY"));

		const FBox2D FallbackDest =
			Widget != nullptr
				? Widget->ContractComputeMapDestLocal(FVector2D(200.0f, 200.0f))
				: FBox2D();
		const FBox2D SharedLetterbox = UGP_MinimapWidget::ComputeSharedMapDestLocal(
			FVector2D(200.0f, 200.0f),
			true,
			FVector2D(256.0f, 128.0f));
		const FBox2D SharedFallback = UGP_MinimapWidget::ComputeSharedMapDestLocal(
			FVector2D(200.0f, 200.0f),
			false,
			FVector2D::ZeroVector);
		Expect(FallbackDest.bIsValid
			&& SharedFallback.bIsValid
			&& FMath::IsNearlyEqual(FallbackDest.GetSize().X, 200.0f)
			&& FMath::IsNearlyEqual(FallbackDest.GetSize().Y, 200.0f)
			&& FMath::IsNearlyEqual(SharedFallback.Min.X, FallbackDest.Min.X)
			&& FMath::IsNearlyEqual(SharedFallback.Min.Y, FallbackDest.Min.Y)
			&& FMath::IsNearlyEqual(SharedLetterbox.GetSize().X, 200.0f)
			&& FMath::IsNearlyEqual(SharedLetterbox.GetSize().Y, 100.0f)
			&& FMath::IsNearlyEqual(SharedLetterbox.Min.Y, 50.0f),
			TEXT("M_BackgroundAndFoWShareMapDestAndLetterbox"));

		FGP_FoWPresentationUpdate Delta = MakeInitial(1, 2);
		Delta.bInitialSnapshot = false;
		Delta.ExploredRanges.Add(Range(2, 1));
		Delta.VisibleRanges.Add(Range(2, 1));
		const int64 RevisionBefore = Widget != nullptr ? Widget->GetConsumedFoWRevision() : -1;
		Expect(Mirror != nullptr && Mirror->ApplyServerUpdate(Delta), TEXT("O_FoWRevisionDelta"));
		const FVector2D DeltaPresenter(0.625f, 0.125f);
		const FVector2D DeltaSurfaceUV =
			UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(DeltaPresenter);
		const int32 DeltaSurfaceX = FMath::Clamp(
			FMath::FloorToInt(DeltaSurfaceUV.X * static_cast<float>(Resolution)),
			0,
			Resolution - 1);
		const int32 DeltaSurfaceY = FMath::Clamp(
			FMath::FloorToInt(DeltaSurfaceUV.Y * static_cast<float>(Resolution)),
			0,
			Resolution - 1);
		Expect(Widget != nullptr
			&& Widget->GetConsumedFoWRevision() == 2
			&& Widget->GetConsumedFoWRevision() != RevisionBefore
			&& Widget->GetFoWPresentationSample(DeltaSurfaceX, DeltaSurfaceY)
				== EGP_FoWState::Visible
			&& Presenter != nullptr
			&& Presenter->GetMinimapFoWStateNormalized(DeltaPresenter) == EGP_FoWState::Visible,
			TEXT("P_RevisionRebuildsFoWOnce"));

		if (Widget != nullptr)
		{
			Widget->ContractUnbindPresenter();
		}
		Expect(Widget != nullptr
			&& Widget->GetBoundPresenterListenerCount() == 0
			&& Widget->GetFoWPresentationSample(0, Resolution - 1) == EGP_FoWState::Unexplored
			&& Widget->GetConsumedFoWRevision() == -1,
			TEXT("Q_TeardownSafeReturnsUnexplored"));

		if (Widget != nullptr)
		{
			Widget->SynchronizeProperties();
			Widget->SynchronizeProperties();
		}
		Expect(Widget != nullptr && Widget->GetBoundPresenterListenerCount() == 0,
			TEXT("R_RepeatedSynchronizeWithoutPlayerStaysUnbound"));

		UGameInstance* GameInstance = World->GetGameInstance();
		ULocalPlayer* LocalPlayer =
			GameInstance != nullptr ? GameInstance->GetFirstGamePlayer() : nullptr;
		UGP_HUDViewModelSubsystem* Subsystem =
			LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
		Expect(Subsystem != nullptr && Subsystem->GetMinimapPresenter() != nullptr,
			TEXT("S_SubsystemPresenterStillOwned"));

		Expect(UGP_MinimapWidget::StaticClass()->HasAnyClassFlags(CLASS_Abstract) == false
			&& !UGP_MinimapWidget::StaticClass()->HasAnyClassFlags(
				CLASS_Deprecated | CLASS_Hidden | CLASS_HideDropDown),
			TEXT("T_WidgetIsPlaceableInUMGDesigner"));
#if WITH_EDITOR
		{
			const FString PaletteCategory =
				Widget != nullptr ? Widget->GetPaletteCategory().ToString() : FString();
			const FString DisplayName =
				UGP_MinimapWidget::StaticClass()->GetMetaData(TEXT("DisplayName"));
			Expect(!PaletteCategory.IsEmpty()
				&& PaletteCategory == TEXT("GP")
				&& DisplayName == TEXT("GP Minimap"),
				TEXT("T2_UMGPaletteCategoryIsGP"));
		}
#endif

		UE_LOG(LogGPMinimapSurfaceContract, Log,
			TEXT("gp.UI.RunMinimapSurfaceContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMinimapSurfaceContract(
		TEXT("gp.UI.RunMinimapSurfaceContractTest"),
		TEXT("Run native minimap background+FoW surface contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMinimapSurfaceContractTest));
}

#endif
