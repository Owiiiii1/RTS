// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPHUDViewModelSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/GPGameState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Player/GPPlayerState.h"
#include "Settings/GPUIPresentationSettings.h"
#include "ViewModels/GPMatchViewModel.h"
#include "ViewModels/GPMatchViewModelAdapter.h"
#include "ViewModels/GPResourceViewModel.h"
#include "ViewModels/GPResourceViewModelAdapter.h"
#include "Widgets/GPHUDRootWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPHUDViewModels, Log, All);

void UGP_HUDViewModelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ResourceViewModel = NewObject<UGP_ResourceViewModel>(this);
	MatchViewModel = NewObject<UGP_MatchViewModel>(this);
	ResourceAdapter = NewObject<UGP_ResourceViewModelAdapter>(this);
	MatchAdapter = NewObject<UGP_MatchViewModelAdapter>(this);

	if (UWorld* World = GetWorld())
	{
		GameStateSetHandle = World->GameStateSetEvent.AddUObject(
			this, &ThisClass::HandleGameStateSet);
	}
	Rebind();
	EnsureProductionHUD();
}

void UGP_HUDViewModelSubsystem::Deinitialize()
{
	TeardownProductionHUD();
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	GameStateSetHandle.Reset();
	UnbindPlayerController();
	UnbindGameState();
	if (ResourceAdapter != nullptr)
	{
		ResourceAdapter->Shutdown();
	}
	if (MatchAdapter != nullptr)
	{
		MatchAdapter->Shutdown();
	}
	bReady = false;
	LocalTeamId = -1;
	Super::Deinitialize();
}

void UGP_HUDViewModelSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	Super::PlayerControllerChanged(NewPlayerController);
	BindPlayerController(Cast<AGP_PlayerController>(NewPlayerController));
	Rebind();
	if (NewPlayerController == nullptr || !NewPlayerController->IsLocalController())
	{
		TeardownProductionHUD();
		return;
	}
	EnsureProductionHUD();
}

void UGP_HUDViewModelSubsystem::Rebind()
{
	bReady = false;
	LocalTeamId = -1;
	if (ResourceAdapter != nullptr)
	{
		ResourceAdapter->Shutdown();
	}
	if (MatchAdapter != nullptr)
	{
		MatchAdapter->Shutdown();
	}

	UWorld* World = GetWorld();
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	AGP_PlayerController* PlayerController =
		LocalPlayer != nullptr && World != nullptr
			? Cast<AGP_PlayerController>(LocalPlayer->GetPlayerController(World))
			: nullptr;
	BindPlayerController(PlayerController);
	AGP_GameState* GameState =
		World != nullptr ? World->GetGameState<AGP_GameState>() : nullptr;
	BindGameState(GameState);

	AGP_PlayerState* LocalPlayerState =
		PlayerController != nullptr ? PlayerController->GetPlayerState<AGP_PlayerState>() : nullptr;
	if (!IsValid(GameState) || !IsValid(LocalPlayerState)
		|| ResourceViewModel == nullptr || MatchViewModel == nullptr
		|| ResourceAdapter == nullptr || MatchAdapter == nullptr)
	{
		ResetViewModels();
		return;
	}

	LocalTeamId = LocalPlayerState->GetTeamId();
	if (LocalTeamId < 1)
	{
		ResetViewModels();
		return;
	}

	AGP_PlayerState* OpponentPlayerState =
		ResolveOpponentPlayerState(GameState, LocalPlayerState, LocalTeamId);
	const bool bResourcesBound = ResourceAdapter->Initialize(
		ResourceViewModel, LocalPlayerState, OpponentPlayerState);
	const bool bMatchBound = MatchAdapter->Initialize(
		MatchViewModel, GameState, LocalTeamId);
	bReady = bResourcesBound && bMatchBound;
}

void UGP_HUDViewModelSubsystem::BindPlayerController(AGP_PlayerController* PlayerController)
{
	if (BoundPlayerController.Get() == PlayerController)
	{
		return;
	}

	UnbindPlayerController();
	BoundPlayerController = PlayerController;
	if (PlayerController != nullptr)
	{
		PlayerStatePresentationReadyHandle =
			PlayerController->OnPlayerStatePresentationReady.AddUObject(
				this, &ThisClass::HandlePlayerStatePresentationReady);
	}
}

void UGP_HUDViewModelSubsystem::UnbindPlayerController()
{
	if (AGP_PlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnPlayerStatePresentationReady.Remove(
			PlayerStatePresentationReadyHandle);
	}
	PlayerStatePresentationReadyHandle.Reset();
	BoundPlayerController.Reset();
}

void UGP_HUDViewModelSubsystem::ResetViewModels()
{
	if (ResourceViewModel != nullptr)
	{
		ResourceViewModel->SetOrbitalFerronite(0.0f);
		ResourceViewModel->SetFerroniteScore(0.0f);
		ResourceViewModel->SetCurrentUnits(0.0f);
		ResourceViewModel->SetMaxUnits(0.0f);
		ResourceViewModel->SetOpponentFerroniteScore(0.0f);
	}
	if (MatchViewModel != nullptr)
	{
		MatchViewModel->SetMatchTimeRemaining(0.0f);
		MatchViewModel->SetMatchStateTag(FGameplayTag());
		MatchViewModel->SetFerroniteThreatValue(0.0f);
		MatchViewModel->SetFerroniteThreatNormalized(0.0f);
		MatchViewModel->SetWinnerTeamId(-1);
		MatchViewModel->SetWinReasonTag(FGameplayTag());
		MatchViewModel->SetMatchDuration(0.0f);
		MatchViewModel->SetMatchFinished(false);
	}
}

void UGP_HUDViewModelSubsystem::BindGameState(AGP_GameState* GameState)
{
	if (BoundGameState.Get() == GameState)
	{
		RebuildPlayerStateTeamBindings(GameState);
		return;
	}

	UnbindGameState();
	BoundGameState = GameState;
	if (GameState != nullptr)
	{
		PlayerStateRosterHandle = GameState->OnPlayerStateRosterChanged.AddUObject(
			this, &ThisClass::HandlePlayerStateRosterChanged);
		RebuildPlayerStateTeamBindings(GameState);
	}
}

void UGP_HUDViewModelSubsystem::UnbindGameState()
{
	ClearPlayerStateTeamBindings();
	if (AGP_GameState* GameState = BoundGameState.Get())
	{
		GameState->OnPlayerStateRosterChanged.Remove(PlayerStateRosterHandle);
	}
	PlayerStateRosterHandle.Reset();
	BoundGameState.Reset();
}

void UGP_HUDViewModelSubsystem::RebuildPlayerStateTeamBindings(AGP_GameState* GameState)
{
	ClearPlayerStateTeamBindings();
	if (GameState == nullptr)
	{
		return;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		if (AGP_PlayerState* GPPlayerState = Cast<AGP_PlayerState>(PlayerState))
		{
			const FDelegateHandle Handle = GPPlayerState->OnTeamIdChanged.AddUObject(
				this, &ThisClass::HandleAnyPlayerTeamIdChanged);
			PlayerStateTeamHandles.Add(GPPlayerState, Handle);
		}
	}
}

void UGP_HUDViewModelSubsystem::ClearPlayerStateTeamBindings()
{
	for (const TPair<TWeakObjectPtr<AGP_PlayerState>, FDelegateHandle>& Entry : PlayerStateTeamHandles)
	{
		if (AGP_PlayerState* PlayerState = Entry.Key.Get())
		{
			PlayerState->OnTeamIdChanged.Remove(Entry.Value);
		}
	}
	PlayerStateTeamHandles.Reset();
}

AGP_PlayerState* UGP_HUDViewModelSubsystem::ResolveOpponentPlayerState(
	const AGP_GameState* GameState,
	const AGP_PlayerState* LocalPlayerState,
	int32 InLocalTeamId) const
{
	AGP_PlayerState* BestOpponent = nullptr;
	if (GameState == nullptr)
	{
		return nullptr;
	}

	for (APlayerState* PlayerState : GameState->PlayerArray)
	{
		AGP_PlayerState* Candidate = Cast<AGP_PlayerState>(PlayerState);
		if (!IsValid(Candidate) || Candidate == LocalPlayerState)
		{
			continue;
		}
		const int32 CandidateTeamId = Candidate->GetTeamId();
		if (CandidateTeamId < 1 || CandidateTeamId == InLocalTeamId)
		{
			continue;
		}
		if (BestOpponent == nullptr || CandidateTeamId < BestOpponent->GetTeamId())
		{
			BestOpponent = Candidate;
		}
	}
	return BestOpponent;
}

void UGP_HUDViewModelSubsystem::HandleGameStateSet(AGameStateBase* NewGameState)
{
	(void)NewGameState;
	Rebind();
}

void UGP_HUDViewModelSubsystem::HandlePlayerStatePresentationReady(APlayerState* PlayerState)
{
	(void)PlayerState;
	Rebind();
}

void UGP_HUDViewModelSubsystem::HandlePlayerStateRosterChanged(
	APlayerState* PlayerState,
	bool bAdded)
{
	(void)PlayerState;
	(void)bAdded;
	Rebind();
}

void UGP_HUDViewModelSubsystem::HandleAnyPlayerTeamIdChanged(
	int32 OldTeamId,
	int32 NewTeamId)
{
	(void)OldTeamId;
	(void)NewTeamId;
	Rebind();
}

namespace GPHUDBootstrapPrivate
{
	static constexpr int32 ProductionHUDZOrder = 100;
}

TSubclassOf<UGP_HUDRootWidget> UGP_HUDViewModelSubsystem::ResolveConfiguredProductionHUDClass() const
{
	const UGP_UIPresentationSettings* Settings = UGP_UIPresentationSettings::Get();
	if (Settings == nullptr || Settings->ProductionHUDWidgetClass.IsNull())
	{
		return nullptr;
	}

	UClass* LoadedClass = Settings->ProductionHUDWidgetClass.LoadSynchronous();
	if (LoadedClass == nullptr)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGPHUDViewModels, Warning,
			TEXT("UGP_HUDViewModelSubsystem: ProductionHUDWidgetClass failed to load (%s)"),
			*Settings->ProductionHUDWidgetClass.ToString());
#endif
		return nullptr;
	}

	if (!LoadedClass->IsChildOf(UGP_HUDRootWidget::StaticClass())
		|| LoadedClass->HasAnyClassFlags(CLASS_Abstract))
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGPHUDViewModels, Warning,
			TEXT("UGP_HUDViewModelSubsystem: ProductionHUDWidgetClass is not a concrete UGP_HUDRootWidget subclass (%s)"),
			*LoadedClass->GetName());
#endif
		return nullptr;
	}

	return LoadedClass;
}

void UGP_HUDViewModelSubsystem::EnsureProductionHUD()
{
	EnsureProductionHUDInternal(ResolveConfiguredProductionHUDClass(), true);
}

void UGP_HUDViewModelSubsystem::TeardownProductionHUD()
{
	if (UGP_HUDRootWidget* HUD = ProductionHUDWidget.Get())
	{
		HUD->RemoveFromParent();
	}
	ProductionHUDWidget = nullptr;
}

void UGP_HUDViewModelSubsystem::EnsureProductionHUDInternal(
	TSubclassOf<UGP_HUDRootWidget> WidgetClass,
	bool bWarnIfUnconfigured)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UWorld* World = GetWorld();
	APlayerController* PlayerController =
		LocalPlayer != nullptr && World != nullptr
			? LocalPlayer->GetPlayerController(World)
			: nullptr;
	if (LocalPlayer == nullptr || PlayerController == nullptr || !PlayerController->IsLocalController())
	{
		return;
	}

	if (WidgetClass == nullptr)
	{
		if (bWarnIfUnconfigured && !bLoggedUnconfiguredHUDClass)
		{
			bLoggedUnconfiguredHUDClass = true;
#if !UE_BUILD_SHIPPING
			UE_LOG(LogGPHUDViewModels, Warning,
				TEXT("UGP_HUDViewModelSubsystem: ProductionHUDWidgetClass is not configured; production HUD bootstrap skipped. TEMP HUD remains available."));
#endif
		}
		return;
	}

	if (IsValid(ProductionHUDWidget)
		&& ProductionHUDWidget->GetClass() == WidgetClass.Get()
		&& ProductionHUDWidget->GetOwningPlayer() == PlayerController)
	{
		if (!ProductionHUDWidget->IsInViewport())
		{
			ProductionHUDWidget->SetIsFocusable(false);
			ProductionHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			ProductionHUDWidget->AddToViewport(GPHUDBootstrapPrivate::ProductionHUDZOrder);
			ProductionHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		return;
	}

	TeardownProductionHUD();

	ProductionHUDWidget = CreateWidget<UGP_HUDRootWidget>(PlayerController, WidgetClass);
	if (ProductionHUDWidget == nullptr)
	{
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGPHUDViewModels, Warning,
			TEXT("UGP_HUDViewModelSubsystem: CreateWidget failed for %s"),
			*GetNameSafe(WidgetClass.Get()));
#endif
		return;
	}

	ProductionHUDWidget->SetIsFocusable(false);
	ProductionHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	ProductionHUDWidget->AddToViewport(GPHUDBootstrapPrivate::ProductionHUDZOrder);
	ProductionHUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
}

#if !UE_BUILD_SHIPPING
void UGP_HUDViewModelSubsystem::EnsureProductionHUDWithClassForContract(
	TSubclassOf<UGP_HUDRootWidget> WidgetClass)
{
	EnsureProductionHUDInternal(WidgetClass, false);
}
#endif

int32 UGP_HUDViewModelSubsystem::GetResourceDelegateCount() const
{
	return ResourceAdapter != nullptr ? ResourceAdapter->GetBoundDelegateCount() : 0;
}

int32 UGP_HUDViewModelSubsystem::GetMatchDelegateCount() const
{
	return MatchAdapter != nullptr ? MatchAdapter->GetBoundDelegateCount() : 0;
}

#if !UE_BUILD_SHIPPING
void UGP_HUDViewModelSubsystem::DebugDumpToLog() const
{
	const UGP_ResourceViewModel* Resources = ResourceViewModel;
	const UGP_MatchViewModel* Match = MatchViewModel;
	UE_LOG(LogGPHUDViewModels, Display,
		TEXT("gp.UI.HUDDump Ready=%s LocalTeamId=%d OrbitalFerronite=%.2f FerroniteScore=%.2f ")
		TEXT("OpponentFerroniteScore=%.2f CurrentUnits=%.0f MaxUnits=%.0f MatchTimeRemaining=%.2f ")
		TEXT("FerroniteThreatValue=%.2f ThreatNormalized=%.2f ThreatPresentationMax=%.2f ")
		TEXT("MatchState=%s Finished=%s WinnerTeamId=%d WinReason=%s MatchDuration=%.2f"),
		bReady ? TEXT("Ready") : TEXT("NotReady"),
		LocalTeamId,
		Resources != nullptr ? Resources->OrbitalFerronite : 0.0f,
		Resources != nullptr ? Resources->FerroniteScore : 0.0f,
		Resources != nullptr ? Resources->OpponentFerroniteScore : 0.0f,
		Resources != nullptr ? Resources->CurrentUnits : 0.0f,
		Resources != nullptr ? Resources->MaxUnits : 0.0f,
		Match != nullptr ? Match->MatchTimeRemaining : 0.0f,
		Match != nullptr ? Match->FerroniteThreatValue : 0.0f,
		Match != nullptr ? Match->FerroniteThreatNormalized : 0.0f,
		MatchAdapter != nullptr ? MatchAdapter->GetThreatPresentationMax() : 0.0f,
		Match != nullptr ? *Match->MatchStateTag.ToString() : TEXT("None"),
		Match != nullptr && Match->bMatchFinished ? TEXT("true") : TEXT("false"),
		Match != nullptr ? Match->WinnerTeamId : -1,
		Match != nullptr ? *Match->WinReasonTag.ToString() : TEXT("None"),
		Match != nullptr ? Match->MatchDuration : 0.0f);
}

void UGP_HUDViewModelSubsystem::DebugDumpHUDStatusToLog() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGP_UIPresentationSettings* Settings = UGP_UIPresentationSettings::Get();
	const FString ConfiguredClass =
		Settings != nullptr && !Settings->ProductionHUDWidgetClass.IsNull()
			? Settings->ProductionHUDWidgetClass.ToString()
			: FString(TEXT("<unconfigured>"));
	const UGP_HUDRootWidget* HUD = ProductionHUDWidget.Get();
	const FString InstanceClass =
		HUD != nullptr ? HUD->GetClass()->GetName() : FString(TEXT("<none>"));
	const FString InstanceName =
		HUD != nullptr ? HUD->GetName() : FString(TEXT("<none>"));

	UE_LOG(LogGPHUDViewModels, Display,
		TEXT("gp.UI.HUDStatus LocalPlayer=%s ConfiguredClass=%s InstancePresent=%s ")
		TEXT("WidgetClass=%s WidgetName=%s Ready=%s"),
		LocalPlayer != nullptr ? *LocalPlayer->GetName() : TEXT("<none>"),
		*ConfiguredClass,
		HUD != nullptr ? TEXT("true") : TEXT("false"),
		*InstanceClass,
		*InstanceName,
		bReady ? TEXT("Ready") : TEXT("NotReady"));
}

namespace GPHUDDumpPrivate
{
	static UGP_HUDViewModelSubsystem* FindLocalHUDSubsystem(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
			{
				return LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>();
			}
		}
		return nullptr;
	}

	static void HUDDump(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (UGP_HUDViewModelSubsystem* Subsystem = FindLocalHUDSubsystem(World))
		{
			Subsystem->DebugDumpToLog();
		}
		else
		{
			UE_LOG(LogGPHUDViewModels, Warning,
				TEXT("gp.UI.HUDDump Ready=NotReady Reason=NoLocalHUDViewModelSubsystem"));
		}
	}

	static void HUDStatus(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || GEngine == nullptr)
		{
			UE_LOG(LogGPHUDViewModels, Warning,
				TEXT("gp.UI.HUDStatus LocalPlayer=<none> Reason=NoWorld"));
			return;
		}

		int32 Dumped = 0;
		for (FLocalPlayerIterator It(GEngine, World); It; ++It)
		{
			if (UGP_HUDViewModelSubsystem* Subsystem =
				(*It) != nullptr ? (*It)->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr)
			{
				Subsystem->DebugDumpHUDStatusToLog();
				++Dumped;
			}
		}
		if (Dumped == 0)
		{
			UE_LOG(LogGPHUDViewModels, Warning,
				TEXT("gp.UI.HUDStatus LocalPlayer=<none> ConfiguredClass=<unknown> ")
				TEXT("InstancePresent=false WidgetClass=<none> WidgetName=<none> Ready=NotReady"));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs GHUDDumpCommand(
		TEXT("gp.UI.HUDDump"),
		TEXT("Dump local production HUD ViewModel state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HUDDump));

	static FAutoConsoleCommandWithWorldAndArgs GHUDStatusCommand(
		TEXT("gp.UI.HUDStatus"),
		TEXT("Dump local production HUD bootstrap status."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HUDStatus));
}
#endif
