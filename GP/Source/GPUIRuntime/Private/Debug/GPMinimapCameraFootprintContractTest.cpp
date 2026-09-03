// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/GPCameraPawn.h"
#include "Engine/World.h"
#include "FogOfWar/GPFoWPresentationTypes.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "ViewModels/GPMinimapPresenter.h"
#include "Widgets/GPMinimapWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMinimapCameraFootprintContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPMinimapCameraFootprintContractPrivate
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

	static bool IsFinitePoint(const FVector2D& Point)
	{
		return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
	}

	static float PolygonArea(const TArray<FVector2D>& Corners)
	{
		if (Corners.Num() < 3)
		{
			return 0.0f;
		}

		float Area = 0.0f;
		for (int32 Index = 0; Index < Corners.Num(); ++Index)
		{
			const FVector2D& A = Corners[Index];
			const FVector2D& B = Corners[(Index + 1) % Corners.Num()];
			Area += A.X * B.Y - B.X * A.Y;
		}
		return FMath::Abs(Area) * 0.5f;
	}

	static FVector2D PolygonCentroid(const TArray<FVector2D>& Corners)
	{
		FVector2D Sum = FVector2D::ZeroVector;
		if (Corners.Num() == 0)
		{
			return Sum;
		}

		for (const FVector2D& Corner : Corners)
		{
			Sum += Corner;
		}
		return Sum / static_cast<float>(Corners.Num());
	}

	static bool AllCornersInUnitSquare(const TArray<FVector2D>& Corners)
	{
		for (const FVector2D& Corner : Corners)
		{
			if (!IsFinitePoint(Corner)
				|| Corner.X < -0.0001f || Corner.X > 1.0001f
				|| Corner.Y < -0.0001f || Corner.Y > 1.0001f)
			{
				return false;
			}
		}
		return Corners.Num() >= 3;
	}

	static bool HasNonAxisAlignedEdge(const TArray<FVector2D>& Corners)
	{
		for (int32 Index = 0; Index < Corners.Num(); ++Index)
		{
			const FVector2D Delta = Corners[(Index + 1) % Corners.Num()] - Corners[Index];
			if (FMath::Abs(Delta.X) > 0.002f && FMath::Abs(Delta.Y) > 0.002f)
			{
				return true;
			}
		}
		return false;
	}

	static void RunMinimapCameraFootprintContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPMinimapCameraFootprintContract, Warning,
				TEXT("gp.UI.RunMinimapCameraFootprintContractTest: missing world or client"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPMinimapCameraFootprintContract, Log,
					TEXT("gp.UI.RunMinimapCameraFootprintContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPMinimapCameraFootprintContract, Error,
					TEXT("gp.UI.RunMinimapCameraFootprintContractTest FAIL: %s"), Label);
			}
		};

		AGP_PlayerController* PlayerController =
			Cast<AGP_PlayerController>(World->GetFirstPlayerController());
		AGP_CameraPawn* CameraPawn =
			PlayerController != nullptr ? Cast<AGP_CameraPawn>(PlayerController->GetPawn()) : nullptr;
		Expect(PlayerController != nullptr && CameraPawn != nullptr,
			TEXT("A0_LocalCameraPawnPresent"));

		UGP_MinimapPresenter* Presenter = NewObject<UGP_MinimapPresenter>(GetTransientPackage());
		UGP_MinimapWidget* Widget = NewObject<UGP_MinimapWidget>(GetTransientPackage());
		Expect(Presenter != nullptr && Widget != nullptr, TEXT("A1_ObjectsCreated"));

		Expect(UGP_MinimapWidget::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr
			&& UGP_MinimapPresenter::StaticClass()->FindFunctionByName(TEXT("Tick")) == nullptr,
			TEXT("A2_NoWidgetOrPresenterTick"));

		UGP_LocalFoWComponent* Mirror = NewObject<UGP_LocalFoWComponent>(GetTransientPackage());
		Expect(Presenter != nullptr && Mirror != nullptr
			&& Presenter->InitializeWithMirror(Mirror)
			&& !Presenter->GetCameraFootprint().bIsValid,
			TEXT("A3_NoCameraMeansInvalidFootprint"));

		FGP_FoWPresentationUpdate Initial = MakeInitial(1, 1);
		if (Mirror != nullptr)
		{
			Mirror->ApplyServerUpdate(Initial);
		}

		if (Presenter != nullptr && PlayerController != nullptr)
		{
			Presenter->Initialize(PlayerController);
			Presenter->Initialize(PlayerController);
		}
		if (CameraPawn != nullptr)
		{
			CameraPawn->Tick(0.0f);
			CameraPawn->ContractNotifyCameraPresentationChanged();
		}
		if (Presenter != nullptr)
		{
			Presenter->ContractRebuildCameraFootprint();
		}
		bool bUsedViewportOverride = false;
		if (Presenter != nullptr && !Presenter->GetCameraFootprint().bIsValid)
		{
			Presenter->ContractSetViewportSizeOverride(1280, 720);
			Presenter->ContractRebuildCameraFootprint();
			bUsedViewportOverride = true;
		}

		const FGP_MinimapCameraFootprint& ReadyFootprint =
			Presenter != nullptr ? Presenter->GetCameraFootprint() : FGP_MinimapCameraFootprint();
		Expect(Presenter != nullptr
			&& ReadyFootprint.bIsValid
			&& ReadyFootprint.NormalizedCorners.Num() >= 3
			&& AllCornersInUnitSquare(ReadyFootprint.NormalizedCorners)
			&& Presenter->GetBoundCameraPresentationDelegateCount() == 1
			&& Presenter->GetBoundCameraBoundsDelegateCount() == 1,
			TEXT("A4_ValidLocalCameraFootprintAndSingleDelegates"));

		if (Widget != nullptr && Presenter != nullptr)
		{
			Widget->ContractBindPresenter(Presenter);
			Widget->ContractBindPresenter(Presenter);
		}

		if (Presenter != nullptr)
		{
			Presenter->ContractSetViewportSizeOverride(0, 0);
		}
		Expect(Presenter != nullptr && !Presenter->GetCameraFootprint().bIsValid,
			TEXT("A5_InvalidViewportNoFootprint"));
		if (Presenter != nullptr)
		{
			if (bUsedViewportOverride)
			{
				Presenter->ContractSetViewportSizeOverride(1280, 720);
			}
			else
			{
				Presenter->ContractClearViewportSizeOverride();
			}
			Presenter->ContractRebuildCameraFootprint();
		}

		const TArray<FVector2D> BeforePan =
			Presenter != nullptr ? Presenter->GetCameraFootprint().NormalizedCorners : TArray<FVector2D>();
		const FVector2D CentroidBefore = PolygonCentroid(BeforePan);
		const FVector OriginalLocation =
			CameraPawn != nullptr ? CameraPawn->GetActorLocation() : FVector::ZeroVector;
		if (CameraPawn != nullptr)
		{
			CameraPawn->SetActorLocation(OriginalLocation + FVector(150.0f, 0.0f, 0.0f), false);
			CameraPawn->Tick(0.0f);
			CameraPawn->ContractNotifyCameraPresentationChanged();
		}
		const TArray<FVector2D> AfterPan =
			Presenter != nullptr ? Presenter->GetCameraFootprint().NormalizedCorners : TArray<FVector2D>();
		const FVector2D CentroidAfterPan = PolygonCentroid(AfterPan);
		Expect(BeforePan.Num() >= 3
			&& AfterPan.Num() >= 3
			&& CentroidAfterPan.X > CentroidBefore.X + 0.0005f
			&& AllCornersInUnitSquare(AfterPan),
			TEXT("B_PanMovesFootprint"));

		if (CameraPawn != nullptr)
		{
			CameraPawn->SetActorLocation(OriginalLocation, false);
			CameraPawn->Tick(0.0f);
			CameraPawn->ContractNotifyCameraPresentationChanged();
		}

		auto UnclampedExtent = [Presenter]() -> float
		{
			TArray<FVector2D> Corners;
			if (Presenter == nullptr || !Presenter->ContractTryGetUnclampedNormalizedCorners(Corners) || Corners.Num() < 3)
			{
				return 0.0f;
			}

			FBox2D Bounds(ForceInit);
			for (const FVector2D& Corner : Corners)
			{
				Bounds += Corner;
			}
			return Bounds.GetSize().Size();
		};

		const float ExtentBeforeZoom = UnclampedExtent();
		if (CameraPawn != nullptr)
		{
			CameraPawn->AddZoomInput(-12.0f);
			for (int32 Step = 0; Step < 12; ++Step)
			{
				CameraPawn->Tick(0.05f);
			}
			CameraPawn->ContractNotifyCameraPresentationChanged();
		}
		const float ExtentAfterZoomOut = UnclampedExtent();
		if (CameraPawn != nullptr)
		{
			CameraPawn->AddZoomInput(24.0f);
			for (int32 Step = 0; Step < 12; ++Step)
			{
				CameraPawn->Tick(0.05f);
			}
			CameraPawn->ContractNotifyCameraPresentationChanged();
		}
		const float ExtentAfterZoomIn = UnclampedExtent();
		Expect(ExtentBeforeZoom > KINDA_SMALL_NUMBER
			&& ExtentAfterZoomOut > ExtentBeforeZoom * 1.02f
			&& ExtentAfterZoomIn < ExtentAfterZoomOut * 0.98f,
			TEXT("C_ZoomOutGrowsZoomInShrinksFootprint"));

		if (CameraPawn != nullptr)
		{
			CameraPawn->SetActorLocation(OriginalLocation, false);
			CameraPawn->AddZoomInput(-24.0f);
			for (int32 Step = 0; Step < 8; ++Step)
			{
				CameraPawn->Tick(0.05f);
			}
			CameraPawn->SetRotateActive(true);
			CameraPawn->AddRotateInput(20.0f);
			CameraPawn->Tick(0.05f);
			CameraPawn->SetRotateActive(false);
			CameraPawn->ContractNotifyCameraPresentationChanged();
		}
		const TArray<FVector2D> RotatedCorners =
			Presenter != nullptr ? Presenter->GetCameraFootprint().NormalizedCorners : TArray<FVector2D>();
		Expect(RotatedCorners.Num() >= 3
			&& HasNonAxisAlignedEdge(RotatedCorners)
			&& AllCornersInUnitSquare(RotatedCorners),
			TEXT("D_YawRotatesFootprintNotAxisAlignedRect"));

		bool bSharedTransform = RotatedCorners.Num() >= 1;
		for (const FVector2D& Corner : RotatedCorners)
		{
			const FVector2D Surface = UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(Corner);
			bSharedTransform = bSharedTransform
				&& Surface.Equals(FVector2D(1.0f - Corner.X, 1.0f - Corner.Y), 0.0001f);
		}
		Expect(bSharedTransform
			&& UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(FVector2D(0.0f, 0.0f))
				.Equals(FVector2D(1.0f, 1.0f), 0.0001f),
			TEXT("E_FootprintUsesSharedSurfaceTransform"));

		FBox CameraBounds(ForceInit);
		const bool bHasBounds =
			CameraPawn != nullptr && CameraPawn->GetResolvedCameraBounds(CameraBounds);
		if (bHasBounds && CameraPawn != nullptr)
		{
			const FVector EdgeLocation(
				CameraBounds.Min.X,
				CameraBounds.Min.Y,
				CameraPawn->GetGroundReferencePlaneZ());
			CameraPawn->SetActorLocation(EdgeLocation, false);
			CameraPawn->Tick(0.0f);
			CameraPawn->ContractNotifyCameraPresentationChanged();
		}
		const FGP_MinimapCameraFootprint& EdgeFootprint =
			Presenter != nullptr ? Presenter->GetCameraFootprint() : FGP_MinimapCameraFootprint();
		Expect(bHasBounds
			&& (!EdgeFootprint.bIsValid || AllCornersInUnitSquare(EdgeFootprint.NormalizedCorners)),
			TEXT("F_EdgeClipStaysFiniteInsideUnitSquare"));

		if (CameraPawn != nullptr)
		{
			CameraPawn->SetActorLocation(OriginalLocation, false);
			for (int32 Step = 0; Step < 20; ++Step)
			{
				CameraPawn->Tick(0.05f);
			}
			CameraPawn->ContractNotifyCameraPresentationChanged();
		}

		Expect(Widget != nullptr
			&& Presenter != nullptr
			&& Widget->GetBoundPresenterListenerCount() == 1
			&& Presenter->GetBoundCameraPresentationDelegateCount() == 1
			&& (!Presenter->GetCameraFootprint().bIsValid
				|| Widget->ContractHasCameraFootprintDraw())
			&& FMath::IsNearlyEqual(CameraPawn != nullptr
				? CameraPawn->GetGroundReferencePlaneZ()
				: 0.0f,
				CameraPawn != nullptr ? CameraPawn->GetActorLocation().Z : 0.0f),
			TEXT("G_RebindNoDuplicateAndPlaneZIsPawnAnchor"));

		const int32 RevisionBeforeIdle =
			Presenter != nullptr ? Presenter->GetCameraFootprint().Revision : 0;
		if (CameraPawn != nullptr)
		{
			CameraPawn->Tick(0.016f);
			CameraPawn->Tick(0.016f);
		}
		Expect(Presenter != nullptr
			&& Presenter->GetCameraFootprint().Revision == RevisionBeforeIdle,
			TEXT("H_IdleTickDoesNotBroadcastFootprint"));

		if (Presenter != nullptr)
		{
			Presenter->Shutdown();
		}
		Expect(Presenter != nullptr
			&& !Presenter->GetCameraFootprint().bIsValid
			&& Presenter->GetBoundCameraPresentationDelegateCount() == 0,
			TEXT("I_ShutdownClearsFootprint"));
		if (Widget != nullptr)
		{
			Widget->ContractUnbindPresenter();
		}

		UE_LOG(LogGPMinimapCameraFootprintContract, Log,
			TEXT("gp.UI.RunMinimapCameraFootprintContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMinimapCameraFootprintContract(
		TEXT("gp.UI.RunMinimapCameraFootprintContractTest"),
		TEXT("Run native minimap camera viewport-footprint contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMinimapCameraFootprintContractTest));
}

#endif
