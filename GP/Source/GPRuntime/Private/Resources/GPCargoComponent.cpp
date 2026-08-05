// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPCargoComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Resources/GPResourceDefinition.h"
#include "Tags/GPGameplayTags.h"

#include <limits>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#endif

DEFINE_LOG_CATEGORY(LogGPCargo);

namespace GPCargoPrivate
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
}

UGP_CargoComponent::UGP_CargoComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	ResourceDefinition = TSoftObjectPtr<UGP_ResourceDefinition>(
		FSoftObjectPath(UGP_ResourceDefinition::DefaultFerroniteAssetPath()));
	CargoCapacity = 50.0f;
	CurrentCargoAmount = 0.0f;
}

void UGP_CargoComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		ClampCargoState();
		// Prefer already-loaded AlwaysCook primary; no silent sync load.
		ResolveResourceDefinition(false);
	}
}

void UGP_CargoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGP_CargoComponent, CargoCapacity);
	DOREPLIFETIME(UGP_CargoComponent, CurrentCargoAmount);
}

float UGP_CargoComponent::GetCargoCapacity() const
{
	return CargoCapacity;
}

float UGP_CargoComponent::GetCurrentCargoAmount() const
{
	return CurrentCargoAmount;
}

float UGP_CargoComponent::GetRemainingCapacity() const
{
	return FMath::Max(0.0f, CargoCapacity - CurrentCargoAmount);
}

float UGP_CargoComponent::GetFillRatio() const
{
	if (!IsFinitePositive(CargoCapacity))
	{
		return 0.0f;
	}
	return FMath::Clamp(CurrentCargoAmount / CargoCapacity, 0.0f, 1.0f);
}

bool UGP_CargoComponent::IsEmpty() const
{
	return CurrentCargoAmount <= KINDA_SMALL_NUMBER;
}

bool UGP_CargoComponent::IsFull() const
{
	return GetRemainingCapacity() <= KINDA_SMALL_NUMBER;
}

TSoftObjectPtr<UGP_ResourceDefinition> UGP_CargoComponent::GetResourceDefinitionSoft() const
{
	return ResourceDefinition;
}

UGP_ResourceDefinition* UGP_CargoComponent::GetResolvedResourceDefinition() const
{
	return ResolveResourceDefinition(false);
}

UGP_ResourceDefinition* UGP_CargoComponent::ResolveResourceDefinition(bool bAllowSynchronousLoad) const
{
	if (CachedResourceDefinition.IsValid())
	{
		return CachedResourceDefinition.Get();
	}

	if (ResourceDefinition.IsNull())
	{
		return nullptr;
	}

	if (UGP_ResourceDefinition* AlreadyLoaded = ResourceDefinition.Get())
	{
		CachedResourceDefinition = AlreadyLoaded;
		return AlreadyLoaded;
	}

	if (UAssetManager::IsInitialized())
	{
		const FSoftObjectPath SoftPath = ResourceDefinition.ToSoftObjectPath();
		const FPrimaryAssetId PrimaryAssetId(
			FPrimaryAssetType(UGP_ResourceDefinition::PrimaryAssetTypeName()),
			FName(*SoftPath.GetAssetName()));
		if (UObject* PrimaryObject = UAssetManager::Get().GetPrimaryAssetObject(PrimaryAssetId))
		{
			if (UGP_ResourceDefinition* AsDefinition = Cast<UGP_ResourceDefinition>(PrimaryObject))
			{
				CachedResourceDefinition = AsDefinition;
				return AsDefinition;
			}
		}
	}

	if (bAllowSynchronousLoad)
	{
		UE_LOG(LogGPCargo, Verbose,
			TEXT("GP Cargo.ResolveResourceDefinition sync load (AlwaysCook primary): Owner=%s Path=%s"),
			*GetNameSafe(GetOwner()),
			*ResourceDefinition.ToSoftObjectPath().ToString());
		if (UGP_ResourceDefinition* Loaded = ResourceDefinition.LoadSynchronous())
		{
			CachedResourceDefinition = Loaded;
			return Loaded;
		}
	}

	return nullptr;
}

EGP_ResourceType UGP_CargoComponent::GetCarriedResourceType() const
{
	if (const UGP_ResourceDefinition* Definition = GetResolvedResourceDefinition())
	{
		if (Definition->ResourceType != EGP_ResourceType::None)
		{
			return Definition->ResourceType;
		}
	}
	return EGP_ResourceType::Ore;
}

bool UGP_CargoComponent::IsFinitePositive(float Value) const
{
	return FMath::IsFinite(Value) && Value > 0.0f;
}

bool UGP_CargoComponent::IsFiniteNonNegative(float Value) const
{
	return FMath::IsFinite(Value) && Value >= 0.0f;
}

bool UGP_CargoComponent::CanAcceptCargo(float Amount) const
{
	return IsFinitePositive(Amount) && GetRemainingCapacity() > KINDA_SMALL_NUMBER;
}

void UGP_CargoComponent::ClampCargoState()
{
	if (!FMath::IsFinite(CargoCapacity) || CargoCapacity < 0.0f)
	{
		CargoCapacity = 0.0f;
	}

	if (!FMath::IsFinite(CurrentCargoAmount))
	{
		CurrentCargoAmount = 0.0f;
	}

	CurrentCargoAmount = FMath::Clamp(CurrentCargoAmount, 0.0f, CargoCapacity);
}

void UGP_CargoComponent::BroadcastCargoChanged(float PreviousAmount, float NewAmount)
{
	const float Delta = NewAmount - PreviousAmount;
	OnCargoAmountChanged.Broadcast(PreviousAmount, NewAmount, CargoCapacity, Delta);
}

void UGP_CargoComponent::ApplyCargoAmount(float NewAmount)
{
	ClampCargoState();
	const float Previous = CurrentCargoAmount;
	const float Clamped = FMath::Clamp(NewAmount, 0.0f, CargoCapacity);
	if (FMath::IsNearlyEqual(Previous, Clamped))
	{
		CurrentCargoAmount = Clamped;
		return;
	}

	CurrentCargoAmount = Clamped;
	BroadcastCargoChanged(Previous, CurrentCargoAmount);
}

float UGP_CargoComponent::AddCargo(float RequestedAmount)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		UE_LOG(LogGPCargo, Warning,
			TEXT("GP Cargo.AddCargo rejected (no authority): Owner=%s Requested=%.3f"),
			*GetNameSafe(Owner),
			RequestedAmount);
		return 0.0f;
	}

	if (!IsFinitePositive(RequestedAmount) || !IsFinitePositive(CargoCapacity))
	{
		return 0.0f;
	}

	ClampCargoState();
	const float Accepted = FMath::Min(RequestedAmount, GetRemainingCapacity());
	if (Accepted <= 0.0f)
	{
		return 0.0f;
	}

	ApplyCargoAmount(CurrentCargoAmount + Accepted);
	return Accepted;
}

float UGP_CargoComponent::RemoveCargo(float RequestedAmount)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		UE_LOG(LogGPCargo, Warning,
			TEXT("GP Cargo.RemoveCargo rejected (no authority): Owner=%s Requested=%.3f"),
			*GetNameSafe(Owner),
			RequestedAmount);
		return 0.0f;
	}

	if (!IsFinitePositive(RequestedAmount))
	{
		return 0.0f;
	}

	ClampCargoState();
	const float Removed = FMath::Min(RequestedAmount, CurrentCargoAmount);
	if (Removed <= 0.0f)
	{
		return 0.0f;
	}

	ApplyCargoAmount(CurrentCargoAmount - Removed);
	return Removed;
}

float UGP_CargoComponent::ClearCargo()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		UE_LOG(LogGPCargo, Warning,
			TEXT("GP Cargo.ClearCargo rejected (no authority): Owner=%s"),
			*GetNameSafe(Owner));
		return 0.0f;
	}

	ClampCargoState();
	const float Removed = CurrentCargoAmount;
	if (Removed <= 0.0f)
	{
		return 0.0f;
	}

	ApplyCargoAmount(0.0f);
	return Removed;
}

void UGP_CargoComponent::OnRep_CurrentCargoAmount(float PreviousAmount)
{
	ClampCargoState();
	BroadcastCargoChanged(PreviousAmount, CurrentCargoAmount);
	UE_LOG(LogGPCargo, Verbose,
		TEXT("GP Cargo.OnRep_CurrentCargoAmount: Owner=%s Prev=%.3f New=%.3f Cap=%.3f"),
		*GetNameSafe(GetOwner()),
		PreviousAmount,
		CurrentCargoAmount,
		CargoCapacity);
}

bool UGP_CargoComponent::ValidateCargoContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	if (ResourceDefinition.IsNull())
	{
		OutErrors.Add(NSLOCTEXT("GPCargo", "ErrDefinitionUnset", "ResourceDefinition soft reference must be assigned."));
	}
	else
	{
		const UGP_ResourceDefinition* Definition = ResolveResourceDefinition(true);
		if (Definition == nullptr)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("GPCargo", "ErrDefinitionUnresolved", "ResourceDefinition could not be resolved: {0}"),
				FText::FromString(ResourceDefinition.ToSoftObjectPath().ToString())));
		}
		else
		{
			if (Definition->ResourceType == EGP_ResourceType::None)
			{
				OutErrors.Add(NSLOCTEXT("GPCargo", "ErrTypeNone", "ResourceDefinition.ResourceType must not be None."));
			}

			if (!Definition->ResourceGameplayTag.IsValid())
			{
				OutErrors.Add(NSLOCTEXT("GPCargo", "ErrTagInvalid", "ResourceDefinition.ResourceGameplayTag must be valid."));
			}
			else if (Definition->ResourceGameplayTag != FGPGameplayTags::Get().Resource_Type_Ferronite)
			{
				OutWarnings.Add(FText::Format(
					NSLOCTEXT(
						"GPCargo",
						"WarnTagUnexpected",
						"ResourceGameplayTag is {0}; Ferronite prototype expects GP.Resource.Type.Ferronite."),
					FText::FromString(Definition->ResourceGameplayTag.ToString())));
			}

			TArray<FText> DefErrors;
			TArray<FText> DefWarnings;
			Definition->ValidateDefinition(DefErrors, DefWarnings);
			OutErrors.Append(DefErrors);
			OutWarnings.Append(DefWarnings);
		}
	}

	if (!FMath::IsFinite(CargoCapacity) || CargoCapacity <= 0.0f)
	{
		OutErrors.Add(NSLOCTEXT("GPCargo", "ErrCapacity", "CargoCapacity must be finite and > 0."));
	}

	if (!FMath::IsFinite(CurrentCargoAmount))
	{
		OutErrors.Add(NSLOCTEXT("GPCargo", "ErrCurrentNonFinite", "CurrentCargoAmount must be finite."));
	}
	else if (CurrentCargoAmount < 0.0f || CurrentCargoAmount > CargoCapacity)
	{
		OutErrors.Add(NSLOCTEXT(
			"GPCargo",
			"ErrCurrentRange",
			"CurrentCargoAmount must be in range [0, CargoCapacity]."));
	}

	return OutErrors.Num() == 0;
}

#if WITH_EDITOR
EDataValidationResult UGP_CargoComponent::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const bool bOk = ValidateCargoContract(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}

	return bOk ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

AGP_CargoDiagnosticHost::AGP_CargoDiagnosticHost()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	CargoComponent = CreateDefaultSubobject<UGP_CargoComponent>(TEXT("CargoComponent"));
}

UGP_CargoComponent* AGP_CargoDiagnosticHost::GetCargoComponent() const
{
	return CargoComponent;
}

#if !UE_BUILD_SHIPPING
namespace GPCargoDebug
{
	static UGP_CargoComponent* FindCargo(UWorld* World, const FString& OptionalOwnerName)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		UGP_CargoComponent* Best = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor))
			{
				continue;
			}

			UGP_CargoComponent* Cargo = Actor->FindComponentByClass<UGP_CargoComponent>();
			if (Cargo == nullptr)
			{
				continue;
			}

			if (!OptionalOwnerName.IsEmpty())
			{
				if (Actor->GetName().Equals(OptionalOwnerName, ESearchCase::IgnoreCase)
					|| Actor->GetPathName().Contains(OptionalOwnerName))
				{
					return Cargo;
				}
				continue;
			}

			if (Best == nullptr || Actor->GetName() < Best->GetOwner()->GetName())
			{
				Best = Cargo;
			}
		}

		return Best;
	}

	static AGP_CargoDiagnosticHost* FindDiagnosticHost(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (TActorIterator<AGP_CargoDiagnosticHost> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static UGP_CargoComponent* EnsureCargo(UWorld* World, const FString& OptionalOwnerName, bool bSpawnHostIfMissing)
	{
		if (UGP_CargoComponent* Existing = FindCargo(World, OptionalOwnerName))
		{
			return Existing;
		}

		if (!bSpawnHostIfMissing || World == nullptr || World->GetNetMode() == NM_Client)
		{
			return nullptr;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_CargoDiagnosticHost* Host = World->SpawnActor<AGP_CargoDiagnosticHost>(
			AGP_CargoDiagnosticHost::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			Params);
		return Host != nullptr ? Host->GetCargoComponent() : nullptr;
	}

	static void CargoInspect(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Inspect: missing world"));
			return;
		}

		const FString OptionalName = Args.Num() > 0 ? Args[0] : FString();
		UGP_CargoComponent* Cargo = FindCargo(World, OptionalName);
		if (Cargo == nullptr)
		{
			UE_LOG(LogGPCargo, Warning,
				TEXT("GP Cargo.Inspect: no UGP_CargoComponent found (use gp.Cargo.SpawnDiagnosticHost)"));
			return;
		}

		AActor* Owner = Cargo->GetOwner();
		UGP_ResourceDefinition* Resolved = Cargo->ResolveResourceDefinition(true);
		FString PrimaryAssetIdStr = TEXT("none");
		FString TagStr = TEXT("none");
		if (Resolved != nullptr)
		{
			PrimaryAssetIdStr = Resolved->GetPrimaryAssetId().ToString();
			TagStr = Resolved->ResourceGameplayTag.ToString();
		}

		TArray<FText> Errors;
		TArray<FText> Warnings;
		const bool bValid = Cargo->ValidateCargoContract(Errors, Warnings);

		UE_LOG(LogGPCargo, Log,
			TEXT("GP Cargo.Inspect: Owner=%s Path=%s Class=%s Component=%s Role=%s NetMode=%s HasAuthority=%s SoftDefinition=%s ResolvedPrimaryAssetId=%s ResourceType=%s ResourceTag=%s CargoCapacity=%.3f CurrentCargoAmount=%.3f RemainingCapacity=%.3f FillRatio=%.3f IsEmpty=%s IsFull=%s ValidationOk=%s ValidationErrors=%d ValidationWarnings=%d Replicates=%s ComponentTickEnabled=%s ActorTickEnabled=%s"),
			*GetNameSafe(Owner),
			Owner != nullptr ? *Owner->GetPathName() : TEXT("none"),
			Owner != nullptr ? *GetNameSafe(Owner->GetClass()) : TEXT("none"),
			*Cargo->GetName(),
			Owner != nullptr ? GPCargoPrivate::RoleToString(Owner->GetLocalRole()) : TEXT("n/a"),
			GPCargoPrivate::NetModeToString(World->GetNetMode()),
			(Owner != nullptr && Owner->HasAuthority()) ? TEXT("true") : TEXT("false"),
			*Cargo->GetResourceDefinitionSoft().ToSoftObjectPath().ToString(),
			*PrimaryAssetIdStr,
			GPResourceTypePrivate::ToString(Cargo->GetCarriedResourceType()),
			*TagStr,
			Cargo->GetCargoCapacity(),
			Cargo->GetCurrentCargoAmount(),
			Cargo->GetRemainingCapacity(),
			Cargo->GetFillRatio(),
			Cargo->IsEmpty() ? TEXT("true") : TEXT("false"),
			Cargo->IsFull() ? TEXT("true") : TEXT("false"),
			bValid ? TEXT("true") : TEXT("false"),
			Errors.Num(),
			Warnings.Num(),
			Cargo->GetIsReplicated() ? TEXT("true") : TEXT("false"),
			Cargo->IsComponentTickEnabled() ? TEXT("true") : TEXT("false"),
			(Owner != nullptr && Owner->IsActorTickEnabled()) ? TEXT("true") : TEXT("false"));
	}

	static void CargoSpawnHost(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.SpawnDiagnosticHost: missing world"));
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.SpawnDiagnosticHost: rejected on client"));
			return;
		}

		if (AGP_CargoDiagnosticHost* Existing = FindDiagnosticHost(World))
		{
			UE_LOG(LogGPCargo, Log,
				TEXT("GP Cargo.SpawnDiagnosticHost: existing Host=%s"),
				*Existing->GetName());
			return;
		}

		UGP_CargoComponent* Cargo = EnsureCargo(World, FString(), true);
		UE_LOG(LogGPCargo, Log,
			TEXT("GP Cargo.SpawnDiagnosticHost: Host=%s Cargo=%s Cap=%.3f Current=%.3f"),
			*GetNameSafe(Cargo != nullptr ? Cargo->GetOwner() : nullptr),
			*GetNameSafe(Cargo),
			Cargo != nullptr ? Cargo->GetCargoCapacity() : 0.0f,
			Cargo != nullptr ? Cargo->GetCurrentCargoAmount() : 0.0f);
	}

	static void CargoAdd(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Add: missing world"));
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Add: rejected on client (authority required)"));
			return;
		}

		float Amount = 0.0f;
		if (Args.Num() < 1 || !LexTryParseString(Amount, *Args[0]))
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Add: usage gp.Cargo.Add <Amount> [OwnerName]"));
			return;
		}

		const FString OptionalName = Args.Num() > 1 ? Args[1] : FString();
		UGP_CargoComponent* Cargo = EnsureCargo(World, OptionalName, OptionalName.IsEmpty());
		if (Cargo == nullptr)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Add: no cargo component found"));
			return;
		}

		const float Before = Cargo->GetCurrentCargoAmount();
		const float Accepted = Cargo->AddCargo(Amount);
		UE_LOG(LogGPCargo, Log,
			TEXT("GP Cargo.Add: Owner=%s Requested=%.3f Accepted=%.3f Before=%.3f After=%.3f Cap=%.3f IsFull=%s"),
			*GetNameSafe(Cargo->GetOwner()),
			Amount,
			Accepted,
			Before,
			Cargo->GetCurrentCargoAmount(),
			Cargo->GetCargoCapacity(),
			Cargo->IsFull() ? TEXT("true") : TEXT("false"));
	}

	static void CargoRemove(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Remove: missing world"));
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Remove: rejected on client (authority required)"));
			return;
		}

		float Amount = 0.0f;
		if (Args.Num() < 1 || !LexTryParseString(Amount, *Args[0]))
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Remove: usage gp.Cargo.Remove <Amount> [OwnerName]"));
			return;
		}

		const FString OptionalName = Args.Num() > 1 ? Args[1] : FString();
		UGP_CargoComponent* Cargo = FindCargo(World, OptionalName);
		if (Cargo == nullptr)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Remove: no cargo component found"));
			return;
		}

		const float Before = Cargo->GetCurrentCargoAmount();
		const float Removed = Cargo->RemoveCargo(Amount);
		UE_LOG(LogGPCargo, Log,
			TEXT("GP Cargo.Remove: Owner=%s Requested=%.3f Removed=%.3f Before=%.3f After=%.3f IsEmpty=%s"),
			*GetNameSafe(Cargo->GetOwner()),
			Amount,
			Removed,
			Before,
			Cargo->GetCurrentCargoAmount(),
			Cargo->IsEmpty() ? TEXT("true") : TEXT("false"));
	}

	static void CargoClear(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Clear: missing world"));
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Clear: rejected on client (authority required)"));
			return;
		}

		const FString OptionalName = Args.Num() > 0 ? Args[0] : FString();
		UGP_CargoComponent* Cargo = FindCargo(World, OptionalName);
		if (Cargo == nullptr)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.Clear: no cargo component found"));
			return;
		}

		const float Before = Cargo->GetCurrentCargoAmount();
		const float Removed = Cargo->ClearCargo();
		UE_LOG(LogGPCargo, Log,
			TEXT("GP Cargo.Clear: Owner=%s Removed=%.3f Before=%.3f After=%.3f"),
			*GetNameSafe(Cargo->GetOwner()),
			Removed,
			Before,
			Cargo->GetCurrentCargoAmount());
	}

	static void CargoRunContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.RunContractTest: missing world"));
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPCargo, Warning, TEXT("GP Cargo.RunContractTest: rejected on client"));
			return;
		}

		UGP_CargoComponent* Cargo = EnsureCargo(World, FString(), true);
		if (Cargo == nullptr)
		{
			UE_LOG(LogGPCargo, Error, TEXT("GP Cargo.RunContractTest: failed to obtain cargo host"));
			return;
		}

		int32 Failures = 0;
		auto Expect = [&Failures](bool bOk, const TCHAR* Label)
		{
			if (!bOk)
			{
				++Failures;
				UE_LOG(LogGPCargo, Error, TEXT("GP Cargo.RunContractTest FAIL: %s"), Label);
			}
			else
			{
				UE_LOG(LogGPCargo, Log, TEXT("GP Cargo.RunContractTest PASS: %s"), Label);
			}
		};

		Cargo->ClearCargo();
		const float Cap = Cargo->GetCargoCapacity();
		Expect(Cap > 0.0f, TEXT("CapacityPositive"));

		const float AddHalf = Cargo->AddCargo(Cap * 0.5f);
		Expect(FMath::IsNearlyEqual(AddHalf, Cap * 0.5f), TEXT("AddWithinCapacity"));
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), Cap * 0.5f), TEXT("CurrentAfterHalf"));

		const float OverflowAccepted = Cargo->AddCargo(Cap);
		Expect(FMath::IsNearlyEqual(OverflowAccepted, Cap * 0.5f), TEXT("OverflowClampAccepted"));
		Expect(Cargo->IsFull(), TEXT("IsFullAfterOverflow"));

		const float BeforeInvalid = Cargo->GetCurrentCargoAmount();
		Expect(Cargo->AddCargo(-10.0f) == 0.0f, TEXT("RejectNegativeAdd"));
		Expect(Cargo->AddCargo(0.0f) == 0.0f, TEXT("RejectZeroAdd"));
		Expect(Cargo->AddCargo(std::numeric_limits<float>::quiet_NaN()) == 0.0f, TEXT("RejectNanAdd"));
		Expect(Cargo->AddCargo(std::numeric_limits<float>::infinity()) == 0.0f, TEXT("RejectInfAdd"));
		Expect(FMath::IsNearlyEqual(Cargo->GetCurrentCargoAmount(), BeforeInvalid), TEXT("NoMutationOnRejectedAdd"));

		const float RemovedPartial = Cargo->RemoveCargo(Cap * 0.2f);
		Expect(FMath::IsNearlyEqual(RemovedPartial, Cap * 0.2f), TEXT("RemovePartial"));

		const float CurrentBeforeOverRemove = Cargo->GetCurrentCargoAmount();
		const float RemovedOver = Cargo->RemoveCargo(Cap * 10.0f);
		Expect(FMath::IsNearlyEqual(RemovedOver, CurrentBeforeOverRemove), TEXT("RemoveMoreThanAvailable"));
		Expect(Cargo->IsEmpty(), TEXT("EmptyAfterOverRemove"));

		Cargo->AddCargo(Cap * 0.4f);
		const float Cleared = Cargo->ClearCargo();
		Expect(FMath::IsNearlyEqual(Cleared, Cap * 0.4f), TEXT("ClearRemovesAll"));
		Expect(Cargo->IsEmpty(), TEXT("EmptyAfterClear"));

		Expect(Cargo->RemoveCargo(-1.0f) == 0.0f, TEXT("RejectNegativeRemove"));
		Expect(Cargo->IsComponentTickEnabled() == false, TEXT("ComponentTickDisabled"));

		UE_LOG(LogGPCargo, Log,
			TEXT("GP Cargo.RunContractTest: Complete Failures=%d Owner=%s Cap=%.3f (map/assets not saved)"),
			Failures,
			*GetNameSafe(Cargo->GetOwner()),
			Cap);
	}

	static FAutoConsoleCommandWithWorldAndArgs GCargoInspectCommand(
		TEXT("gp.Cargo.Inspect"),
		TEXT("Inspect UGP_CargoComponent on owner (optional name). Non-shipping."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CargoInspect));

	static FAutoConsoleCommandWithWorldAndArgs GCargoSpawnHostCommand(
		TEXT("gp.Cargo.SpawnDiagnosticHost"),
		TEXT("Authority: spawn transient AGP_CargoDiagnosticHost with CargoComponent. Do not save maps."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CargoSpawnHost));

	static FAutoConsoleCommandWithWorldAndArgs GCargoAddCommand(
		TEXT("gp.Cargo.Add"),
		TEXT("Authority: AddCargo. Usage: gp.Cargo.Add <Amount> [OwnerName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CargoAdd));

	static FAutoConsoleCommandWithWorldAndArgs GCargoRemoveCommand(
		TEXT("gp.Cargo.Remove"),
		TEXT("Authority: RemoveCargo. Usage: gp.Cargo.Remove <Amount> [OwnerName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CargoRemove));

	static FAutoConsoleCommandWithWorldAndArgs GCargoClearCommand(
		TEXT("gp.Cargo.Clear"),
		TEXT("Authority: ClearCargo. Usage: gp.Cargo.Clear [OwnerName]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CargoClear));

	static FAutoConsoleCommandWithWorldAndArgs GCargoRunContractTestCommand(
		TEXT("gp.Cargo.RunContractTest"),
		TEXT("Authority: deterministic cargo contract checks on diagnostic host. Does not save maps."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CargoRunContractTest));
}
#endif
