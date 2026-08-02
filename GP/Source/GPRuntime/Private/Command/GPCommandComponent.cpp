// Copyright Epic Games, Inc. All Rights Reserved.

#include "Command/GPCommandComponent.h"

#include "Command/GPCommandRequest.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Player/GPSelectionComponent.h"
#include "Tags/GPGameplayTags.h"
#include "Units/GPUnitBase.h"

namespace GPCommandPrivate
{
	static constexpr int32 MaxIssuingUnits = 24;
}

UGP_CommandComponent::UGP_CommandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UGP_CommandComponent::BuildSmartCommand(
	AActor* TargetActor,
	const FVector& TargetLocation,
	bool bQueue,
	FGP_CommandRequest& OutRequest) const
{
	OutRequest = FGP_CommandRequest{};

	const AGP_PlayerController* PlayerController = Cast<AGP_PlayerController>(GetOwner());
	if (PlayerController == nullptr)
	{
		return false;
	}

	const UGP_SelectionComponent* SelectionComponent = PlayerController->GetSelectionComponent();
	if (SelectionComponent == nullptr)
	{
		return false;
	}

	const AGP_PlayerState* GPPlayerState = PlayerController->GetPlayerState<AGP_PlayerState>();
	if (GPPlayerState == nullptr)
	{
		return false;
	}

	// Local playable TeamId: -1 unassigned, 0 neutral, 1+ playable (same as selection policy).
	const int32 LocalTeamId = GPPlayerState->GetTeamId();
	if (LocalTeamId < 1)
	{
		return false;
	}

	const TArray<TWeakObjectPtr<AGP_UnitBase>>& SelectedUnits = SelectionComponent->GetSelectedUnits();

	TArray<TObjectPtr<AGP_UnitBase>> NormalizedUnits;
	NormalizedUnits.Reserve(FMath::Min(SelectedUnits.Num(), GPCommandPrivate::MaxIssuingUnits));

	TSet<const AGP_UnitBase*> SeenUnits;
	SeenUnits.Reserve(NormalizedUnits.Max());

	for (const TWeakObjectPtr<AGP_UnitBase>& WeakUnit : SelectedUnits)
	{
		AGP_UnitBase* Unit = WeakUnit.Get();
		if (Unit == nullptr)
		{
			continue;
		}

		if (SeenUnits.Contains(Unit))
		{
			continue;
		}
		SeenUnits.Add(Unit);

		NormalizedUnits.Add(Unit);
		if (NormalizedUnits.Num() >= GPCommandPrivate::MaxIssuingUnits)
		{
			break;
		}
	}

	if (NormalizedUnits.Num() == 0)
	{
		return false;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	FGameplayTag CommandTag;
	AActor* RequestTargetActor = nullptr;

	if (TargetActor == nullptr)
	{
		CommandTag = GPTags.Command_Move;
	}
	else if (const AGP_UnitBase* TargetUnit = Cast<AGP_UnitBase>(TargetActor))
	{
		const int32 TargetTeamId = TargetUnit->GetTeamId();

		if (TargetTeamId >= 1 && TargetTeamId != LocalTeamId)
		{
			// Enemy (assigned, different team). Speculative — server must validate attackability.
			CommandTag = GPTags.Command_Attack;
			RequestTargetActor = TargetActor;
		}
		else if (TargetUnit->HasCapabilityTag(GPTags.Resource_Node))
		{
			// Canonical path: UnitBase CapabilityTags + GP.Resource.Node.
			CommandTag = GPTags.Command_Mine;
			RequestTargetActor = TargetActor;
		}
		else if (TargetTeamId >= 1 && TargetTeamId == LocalTeamId)
		{
			CommandTag = GPTags.Command_Move;
			RequestTargetActor = nullptr;
		}
		else
		{
			// Neutral (0) or unassigned (-1): speculative Attack. Server must validate.
			CommandTag = GPTags.Command_Attack;
			RequestTargetActor = TargetActor;
		}
	}
	else
	{
		// Non-UnitBase: no canonical Resource.Node accessor outside UnitBase CapabilityTags.
		// Mine mapping deferred for non-UnitBase actors. Deterministic Move fallback.
		CommandTag = GPTags.Command_Move;
		RequestTargetActor = nullptr;
	}

	if (!CommandTag.IsValid()
		|| NormalizedUnits.Num() < 1
		|| NormalizedUnits.Num() > GPCommandPrivate::MaxIssuingUnits)
	{
		OutRequest = FGP_CommandRequest{};
		return false;
	}

	OutRequest.CommandTag = CommandTag;
	OutRequest.IssuingUnits = MoveTemp(NormalizedUnits);
	OutRequest.TargetLocation = TargetLocation;
	OutRequest.TargetActor = RequestTargetActor;
	OutRequest.bQueue = bQueue;
	return true;
}
