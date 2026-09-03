// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelDataTools.h"
#include "VoxelWorld.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPVoxelPluginCompileProbe, Log, All);

namespace GPVoxelPluginCompileProbe
{
	static void RunCompileProbe(const TArray<FString>& Args)
	{
		(void)Args;
		int32 Failures = 0;

		const bool bWorldClass = AVoxelWorld::StaticClass() != nullptr;
		const bool bSphereTools = UVoxelSphereTools::StaticClass() != nullptr;
		const bool bDataTools = UVoxelDataTools::StaticClass() != nullptr;
		const bool bIsFree = !UVoxelBlueprintLibrary::IsVoxelPluginPro();

		if (!bWorldClass)
		{
			++Failures;
			UE_LOG(LogGPVoxelPluginCompileProbe, Error,
				TEXT("gp.Voxel.RunPluginCompileProbeContractTest FAIL: A_WorldClass"));
		}
		else
		{
			UE_LOG(LogGPVoxelPluginCompileProbe, Log,
				TEXT("gp.Voxel.RunPluginCompileProbeContractTest PASS: A_WorldClass %s"),
				*AVoxelWorld::StaticClass()->GetName());
		}

		if (!bSphereTools)
		{
			++Failures;
			UE_LOG(LogGPVoxelPluginCompileProbe, Error,
				TEXT("gp.Voxel.RunPluginCompileProbeContractTest FAIL: B_SphereTools"));
		}
		else
		{
			UE_LOG(LogGPVoxelPluginCompileProbe, Log,
				TEXT("gp.Voxel.RunPluginCompileProbeContractTest PASS: B_SphereTools %s"),
				*UVoxelSphereTools::StaticClass()->GetName());
		}

		if (!bDataTools)
		{
			++Failures;
			UE_LOG(LogGPVoxelPluginCompileProbe, Error,
				TEXT("gp.Voxel.RunPluginCompileProbeContractTest FAIL: C_DataTools"));
		}
		else
		{
			UE_LOG(LogGPVoxelPluginCompileProbe, Log,
				TEXT("gp.Voxel.RunPluginCompileProbeContractTest PASS: C_DataTools %s"),
				*UVoxelDataTools::StaticClass()->GetName());
		}

		if (!bIsFree)
		{
			++Failures;
			UE_LOG(LogGPVoxelPluginCompileProbe, Error,
				TEXT("gp.Voxel.RunPluginCompileProbeContractTest FAIL: D_IsVoxelPluginProFalse"));
		}
		else
		{
			UE_LOG(LogGPVoxelPluginCompileProbe, Log,
				TEXT("gp.Voxel.RunPluginCompileProbeContractTest PASS: D_IsVoxelPluginProFalse"));
		}

		UE_LOG(LogGPVoxelPluginCompileProbe, Log,
			TEXT("gp.Voxel.RunPluginCompileProbeContractTest: Complete Failures=%d Cancelled=false"),
			Failures);
	}

	static FAutoConsoleCommand GCompileProbe(
		TEXT("gp.Voxel.RunPluginCompileProbeContractTest"),
		TEXT("Compile/link probe: GPRuntime can see Voxel Plugin Free runtime types. Does not spawn terrain."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RunCompileProbe));
}

#endif
