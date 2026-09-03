// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/GPCameraPawn.h"
#include "Engine/World.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Math/Box2D.h"
#include "Player/GPPlayerController.h"
#include "Player/GPSelectionComponent.h"
#include "ViewModels/GPMinimapPresenter.h"
#include "Widgets/GPMinimapWidget.h"

#include <limits>

DEFINE_LOG_CATEGORY_STATIC(LogGPMinimapClickToPanContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPMinimapClickToPanContractPrivate
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

	static void RunMinimapClickToPanContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMinimapClickToPanContract, Warning,
				TEXT("gp.UI.RunMinimapClickToPanContractTest: missing world or client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPMinimapClickToPanContract, Log,
					TEXT("gp.UI.RunMinimapClickToPanContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPMinimapClickToPanContract, Error,
					TEXT("gp.UI.RunMinimapClickToPanContractTest FAIL: %s"), Label);
			}
		};

		AGP_PlayerController* PlayerController =
			Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		AGP_CameraPawn* CameraPawn =
			PlayerController != nullptr ? Cast<AGP_CameraPawn>(PlayerController->GetPawn()) : nullptr;
		UGP_SelectionComponent* Selection =
			PlayerController != nullptr ? PlayerController->GetSelectionComponent() : nullptr;
		const int32 SelectedBefore =
			Selection != nullptr ? Selection->GetSelectedUnits().Num() : 0;

		Expect(UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_MinimapPresenter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr,
			TEXT("E_NoWidgetOrPresenterTick"));

		const FBox2D SquareDest(FVector2D(0.0f, 0.0f), FVector2D(100.0f, 100.0f));
		FVector2D CenterUV = FVector2D::ZeroVector;
		Expect(UGP_MinimapWidget::TryConvertLocalPointToSurfaceUV(
				FVector2D(50.0f, 50.0f), SquareDest, CenterUV)
			&& CenterUV.Equals(FVector2D(0.5f, 0.5f), 0.0001f)
			&& UGP_MinimapWidget::SurfaceUVToPresenterNormalized(CenterUV)
				.Equals(FVector2D(0.5f, 0.5f), 0.0001f),
			TEXT("A_CenterMapsToHalfHalf"));

		FVector2D VisualLeftUV = FVector2D::ZeroVector;
		FVector2D VisualRightUV = FVector2D::ZeroVector;
		FVector2D VisualTopUV = FVector2D::ZeroVector;
		FVector2D VisualBottomUV = FVector2D::ZeroVector;
		const bool bLeft = UGP_MinimapWidget::TryConvertLocalPointToSurfaceUV(
			FVector2D(0.0f, 50.0f), SquareDest, VisualLeftUV);
		const bool bRight = UGP_MinimapWidget::TryConvertLocalPointToSurfaceUV(
			FVector2D(100.0f, 50.0f), SquareDest, VisualRightUV);
		const bool bTop = UGP_MinimapWidget::TryConvertLocalPointToSurfaceUV(
			FVector2D(50.0f, 0.0f), SquareDest, VisualTopUV);
		const bool bBottom = UGP_MinimapWidget::TryConvertLocalPointToSurfaceUV(
			FVector2D(50.0f, 100.0f), SquareDest, VisualBottomUV);
		const FVector2D PresenterFromVisualLeft =
			UGP_MinimapWidget::SurfaceUVToPresenterNormalized(VisualLeftUV);
		const FVector2D PresenterFromVisualRight =
			UGP_MinimapWidget::SurfaceUVToPresenterNormalized(VisualRightUV);
		const FVector2D PresenterFromVisualTop =
			UGP_MinimapWidget::SurfaceUVToPresenterNormalized(VisualTopUV);
		const FVector2D PresenterFromVisualBottom =
			UGP_MinimapWidget::SurfaceUVToPresenterNormalized(VisualBottomUV);
		Expect(bLeft && bRight && bTop && bBottom
			&& PresenterFromVisualLeft.X > PresenterFromVisualRight.X
			&& PresenterFromVisualTop.Y > PresenterFromVisualBottom.Y,
			TEXT("A_SharedInverseMapsLeftRightTopBottom"));

		const FBox2D LetterboxedDest(FVector2D(10.0f, 20.0f), FVector2D(90.0f, 80.0f));
		FVector2D InsideUV = FVector2D::ZeroVector;
		FVector2D OutsideUV = FVector2D(9.0f, 9.0f);
		const EGP_MinimapPointerResult OutsideResult = UGP_MinimapWidget::ResolvePointerOnMap(
			EKeys::LeftMouseButton, FVector2D(5.0f, 5.0f), LetterboxedDest, OutsideUV);
		const EGP_MinimapPointerResult InsideResult = UGP_MinimapWidget::ResolvePointerOnMap(
			EKeys::LeftMouseButton, FVector2D(50.0f, 50.0f), LetterboxedDest, InsideUV);
		const EGP_MinimapPointerResult RightClickResult = UGP_MinimapWidget::ResolvePointerOnMap(
			EKeys::RightMouseButton, FVector2D(50.0f, 50.0f), LetterboxedDest, InsideUV);
		Expect(OutsideResult == EGP_MinimapPointerResult::ConsumedNoPan
			&& InsideResult == EGP_MinimapPointerResult::PanRequested
			&& RightClickResult == EGP_MinimapPointerResult::Ignored,
			TEXT("B_LetterboxRejectedInsideAcceptedRmbIgnored"));

		const FBox2D FallbackSquare = UGP_MinimapWidget::ComputeSharedMapDestLocal(
			FVector2D(128.0f, 128.0f), false, FVector2D::ZeroVector);
		FVector2D FallbackUV = FVector2D::ZeroVector;
		Expect(FallbackSquare.GetSize().Equals(FVector2D(128.0f, 128.0f), 0.01f)
			&& UGP_MinimapWidget::TryConvertLocalPointToSurfaceUV(
				FallbackSquare.GetCenter(), FallbackSquare, FallbackUV),
			TEXT("B_FallbackSquareMapDestAcceptsCenter"));

		const FBox2D TexturedDest = UGP_MinimapWidget::ComputeSharedMapDestLocal(
			FVector2D(100.0f, 100.0f), true, FVector2D(200.0f, 100.0f));
		FVector2D LetterboxPointUV = FVector2D::ZeroVector;
		Expect(TexturedDest.GetSize().X > TexturedDest.GetSize().Y
			&& !UGP_MinimapWidget::TryConvertLocalPointToSurfaceUV(
				FVector2D(50.0f, 5.0f), TexturedDest, LetterboxPointUV)
			&& UGP_MinimapWidget::TryConvertLocalPointToSurfaceUV(
				TexturedDest.GetCenter(), TexturedDest, LetterboxPointUV),
			TEXT("B_TexturedLetterboxUsesSameMapDest"));

		UGP_MinimapPresenter* Presenter = NewObject<UGP_MinimapPresenter>(GetTransientPackage());
		UGP_MinimapWidget* Widget = NewObject<UGP_MinimapWidget>(GetTransientPackage());
		UGP_LocalFoWComponent* Mirror = NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		Expect(Presenter != nullptr && Widget != nullptr && Mirror != nullptr, TEXT("D_ObjectsCreated"));
		Expect(PlayerController != nullptr && CameraPawn != nullptr, TEXT("C_LocalCameraPawnPresent"));

		const FVector OriginalLocation =
			CameraPawn != nullptr ? CameraPawn->GetActorLocation() : FVector::ZeroVector;
		const FRotator OriginalRotation =
			CameraPawn != nullptr ? CameraPawn->GetActorRotation() : FRotator::ZeroRotator;
		FVector OriginalViewLocation = FVector::ZeroVector;
		FRotator OriginalViewRotation = FRotator::ZeroRotator;
		float OriginalFOV = 0.0f;
		const bool bHasView = CameraPawn != nullptr
			&& CameraPawn->GetPresentationView(OriginalViewLocation, OriginalViewRotation, OriginalFOV);
		const FVector OriginalViewOffset = OriginalViewLocation - OriginalLocation;

		Expect(Presenter != nullptr && !Presenter->PanCameraToMinimapNormalized(FVector2D(0.5f, 0.5f)),
			TEXT("D_NotReadyNoMove"));

		if (Presenter != nullptr && Mirror != nullptr)
		{
			Presenter->InitializeWithMirror(Mirror);
			Mirror->ApplyServerUpdate(MakeInitial(1, 1));
		}
		Expect(Presenter != nullptr
			&& Presenter->IsMinimapReady()
			&& !Presenter->PanCameraToMinimapNormalized(FVector2D(0.5f, 0.5f)),
			TEXT("D_NoCameraNoMove"));

		if (Presenter != nullptr && CameraPawn != nullptr)
		{
			FBox ResolvedBounds(ForceInit);
			if (CameraPawn->GetResolvedCameraBounds(ResolvedBounds))
			{
				Presenter->ContractBindCameraPawn(CameraPawn);
				Presenter->ContractApplyDisplayedWorldBounds(ResolvedBounds);
			}
		}
		if (Widget != nullptr && Presenter != nullptr)
		{
			Widget->TakeWidget();
			Widget->ContractBindPresenter(Presenter);
		}

		FVector2D WidgetSurfaceUV = FVector2D::ZeroVector;
		const EGP_MinimapPointerResult WidgetPointer =
			Widget != nullptr
				? Widget->ContractResolvePointerOnMap(
					EKeys::LeftMouseButton,
					FVector2D(64.0f, 64.0f),
					FVector2D(128.0f, 128.0f),
					WidgetSurfaceUV)
				: EGP_MinimapPointerResult::Ignored;
		Expect(WidgetPointer == EGP_MinimapPointerResult::PanRequested
			&& WidgetSurfaceUV.Equals(FVector2D(0.5f, 0.5f), 0.05f),
			TEXT("A_WidgetMapDestCenter"));

		const int32 FootprintRevisionBefore =
			Presenter != nullptr ? Presenter->GetCameraFootprint().Revision : 0;
		const FVector2D ClickNormalized(0.25f, 0.75f);
		const FVector2D ClickSurfaceUV =
			UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(ClickNormalized);
		const bool bPanned =
			Presenter != nullptr && Presenter->PanCameraToMinimapNormalized(ClickNormalized);
		if (Widget != nullptr)
		{
			Widget->ContractHandleMapLeftClick(ClickSurfaceUV);
		}
		const FVector AfterPan =
			CameraPawn != nullptr ? CameraPawn->GetActorLocation() : FVector::ZeroVector;
		const FRotator AfterRotation =
			CameraPawn != nullptr ? CameraPawn->GetActorRotation() : FRotator::ZeroRotator;
		FVector AfterViewLocation = FVector::ZeroVector;
		FRotator AfterViewRotation = FRotator::ZeroRotator;
		float AfterFOV = 0.0f;
		if (CameraPawn != nullptr)
		{
			CameraPawn->GetPresentationView(AfterViewLocation, AfterViewRotation, AfterFOV);
		}
		const FVector AfterViewOffset = AfterViewLocation - AfterPan;
		const FVector ExpectedWorld = Presenter != nullptr
			? Presenter->MinimapNormalizedToWorld(ClickNormalized, OriginalLocation.Z)
			: FVector::ZeroVector;
		FBox CameraBounds(ForceInit);
		const bool bHasBounds =
			CameraPawn != nullptr && CameraPawn->GetResolvedCameraBounds(CameraBounds);
		UE_LOG(LogGPMinimapClickToPanContract, Log,
			TEXT("C_Click debug Ready=%d Panned=%d After=(%.2f,%.2f,%.2f) Expected=(%.2f,%.2f,%.2f) Yaw %.3f->%.3f FOV %.3f->%.3f OffsetSz %.2f->%.2f"),
			Presenter != nullptr && Presenter->IsMinimapReady() ? 1 : 0,
			bPanned ? 1 : 0,
			AfterPan.X, AfterPan.Y, AfterPan.Z,
			ExpectedWorld.X, ExpectedWorld.Y, ExpectedWorld.Z,
			OriginalRotation.Yaw, AfterRotation.Yaw,
			OriginalFOV, AfterFOV,
			OriginalViewOffset.Size(), AfterViewOffset.Size());
		Expect(Presenter != nullptr && Presenter->IsMinimapReady(), TEXT("C_PresenterReadyForPan"));
		Expect(bPanned
			&& CameraPawn != nullptr
			&& FVector2D(AfterPan.X, AfterPan.Y).Equals(FVector2D(ExpectedWorld.X, ExpectedWorld.Y), 1.0f),
			TEXT("C_ClickMovesAnchorXY"));
		Expect(CameraPawn != nullptr
			&& FMath::IsNearlyEqual(AfterPan.Z, OriginalLocation.Z, 0.1f),
			TEXT("C_PreservesZ"));
		Expect(CameraPawn != nullptr
			&& AfterRotation.Equals(OriginalRotation, 0.01f),
			TEXT("C_PreservesYaw"));
		Expect(!bHasView
			|| FMath::IsNearlyEqual(AfterViewOffset.Size(), OriginalViewOffset.Size(), 5.0f),
			TEXT("C_PreservesZoomArmProxy"));
		Expect(!bHasBounds
			|| (AfterPan.X >= CameraBounds.Min.X - 0.1f && AfterPan.X <= CameraBounds.Max.X + 0.1f
				&& AfterPan.Y >= CameraBounds.Min.Y - 0.1f && AfterPan.Y <= CameraBounds.Max.Y + 0.1f),
			TEXT("C_TargetClampedToCameraBounds"));

		if (Presenter != nullptr)
		{
			Presenter->ContractRebuildCameraFootprint();
		}
		Expect(Presenter != nullptr
			&& Presenter->GetCameraFootprint().Revision >= FootprintRevisionBefore
			&& (Presenter->GetCameraFootprint().Revision > FootprintRevisionBefore
				|| !Presenter->GetCameraFootprint().bIsValid),
			TEXT("C_FootprintFollowsNavigation"));

		const FVector2D InvalidNaN(
			std::numeric_limits<float>::quiet_NaN(),
			0.5f);
		Expect(Presenter != nullptr
			&& !Presenter->PanCameraToMinimapNormalized(FVector2D(-0.1f, 0.5f))
			&& !Presenter->PanCameraToMinimapNormalized(InvalidNaN),
			TEXT("D_InvalidNormalizedRejected"));

		const FVector LocationBeforeWidgetClick =
			CameraPawn != nullptr ? CameraPawn->GetActorLocation() : FVector::ZeroVector;
		if (Widget != nullptr)
		{
			Widget->ContractUnbindPresenter();
			Widget->ContractHandleMapLeftClick(FVector2D(0.1f, 0.1f));
		}
		const FVector LocationAfterUnboundClick =
			CameraPawn != nullptr ? CameraPawn->GetActorLocation() : FVector::ZeroVector;
		Expect(LocationAfterUnboundClick.Equals(LocationBeforeWidgetClick, 0.1f),
			TEXT("D_NoPresenterNoMove"));

		if (Widget != nullptr)
		{
			Widget->ReleaseSlateResources(true);
		}
		Expect(Widget != nullptr && Widget->GetBoundPresenterListenerCount() == 0,
			TEXT("D_ReleaseSlateResourcesSafe"));

		const int32 SelectedAfter =
			Selection != nullptr ? Selection->GetSelectedUnits().Num() : 0;
		Expect(SelectedAfter == SelectedBefore, TEXT("E_SelectionUnchanged"));

		if (CameraPawn != nullptr)
		{
			CameraPawn->SetCameraAnchorWorldXY(FVector2D(OriginalLocation.X, OriginalLocation.Y));
		}
		if (Presenter != nullptr)
		{
			Presenter->Shutdown();
		}

		UE_LOG(LogGPMinimapClickToPanContract, Log,
			TEXT("gp.UI.RunMinimapClickToPanContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMinimapClickToPanContract(
		TEXT("gp.UI.RunMinimapClickToPanContractTest"),
		TEXT("Run native minimap LMB click-to-pan contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMinimapClickToPanContractTest));
}

#endif
