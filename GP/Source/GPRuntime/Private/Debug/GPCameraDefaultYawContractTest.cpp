// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include "Camera/GPCameraConfigDataAsset.h"
#include "Camera/GPCameraPawn.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPCameraDefaultYawContract, Log, All);

namespace GPCameraDefaultYawContract
{
	static bool YawEquals(float Actual, float Expected)
	{
		return FMath::IsNearlyEqual(
			FRotator::NormalizeAxis(Actual),
			FRotator::NormalizeAxis(Expected),
			0.05f);
	}

	static AGP_CameraPawn* SpawnAtYaw(UWorld* World, float YawDegrees)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<AGP_CameraPawn>(
			FVector(80000.0f, 80000.0f, 200.0f),
			FRotator(0.0f, YawDegrees, 0.0f),
			Params);
	}

	static void RunCameraDefaultYawContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (bOk)
			{
				UE_LOG(LogGPCameraDefaultYawContract, Log,
					TEXT("gp.Camera.RunDefaultYawContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPCameraDefaultYawContract, Error,
					TEXT("gp.Camera.RunDefaultYawContractTest FAIL: %s"), Label);
			}
		};

		const UGP_CameraConfigDataAsset* Config = GetDefault<UGP_CameraConfigDataAsset>();
		const float DefaultYaw = Config != nullptr ? Config->DefaultYaw : 0.0f;
		Expect(Config != nullptr && FMath::IsNearlyEqual(DefaultYaw, 90.0f), TEXT("A_ConfigDefaultYaw90"));

		if (World == nullptr || Config == nullptr)
		{
			UE_LOG(LogGPCameraDefaultYawContract, Log,
				TEXT("gp.Camera.RunDefaultYawContractTest: Complete Failures=%d Cancelled=false"),
				Failures + 1);
			return;
		}

		AGP_CameraPawn* ZeroYawPawn = SpawnAtYaw(World, 0.0f);
		Expect(ZeroYawPawn != nullptr
				&& YawEquals(ZeroYawPawn->GetActorRotation().Yaw, DefaultYaw)
				&& YawEquals(ZeroYawPawn->ContractGetCurrentYaw(), DefaultYaw),
			TEXT("B_SpawnYaw0UsesDefaultYaw"));

		const float SpawnYaws[] = { 90.0f, -90.0f, 180.0f, 37.0f };
		bool bIgnoredSpawnRotation = ZeroYawPawn != nullptr;
		TArray<AGP_CameraPawn*> Spawned;
		if (ZeroYawPawn != nullptr)
		{
			Spawned.Add(ZeroYawPawn);
		}
		for (const float SpawnYaw : SpawnYaws)
		{
			AGP_CameraPawn* Pawn = SpawnAtYaw(World, SpawnYaw);
			if (Pawn == nullptr
				|| !YawEquals(Pawn->GetActorRotation().Yaw, DefaultYaw)
				|| !YawEquals(Pawn->ContractGetCurrentYaw(), DefaultYaw))
			{
				bIgnoredSpawnRotation = false;
			}
			if (Pawn != nullptr)
			{
				Spawned.Add(Pawn);
			}
		}
		Expect(bIgnoredSpawnRotation, TEXT("C_SpawnRotationDoesNotLeak"));

		AGP_CameraPawn* RotatePawn = Spawned.Num() > 0 ? Spawned[0] : nullptr;
		if (RotatePawn != nullptr)
		{
			const float Before = RotatePawn->ContractGetCurrentYaw();
			RotatePawn->ContractSimulateRotateInput(10.0f);
			const float Expected = FMath::UnwindDegrees(Before + (10.0f * Config->RotateSpeed));
			Expect(YawEquals(RotatePawn->ContractGetCurrentYaw(), Expected)
					&& !YawEquals(RotatePawn->ContractGetCurrentYaw(), Before),
				TEXT("D_RotateInputChangesYawFromDefault"));
		}
		else
		{
			Expect(false, TEXT("D_RotateInputChangesYawFromDefault"));
		}

		for (AGP_CameraPawn* Pawn : Spawned)
		{
			if (IsValid(Pawn))
			{
				Pawn->Destroy();
			}
		}

		UE_LOG(LogGPCameraDefaultYawContract, Log,
			TEXT("gp.Camera.RunDefaultYawContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GCameraDefaultYawContract(
		TEXT("gp.Camera.RunDefaultYawContractTest"),
		TEXT("Startup camera yaw is CameraConfig.DefaultYaw, not pawn spawn rotation."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunCameraDefaultYawContractTest));
}

#endif
