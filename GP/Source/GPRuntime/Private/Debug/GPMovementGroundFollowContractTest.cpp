// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Debug/GPContractTestCoordinator.h"
#include "Engine/World.h"
#include "Units/GPMovementComponent.h"
#include "Units/GPUnit.h"
#include "Voxel/GPVoxelRuntimeProbeAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPGroundFollowContract, Log, All);

namespace GPMovementGroundFollowContract
{
	static void RunGroundFollowContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPGroundFollowContract, Warning,
				TEXT("gp.Movement.RunGroundFollowContractTest: missing world or client"));
			return;
		}

		GPContractTestCoordinator::FExecutionToken Token;
		if (!GPContractTestCoordinator::TryAcquire(
			World, TEXT("MovementGroundFollowContract"), TEXT("MovementGroundFollow"), Token))
		{
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bCondition, const TCHAR* Label)
		{
			if (!bCondition)
			{
				++Failures;
				UE_LOG(LogGPGroundFollowContract, Error,
					TEXT("gp.Movement.RunGroundFollowContractTest FAIL: %s"), Label);
			}
			else
			{
				UE_LOG(LogGPGroundFollowContract, Log,
					TEXT("gp.Movement.RunGroundFollowContractTest PASS: %s"), Label);
			}
		};

		const FVector Impact(1000.f, 2000.f, 300.f);
		const GPVoxelCraterMath::FShallowCrater DefaultCrater = GPVoxelCraterMath::Make(Impact, 400.f, 80.f);
		Expect(FMath::IsNearlyEqual(DefaultCrater.RadiusCm, 400.f), TEXT("A_Radius400"));
		Expect(FMath::IsNearlyEqual(DefaultCrater.DepthCm, 80.f), TEXT("A_Depth80"));
		Expect(FMath::IsNearlyEqual(DefaultCrater.CenterZOffsetCm, 320.f), TEXT("A_CenterOffsetPlus320"));
		Expect(FMath::IsNearlyEqual(DefaultCrater.EditCenter.Z, Impact.Z + 320.f), TEXT("A_EditCenterZ"));
		Expect(FMath::IsNearlyEqual(DefaultCrater.CenterZOffsetCm - DefaultCrater.RadiusCm, -80.f), TEXT("A_SphereBottomMinus80"));
		Expect(FMath::IsNearlyEqual(DefaultCrater.EditCenter.X, Impact.X)
			&& FMath::IsNearlyEqual(DefaultCrater.EditCenter.Y, Impact.Y), TEXT("A_EditCenterXY"));

		const GPVoxelCraterMath::FShallowCrater Wide = GPVoxelCraterMath::Make(Impact, 600.f, 100.f);
		Expect(FMath::IsNearlyEqual(Wide.CenterZOffsetCm, 500.f), TEXT("A_WideOffsetPlus500"));
		Expect(FMath::IsNearlyEqual(Wide.CenterZOffsetCm - Wide.RadiusCm, -100.f), TEXT("A_WideSphereBottomMinus100"));

		const GPVoxelCraterMath::FShallowCrater Clamped = GPVoxelCraterMath::Make(Impact, 400.f, 900.f);
		Expect(FMath::IsNearlyEqual(Clamped.RadiusCm, 400.f), TEXT("A_DepthClampRadius"));
		Expect(Clamped.DepthCm <= Clamped.RadiusCm, TEXT("A_DepthNotAboveRadius"));
		Expect(FMath::IsNearlyEqual(Clamped.DepthCm, 400.f), TEXT("A_DepthClampedToRadius"));
		Expect(FMath::IsNearlyEqual(Clamped.CenterZOffsetCm, 0.f), TEXT("A_FullRadiusOffsetZero"));

		constexpr float BaseX = 120000.f;
		constexpr float BaseY = 120000.f;
		constexpr float TickDt = 0.05f;
		const FVector HighCenter(BaseX, BaseY, 20000.f);
		const FVector HighExtent(2500.f, 2500.f, 20.f);
		const FVector LowCenter(BaseX + 4000.f, BaseY, 19820.f);
		const FVector LowExtent(1500.f, 2500.f, 20.f);
		const float HighTop = HighCenter.Z + HighExtent.Z;
		const float LowTop = LowCenter.Z + LowExtent.Z;

		auto SpawnFloor = [World](const FVector& Center, const FVector& Extent) -> AActor*
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			AActor* Floor = World->SpawnActor<AActor>(AActor::StaticClass(), Center, FRotator::ZeroRotator, Params);
			if (Floor == nullptr)
			{
				return nullptr;
			}
			UBoxComponent* Box = NewObject<UBoxComponent>(Floor, TEXT("GroundFollowFloor"));
			Box->SetBoxExtent(Extent);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->SetCanEverAffectNavigation(false);
			Box->SetMobility(EComponentMobility::Movable);
			Floor->SetRootComponent(Box);
			Box->RegisterComponent();
			Box->SetWorldLocation(Center);
			Floor->SetActorEnableCollision(true);
			return Floor;
		};

		AActor* HighFloor = SpawnFloor(HighCenter, HighExtent);
		AActor* LowFloor = SpawnFloor(LowCenter, LowExtent);
		Expect(HighFloor != nullptr && LowFloor != nullptr, TEXT("B_FloorsSpawned"));

		FActorSpawnParameters UnitParams;
		UnitParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UnitParams.ObjectFlags |= RF_Transient;
		AGP_Unit* Unit = World->SpawnActor<AGP_Unit>(
			AGP_Unit::StaticClass(), HighCenter, FRotator::ZeroRotator, UnitParams);
		UGP_MovementComponent* Movement = Unit != nullptr ? Unit->GetUnitMovementComponent() : nullptr;
		UCapsuleComponent* Capsule = Unit != nullptr ? Cast<UCapsuleComponent>(Unit->GetRootComponent()) : nullptr;
		Expect(Unit != nullptr && Movement != nullptr && Capsule != nullptr, TEXT("B_UnitSpawned"));

		if (Unit != nullptr && Movement != nullptr && Capsule != nullptr && HighFloor != nullptr && LowFloor != nullptr)
		{
			const float Support = Capsule->GetScaledCapsuleHalfHeight();
			Expect(Support > 1.0f, TEXT("B_CapsuleSupportOffset"));
			const float HighActorZ = HighTop + Support;
			const float LowActorZ = LowTop + Support;
			Unit->SetActorLocation(FVector(BaseX, BaseY, HighActorZ), false);

			Movement->MoveSpeed = 2000.0f;
			Movement->bFollowGroundSurface = true;
			const FVector LowDest(LowCenter.X, BaseY, HighActorZ);
			const FGP_MovementRequestOutcome Outward = Movement->RequestMove(LowDest, 71u);
			Expect(Outward.IsAccepted(), TEXT("B_MoveAccepted"));

			auto StepTicks = [Movement](int32 Count)
			{
				for (int32 Index = 0; Index < Count; ++Index)
				{
					Movement->TickComponent(TickDt, LEVELTICK_All, &Movement->PrimaryComponentTick);
				}
			};

			StepTicks(5);
			const FVector AfterFlat = Unit->GetActorLocation();
			Expect(FMath::Abs(AfterFlat.Z - HighActorZ) < 3.0f, TEXT("B_FlatFloorKeepsSupportZ"));
			Expect(FVector::Dist2D(AfterFlat, FVector(BaseX, BaseY, HighActorZ)) > 100.0f, TEXT("B_FlatFloorXYAdvances"));
			Expect(FMath::IsFinite(AfterFlat.Z), TEXT("B_FlatZFinite"));

			StepTicks(50);
			const FVector AfterLow = Unit->GetActorLocation();
			Expect(AfterLow.Z < HighActorZ - 100.0f, TEXT("C_ZDropsOnLowerFloor"));
			Expect(FMath::Abs(AfterLow.Z - LowActorZ) < 25.0f, TEXT("C_ZNearLowerSupport"));
			Expect(AfterLow.X > BaseX + 1000.0f, TEXT("C_XYContinues"));
			Expect(FMath::IsFinite(AfterLow.Z), TEXT("C_LowZFinite"));

			const FVector HighDest(BaseX, BaseY, AfterLow.Z);
			const FGP_MovementRequestOutcome ReturnMove = Movement->RequestMove(HighDest, 72u);
			Expect(ReturnMove.IsAccepted(), TEXT("D_ReturnAccepted"));
			StepTicks(50);
			const FVector AfterHigh = Unit->GetActorLocation();
			Expect(AfterHigh.Z > LowActorZ + 100.0f, TEXT("D_ZRises"));
			Expect(FMath::Abs(AfterHigh.Z - HighActorZ) < 25.0f, TEXT("D_ZNearHighSupport"));
			Expect(FMath::IsFinite(AfterHigh.Z), TEXT("D_HighZFinite"));

			Movement->StopMove(EGP_MovementStopReason::Manual);
		}

		AGP_Unit* MissUnit = World->SpawnActor<AGP_Unit>(
			AGP_Unit::StaticClass(), FVector(BaseX, BaseY + 20000.0f, 40000.0f), FRotator::ZeroRotator, UnitParams);
		UGP_MovementComponent* MissMovement = MissUnit != nullptr ? MissUnit->GetUnitMovementComponent() : nullptr;
		if (MissUnit != nullptr && MissMovement != nullptr)
		{
			const FVector MissStart(BaseX, BaseY + 20000.0f, 40000.0f);
			MissUnit->SetActorLocation(MissStart, false);
			MissMovement->MoveSpeed = 2000.0f;
			MissMovement->bFollowGroundSurface = true;
			const FGP_MovementRequestOutcome MissMove = MissMovement->RequestMove(
				MissStart + FVector(800.0f, 0.0f, 0.0f), 73u);
			Expect(MissMove.IsAccepted(), TEXT("E_MissMoveAccepted"));
			for (int32 Index = 0; Index < 8; ++Index)
			{
				MissMovement->TickComponent(TickDt, LEVELTICK_All, &MissMovement->PrimaryComponentTick);
			}
			const FVector AfterMiss = MissUnit->GetActorLocation();
			Expect(FMath::IsFinite(AfterMiss.X) && FMath::IsFinite(AfterMiss.Y) && FMath::IsFinite(AfterMiss.Z),
				TEXT("E_NoNaN"));
			Expect(FMath::Abs(AfterMiss.Z - MissStart.Z) < 1.0f, TEXT("E_ZPreservedOnMiss"));
			Expect(FVector::Dist2D(AfterMiss, MissStart) > 50.0f, TEXT("E_XYStillMoves"));
			Expect(MissMovement->IsMoving() || FVector::Dist2D(AfterMiss, MissStart + FVector(800.0f, 0.0f, 0.0f)) <= MissMovement->AcceptanceRadius,
				TEXT("E_MoveNotCancelled"));
			MissMovement->StopMove(EGP_MovementStopReason::Manual);
		}
		else
		{
			Expect(false, TEXT("E_MissUnitSpawned"));
		}

		if (MissUnit != nullptr)
		{
			MissUnit->Destroy();
		}
		if (Unit != nullptr)
		{
			Unit->Destroy();
		}
		if (HighFloor != nullptr)
		{
			HighFloor->Destroy();
		}
		if (LowFloor != nullptr)
		{
			LowFloor->Destroy();
		}

		UE_LOG(LogGPGroundFollowContract, Log,
			TEXT("gp.Movement.RunGroundFollowContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
		GPContractTestCoordinator::Release(Token.ExecutionId, Failures, false, nullptr);
	}

	static FAutoConsoleCommandWithWorldAndArgs GGroundFollowContract(
		TEXT("gp.Movement.RunGroundFollowContractTest"),
		TEXT("Shallow-crater math plus ground-follow Z on flat, lower, higher, and missing terrain."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunGroundFollowContractTest));
}

#endif
