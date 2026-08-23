// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPLaunchMenuPresenter.h"

#include "Buildings/GPMainBase.h"
#include "Game/GPGameState.h"
#include "Resources/GPStorageComponent.h"

bool UGP_LaunchMenuPresenter::Initialize(AGP_GameState* InGameState, int32 InLocalTeamId)
{
	Shutdown();
	if (!IsValid(InGameState) || InLocalTeamId < 1)
	{
		RebuildPresentation();
		return false;
	}

	BoundGameState = InGameState;
	LocalTeamId = InLocalTeamId;
	ResolvedMainBaseHandle = InGameState->OnResolvedMainBaseChanged.AddUObject(
		this, &ThisClass::HandleResolvedMainBaseChanged);
	BindLocalMainBaseStorage();
	RebuildPresentation();
	return true;
}

void UGP_LaunchMenuPresenter::Shutdown()
{
	if (AGP_GameState* GameState = BoundGameState.Get())
	{
		GameState->OnResolvedMainBaseChanged.Remove(ResolvedMainBaseHandle);
	}
	UnbindLocalMainBaseStorage();
	BoundGameState.Reset();
	LocalTeamId = -1;
	ResolvedMainBaseHandle.Reset();
	RebuildPresentation();
}

void UGP_LaunchMenuPresenter::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

int32 UGP_LaunchMenuPresenter::GetBoundDelegateCount() const
{
	return (ResolvedMainBaseHandle.IsValid() ? 1 : 0)
		+ (BoundStorage.IsValid() ? 1 : 0);
}

void UGP_LaunchMenuPresenter::BindLocalMainBaseStorage()
{
	UnbindLocalMainBaseStorage();
	AGP_GameState* GameState = BoundGameState.Get();
	if (GameState == nullptr || LocalTeamId < 1)
	{
		return;
	}

	AGP_MainBase* MainBase = GameState->FindMainBaseForTeamClientSafe(LocalTeamId);
	UGP_StorageComponent* Storage =
		IsValid(MainBase) ? MainBase->GetStorageComponent() : nullptr;
	if (!IsValid(Storage))
	{
		return;
	}

	Storage->OnStorageChanged.AddDynamic(this, &ThisClass::HandleStorageChanged);
	BoundStorage = Storage;
}

void UGP_LaunchMenuPresenter::UnbindLocalMainBaseStorage()
{
	if (UGP_StorageComponent* Storage = BoundStorage.Get())
	{
		Storage->OnStorageChanged.RemoveDynamic(this, &ThisClass::HandleStorageChanged);
	}
	BoundStorage.Reset();
}

void UGP_LaunchMenuPresenter::RebuildPresentation()
{
	Rows.Reset();
	ReadyLaunchContainerCount = 0;
	bCanLaunchReadyContainer = false;

	UGP_StorageComponent* Storage = BoundStorage.Get();
	if (IsValid(Storage))
	{
		const float Capacity = FMath::Max(0.0f, Storage->GetContainerCapacity());
		const TArray<FGP_StorageContainer>& Containers = Storage->GetContainers();
		Rows.Reserve(Containers.Num());
		for (int32 Index = 0; Index < Containers.Num(); ++Index)
		{
			const FGP_StorageContainer& Container = Containers[Index];
			FGP_LaunchContainerRow Row;
			Row.Index = Index;
			Row.StoredAmount = Container.CurrentAmount;
			Row.Capacity = Capacity;
			Row.FillNormalized = Capacity > KINDA_SMALL_NUMBER
				? FMath::Clamp(Container.CurrentAmount / Capacity, 0.0f, 1.0f)
				: 0.0f;
			Row.bIsReadyForLaunch = Container.State == EGP_StorageContainerState::Ready;
			Rows.Add(Row);
		}
		ReadyLaunchContainerCount = Storage->GetReadyCount();
		bCanLaunchReadyContainer = ReadyLaunchContainerCount > 0 && !Storage->IsLaunchInFlight();
	}

	OnLaunchMenuPresentationChanged.Broadcast();
}

void UGP_LaunchMenuPresenter::HandleResolvedMainBaseChanged(
	int32 TeamId,
	AGP_MainBase* Previous,
	AGP_MainBase* NewBase)
{
	(void)Previous;
	(void)NewBase;
	if (TeamId != LocalTeamId)
	{
		return;
	}
	BindLocalMainBaseStorage();
	RebuildPresentation();
}

void UGP_LaunchMenuPresenter::HandleStorageChanged(
	float PreviousTotalStored,
	float NewTotalStored,
	float TotalCapacity)
{
	(void)PreviousTotalStored;
	(void)NewTotalStored;
	(void)TotalCapacity;
	RebuildPresentation();
}
