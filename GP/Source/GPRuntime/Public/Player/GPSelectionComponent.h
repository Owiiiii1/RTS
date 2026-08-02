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
 *
 * Phase B1 mutation APIs canonicalize containers (order, dedupe, cap 24, local-only)
 * but do not authorize gameplay eligibility (TeamId, CapabilityTags, building rules, FoW).
 * The input layer must pass already-validated actors.
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
	bool IsUnitSelected(const AGP_UnitBase* Unit) const;

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

	/** Container-only: replace selection with exactly Unit, or clear if null/invalid. */
	void ReplaceSelectionWithUnit(AGP_UnitBase* Unit);

	/** Container-only: append Unit if valid, not duplicate, and under cap. */
	bool AddUnitToSelection(AGP_UnitBase* Unit);

	/** Container-only: remove Unit by actor identity if present. */
	bool RemoveUnitFromSelection(AGP_UnitBase* Unit);

	/**
	 * Container-only toggle.
	 * @return true if Unit is selected after the call; false if not selected or impossible.
	 */
	bool ToggleUnitSelection(AGP_UnitBase* Unit);

	/** Container-only: replace selection with a canonicalized copy of Units. */
	void SetSelectionFromUnits(const TArray<TWeakObjectPtr<AGP_UnitBase>>& Units);

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
	int32 FindSelectedUnitIndex(const AGP_UnitBase* Unit) const;

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
