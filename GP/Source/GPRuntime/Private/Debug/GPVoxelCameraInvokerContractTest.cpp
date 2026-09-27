// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include "Camera/GPCameraPawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "VoxelComponents/VoxelInvokerComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPVoxelCameraInvokerContract, Log, All);

namespace GPVoxelCameraInvokerContract
{
	static void RunCameraInvokerContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (bOk)
			{
				UE_LOG(LogGPVoxelCameraInvokerContract, Log,
					TEXT("gp.Voxel.RunCameraInvokerContractTest PASS: %s"), Label);
			}
			else
			{
				++Failures;
				UE_LOG(LogGPVoxelCameraInvokerContract, Error,
					TEXT("gp.Voxel.RunCameraInvokerContractTest FAIL: %s"), Label);
			}
		};

		const AGP_CameraPawn* CDO = GetDefault<AGP_CameraPawn>();
		TArray<UVoxelSimpleInvokerComponent*> CDOInvokers;
		if (CDO != nullptr)
		{
			CDO->GetComponents<UVoxelSimpleInvokerComponent>(CDOInvokers);
		}
		Expect(CDO != nullptr && CDOInvokers.Num() == 1, TEXT("A_CDOExactlyOneSimpleInvoker"));
		if (CDOInvokers.Num() == 1 && CDOInvokers[0] != nullptr)
		{
			UVoxelSimpleInvokerComponent* Invoker = CDOInvokers[0];
			Expect(Invoker->GetFName() == TEXT("VoxelInvoker"), TEXT("B_ComponentNameVoxelInvoker"));
			Expect(FMath::IsNearlyEqual(Invoker->LODRange, 20000.f), TEXT("C_LODRange20000cm"));
			Expect(FMath::IsNearlyEqual(Invoker->CollisionsRange, 20000.f), TEXT("D_CollisionsRange20000cm"));
			Expect(Invoker->bUseForLOD, TEXT("E_UseForLOD"));
			Expect(Invoker->bUseForCollisions, TEXT("F_UseForCollisions"));
			Expect(!Invoker->bUseForNavmesh, TEXT("G_NavmeshDisabled"));
			Expect(FMath::IsNearlyEqual(Invoker->NavmeshRange, 0.f), TEXT("H_NavmeshRangeZero"));
		}

		if (World != nullptr)
		{
			int32 LocalPawns = 0;
			int32 RemoteEnabled = 0;
			for (TActorIterator<AGP_CameraPawn> It(World); It; ++It)
			{
				AGP_CameraPawn* Pawn = *It;
				if (Pawn == nullptr)
				{
					continue;
				}
				TArray<UVoxelSimpleInvokerComponent*> Invokers;
				Pawn->GetComponents<UVoxelSimpleInvokerComponent>(Invokers);
				Expect(Invokers.Num() == 1, TEXT("I_RuntimeExactlyOneSimpleInvoker"));
				if (Invokers.Num() != 1 || Invokers[0] == nullptr)
				{
					continue;
				}
				if (Pawn->IsLocallyControlled())
				{
					++LocalPawns;
					Expect(Invokers[0]->IsInvokerEnabled(), TEXT("J_LocalPawnInvokerEnabled"));
				}
				else if (Invokers[0]->IsInvokerEnabled())
				{
					++RemoteEnabled;
				}
			}
			Expect(RemoteEnabled == 0, TEXT("K_RemotePawnInvokerDisabled"));
			UE_LOG(LogGPVoxelCameraInvokerContract, Log,
				TEXT("gp.Voxel.RunCameraInvokerContractTest NOTE: localPawns=%d remoteEnabled=%d"),
				LocalPawns, RemoteEnabled);
		}

		UE_LOG(LogGPVoxelCameraInvokerContract, Log,
			TEXT("gp.Voxel.RunCameraInvokerContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommandWithWorldAndArgs GCameraInvokerContract(
		TEXT("gp.Voxel.RunCameraInvokerContractTest"),
		TEXT("CDO/runtime check: AGP_CameraPawn owns one local-gated UVoxelSimpleInvokerComponent."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunCameraInvokerContractTest));
}

#endif
