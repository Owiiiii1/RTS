// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GPLocalFoWUnitPresentationSubsystem.generated.h"

class AGP_UnitBase;
class UGP_LocalFoWComponent;

/**
 * Local presentation-only registry for FoW-gating replicated unit/building visuals.
 *
 * It never discovers actors. UnitBase instances register/unregister through lifecycle hooks. Mirror
 * updates evaluate immediately; a bounded 10 Hz pass catches actor movement across a static FoW edge.
 */
UCLASS()
class GPRUNTIME_API UGP_LocalFoWUnitPresentationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void RegisterUnit(AGP_UnitBase* Unit);
	void UnregisterUnit(AGP_UnitBase* Unit);
	void NotifyUnitTeamChanged(AGP_UnitBase* Unit);

	static bool ShouldPresentUnitForLocalPlayer(
		const AGP_UnitBase* Unit,
		int32 LocalTeamId,
		const UGP_LocalFoWComponent* LocalFoW);
	static void ApplyUnitPresentationForLocalPlayer(
		AGP_UnitBase* Unit,
		int32 LocalTeamId,
		const UGP_LocalFoWComponent* LocalFoW);

	int32 GetRegisteredUnitCount() const { return RegisteredUnits.Num(); }
	static constexpr float GetEvaluationIntervalSeconds() { return 0.1f; }

private:
	void EvaluateRegisteredUnits();
	void EvaluateUnit(AGP_UnitBase* Unit);
	void RefreshLocalMirrorBinding();
	void UnbindLocalMirror();
	void HandleLocalFoWUpdated(UGP_LocalFoWComponent* UpdatedMirror);
	int32 ResolveLocalTeamId() const;

	TArray<TWeakObjectPtr<AGP_UnitBase>> RegisteredUnits;
	TWeakObjectPtr<UGP_LocalFoWComponent> BoundLocalFoW;
	FDelegateHandle LocalFoWUpdatedHandle;
	FTimerHandle EvaluationTimerHandle;
};
