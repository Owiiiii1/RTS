// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPMatchViewModelAdapter.h"

#include "Buildings/GPMainBase.h"
#include "Game/GPGameState.h"
#include "Game/GPMatchResult.h"
#include "Resources/GPStorageComponent.h"
#include "ViewModels/GPMatchViewModel.h"

float UGP_MatchViewModelAdapter::ComputeThreatPresentationMax(
	float TotalCapacity,
	float ThreatPerStoredUnit)
{
	if (!FMath::IsFinite(TotalCapacity) || !FMath::IsFinite(ThreatPerStoredUnit)
		|| TotalCapacity <= 0.0f || ThreatPerStoredUnit <= 0.0f)
	{
		return 0.0f;
	}

	const float PresentationMax = TotalCapacity * ThreatPerStoredUnit;
	return (FMath::IsFinite(PresentationMax) && PresentationMax > 0.0f) ? PresentationMax : 0.0f;
}

float UGP_MatchViewModelAdapter::ComputeFerroniteThreatNormalized(
	float ThreatValue,
	float TotalCapacity,
	float ThreatPerStoredUnit)
{
	const float PresentationMax = ComputeThreatPresentationMax(TotalCapacity, ThreatPerStoredUnit);
	if (PresentationMax <= 0.0f)
	{
		return 0.0f;
	}

	const float FiniteThreat = FMath::IsFinite(ThreatValue) ? ThreatValue : 0.0f;
	return FMath::Clamp(FiniteThreat / PresentationMax, 0.0f, 1.0f);
}

bool UGP_MatchViewModelAdapter::Initialize(
	UGP_MatchViewModel* InViewModel,
	AGP_GameState* InGameState,
	int32 InLocalTeamId)
{
	Shutdown();
	ViewModel = InViewModel;
	if (ViewModel == nullptr || !IsValid(InGameState))
	{
		return false;
	}

	BoundGameState = InGameState;
	LocalTeamId = InLocalTeamId;
	MatchStateHandle = InGameState->OnMatchStateTagChanged.AddUObject(
		this, &ThisClass::HandleMatchStateChanged);
	MatchTimeHandle = InGameState->OnMatchTimeRemainingChanged.AddUObject(
		this, &ThisClass::HandleMatchTimeChanged);
	TeamThreatHandle = InGameState->OnTeamFerroniteThreatValueChanged.AddUObject(
		this, &ThisClass::HandleTeamThreatChanged);
	MatchResultHandle = InGameState->OnMatchResultChanged.AddUObject(
		this, &ThisClass::HandleMatchResultChanged);
	ResolvedMainBaseHandle = InGameState->OnResolvedMainBaseChanged.AddUObject(
		this, &ThisClass::HandleResolvedMainBaseChanged);
	BindLocalMainBaseStorage();
	RefreshSnapshot();
	return true;
}

void UGP_MatchViewModelAdapter::Shutdown()
{
	UnbindLocalMainBaseStorage();
	if (AGP_GameState* GameState = BoundGameState.Get())
	{
		GameState->OnMatchStateTagChanged.Remove(MatchStateHandle);
		GameState->OnMatchTimeRemainingChanged.Remove(MatchTimeHandle);
		GameState->OnTeamFerroniteThreatValueChanged.Remove(TeamThreatHandle);
		GameState->OnMatchResultChanged.Remove(MatchResultHandle);
		GameState->OnResolvedMainBaseChanged.Remove(ResolvedMainBaseHandle);
	}
	BoundGameState.Reset();
	LocalTeamId = -1;
	MatchStateHandle.Reset();
	MatchTimeHandle.Reset();
	TeamThreatHandle.Reset();
	MatchResultHandle.Reset();
	ResolvedMainBaseHandle.Reset();
}

void UGP_MatchViewModelAdapter::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UGP_MatchViewModelAdapter::BindLocalMainBaseStorage()
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

void UGP_MatchViewModelAdapter::UnbindLocalMainBaseStorage()
{
	if (UGP_StorageComponent* Storage = BoundStorage.Get())
	{
		Storage->OnStorageChanged.RemoveDynamic(this, &ThisClass::HandleStorageChanged);
	}
	BoundStorage.Reset();
}

void UGP_MatchViewModelAdapter::RefreshThreatPresentation()
{
	if (ViewModel == nullptr)
	{
		return;
	}

	AGP_GameState* GameState = BoundGameState.Get();
	const float ThreatValue =
		GameState != nullptr ? GameState->GetFerroniteThreatValueForTeam(LocalTeamId) : 0.0f;
	ViewModel->SetFerroniteThreatValue(ThreatValue);

	UGP_StorageComponent* Storage = BoundStorage.Get();
	const float TotalCapacity = IsValid(Storage) ? Storage->GetTotalCapacity() : 0.0f;
	const float ThreatPerStoredUnit = IsValid(Storage) ? Storage->GetThreatPerStoredUnit() : 0.0f;
	ViewModel->SetFerroniteThreatNormalized(
		ComputeFerroniteThreatNormalized(ThreatValue, TotalCapacity, ThreatPerStoredUnit));
}

float UGP_MatchViewModelAdapter::GetThreatPresentationMax() const
{
	UGP_StorageComponent* Storage = BoundStorage.Get();
	if (!IsValid(Storage))
	{
		return 0.0f;
	}
	return ComputeThreatPresentationMax(
		Storage->GetTotalCapacity(),
		Storage->GetThreatPerStoredUnit());
}

void UGP_MatchViewModelAdapter::RefreshSnapshot()
{
	AGP_GameState* GameState = BoundGameState.Get();
	if (ViewModel == nullptr || GameState == nullptr)
	{
		return;
	}

	const FGP_MatchResult& Result = GameState->GetMatchResult();
	ViewModel->SetMatchTimeRemaining(GameState->GetMatchTimeRemaining());
	ViewModel->SetMatchStateTag(GameState->GetMatchStateTag());
	RefreshThreatPresentation();
	ViewModel->SetWinnerTeamId(GameState->GetWinnerTeamId());
	ViewModel->SetWinReasonTag(GameState->GetWinReasonTag());
	ViewModel->SetMatchDuration(Result.MatchDuration);
	ViewModel->SetMatchFinished(GameState->IsMatchFinished());
}

void UGP_MatchViewModelAdapter::HandleMatchStateChanged(
	FGameplayTag OldTag,
	FGameplayTag NewTag)
{
	(void)OldTag;
	if (ViewModel != nullptr)
	{
		ViewModel->SetMatchStateTag(NewTag);
	}
	RefreshSnapshot();
}

void UGP_MatchViewModelAdapter::HandleMatchTimeChanged(float OldTime, float NewTime)
{
	(void)OldTime;
	if (ViewModel != nullptr)
	{
		ViewModel->SetMatchTimeRemaining(NewTime);
	}
}

void UGP_MatchViewModelAdapter::HandleTeamThreatChanged(
	int32 TeamId,
	float OldThreat,
	float NewThreat)
{
	(void)OldThreat;
	(void)NewThreat;
	if (TeamId == LocalTeamId)
	{
		RefreshThreatPresentation();
	}
}

void UGP_MatchViewModelAdapter::HandleResolvedMainBaseChanged(
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
	RefreshThreatPresentation();
}

void UGP_MatchViewModelAdapter::HandleStorageChanged(
	float PreviousTotalStored,
	float NewTotalStored,
	float TotalCapacity)
{
	(void)PreviousTotalStored;
	(void)NewTotalStored;
	(void)TotalCapacity;
	RefreshThreatPresentation();
}

void UGP_MatchViewModelAdapter::HandleMatchResultChanged(
	int32 OldWinnerTeamId,
	int32 NewWinnerTeamId,
	FGameplayTag OldWinReasonTag,
	FGameplayTag NewWinReasonTag)
{
	(void)OldWinnerTeamId;
	(void)NewWinnerTeamId;
	(void)OldWinReasonTag;
	(void)NewWinReasonTag;
	RefreshSnapshot();
}

int32 UGP_MatchViewModelAdapter::GetBoundDelegateCount() const
{
	return (MatchStateHandle.IsValid() ? 1 : 0)
		+ (MatchTimeHandle.IsValid() ? 1 : 0)
		+ (TeamThreatHandle.IsValid() ? 1 : 0)
		+ (MatchResultHandle.IsValid() ? 1 : 0)
		+ (ResolvedMainBaseHandle.IsValid() ? 1 : 0)
		+ (BoundStorage.IsValid() ? 1 : 0);
}
