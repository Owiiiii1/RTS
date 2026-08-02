// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/StaticArray.h"
#include "Units/GPUnitBase.h"
#include "GPSelectionComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FGPOnSelectionChanged);

/**
 * Local-only selection / inspect / marquee / control-group state.
 * Owned by AGP_PlayerController. No tick, replication, RPC, hit-testing, or commands.
 */
UCLASS(ClassGroup = (GP), meta = (BlueprintSpawnableComponent))
class GPRUNTIME_API UGP_SelectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGP_SelectionComponent();

	FGPOnSelectionChanged& OnSelectionChanged();

	const TArray<TWeakObjectPtr<AGP_UnitBase>>& GetSelectedUnits() const;
	AActor* GetInspectedTarget() const;

	int32 GetSelectionCount() const;
	bool HasSelection() const;

	bool IsMarqueeActive() const;
	FVector2D GetMarqueeStart() const;
	FVector2D GetMarqueeCurrent() const;

	static constexpr int32 GetMaxSelectionCount()
	{
		return MaxSelectionCount;
	}

	void ClearSelection();
	void ClearInspectedTarget();
	void ClearAllSelectionState();
	void SetInspectedTarget(AActor* NewTarget);

	void BeginMarquee(const FVector2D& ScreenStart);
	void UpdateMarquee(const FVector2D& ScreenCurrent);
	void EndMarquee();
	void CancelMarquee();

	void AssignControlGroup(int32 GroupNumber);
	void AppendToControlGroup(int32 GroupNumber);
	void RecallControlGroup(int32 GroupNumber);
	void AppendControlGroupToSelection(int32 GroupNumber);
	void ClearControlGroup(int32 GroupNumber);

	void PruneInvalidEntries();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	static constexpr int32 MaxSelectionCount = 24;
	static constexpr int32 ControlGroupCount = 9;

	bool IsLocalSelectionContext() const;
	void NotifySelectionChanged();
	bool TryResolveControlGroupIndex(int32 GroupNumber, int32& OutIndex) const;

	static bool AreWeakUnitArraysEqual(
		const TArray<TWeakObjectPtr<AGP_UnitBase>>& A,
		const TArray<TWeakObjectPtr<AGP_UnitBase>>& B);

	static void PruneAndClampUnitArray(TArray<TWeakObjectPtr<AGP_UnitBase>>& Units);

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AGP_UnitBase>> SelectedUnits;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> InspectedTarget;

	UPROPERTY(Transient)
	bool bMarqueeActive = false;

	UPROPERTY(Transient)
	FVector2D MarqueeStart = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D MarqueeCurrent = FVector2D::ZeroVector;

	/** Non-reflected: nested TStaticArray of TArray is not UPROPERTY-safe here. */
	TStaticArray<TArray<TWeakObjectPtr<AGP_UnitBase>>, ControlGroupCount> ControlGroups;
	TStaticArray<double, ControlGroupCount> LastGroupRecallTimes;

	FGPOnSelectionChanged SelectionChangedDelegate;
};
