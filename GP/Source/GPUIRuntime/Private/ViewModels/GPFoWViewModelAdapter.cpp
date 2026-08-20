// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPFoWViewModelAdapter.h"

#include "FogOfWar/GPLocalFoWComponent.h"
#include "Player/GPPlayerController.h"
#include "ViewModels/GPFoWViewModel.h"

UGP_FoWViewModelAdapter* UGP_FoWViewModelAdapter::CreateForPlayerController(
	AGP_PlayerController* PlayerController)
{
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	UGP_FoWViewModelAdapter* Adapter =
		NewObject<UGP_FoWViewModelAdapter>(PlayerController);
	return Adapter != nullptr
		&& Adapter->InitializeWithMirror(PlayerController->GetLocalFogOfWarComponent())
		? Adapter
		: nullptr;
}

bool UGP_FoWViewModelAdapter::InitializeWithMirror(UGP_LocalFoWComponent* Mirror)
{
	Shutdown();
	if (!IsValid(Mirror))
	{
		return false;
	}

	if (ViewModel == nullptr)
	{
		ViewModel = NewObject<UGP_FoWViewModel>(this);
	}
	if (ViewModel == nullptr)
	{
		return false;
	}

	BoundMirror = Mirror;
	MirrorUpdatedHandle = Mirror->OnLocalFoWUpdated.AddUObject(
		this,
		&ThisClass::HandleMirrorUpdated);
	ViewModel->RefreshFromMirror(Mirror);
	return true;
}

void UGP_FoWViewModelAdapter::Shutdown()
{
	if (UGP_LocalFoWComponent* Mirror = BoundMirror.Get())
	{
		if (MirrorUpdatedHandle.IsValid())
		{
			Mirror->OnLocalFoWUpdated.Remove(MirrorUpdatedHandle);
		}
	}
	MirrorUpdatedHandle.Reset();
	BoundMirror.Reset();
	if (ViewModel != nullptr)
	{
		ViewModel->RefreshFromMirror(nullptr);
	}
}

void UGP_FoWViewModelAdapter::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UGP_FoWViewModelAdapter::HandleMirrorUpdated(UGP_LocalFoWComponent* Mirror)
{
	if (ViewModel != nullptr && Mirror == BoundMirror.Get())
	{
		ViewModel->RefreshFromMirror(Mirror);
	}
}
