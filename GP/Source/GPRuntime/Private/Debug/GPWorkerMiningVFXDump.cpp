// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/GPWorker.h"

#if !UE_BUILD_SHIPPING

#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemEmitterState.h"
#include "Resources/GPMiningComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPMiningVFX, Log, All);

namespace GPMiningVFXDumpPrivate
{
	static FString EnumToString(const UEnum* Enum, int64 Value)
	{
		return Enum != nullptr ? Enum->GetNameStringByValue(Value) : FString(TEXT("?"));
	}

	static bool ShouldLogParameter(const FString& Name)
	{
		return Name.Contains(TEXT("Loop"))
			|| Name.Contains(TEXT("Inactive"))
			|| Name.Contains(TEXT("Life Cycle"))
			|| Name.Contains(TEXT("Lifetime"))
			|| Name.Contains(TEXT("Spawn Count"))
			|| Name.Contains(TEXT("SpawnBurst"))
			|| Name.Contains(TEXT("UseLoop"));
	}

	static void LogCompiledVariables(const TCHAR* Label, const UNiagaraScript* Script)
	{
		if (Script == nullptr)
		{
			return;
		}

		int32 Logged = 0;
		for (const FNiagaraVariable& Variable : Script->GetVMExecutableData().StaticVariablesWritten)
		{
			const FString Name = Variable.GetName().ToString();
			if (!ShouldLogParameter(Name) && !Name.Contains(TEXT("Behavior")) && !Name.Contains(TEXT("Inactive")))
			{
				continue;
			}
			UE_LOG(LogGPMiningVFX, Log, TEXT("gp.Worker.DumpMiningVFX Compiled %s %s"), Label, *Variable.ToString());
			if (++Logged >= 40)
			{
				break;
			}
		}
		if (Logged == 0)
		{
			UE_LOG(LogGPMiningVFX, Log, TEXT("gp.Worker.DumpMiningVFX Compiled %s: no loop matches"), Label);
		}
	}

	static void LogParameterStore(const TCHAR* Label, const UNiagaraScript* Script)
	{
		if (Script == nullptr)
		{
			UE_LOG(LogGPMiningVFX, Log, TEXT("gp.Worker.DumpMiningVFX Params %s: script=none"), Label);
			return;
		}

		const FNiagaraParameterStore& Store = Script->RapidIterationParameters;
		int32 Logged = 0;
		for (const FNiagaraVariableWithOffset& Param : Store.ReadParameterVariables())
		{
			const FString Name = Param.GetName().ToString();
			if (!ShouldLogParameter(Name))
			{
				continue;
			}

			FNiagaraVariable Copy(Param.GetType(), Param.GetName());
			if (const uint8* Data = Store.GetParameterData(Param.Offset, Param.GetSizeInBytes()))
			{
				Copy.SetData(Data);
			}

			UE_LOG(LogGPMiningVFX, Log, TEXT("gp.Worker.DumpMiningVFX Param %s %s = %s"),
				Label, *Name, *Copy.ToString());
			if (++Logged >= 40)
			{
				UE_LOG(LogGPMiningVFX, Log, TEXT("gp.Worker.DumpMiningVFX Param %s truncated"), Label);
				break;
			}
		}

		if (Logged == 0)
		{
			UE_LOG(LogGPMiningVFX, Log, TEXT("gp.Worker.DumpMiningVFX Params %s: no loop/spawn matches (%d stored)"),
				Label, Store.Num());
		}
	}

	static void LogSystemAsset(const UNiagaraSystem* System)
	{
		if (System == nullptr)
		{
			UE_LOG(LogGPMiningVFX, Log, TEXT("gp.Worker.DumpMiningVFX System: none"));
			return;
		}

		const FNiagaraSystemStateData& State = System->GetSystemStateData();
		UE_LOG(LogGPMiningVFX, Log,
			TEXT("gp.Worker.DumpMiningVFX System %s IgnoreSystemState=%s RunSpawn=%s RunUpdate=%s LoopBehavior=%s LoopDuration=[%.3f,%.3f] LoopCount=%d LoopDelayEnabled=%s LoopDelay=[%.3f,%.3f] InactiveResponse=%s Warmup=%.3f WarmupTicks=%d HasGPU=%s FixedBounds=%s EffectType=%s"),
			*System->GetPathName(),
			State.bIgnoreSystemState ? TEXT("true") : TEXT("false"),
			State.bRunSpawnScript ? TEXT("true") : TEXT("false"),
			State.bRunUpdateScript ? TEXT("true") : TEXT("false"),
			*EnumToString(StaticEnum<ENiagaraLoopBehavior>(), static_cast<int64>(State.LoopBehavior)),
			State.LoopDuration.Min,
			State.LoopDuration.Max,
			State.LoopCount,
			State.bLoopDelayEnabled ? TEXT("true") : TEXT("false"),
			State.LoopDelay.Min,
			State.LoopDelay.Max,
			*EnumToString(StaticEnum<ENiagaraSystemInactiveResponse>(), static_cast<int64>(State.InactiveResponse)),
			System->GetWarmupTime(),
			System->GetWarmupTickCount(),
			System->HasAnyGPUEmitters() ? TEXT("true") : TEXT("false"),
			*System->GetFixedBounds().ToString(),
			*GetPathNameSafe(System->GetEffectType()));

		LogParameterStore(TEXT("SystemSpawn"), System->GetSystemSpawnScript());
		LogParameterStore(TEXT("SystemUpdate"), System->GetSystemUpdateScript());
#if WITH_EDITORONLY_DATA
		LogCompiledVariables(TEXT("SystemUpdateCompiled"), System->GetSystemUpdateScript());
#endif

		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			const FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
			UE_LOG(LogGPMiningVFX, Log,
				TEXT("gp.Worker.DumpMiningVFX Emitter %s Enabled=%s Mode=%s SimTarget=%s BoundsMode=%s FixedBounds=%s LocalSpace=%s"),
				*Handle.GetUniqueInstanceName(),
				Handle.GetIsEnabled() ? TEXT("true") : TEXT("false"),
				*EnumToString(StaticEnum<ENiagaraEmitterMode>(), static_cast<int64>(Handle.GetEmitterMode())),
				EmitterData != nullptr
					? *EnumToString(StaticEnum<ENiagaraSimTarget>(), static_cast<int64>(EmitterData->SimTarget))
					: TEXT("none"),
				EmitterData != nullptr
					? *EnumToString(StaticEnum<ENiagaraEmitterCalculateBoundMode>(), static_cast<int64>(EmitterData->CalculateBoundsMode))
					: TEXT("none"),
				EmitterData != nullptr ? *EmitterData->FixedBounds.ToString() : TEXT("none"),
				EmitterData != nullptr && EmitterData->bLocalSpace ? TEXT("true") : TEXT("false"));

			if (EmitterData != nullptr)
			{
				TArray<UNiagaraScript*> Scripts;
				EmitterData->GetScripts(Scripts, false, false);
				for (const UNiagaraScript* Script : Scripts)
				{
					if (Script == nullptr)
					{
						continue;
					}
					LogParameterStore(*FString::Printf(TEXT("%s.%s"),
						*Handle.GetUniqueInstanceName(),
						*EnumToString(StaticEnum<ENiagaraScriptUsage>(), static_cast<int64>(Script->GetUsage()))),
						Script);
				}
			}
		}
	}

	static void LogComponent(const AGP_Worker* Worker, const UNiagaraComponent* Component, bool bEffectExpected)
	{
		if (Component == nullptr)
		{
			return;
		}

		const USceneComponent* Parent = Component->GetAttachParent();
		const FBoxSphereBounds Bounds = Component->CalcBounds(Component->GetComponentTransform());
		UE_LOG(LogGPMiningVFX, Log,
			TEXT("gp.Worker.DumpMiningVFX Component %s Asset=%s ExpectedActive=%s IsActive=%s IsComplete=%s Execution=%s Visible=%s HiddenInGame=%s AutoActivate=%s Parent=%s Bounds=%s"),
			*Component->GetName(),
			*GetPathNameSafe(Component->GetAsset()),
			bEffectExpected ? TEXT("true") : TEXT("false"),
			Component->IsActive() ? TEXT("true") : TEXT("false"),
			Component->IsComplete() ? TEXT("true") : TEXT("false"),
			*EnumToString(StaticEnum<ENiagaraExecutionState>(), static_cast<int64>(Component->GetExecutionState())),
			Component->IsVisible() ? TEXT("true") : TEXT("false"),
			Component->bHiddenInGame ? TEXT("true") : TEXT("false"),
			Component->bAutoActivate ? TEXT("true") : TEXT("false"),
			Parent != nullptr ? *Parent->GetName() : TEXT("none"),
			*Bounds.GetBox().ToString());
	}

	static void RunDump(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPMiningVFX, Warning, TEXT("gp.Worker.DumpMiningVFX: no world"));
			return;
		}

		int32 WorkerCount = 0;
		TSet<const UNiagaraSystem*> LoggedSystems;
		for (TActorIterator<AGP_Worker> It(World); It; ++It)
		{
			AGP_Worker* Worker = *It;
			if (!IsValid(Worker))
			{
				continue;
			}

			++WorkerCount;
			const UGP_MiningComponent* Mining = Worker->GetMiningComponent();
			const EGP_MiningState MiningState = IsValid(Mining)
				? Mining->GetMiningState()
				: EGP_MiningState::Invalid;
			const bool bEffectExpected = MiningState == EGP_MiningState::Mining;
			UE_LOG(LogGPMiningVFX, Log,
				TEXT("gp.Worker.DumpMiningVFX Worker %s Mining=%d EffectExpected=%s"),
				*Worker->GetName(),
				static_cast<int32>(MiningState),
				bEffectExpected ? TEXT("true") : TEXT("false"));

			TInlineComponentArray<UNiagaraComponent*> Components(Worker);
			UE_LOG(LogGPMiningVFX, Log, TEXT("gp.Worker.DumpMiningVFX NiagaraCount=%d"), Components.Num());
			for (const UNiagaraComponent* Component : Components)
			{
				LogComponent(Worker, Component, bEffectExpected);
				if (const UNiagaraSystem* System = Component != nullptr ? Component->GetAsset() : nullptr)
				{
					if (!LoggedSystems.Contains(System))
					{
						LoggedSystems.Add(System);
						LogSystemAsset(System);
					}
				}
			}
		}

		UE_LOG(LogGPMiningVFX, Log, TEXT("gp.Worker.DumpMiningVFX Complete Workers=%d"), WorkerCount);
	}

	static FAutoConsoleCommandWithWorldAndArgs GDumpMiningVFX(
		TEXT("gp.Worker.DumpMiningVFX"),
		TEXT("Log Worker mining state and the Niagara component/system lifecycle. Non-shipping diagnostic."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunDump));
}

#endif
