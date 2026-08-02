// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/GPSelectionComponent.h"

#include "Player/GPPlayerController.h"

UGP_SelectionComponent::UGP_SelectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

	for (int32 Index = 0; Index < ControlGroupCount; ++Index)
	{
		LastGroupRecallTimes[Index] = -DBL_MAX;
	}
}

FGPOnSelectionChanged& UGP_SelectionComponent::OnSelectionChanged()
{
	return SelectionChangedDelegate;
}

const TArray<TWeakObjectPtr<AGP_UnitBase>>& UGP_SelectionComponent::GetSelectedUnits() const
{
	return SelectedUnits;
}

AActor* UGP_SelectionComponent::GetInspectedTarget() const
{
	return InspectedTarget.Get();
}

int32 UGP_SelectionComponent::GetSelectionCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AGP_UnitBase>& Unit : SelectedUnits)
	{
		if (Unit.IsValid())
		{
			++Count;
		}
	}
	return Count;
}

bool UGP_SelectionComponent::HasSelection() const
{
	return GetSelectionCount() > 0;
}

bool UGP_SelectionComponent::IsUnitSelected(const AGP_UnitBase* Unit) const
{
	return FindSelectedUnitIndex(Unit) != INDEX_NONE;
}

bool UGP_SelectionComponent::IsMarqueeActive() const
{
	return bMarqueeActive;
}

FVector2D UGP_SelectionComponent::GetMarqueeStart() const
{
	return MarqueeStart;
}

FVector2D UGP_SelectionComponent::GetMarqueeCurrent() const
{
	return MarqueeCurrent;
}

void UGP_SelectionComponent::ClearSelection()
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	if (SelectedUnits.Num() == 0)
	{
		return;
	}

	const bool bHadValidSelection = HasSelection();
	SelectedUnits.Reset();
	if (bHadValidSelection)
	{
		NotifySelectionChanged();
	}
}

void UGP_SelectionComponent::ClearInspectedTarget()
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	if (!InspectedTarget.IsValid() && !InspectedTarget.IsStale())
	{
		return;
	}

	InspectedTarget.Reset();
	NotifySelectionChanged();
}

void UGP_SelectionComponent::ClearAllSelectionState()
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	const bool bHadValidSelection = HasSelection();
	const bool bHadInspect = InspectedTarget.IsValid() || InspectedTarget.IsStale();

	SelectedUnits.Reset();
	InspectedTarget.Reset();
	bMarqueeActive = false;
	MarqueeStart = FVector2D::ZeroVector;
	MarqueeCurrent = FVector2D::ZeroVector;

	if (bHadValidSelection || bHadInspect)
	{
		NotifySelectionChanged();
	}
}

void UGP_SelectionComponent::SetInspectedTarget(AActor* NewTarget)
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	AActor* Current = InspectedTarget.Get();
	if (NewTarget == nullptr)
	{
		if (Current == nullptr && !InspectedTarget.IsStale())
		{
			return;
		}
		InspectedTarget.Reset();
		NotifySelectionChanged();
		return;
	}

	if (Current == NewTarget)
	{
		return;
	}

	InspectedTarget = NewTarget;
	NotifySelectionChanged();
}

void UGP_SelectionComponent::ReplaceSelectionWithUnit(AGP_UnitBase* Unit)
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	TArray<TWeakObjectPtr<AGP_UnitBase>> Previous = SelectedUnits;
	PruneAndClampUnitArray(Previous);

	SelectedUnits.Reset();
	if (IsValid(Unit))
	{
		SelectedUnits.Add(Unit);
	}

	if (!AreWeakUnitArraysEqual(Previous, SelectedUnits))
	{
		NotifySelectionChanged();
	}
}

bool UGP_SelectionComponent::AddUnitToSelection(AGP_UnitBase* Unit)
{
	if (!IsLocalSelectionContext())
	{
		return false;
	}

	TArray<TWeakObjectPtr<AGP_UnitBase>> Previous = SelectedUnits;
	PruneAndClampUnitArray(Previous);

	PruneAndClampUnitArray(SelectedUnits);

	bool bAdded = false;
	if (IsValid(Unit)
		&& FindSelectedUnitIndex(Unit) == INDEX_NONE
		&& SelectedUnits.Num() < MaxSelectionCount)
	{
		SelectedUnits.Add(Unit);
		bAdded = true;
	}

	if (!AreWeakUnitArraysEqual(Previous, SelectedUnits))
	{
		NotifySelectionChanged();
	}

	return bAdded;
}

bool UGP_SelectionComponent::RemoveUnitFromSelection(AGP_UnitBase* Unit)
{
	if (!IsLocalSelectionContext())
	{
		return false;
	}

	TArray<TWeakObjectPtr<AGP_UnitBase>> Previous = SelectedUnits;
	PruneAndClampUnitArray(Previous);

	PruneAndClampUnitArray(SelectedUnits);

	bool bRemoved = false;
	if (IsValid(Unit))
	{
		const int32 Index = FindSelectedUnitIndex(Unit);
		if (Index != INDEX_NONE)
		{
			SelectedUnits.RemoveAt(Index);
			bRemoved = true;
		}
	}

	if (!AreWeakUnitArraysEqual(Previous, SelectedUnits))
	{
		NotifySelectionChanged();
	}

	return bRemoved;
}

bool UGP_SelectionComponent::ToggleUnitSelection(AGP_UnitBase* Unit)
{
	if (!IsLocalSelectionContext() || !IsValid(Unit))
	{
		return false;
	}

	TArray<TWeakObjectPtr<AGP_UnitBase>> Previous = SelectedUnits;
	PruneAndClampUnitArray(Previous);

	PruneAndClampUnitArray(SelectedUnits);

	const int32 Index = FindSelectedUnitIndex(Unit);
	if (Index != INDEX_NONE)
	{
		SelectedUnits.RemoveAt(Index);
		if (!AreWeakUnitArraysEqual(Previous, SelectedUnits))
		{
			NotifySelectionChanged();
		}
		return false;
	}

	if (SelectedUnits.Num() >= MaxSelectionCount)
	{
		if (!AreWeakUnitArraysEqual(Previous, SelectedUnits))
		{
			NotifySelectionChanged();
		}
		return false;
	}

	SelectedUnits.Add(Unit);
	if (!AreWeakUnitArraysEqual(Previous, SelectedUnits))
	{
		NotifySelectionChanged();
	}
	return true;
}

void UGP_SelectionComponent::SetSelectionFromUnits(
	const TArray<TWeakObjectPtr<AGP_UnitBase>>& Units)
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	TArray<TWeakObjectPtr<AGP_UnitBase>> Previous = SelectedUnits;
	PruneAndClampUnitArray(Previous);

	TArray<TWeakObjectPtr<AGP_UnitBase>> Incoming = Units;
	PruneAndClampUnitArray(Incoming);

	SelectedUnits = MoveTemp(Incoming);

	if (!AreWeakUnitArraysEqual(Previous, SelectedUnits))
	{
		NotifySelectionChanged();
	}
}

void UGP_SelectionComponent::BeginMarquee(const FVector2D& ScreenStart)
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	bMarqueeActive = true;
	MarqueeStart = ScreenStart;
	MarqueeCurrent = ScreenStart;
}

void UGP_SelectionComponent::UpdateMarquee(const FVector2D& ScreenCurrent)
{
	if (!IsLocalSelectionContext() || !bMarqueeActive)
	{
		return;
	}

	MarqueeCurrent = ScreenCurrent;
}

void UGP_SelectionComponent::EndMarquee()
{
	if (!IsLocalSelectionContext() || !bMarqueeActive)
	{
		return;
	}

	// Keep last start/current for readers; no hit-test / selection mutation in Phase A.
	bMarqueeActive = false;
}

void UGP_SelectionComponent::CancelMarquee()
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	bMarqueeActive = false;
	MarqueeStart = FVector2D::ZeroVector;
	MarqueeCurrent = FVector2D::ZeroVector;
}

void UGP_SelectionComponent::AssignControlGroup(int32 GroupNumber)
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	int32 GroupIndex = INDEX_NONE;
	if (!TryResolveControlGroupIndex(GroupNumber, GroupIndex))
	{
		return;
	}

	PruneAndClampUnitArray(SelectedUnits);
	ControlGroups[GroupIndex] = SelectedUnits;
}

void UGP_SelectionComponent::AppendToControlGroup(int32 GroupNumber)
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	int32 GroupIndex = INDEX_NONE;
	if (!TryResolveControlGroupIndex(GroupNumber, GroupIndex))
	{
		return;
	}

	PruneAndClampUnitArray(SelectedUnits);
	PruneAndClampUnitArray(ControlGroups[GroupIndex]);

	for (const TWeakObjectPtr<AGP_UnitBase>& Unit : SelectedUnits)
	{
		if (!Unit.IsValid())
		{
			continue;
		}

		bool bAlreadyPresent = false;
		for (const TWeakObjectPtr<AGP_UnitBase>& Existing : ControlGroups[GroupIndex])
		{
			if (Existing.Get() == Unit.Get())
			{
				bAlreadyPresent = true;
				break;
			}
		}

		if (!bAlreadyPresent)
		{
			ControlGroups[GroupIndex].Add(Unit);
		}
	}

	PruneAndClampUnitArray(ControlGroups[GroupIndex]);
}

void UGP_SelectionComponent::RecallControlGroup(int32 GroupNumber)
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	int32 GroupIndex = INDEX_NONE;
	if (!TryResolveControlGroupIndex(GroupNumber, GroupIndex))
	{
		return;
	}

	PruneAndClampUnitArray(ControlGroups[GroupIndex]);

	TArray<TWeakObjectPtr<AGP_UnitBase>> Previous = SelectedUnits;
	PruneAndClampUnitArray(Previous);

	SelectedUnits = ControlGroups[GroupIndex];
	PruneAndClampUnitArray(SelectedUnits);

	if (const UWorld* World = GetWorld())
	{
		LastGroupRecallTimes[GroupIndex] = static_cast<double>(World->GetTimeSeconds());
	}

	if (!AreWeakUnitArraysEqual(Previous, SelectedUnits))
	{
		NotifySelectionChanged();
	}
}

void UGP_SelectionComponent::AppendControlGroupToSelection(int32 GroupNumber)
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	int32 GroupIndex = INDEX_NONE;
	if (!TryResolveControlGroupIndex(GroupNumber, GroupIndex))
	{
		return;
	}

	PruneAndClampUnitArray(SelectedUnits);
	PruneAndClampUnitArray(ControlGroups[GroupIndex]);

	TArray<TWeakObjectPtr<AGP_UnitBase>> Previous = SelectedUnits;

	for (const TWeakObjectPtr<AGP_UnitBase>& Unit : ControlGroups[GroupIndex])
	{
		if (!Unit.IsValid())
		{
			continue;
		}

		bool bAlreadyPresent = false;
		for (const TWeakObjectPtr<AGP_UnitBase>& Existing : SelectedUnits)
		{
			if (Existing.Get() == Unit.Get())
			{
				bAlreadyPresent = true;
				break;
			}
		}

		if (!bAlreadyPresent)
		{
			SelectedUnits.Add(Unit);
		}
	}

	PruneAndClampUnitArray(SelectedUnits);

	if (!AreWeakUnitArraysEqual(Previous, SelectedUnits))
	{
		NotifySelectionChanged();
	}
}

void UGP_SelectionComponent::ClearControlGroup(int32 GroupNumber)
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	int32 GroupIndex = INDEX_NONE;
	if (!TryResolveControlGroupIndex(GroupNumber, GroupIndex))
	{
		return;
	}

	ControlGroups[GroupIndex].Reset();
}

void UGP_SelectionComponent::PruneInvalidEntries()
{
	if (!IsLocalSelectionContext())
	{
		return;
	}

	TArray<TWeakObjectPtr<AGP_UnitBase>> PreviousSelection = SelectedUnits;
	PruneAndClampUnitArray(PreviousSelection);

	const bool bInspectWasStale = InspectedTarget.IsStale();

	PruneAndClampUnitArray(SelectedUnits);

	if (bInspectWasStale)
	{
		InspectedTarget.Reset();
	}

	for (int32 Index = 0; Index < ControlGroupCount; ++Index)
	{
		PruneAndClampUnitArray(ControlGroups[Index]);
	}

	if (!AreWeakUnitArraysEqual(PreviousSelection, SelectedUnits) || bInspectWasStale)
	{
		NotifySelectionChanged();
	}
}

void UGP_SelectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SelectedUnits.Reset();
	InspectedTarget.Reset();

	for (int32 Index = 0; Index < ControlGroupCount; ++Index)
	{
		ControlGroups[Index].Reset();
		LastGroupRecallTimes[Index] = -DBL_MAX;
	}

	bMarqueeActive = false;
	MarqueeStart = FVector2D::ZeroVector;
	MarqueeCurrent = FVector2D::ZeroVector;

	SelectionChangedDelegate.Clear();

	Super::EndPlay(EndPlayReason);
}

bool UGP_SelectionComponent::IsLocalSelectionContext() const
{
	const AGP_PlayerController* OwnerPC = Cast<AGP_PlayerController>(GetOwner());
	return OwnerPC != nullptr && OwnerPC->IsLocalController();
}

void UGP_SelectionComponent::NotifySelectionChanged()
{
	SelectionChangedDelegate.Broadcast();
}

bool UGP_SelectionComponent::TryResolveControlGroupIndex(int32 GroupNumber, int32& OutIndex) const
{
	if (GroupNumber < 1 || GroupNumber > ControlGroupCount)
	{
		return false;
	}

	OutIndex = GroupNumber - 1;
	return true;
}

int32 UGP_SelectionComponent::FindSelectedUnitIndex(const AGP_UnitBase* Unit) const
{
	if (Unit == nullptr)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < SelectedUnits.Num(); ++Index)
	{
		if (SelectedUnits[Index].Get() == Unit)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool UGP_SelectionComponent::AreWeakUnitArraysEqual(
	const TArray<TWeakObjectPtr<AGP_UnitBase>>& A,
	const TArray<TWeakObjectPtr<AGP_UnitBase>>& B)
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		if (A[Index].Get() != B[Index].Get())
		{
			return false;
		}
	}

	return true;
}

void UGP_SelectionComponent::PruneAndClampUnitArray(TArray<TWeakObjectPtr<AGP_UnitBase>>& Units)
{
	TArray<TWeakObjectPtr<AGP_UnitBase>> Compacted;
	Compacted.Reserve(FMath::Min(Units.Num(), MaxSelectionCount));

	for (const TWeakObjectPtr<AGP_UnitBase>& Unit : Units)
	{
		if (!Unit.IsValid())
		{
			continue;
		}

		bool bAlreadyPresent = false;
		for (const TWeakObjectPtr<AGP_UnitBase>& Existing : Compacted)
		{
			if (Existing.Get() == Unit.Get())
			{
				bAlreadyPresent = true;
				break;
			}
		}

		if (!bAlreadyPresent)
		{
			Compacted.Add(Unit);
			if (Compacted.Num() >= MaxSelectionCount)
			{
				break;
			}
		}
	}

	Units = MoveTemp(Compacted);
}
