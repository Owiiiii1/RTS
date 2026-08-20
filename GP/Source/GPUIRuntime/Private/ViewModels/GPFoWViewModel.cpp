// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPFoWViewModel.h"

#include "FogOfWar/GPLocalFoWComponent.h"

EGP_FoWState UGP_FoWViewModel::GetStateAtWorldLocation(const FVector& WorldLocation) const
{
	const UGP_LocalFoWComponent* Mirror = BoundMirror.Get();
	return Mirror != nullptr
		? Mirror->GetStateAtWorldLocation(WorldLocation)
		: EGP_FoWState::Unexplored;
}

void UGP_FoWViewModel::RefreshFromMirror(UGP_LocalFoWComponent* Mirror)
{
	BoundMirror = Mirror;
	const bool bReady = Mirror != nullptr && Mirror->IsReady();

	UE_MVVM_SET_PROPERTY_VALUE(LocalTeamId, bReady ? Mirror->GetLocalTeamId() : -1);
	UE_MVVM_SET_PROPERTY_VALUE(
		GridOrigin,
		bReady ? Mirror->GetGridOriginWorldXY() : FVector2D::ZeroVector);
	UE_MVVM_SET_PROPERTY_VALUE(
		GridDimensions,
		bReady ? Mirror->GetGridDimensions() : FIntPoint::ZeroValue);
	UE_MVVM_SET_PROPERTY_VALUE(CellSizeCm, bReady ? Mirror->GetCellSizeCm() : 0.0f);
	UE_MVVM_SET_PROPERTY_VALUE(Revision, bReady ? Mirror->GetRevision() : 0);
	UE_MVVM_SET_PROPERTY_VALUE(bIsReady, bReady);
}
