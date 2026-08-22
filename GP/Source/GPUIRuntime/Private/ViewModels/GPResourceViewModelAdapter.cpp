// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPResourceViewModelAdapter.h"

#include "AbilitySystemComponent.h"
#include "AttributeSets/GPPlayerAttributeSet.h"
#include "Buildings/GPMainBase.h"
#include "Game/GPGameState.h"
#include "Player/GPPlayerState.h"
#include "Resources/GPStorageComponent.h"
#include "ViewModels/GPResourceViewModel.h"

bool UGP_ResourceViewModelAdapter::Initialize(
	UGP_ResourceViewModel* InViewModel,
	AGP_PlayerState* InLocalPlayerState,
	AGP_PlayerState* InOpponentPlayerState,
	AGP_GameState* InGameState,
	int32 InLocalTeamId)
{
	Shutdown();
	ViewModel = InViewModel;
	if (ViewModel == nullptr || !IsValid(InLocalPlayerState))
	{
		return false;
	}

	UAbilitySystemComponent* LocalASC = InLocalPlayerState->GetAbilitySystemComponent();
	if (!IsValid(LocalASC))
	{
		return false;
	}

	BoundLocalASC = LocalASC;
	BoundGameState = InGameState;
	LocalTeamId = InLocalTeamId;
	OrbitalFerroniteHandle = LocalASC->GetGameplayAttributeValueChangeDelegate(
		UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute()).AddUObject(
			this, &ThisClass::HandleOrbitalFerroniteChanged);
	FerroniteScoreHandle = LocalASC->GetGameplayAttributeValueChangeDelegate(
		UGP_PlayerAttributeSet::GetFerroniteScoreAttribute()).AddUObject(
			this, &ThisClass::HandleFerroniteScoreChanged);
	CurrentUnitsHandle = LocalASC->GetGameplayAttributeValueChangeDelegate(
		UGP_PlayerAttributeSet::GetCurrentUnitsAttribute()).AddUObject(
			this, &ThisClass::HandleCurrentUnitsChanged);
	MaxUnitsHandle = LocalASC->GetGameplayAttributeValueChangeDelegate(
		UGP_PlayerAttributeSet::GetMaxUnitsAttribute()).AddUObject(
			this, &ThisClass::HandleMaxUnitsChanged);

	if (IsValid(InOpponentPlayerState))
	{
		if (UAbilitySystemComponent* OpponentASC = InOpponentPlayerState->GetAbilitySystemComponent())
		{
			BoundOpponentASC = OpponentASC;
			OpponentFerroniteScoreHandle = OpponentASC->GetGameplayAttributeValueChangeDelegate(
				UGP_PlayerAttributeSet::GetFerroniteScoreAttribute()).AddUObject(
					this, &ThisClass::HandleOpponentFerroniteScoreChanged);
		}
	}

	if (IsValid(InGameState))
	{
		ResolvedMainBaseHandle = InGameState->OnResolvedMainBaseChanged.AddUObject(
			this, &ThisClass::HandleResolvedMainBaseChanged);
		BindLocalMainBaseStorage();
	}

	RefreshSnapshot();
	return true;
}

void UGP_ResourceViewModelAdapter::Shutdown()
{
	if (UAbilitySystemComponent* LocalASC = BoundLocalASC.Get())
	{
		LocalASC->GetGameplayAttributeValueChangeDelegate(
			UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute()).Remove(OrbitalFerroniteHandle);
		LocalASC->GetGameplayAttributeValueChangeDelegate(
			UGP_PlayerAttributeSet::GetFerroniteScoreAttribute()).Remove(FerroniteScoreHandle);
		LocalASC->GetGameplayAttributeValueChangeDelegate(
			UGP_PlayerAttributeSet::GetCurrentUnitsAttribute()).Remove(CurrentUnitsHandle);
		LocalASC->GetGameplayAttributeValueChangeDelegate(
			UGP_PlayerAttributeSet::GetMaxUnitsAttribute()).Remove(MaxUnitsHandle);
	}
	if (UAbilitySystemComponent* OpponentASC = BoundOpponentASC.Get())
	{
		OpponentASC->GetGameplayAttributeValueChangeDelegate(
			UGP_PlayerAttributeSet::GetFerroniteScoreAttribute()).Remove(OpponentFerroniteScoreHandle);
	}
	if (AGP_GameState* GameState = BoundGameState.Get())
	{
		GameState->OnResolvedMainBaseChanged.Remove(ResolvedMainBaseHandle);
	}
	UnbindLocalMainBaseStorage();
	if (ViewModel != nullptr)
	{
		ViewModel->SetPlanetFerronite(0.0f);
	}

	BoundLocalASC.Reset();
	BoundOpponentASC.Reset();
	BoundGameState.Reset();
	LocalTeamId = -1;
	OrbitalFerroniteHandle.Reset();
	FerroniteScoreHandle.Reset();
	CurrentUnitsHandle.Reset();
	MaxUnitsHandle.Reset();
	OpponentFerroniteScoreHandle.Reset();
	ResolvedMainBaseHandle.Reset();
}

void UGP_ResourceViewModelAdapter::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UGP_ResourceViewModelAdapter::RefreshSnapshot()
{
	if (ViewModel == nullptr)
	{
		return;
	}

	if (const UAbilitySystemComponent* LocalASC = BoundLocalASC.Get())
	{
		ViewModel->SetOrbitalFerronite(LocalASC->GetNumericAttribute(
			UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute()));
		ViewModel->SetFerroniteScore(LocalASC->GetNumericAttribute(
			UGP_PlayerAttributeSet::GetFerroniteScoreAttribute()));
		ViewModel->SetCurrentUnits(LocalASC->GetNumericAttribute(
			UGP_PlayerAttributeSet::GetCurrentUnitsAttribute()));
		ViewModel->SetMaxUnits(LocalASC->GetNumericAttribute(
			UGP_PlayerAttributeSet::GetMaxUnitsAttribute()));
	}

	const UAbilitySystemComponent* OpponentASC = BoundOpponentASC.Get();
	ViewModel->SetOpponentFerroniteScore(
		OpponentASC != nullptr
			? OpponentASC->GetNumericAttribute(UGP_PlayerAttributeSet::GetFerroniteScoreAttribute())
			: 0.0f);
	RefreshPlanetFerronite();
}

void UGP_ResourceViewModelAdapter::ApplyOwnAttribute(
	const FGameplayAttribute& Attribute,
	float NewValue)
{
	if (ViewModel == nullptr)
	{
		return;
	}

	if (Attribute == UGP_PlayerAttributeSet::GetOrbitalFerroniteAttribute())
	{
		ViewModel->SetOrbitalFerronite(NewValue);
	}
	else if (Attribute == UGP_PlayerAttributeSet::GetFerroniteScoreAttribute())
	{
		ViewModel->SetFerroniteScore(NewValue);
	}
	else if (Attribute == UGP_PlayerAttributeSet::GetCurrentUnitsAttribute())
	{
		ViewModel->SetCurrentUnits(NewValue);
	}
	else if (Attribute == UGP_PlayerAttributeSet::GetMaxUnitsAttribute())
	{
		ViewModel->SetMaxUnits(NewValue);
	}
}

void UGP_ResourceViewModelAdapter::HandleOrbitalFerroniteChanged(const FOnAttributeChangeData& Data)
{
	ApplyOwnAttribute(Data.Attribute, Data.NewValue);
}

void UGP_ResourceViewModelAdapter::HandleFerroniteScoreChanged(const FOnAttributeChangeData& Data)
{
	ApplyOwnAttribute(Data.Attribute, Data.NewValue);
}

void UGP_ResourceViewModelAdapter::HandleCurrentUnitsChanged(const FOnAttributeChangeData& Data)
{
	ApplyOwnAttribute(Data.Attribute, Data.NewValue);
}

void UGP_ResourceViewModelAdapter::HandleMaxUnitsChanged(const FOnAttributeChangeData& Data)
{
	ApplyOwnAttribute(Data.Attribute, Data.NewValue);
}

void UGP_ResourceViewModelAdapter::HandleOpponentFerroniteScoreChanged(
	const FOnAttributeChangeData& Data)
{
	if (ViewModel != nullptr)
	{
		ViewModel->SetOpponentFerroniteScore(Data.NewValue);
	}
}

int32 UGP_ResourceViewModelAdapter::GetBoundDelegateCount() const
{
	return (OrbitalFerroniteHandle.IsValid() ? 1 : 0)
		+ (FerroniteScoreHandle.IsValid() ? 1 : 0)
		+ (CurrentUnitsHandle.IsValid() ? 1 : 0)
		+ (MaxUnitsHandle.IsValid() ? 1 : 0)
		+ (OpponentFerroniteScoreHandle.IsValid() ? 1 : 0)
		+ (ResolvedMainBaseHandle.IsValid() ? 1 : 0)
		+ (BoundStorage.IsValid() ? 1 : 0);
}

void UGP_ResourceViewModelAdapter::BindLocalMainBaseStorage()
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

void UGP_ResourceViewModelAdapter::UnbindLocalMainBaseStorage()
{
	if (UGP_StorageComponent* Storage = BoundStorage.Get())
	{
		Storage->OnStorageChanged.RemoveDynamic(this, &ThisClass::HandleStorageChanged);
	}
	BoundStorage.Reset();
}

void UGP_ResourceViewModelAdapter::RefreshPlanetFerronite()
{
	if (ViewModel == nullptr)
	{
		return;
	}

	UGP_StorageComponent* Storage = BoundStorage.Get();
	const float Stored = IsValid(Storage) ? Storage->GetTotalStored() : 0.0f;
	ViewModel->SetPlanetFerronite(Stored);
}

void UGP_ResourceViewModelAdapter::HandleResolvedMainBaseChanged(
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
	RefreshPlanetFerronite();
}

void UGP_ResourceViewModelAdapter::HandleStorageChanged(
	float PreviousTotalStored,
	float NewTotalStored,
	float TotalCapacity)
{
	(void)PreviousTotalStored;
	(void)NewTotalStored;
	(void)TotalCapacity;
	RefreshPlanetFerronite();
}

#if !UE_BUILD_SHIPPING
void UGP_ResourceViewModelAdapter::InitializeForContract(UGP_ResourceViewModel* InViewModel)
{
	Shutdown();
	ViewModel = InViewModel;
}

void UGP_ResourceViewModelAdapter::ApplyOwnAttributeForContract(
	const FGameplayAttribute& Attribute,
	float NewValue)
{
	ApplyOwnAttribute(Attribute, NewValue);
}

void UGP_ResourceViewModelAdapter::ApplyOpponentScoreForContract(float NewValue)
{
	if (ViewModel != nullptr)
	{
		ViewModel->SetOpponentFerroniteScore(NewValue);
	}
}
#endif
