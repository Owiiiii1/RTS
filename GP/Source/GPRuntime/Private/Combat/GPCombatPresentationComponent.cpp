// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/GPCombatPresentationComponent.h"

#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Units/GPUnitBase.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/GPPlayerController.h"
#include "Player/GPSelectionComponent.h"
#endif

DEFINE_LOG_CATEGORY(LogGPCombatPresentation);

namespace GPCombatPresentationPrivate
{
	static const TCHAR* NetModeToString(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None:
			return TEXT("None");
		case ROLE_SimulatedProxy:
			return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy:
			return TEXT("AutonomousProxy");
		case ROLE_Authority:
			return TEXT("Authority");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* EventTypeToString(EGP_CombatPresentationEventType EventType)
	{
		switch (EventType)
		{
		case EGP_CombatPresentationEventType::MeleeImpact:
			return TEXT("MeleeImpact");
		default:
			return TEXT("Unknown");
		}
	}

	/**
	 * Serial-number comparison for uint32 sequences that skip 0.
	 * Incoming is duplicate/stale when not strictly newer than LastProcessed.
	 * Delta uses int32 wrap-safe distance (half-range assumption; S26A practically never wraps).
	 */
	static bool IsDuplicateOrStaleSequence(uint32 Incoming, uint32 LastProcessed)
	{
		if (Incoming == 0)
		{
			return true;
		}

		if (LastProcessed == 0)
		{
			return false;
		}

		if (Incoming == LastProcessed)
		{
			return true;
		}

		const int32 Delta = static_cast<int32>(Incoming - LastProcessed);
		return Delta <= 0;
	}
}

UGP_CombatPresentationComponent::UGP_CombatPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

uint32 UGP_CombatPresentationComponent::GetAuthorityNextPresentationSequence() const
{
	uint32 Next = AuthorityPresentationSequence + 1u;
	if (Next == 0u)
	{
		Next = 1u;
	}
	return Next;
}

uint32 UGP_CombatPresentationComponent::GetLastProcessedPresentationSequence() const
{
	return LastProcessedPresentationSequence;
}

void UGP_CombatPresentationComponent::AuthorityEmitAttackHitPresentation(
	uint32 AttackSerial,
	AGP_UnitBase* Target,
	EGP_CombatPresentationEventType EventType,
	float AuthoritativeWorldTime,
	float AppliedDamage,
	bool bBlocked,
	bool bTargetDiedFromHit)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	++AuthorityPresentationSequence;
	if (AuthorityPresentationSequence == 0u)
	{
		AuthorityPresentationSequence = 1u;
	}

	FGP_CombatPresentationEvent Event;
	Event.PresentationSequence = AuthorityPresentationSequence;
	Event.AttackSerial = AttackSerial;
	Event.Target = Target;
	Event.EventType = EventType;
	Event.AuthoritativeWorldTime = AuthoritativeWorldTime;
	Event.AppliedDamage = AppliedDamage;
	Event.bBlocked = bBlocked;
	Event.bTargetDiedFromHit = bTargetDiedFromHit;

	UE_LOG(LogGPCombatPresentation, Log,
		TEXT("GP CombatPresentationEmit: Source=%s Target=%s Sequence=%u AttackSerial=%u EventType=%s AppliedDamage=%.2f Blocked=%s TargetDied=%s Time=%.3f"),
		*GetNameSafe(Owner),
		*GetNameSafe(Target),
		Event.PresentationSequence,
		Event.AttackSerial,
		GPCombatPresentationPrivate::EventTypeToString(Event.EventType),
		Event.AppliedDamage,
		Event.bBlocked ? TEXT("true") : TEXT("false"),
		Event.bTargetDiedFromHit ? TEXT("true") : TEXT("false"),
		Event.AuthoritativeWorldTime);

	// Multicast Implementation is the only local presentation entry (listen server included).
	Multicast_CombatPresentationEvent(Event);
}

void UGP_CombatPresentationComponent::Multicast_CombatPresentationEvent_Implementation(
	FGP_CombatPresentationEvent Event)
{
	HandleCombatPresentationEvent(Event);
}

void UGP_CombatPresentationComponent::HandleCombatPresentationEvent(const FGP_CombatPresentationEvent& Event)
{
	if (GPCombatPresentationPrivate::IsDuplicateOrStaleSequence(
		Event.PresentationSequence,
		LastProcessedPresentationSequence))
	{
		UE_LOG(LogGPCombatPresentation, Verbose,
			TEXT("GP CombatPresentationDuplicate: Source=%s Sequence=%u LastProcessed=%u AttackSerial=%u"),
			*GetNameSafe(GetOwner()),
			Event.PresentationSequence,
			LastProcessedPresentationSequence,
			Event.AttackSerial);
		return;
	}

	LastProcessedPresentationSequence = Event.PresentationSequence;

	const UWorld* World = GetWorld();
	const ENetMode NetMode = World != nullptr ? World->GetNetMode() : NM_MAX;
	if (NetMode == NM_DedicatedServer)
	{
		UE_LOG(LogGPCombatPresentation, Verbose,
			TEXT("GP CombatPresentationAcceptedDedicated: Source=%s Sequence=%u AttackSerial=%u (visual suppressed)"),
			*GetNameSafe(GetOwner()),
			Event.PresentationSequence,
			Event.AttackSerial);
		return;
	}

	AGP_UnitBase* SourceUnit = Cast<AGP_UnitBase>(GetOwner());
	PlayCombatPresentationDebug(Event, SourceUnit);
}

void UGP_CombatPresentationComponent::PlayCombatPresentationDebug(
	const FGP_CombatPresentationEvent& Event,
	AGP_UnitBase* SourceUnit)
{
	const UWorld* World = GetWorld();
	const ENetMode NetMode = World != nullptr ? World->GetNetMode() : NM_MAX;
	const ENetRole LocalRole = GetOwner() != nullptr ? GetOwner()->GetLocalRole() : ROLE_None;

	UE_LOG(LogGPCombatPresentation, Log,
		TEXT("GP CombatPresentationAccepted: Source=%s Target=%s Sequence=%u AttackSerial=%u EventType=%s AppliedDamage=%.2f Blocked=%s TargetDied=%s Role=%s NetMode=%s Time=%.3f"),
		*GetNameSafe(SourceUnit),
		*GetNameSafe(Event.Target.Get()),
		Event.PresentationSequence,
		Event.AttackSerial,
		GPCombatPresentationPrivate::EventTypeToString(Event.EventType),
		Event.AppliedDamage,
		Event.bBlocked ? TEXT("true") : TEXT("false"),
		Event.bTargetDiedFromHit ? TEXT("true") : TEXT("false"),
		GPCombatPresentationPrivate::RoleToString(LocalRole),
		GPCombatPresentationPrivate::NetModeToString(NetMode),
		Event.AuthoritativeWorldTime);

#if !UE_BUILD_SHIPPING
	if (World == nullptr || SourceUnit == nullptr || !IsValid(SourceUnit))
	{
		return;
	}

	AGP_UnitBase* TargetUnit = Event.Target.Get();
	if (!IsValid(TargetUnit))
	{
		return;
	}

	const FVector Start = SourceUnit->GetActorLocation();
	const FVector End = TargetUnit->GetActorLocation();
	const FColor LineColor = Event.bBlocked
		? FColor(220, 180, 40)
		: (Event.bTargetDiedFromHit ? FColor(220, 40, 40) : FColor(40, 200, 80));
	const float DurationSeconds = 0.35f;
	const float Thickness = Event.bBlocked ? 1.5f : 2.5f;
	DrawDebugLine(World, Start, End, LineColor, false, DurationSeconds, 0, Thickness);
#endif
}

#if !UE_BUILD_SHIPPING
namespace GPCombatPresentationDebug
{
	static AGP_UnitBase* FindFirstSelectedUnit(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		AGP_PlayerController* LocalPC = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AGP_PlayerController* PC = Cast<AGP_PlayerController>(It->Get());
			if (PC != nullptr && PC->IsLocalController())
			{
				LocalPC = PC;
				break;
			}
		}

		if (LocalPC == nullptr)
		{
			return nullptr;
		}

		const UGP_SelectionComponent* Selection = LocalPC->GetSelectionComponent();
		if (Selection == nullptr)
		{
			return nullptr;
		}

		for (const TWeakObjectPtr<AGP_UnitBase>& WeakUnit : Selection->GetSelectedUnits())
		{
			AGP_UnitBase* Unit = WeakUnit.Get();
			if (IsValid(Unit))
			{
				return Unit;
			}
		}

		return nullptr;
	}

	static AGP_UnitBase* FindFallbackUnit(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		AGP_UnitBase* Best = nullptr;
		for (TActorIterator<AGP_UnitBase> It(World); It; ++It)
		{
			AGP_UnitBase* Unit = *It;
			if (!IsValid(Unit) || Unit->GetCombatPresentationComponent() == nullptr)
			{
				continue;
			}

			if (Best == nullptr || Unit->GetName() < Best->GetName())
			{
				Best = Unit;
			}
		}

		return Best;
	}

	static void CombatPresentationInspect(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPCombatPresentation, Warning, TEXT("GP CombatPresentation.Inspect: missing world"));
			return;
		}

		AGP_UnitBase* Source = FindFirstSelectedUnit(World);
		const TCHAR* SourcePolicy = TEXT("Selected");
		if (Source == nullptr)
		{
			Source = FindFallbackUnit(World);
			SourcePolicy = TEXT("Fallback");
		}

		if (Source == nullptr)
		{
			UE_LOG(LogGPCombatPresentation, Warning, TEXT("GP CombatPresentation.Inspect: no unit"));
			return;
		}

		const UGP_CombatPresentationComponent* Presentation = Source->GetCombatPresentationComponent();
		const ENetMode NetMode = World->GetNetMode();
		const bool bDedicatedVisualSuppressed = (NetMode == NM_DedicatedServer);

		UE_LOG(LogGPCombatPresentation, Log,
			TEXT("GP CombatPresentation.Inspect: Source=%s Policy=%s Component=%s LastProcessedSequence=%u AuthorityNextSequence=%u Role=%s NetMode=%s DedicatedVisualSuppressed=%s"),
			*Source->GetName(),
			SourcePolicy,
			Presentation != nullptr ? TEXT("present") : TEXT("missing"),
			Presentation != nullptr ? Presentation->GetLastProcessedPresentationSequence() : 0u,
			Presentation != nullptr ? Presentation->GetAuthorityNextPresentationSequence() : 0u,
			GPCombatPresentationPrivate::RoleToString(Source->GetLocalRole()),
			GPCombatPresentationPrivate::NetModeToString(NetMode),
			bDedicatedVisualSuppressed ? TEXT("true") : TEXT("false"));
	}

	static FAutoConsoleCommandWithWorldAndArgs GCombatPresentationInspectCommand(
		TEXT("gp.CombatPresentation.Inspect"),
		TEXT("Inspect combat presentation component sequence/role state for selected or fallback Source unit."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CombatPresentationInspect));
}
#endif
