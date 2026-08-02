// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/GPGameMode.h"
#include "Camera/GPCameraPawn.h"
#include "Game/GPGameState.h"
#include "GameFramework/PlayerController.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Tags/GPGameplayTags.h"
#include "TimerManager.h"

AGP_GameMode::AGP_GameMode()
{
	GameStateClass = AGP_GameState::StaticClass();
	PlayerControllerClass = AGP_PlayerController::StaticClass();
	PlayerStateClass = AGP_PlayerState::StaticClass();
	DefaultPawnClass = AGP_CameraPawn::StaticClass();
	PrimaryActorTick.bCanEverTick = false;
	MatchDurationSeconds = 600.0f;
	ExpectedHumanPlayers = 2;
}

AGP_GameState* AGP_GameMode::GetGPGameState() const
{
	return GetGameState<AGP_GameState>();
}

int32 AGP_GameMode::GetConnectedHumanPlayerCount() const
{
	// APlayerControllerIterator excludes AAIController (AI uses AAIController, not APC).
	// Does not depend on AGP_PlayerState.
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0;
	}

	int32 Count = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController != nullptr)
		{
			++Count;
		}
	}
	return Count;
}

bool AGP_GameMode::IsWinReasonBranchTag(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return false;
	}

	const FGameplayTag WinReasonRoot = FGPGameplayTags::Get().Match_WinReason_DeliveryQuota.RequestDirectParent();
	return WinReasonRoot.IsValid() && Tag.MatchesTag(WinReasonRoot);
}

void AGP_GameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AGP_GameMode::BeginPlay: AGP_GameState unavailable (GameStateClass must be AGP_GameState). Match flow not initialized."));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	GPGameState->SetMatchStateTag(GPTags.Match_State_WaitingForPlayers);
	GPGameState->SetMatchTimeRemaining(0.0f);
	GPGameState->ClearMatchResult();

	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameMode::BeginPlay: match flow initialized to WaitingForPlayers (ExpectedHumanPlayers=%d, MatchDurationSeconds=%.1f)."),
		ExpectedHumanPlayers, MatchDurationSeconds);
}

void AGP_GameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!HasAuthority())
	{
		return;
	}

	const int32 HumanCount = GetConnectedHumanPlayerCount();
	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameMode::PostLogin: human players now %d (ExpectedHumanPlayers=%d)."),
		HumanCount, ExpectedHumanPlayers);

	TryStartMatch();
}

void AGP_GameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (!HasAuthority())
	{
		return;
	}

	const int32 HumanCount = GetConnectedHumanPlayerCount();
	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameMode::Logout: human players now %d. No disconnect winner evaluation in GP-S07."),
		HumanCount);

	// Match already Playing/Finished: do not FinishMatch / OpponentDisconnect.
	// Pre-start: TryStartMatch cannot become newly satisfied by a logout alone.
	TryStartMatch();
}

void AGP_GameMode::TryStartMatch()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameMode::TryStartMatch denied without authority."));
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("AGP_GameMode::TryStartMatch: AGP_GameState unavailable."));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const FGameplayTag CurrentState = GPGameState->GetMatchStateTag();
	if (CurrentState == GPTags.Match_State_Playing || CurrentState == GPTags.Match_State_Finished)
	{
		return;
	}

	const int32 Threshold = FMath::Max(1, ExpectedHumanPlayers);
	const int32 HumanCount = GetConnectedHumanPlayerCount();
	if (HumanCount >= Threshold)
	{
		StartMatchFlow();
	}
	else
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("AGP_GameMode::TryStartMatch: waiting for humans (%d / %d)."),
			HumanCount, Threshold);
	}
}

void AGP_GameMode::StartMatchFlow()
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameMode::StartMatchFlow denied without authority."));
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("AGP_GameMode::StartMatchFlow: AGP_GameState unavailable."));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	const FGameplayTag CurrentState = GPGameState->GetMatchStateTag();
	if (CurrentState == GPTags.Match_State_Playing || CurrentState == GPTags.Match_State_Finished)
	{
		return;
	}

	StopMatchCountdown();
	bTimeoutEvaluationTriggered = false;

	GPGameState->ClearMatchResult();
	GPGameState->SetMatchStateTag(GPTags.Match_State_Playing);

	const float InitialDuration = FMath::Max(0.0f, MatchDurationSeconds);
	GPGameState->SetMatchTimeRemaining(InitialDuration);

	if (InitialDuration > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				MatchCountdownHandle,
				this,
				&AGP_GameMode::HandleMatchCountdownTick,
				1.0f,
				true,
				1.0f);
		}
	}
	else
	{
		HandleMatchTimeExpired();
	}

	OnMatchFlowStarted();
}

void AGP_GameMode::StopMatchCountdown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchCountdownHandle);
	}
	MatchCountdownHandle.Invalidate();
}

void AGP_GameMode::HandleMatchCountdownTick()
{
	if (!HasAuthority())
	{
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		StopMatchCountdown();
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPGameState->GetMatchStateTag() != GPTags.Match_State_Playing)
	{
		StopMatchCountdown();
		return;
	}

	const float CurrentRemaining = GPGameState->GetMatchTimeRemaining();
	const float NewRemaining = FMath::Max(0.0f, CurrentRemaining - 1.0f);
	GPGameState->SetMatchTimeRemaining(NewRemaining);

	UE_LOG(LogTemp, Verbose, TEXT("AGP_GameMode::HandleMatchCountdownTick: MatchTimeRemaining=%.1f"), NewRemaining);

	if (NewRemaining <= 0.0f)
	{
		HandleMatchTimeExpired();
	}
}

void AGP_GameMode::HandleMatchTimeExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bTimeoutEvaluationTriggered)
	{
		return;
	}

	bTimeoutEvaluationTriggered = true;
	StopMatchCountdown();

	if (AGP_GameState* GPGameState = GetGPGameState())
	{
		GPGameState->SetMatchTimeRemaining(0.0f);
	}

	EvaluateAndFinishMatch();
}

void AGP_GameMode::EvaluateAndFinishMatch()
{
	UE_LOG(LogTemp, Warning,
		TEXT("AGP_GameMode::EvaluateAndFinishMatch: match timer expired, but authoritative PlayerState/FerroniteScore evaluation is unavailable in GP-S07. Leaving MatchState=Playing with MatchTimeRemaining=0 (intentional integration gap). Later slice must call FinishMatch(WinnerTeamId, GP.Match.WinReason.TimerScore)."));
}

void AGP_GameMode::FinishMatch(int32 InWinnerTeamId, FGameplayTag InWinReasonTag)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AGP_GameMode::FinishMatch denied without authority."));
		return;
	}

	AGP_GameState* GPGameState = GetGPGameState();
	if (GPGameState == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("AGP_GameMode::FinishMatch: AGP_GameState unavailable."));
		return;
	}

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPGameState->GetMatchStateTag() == GPTags.Match_State_Finished)
	{
		return;
	}

	if (InWinnerTeamId < -1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameMode::FinishMatch rejected WinnerTeamId=%d (must be >= -1)."),
			InWinnerTeamId);
		return;
	}

	if (!IsWinReasonBranchTag(InWinReasonTag))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AGP_GameMode::FinishMatch rejected invalid or non-GP.Match.WinReason tag '%s'."),
			*InWinReasonTag.ToString());
		return;
	}

	StopMatchCountdown();
	GPGameState->SetMatchResult(InWinnerTeamId, InWinReasonTag);
	GPGameState->SetMatchStateTag(GPTags.Match_State_Finished);
	OnMatchFlowFinished(InWinnerTeamId, InWinReasonTag);
}

void AGP_GameMode::OnMatchFlowStarted()
{
	UE_LOG(LogTemp, Log, TEXT("AGP_GameMode::OnMatchFlowStarted: match is Playing; countdown owned by GameMode."));
}

void AGP_GameMode::OnMatchFlowFinished(int32 WinnerTeamId, FGameplayTag WinReasonTag)
{
	UE_LOG(LogTemp, Log,
		TEXT("AGP_GameMode::OnMatchFlowFinished: WinnerTeamId=%d WinReason=%s"),
		WinnerTeamId, *WinReasonTag.ToString());
}
