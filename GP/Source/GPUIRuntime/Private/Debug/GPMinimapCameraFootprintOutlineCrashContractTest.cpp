// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"
#include "Math/Box2D.h"
#include "Widgets/GPMinimapWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMinimapCameraFootprintOutlineCrashContract, Log, All);

#if !UE_BUILD_SHIPPING

namespace GPMinimapCameraFootprintOutlineCrashContractPrivate
{
	static void RunMinimapCameraFootprintOutlineCrashContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		(void)World;

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (bCondition)
			{
				UE_LOG(LogGPMinimapCameraFootprintOutlineCrashContract, Log,
					TEXT("gp.UI.RunMinimapCameraFootprintOutlineCrashContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPMinimapCameraFootprintOutlineCrashContract, Error,
					TEXT("gp.UI.RunMinimapCameraFootprintOutlineCrashContractTest FAIL: %s"), Label);
			}
		};

		const FBox2D MapDest(FVector2D(10.0f, 20.0f), FVector2D(110.0f, 120.0f));
		TArray<FVector2D> Corners;
		Corners.Add(FVector2D(0.0f, 0.0f));
		Corners.Add(FVector2D(1.0f, 0.0f));
		Corners.Add(FVector2D(1.0f, 1.0f));
		Corners.Add(FVector2D(0.0f, 1.0f));

		TArray<FVector2f> Outline;
		Expect(UGP_MinimapWidget::BuildClosedCameraFootprintOutlinePoints(Corners, MapDest, Outline)
			&& Outline.Num() == Corners.Num() + 1
			&& Outline.Num() >= 4
			&& Outline.Last().Equals(Outline[0]),
			TEXT("A_ClosedOutlineCopiesFirstPointWithoutSelfAdd"));

		const FVector2D DestSize = MapDest.GetSize();
		bool bMappedThroughSharedTransform = Outline.Num() >= 4;
		for (int32 Index = 0; Index < Corners.Num(); ++Index)
		{
			const FVector2D SurfaceUV = UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(Corners[Index]);
			const FVector2D Expected = MapDest.Min + SurfaceUV * DestSize;
			bMappedThroughSharedTransform = bMappedThroughSharedTransform
				&& Outline[Index].Equals(FVector2f(static_cast<float>(Expected.X), static_cast<float>(Expected.Y)));
		}
		Expect(bMappedThroughSharedTransform, TEXT("B_OutlineUsesSharedSurfaceTransform"));

		TArray<FVector2D> TooFew;
		TooFew.Add(FVector2D(0.0f, 0.0f));
		TooFew.Add(FVector2D(1.0f, 0.0f));
		TArray<FVector2f> Rejected;
		Rejected.Add(FVector2f(1.0f, 1.0f));
		Expect(!UGP_MinimapWidget::BuildClosedCameraFootprintOutlinePoints(TooFew, MapDest, Rejected)
			&& Rejected.Num() == 0,
			TEXT("C_FewerThanThreeCornersRejected"));

		TArray<FVector2D> Triangle;
		Triangle.Add(FVector2D(0.1f, 0.1f));
		Triangle.Add(FVector2D(0.9f, 0.2f));
		Triangle.Add(FVector2D(0.4f, 0.8f));
		TArray<FVector2f> TriangleOutline;
		Expect(UGP_MinimapWidget::BuildClosedCameraFootprintOutlinePoints(Triangle, MapDest, TriangleOutline)
			&& TriangleOutline.Num() == 4
			&& TriangleOutline.Last().Equals(TriangleOutline[0]),
			TEXT("D_ThreeCornersCloseWithCopiedFirstPoint"));

		UE_LOG(LogGPMinimapCameraFootprintOutlineCrashContract, Log,
			TEXT("gp.UI.RunMinimapCameraFootprintOutlineCrashContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GMinimapCameraFootprintOutlineCrashContract(
		TEXT("gp.UI.RunMinimapCameraFootprintOutlineCrashContractTest"),
		TEXT("Run minimap camera-footprint closed-outline construction crash regression."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
			&RunMinimapCameraFootprintOutlineCrashContractTest));
}

#endif
