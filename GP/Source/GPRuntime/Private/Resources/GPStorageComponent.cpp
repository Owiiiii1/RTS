// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPStorageComponent.h"

#include "Engine/AssetManager.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Resources/GPResourceDefinition.h"

#include <limits>

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY(LogGPStorage);

UGP_StorageComponent::UGP_StorageComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	ResourceDefinition = TSoftObjectPtr<UGP_ResourceDefinition>(
		FSoftObjectPath(UGP_ResourceDefinition::DefaultFerroniteAssetPath()));
	ContainerCapacity = 100.0f;
	ContainerCount = 5;
}

void UGP_StorageComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() != nullptr && GetOwner()->HasAuthority())
	{
		EnsureContainerArray();
		ResolveResourceDefinition(false);
	}
}

void UGP_StorageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGP_StorageComponent, ContainerCapacity);
	DOREPLIFETIME(UGP_StorageComponent, ContainerCount);
	DOREPLIFETIME(UGP_StorageComponent, Containers);
}

TSoftObjectPtr<UGP_ResourceDefinition> UGP_StorageComponent::GetResourceDefinitionSoft() const
{
	return ResourceDefinition;
}

UGP_ResourceDefinition* UGP_StorageComponent::GetResolvedResourceDefinition() const
{
	return CachedResourceDefinition.Get();
}

UGP_ResourceDefinition* UGP_StorageComponent::ResolveResourceDefinition(bool bAllowSynchronousLoad) const
{
	if (CachedResourceDefinition.IsValid())
	{
		return CachedResourceDefinition.Get();
	}

	if (ResourceDefinition.IsNull())
	{
		return nullptr;
	}

	if (UGP_ResourceDefinition* Loaded = ResourceDefinition.Get())
	{
		CachedResourceDefinition = Loaded;
		return Loaded;
	}

	if (bAllowSynchronousLoad)
	{
		if (UGP_ResourceDefinition* SyncLoaded = ResourceDefinition.LoadSynchronous())
		{
			CachedResourceDefinition = SyncLoaded;
			return SyncLoaded;
		}
	}

	return nullptr;
}

float UGP_StorageComponent::GetThreatPerStoredUnit() const
{
	if (const UGP_ResourceDefinition* Definition = ResolveResourceDefinition(true))
	{
		if (FMath::IsFinite(Definition->ThreatPerStoredUnit) && Definition->ThreatPerStoredUnit >= 0.0f)
		{
			return Definition->ThreatPerStoredUnit;
		}
	}
	// Code default matches UGP_ResourceDefinition::ThreatPerStoredUnit (0.5).
	return 0.5f;
}

#if !UE_BUILD_SHIPPING
void UGP_StorageComponent::DebugForceContainerLaunching(int32 Index, bool bLaunching)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	EnsureContainerArray();
	if (!Containers.IsValidIndex(Index))
	{
		return;
	}

	FGP_StorageContainer& Container = Containers[Index];
	if (bLaunching)
	{
		Container.State = EGP_StorageContainerState::Launching;
	}
	else
	{
		RefreshContainerState(Container);
	}
}
#endif

float UGP_StorageComponent::GetTotalStored() const
{
	float Total = 0.0f;
	for (const FGP_StorageContainer& Container : Containers)
	{
		Total += Container.CurrentAmount;
	}
	return Total;
}

float UGP_StorageComponent::GetTotalCapacity() const
{
	return ContainerCapacity * static_cast<float>(FMath::Max(0, ContainerCount));
}

float UGP_StorageComponent::GetTotalRemaining() const
{
	float Remaining = 0.0f;
	for (const FGP_StorageContainer& Container : Containers)
	{
		if (!IsAcceptableFillTarget(Container))
		{
			continue;
		}
		Remaining += FMath::Max(0.0f, ContainerCapacity - Container.CurrentAmount);
	}
	return Remaining;
}

int32 UGP_StorageComponent::GetReadyCount() const
{
	int32 Count = 0;
	for (const FGP_StorageContainer& Container : Containers)
	{
		if (Container.State == EGP_StorageContainerState::Ready)
		{
			++Count;
		}
	}
	return Count;
}

int32 UGP_StorageComponent::GetLaunchingCount() const
{
	int32 Count = 0;
	for (const FGP_StorageContainer& Container : Containers)
	{
		if (Container.State == EGP_StorageContainerState::Launching)
		{
			++Count;
		}
	}
	return Count;
}

bool UGP_StorageComponent::IsStorageFull() const
{
	return GetTotalRemaining() <= KINDA_SMALL_NUMBER;
}

void UGP_StorageComponent::EnsureContainerArray()
{
	const int32 Desired = FMath::Max(1, ContainerCount);
	if (Containers.Num() != Desired)
	{
		Containers.SetNum(Desired);
	}

	for (FGP_StorageContainer& Container : Containers)
	{
		if (!FMath::IsFinite(Container.CurrentAmount) || Container.CurrentAmount < 0.0f)
		{
			Container.CurrentAmount = 0.0f;
		}
		Container.CurrentAmount = FMath::Clamp(Container.CurrentAmount, 0.0f, ContainerCapacity);
		RefreshContainerState(Container);
	}
}

void UGP_StorageComponent::RefreshContainerState(FGP_StorageContainer& Container) const
{
	if (Container.State == EGP_StorageContainerState::Launching)
	{
		return;
	}

	if (Container.CurrentAmount <= KINDA_SMALL_NUMBER)
	{
		Container.CurrentAmount = 0.0f;
		Container.State = EGP_StorageContainerState::Empty;
	}
	else if (Container.CurrentAmount + KINDA_SMALL_NUMBER >= ContainerCapacity)
	{
		Container.CurrentAmount = ContainerCapacity;
		Container.State = EGP_StorageContainerState::Ready;
	}
	else
	{
		Container.State = EGP_StorageContainerState::Filling;
	}
}

bool UGP_StorageComponent::IsAcceptableFillTarget(const FGP_StorageContainer& Container) const
{
	if (Container.State == EGP_StorageContainerState::Launching)
	{
		return false;
	}
	return Container.CurrentAmount + KINDA_SMALL_NUMBER < ContainerCapacity;
}

bool UGP_StorageComponent::IsFinitePositive(float Value) const
{
	return FMath::IsFinite(Value) && Value > 0.0f;
}

FGP_StorageAddResult UGP_StorageComponent::AddPlanetaryFerronite(float RequestedAmount)
{
	FGP_StorageAddResult Result;
	Result.Requested = RequestedAmount;

	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		Result.bRejectedInvalidInput = true;
		Result.Rejected = RequestedAmount;
		UE_LOG(LogGPStorage, Warning, TEXT("AddPlanetaryFerronite denied without authority on %s"), *GetNameSafe(Owner));
		return Result;
	}

	if (!IsFinitePositive(RequestedAmount))
	{
		Result.bRejectedInvalidInput = true;
		Result.Rejected = RequestedAmount;
		return Result;
	}

	EnsureContainerArray();
	const float PreviousTotal = GetTotalStored();
	float Remaining = RequestedAmount;

	for (int32 Index = 0; Index < Containers.Num() && Remaining > KINDA_SMALL_NUMBER; ++Index)
	{
		FGP_StorageContainer& Container = Containers[Index];
		if (!IsAcceptableFillTarget(Container))
		{
			continue;
		}

		const float Free = ContainerCapacity - Container.CurrentAmount;
		if (Free <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Before = Container.CurrentAmount;
		const float Add = FMath::Min(Remaining, Free);
		Container.CurrentAmount = Before + Add;
		Remaining -= Add;
		Result.Accepted += Add;
		++Result.ContainersTouched;

		const EGP_StorageContainerState PrevState = Container.State;
		RefreshContainerState(Container);
		if (PrevState != EGP_StorageContainerState::Ready
			&& Container.State == EGP_StorageContainerState::Ready)
		{
			Result.bReadyContainerCreated = true;
		}
	}

	Result.Rejected = FMath::Max(0.0f, RequestedAmount - Result.Accepted);
	Result.bStorageFullAfter = IsStorageFull();

	if (Result.Accepted > KINDA_SMALL_NUMBER)
	{
		BroadcastStorageChanged(PreviousTotal);
	}

	return Result;
}

float UGP_StorageComponent::RemovePlanetaryFerronite(float RequestedAmount)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		UE_LOG(LogGPStorage, Warning, TEXT("RemovePlanetaryFerronite denied without authority on %s"), *GetNameSafe(Owner));
		return 0.0f;
	}

	if (!IsFinitePositive(RequestedAmount))
	{
		return 0.0f;
	}

	EnsureContainerArray();
	const float PreviousTotal = GetTotalStored();
	float Remaining = RequestedAmount;
	float Removed = 0.0f;

	for (int32 Index = Containers.Num() - 1; Index >= 0 && Remaining > KINDA_SMALL_NUMBER; --Index)
	{
		FGP_StorageContainer& Container = Containers[Index];
		if (Container.State == EGP_StorageContainerState::Launching)
		{
			continue;
		}
		if (Container.CurrentAmount <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Take = FMath::Min(Remaining, Container.CurrentAmount);
		Container.CurrentAmount -= Take;
		Remaining -= Take;
		Removed += Take;
		RefreshContainerState(Container);
	}

	if (Removed > KINDA_SMALL_NUMBER)
	{
		BroadcastStorageChanged(PreviousTotal);
	}

	return Removed;
}

void UGP_StorageComponent::BroadcastStorageChanged(float PreviousTotal)
{
	const float NewTotal = GetTotalStored();
	OnStorageChanged.Broadcast(PreviousTotal, NewTotal, GetTotalCapacity());
}

void UGP_StorageComponent::OnRep_Containers()
{
	const float Total = GetTotalStored();
	OnStorageChanged.Broadcast(Total, Total, GetTotalCapacity());
}

bool UGP_StorageComponent::ValidateStorageContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	const AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		OutErrors.Add(NSLOCTEXT("GPStorage", "ErrOwner", "StorageComponent requires an owner actor."));
	}

	if (!FMath::IsFinite(ContainerCapacity) || ContainerCapacity <= 0.0f)
	{
		OutErrors.Add(NSLOCTEXT("GPStorage", "ErrCapacity", "ContainerCapacity must be finite and > 0."));
	}
	if (ContainerCount <= 0)
	{
		OutErrors.Add(NSLOCTEXT("GPStorage", "ErrCount", "ContainerCount must be > 0."));
	}
	if (Containers.Num() != ContainerCount)
	{
		OutErrors.Add(NSLOCTEXT("GPStorage", "ErrArraySize", "Containers array size must equal ContainerCount."));
	}
	if (PrimaryComponentTick.bCanEverTick)
	{
		OutErrors.Add(NSLOCTEXT("GPStorage", "ErrTick", "StorageComponent must not enable permanent Tick."));
	}
	if (!GetIsReplicated())
	{
		OutWarnings.Add(NSLOCTEXT("GPStorage", "WarnRep", "StorageComponent should be replicated."));
	}

	float Sum = 0.0f;
	for (int32 Index = 0; Index < Containers.Num(); ++Index)
	{
		const FGP_StorageContainer& Container = Containers[Index];
		if (!FMath::IsFinite(Container.CurrentAmount)
			|| Container.CurrentAmount < 0.0f
			|| Container.CurrentAmount > ContainerCapacity + KINDA_SMALL_NUMBER)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("GPStorage", "ErrAmount", "Container {0} amount out of range."),
				FText::AsNumber(Index)));
		}

		Sum += Container.CurrentAmount;

		const bool bEmpty = Container.CurrentAmount <= KINDA_SMALL_NUMBER;
		const bool bFull = Container.CurrentAmount + KINDA_SMALL_NUMBER >= ContainerCapacity;
		if (Container.State == EGP_StorageContainerState::Launching)
		{
			continue;
		}
		if (bEmpty && Container.State != EGP_StorageContainerState::Empty)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("GPStorage", "ErrStateEmpty", "Container {0} amount empty but state != Empty."),
				FText::AsNumber(Index)));
		}
		else if (bFull && Container.State != EGP_StorageContainerState::Ready)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("GPStorage", "ErrStateReady", "Container {0} amount full but state != Ready."),
				FText::AsNumber(Index)));
		}
		else if (!bEmpty && !bFull && Container.State != EGP_StorageContainerState::Filling)
		{
			OutErrors.Add(FText::Format(
				NSLOCTEXT("GPStorage", "ErrStateFilling", "Container {0} partial amount but state != Filling."),
				FText::AsNumber(Index)));
		}
	}

	if (!FMath::IsNearlyEqual(Sum, GetTotalStored()))
	{
		OutErrors.Add(NSLOCTEXT("GPStorage", "ErrTotal", "TotalStored must equal sum of container amounts."));
	}

	if (ResourceDefinition.IsNull())
	{
		OutWarnings.Add(NSLOCTEXT("GPStorage", "WarnDefNull", "ResourceDefinition soft pointer is null."));
	}

	return OutErrors.Num() == 0;
}

#if WITH_EDITOR
EDataValidationResult UGP_StorageComponent::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const bool bOk = ValidateStorageContract(Errors, Warnings);
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

#if !UE_BUILD_SHIPPING
#include "Buildings/GPMainBase.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UObject/Package.h"

#include <limits>

namespace GPStorageDebug
{
	TWeakObjectPtr<UGP_StorageContractTestRunner> GActiveStorageContractTest;

	static const TCHAR* NetModeToString(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone: return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer: return TEXT("ListenServer");
		case NM_Client: return TEXT("Client");
		default: return TEXT("Unknown");
		}
	}

	static const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None: return TEXT("None");
		case ROLE_SimulatedProxy: return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy: return TEXT("AutonomousProxy");
		case ROLE_Authority: return TEXT("Authority");
		default: return TEXT("Unknown");
		}
	}

	static const TCHAR* ContainerStateToString(EGP_StorageContainerState State)
	{
		switch (State)
		{
		case EGP_StorageContainerState::Empty: return TEXT("Empty");
		case EGP_StorageContainerState::Filling: return TEXT("Filling");
		case EGP_StorageContainerState::Ready: return TEXT("Ready");
		case EGP_StorageContainerState::Launching: return TEXT("Launching");
		default: return TEXT("Unknown");
		}
	}

	static AGP_MainBase* FindMainBase(UWorld* World, const FString& OptionalName)
	{
		if (World == nullptr)
		{
			return nullptr;
		}
		AGP_MainBase* Fallback = nullptr;
		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			AGP_MainBase* Base = *It;
			if (!IsValid(Base))
			{
				continue;
			}
			if (!OptionalName.IsEmpty() && Base->GetName() == OptionalName)
			{
				return Base;
			}
			if (Fallback == nullptr)
			{
				Fallback = Base;
			}
		}
		return OptionalName.IsEmpty() ? Fallback : nullptr;
	}

	static UGP_StorageComponent* FindStorage(UWorld* World, const FString& OptionalName)
	{
		if (AGP_MainBase* Base = FindMainBase(World, OptionalName))
		{
			return Base->GetStorageComponent();
		}
		if (World == nullptr || !OptionalName.IsEmpty())
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (UGP_StorageComponent* Storage = It->FindComponentByClass<UGP_StorageComponent>())
			{
				return Storage;
			}
		}
		return nullptr;
	}

	static void StorageList(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Storage.List: missing world"));
			return;
		}

		int32 Count = 0;
		for (TActorIterator<AGP_MainBase> It(World); It; ++It)
		{
			AGP_MainBase* Base = *It;
			if (!IsValid(Base))
			{
				continue;
			}
			++Count;
			UGP_StorageComponent* Storage = Base->GetStorageComponent();
			UE_LOG(LogGPStorage, Log,
				TEXT("GP Storage.List: MainBase=%s TeamId=%d DropOff=%.1f Stored=%.1f/%.1f Remaining=%.1f Ready=%d Launching=%d Full=%s ThreatPerUnit=%.3f"),
				*Base->GetName(),
				Base->GetTeamId(),
				Base->GetDropOffRangeCm(),
				IsValid(Storage) ? Storage->GetTotalStored() : -1.0f,
				IsValid(Storage) ? Storage->GetTotalCapacity() : -1.0f,
				IsValid(Storage) ? Storage->GetTotalRemaining() : -1.0f,
				IsValid(Storage) ? Storage->GetReadyCount() : -1,
				IsValid(Storage) ? Storage->GetLaunchingCount() : -1,
				IsValid(Storage) && Storage->IsStorageFull() ? TEXT("true") : TEXT("false"),
				IsValid(Storage) ? Storage->GetThreatPerStoredUnit() : -1.0f);
		}
		UE_LOG(LogGPStorage, Log, TEXT("GP Storage.List Summary: MainBases=%d NetMode=%s"), Count, NetModeToString(World->GetNetMode()));
	}

	static void StorageSpawnDiagnostic(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Storage.SpawnDiagnostic: missing world or client"));
			return;
		}

		int32 TeamId = 1;
		if (Args.Num() > 0)
		{
			LexTryParseString(TeamId, *Args[0]);
		}
		if (TeamId < 1)
		{
			TeamId = 1;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		const FVector Loc(-45000.0f, 0.0f, 100.0f);
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(AGP_MainBase::StaticClass(), Loc, FRotator::ZeroRotator, Params);
		if (IsValid(Base))
		{
			Base->SetTeamId(TeamId);
		}
		UGP_StorageComponent* Storage = IsValid(Base) ? Base->GetStorageComponent() : nullptr;
		UE_LOG(LogGPStorage, Log,
			TEXT("GP Storage.SpawnDiagnostic: MainBase=%s TeamId=%d Storage=%s Cap=%.1f Containers=%d DropOff=%.1f"),
			*GetNameSafe(Base),
			TeamId,
			*GetNameSafe(Storage),
			IsValid(Storage) ? Storage->GetTotalCapacity() : -1.0f,
			IsValid(Storage) ? Storage->GetContainerCount() : -1,
			IsValid(Base) ? Base->GetDropOffRangeCm() : -1.0f);
	}

	static void StorageInspect(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}
		UGP_StorageComponent* Storage = FindStorage(World, Args.Num() > 0 ? Args[0] : FString());
		if (!IsValid(Storage))
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Storage.Inspect: no storage found"));
			return;
		}
		AActor* Owner = Storage->GetOwner();
		TArray<FText> Errors;
		TArray<FText> Warnings;
		const bool bValid = Storage->ValidateStorageContract(Errors, Warnings);
		UE_LOG(LogGPStorage, Log,
			TEXT("GP Storage.Inspect: Owner=%s Role=%s NetMode=%s Authority=%s Stored=%.1f/%.1f Remaining=%.1f Ready=%d Launching=%d Full=%s ThreatPerUnit=%.3f SoftDef=%s Tick=%s ValidationOk=%s Errors=%d Warnings=%d"),
			*GetNameSafe(Owner),
			Owner != nullptr ? RoleToString(Owner->GetLocalRole()) : TEXT("n/a"),
			NetModeToString(World->GetNetMode()),
			(Owner != nullptr && Owner->HasAuthority()) ? TEXT("true") : TEXT("false"),
			Storage->GetTotalStored(),
			Storage->GetTotalCapacity(),
			Storage->GetTotalRemaining(),
			Storage->GetReadyCount(),
			Storage->GetLaunchingCount(),
			Storage->IsStorageFull() ? TEXT("true") : TEXT("false"),
			Storage->GetThreatPerStoredUnit(),
			*Storage->GetResourceDefinitionSoft().ToSoftObjectPath().ToString(),
			Storage->IsComponentTickEnabled() ? TEXT("true") : TEXT("false"),
			bValid ? TEXT("true") : TEXT("false"),
			Errors.Num(),
			Warnings.Num());

		const TArray<FGP_StorageContainer>& Containers = Storage->GetContainers();
		for (int32 Index = 0; Index < Containers.Num(); ++Index)
		{
			UE_LOG(LogGPStorage, Log,
				TEXT("GP Storage.Inspect Container[%d]: Amount=%.1f State=%s"),
				Index,
				Containers[Index].CurrentAmount,
				ContainerStateToString(Containers[Index].State));
		}
	}

	static void StorageAdd(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || World->GetNetMode() == NM_Client || Args.Num() < 2)
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Storage.Add: usage gp.Storage.Add <Name> <Amount>"));
			return;
		}
		UGP_StorageComponent* Storage = FindStorage(World, Args[0]);
		float Amount = 0.0f;
		if (!IsValid(Storage) || !LexTryParseString(Amount, *Args[1]))
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Storage.Add: invalid storage or amount"));
			return;
		}
		const float Before = Storage->GetTotalStored();
		const FGP_StorageAddResult Result = Storage->AddPlanetaryFerronite(Amount);
		UE_LOG(LogGPStorage, Log,
			TEXT("GP Storage.Add: Owner=%s Requested=%.1f Accepted=%.1f Rejected=%.1f Before=%.1f After=%.1f ReadyCreated=%s Full=%s"),
			*GetNameSafe(Storage->GetOwner()),
			Result.Requested,
			Result.Accepted,
			Result.Rejected,
			Before,
			Storage->GetTotalStored(),
			Result.bReadyContainerCreated ? TEXT("true") : TEXT("false"),
			Result.bStorageFullAfter ? TEXT("true") : TEXT("false"));
	}

	static void StorageRunContractTest(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Storage.RunContractTest: missing world or client"));
			return;
		}
		if (GActiveStorageContractTest.IsValid())
		{
			UE_LOG(LogGPStorage, Warning, TEXT("GP Storage.RunContractTest: rejected — already running"));
			return;
		}
		UGP_StorageContractTestRunner* Runner = NewObject<UGP_StorageContractTestRunner>(GetTransientPackage());
		Runner->AddToRoot();
		GActiveStorageContractTest = Runner;
		Runner->Start(World);
	}

	static FAutoConsoleCommandWithWorldAndArgs GStorageList(
		TEXT("gp.Storage.List"),
		TEXT("List MainBase storage summaries."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StorageList));

	static FAutoConsoleCommandWithWorldAndArgs GStorageSpawn(
		TEXT("gp.Storage.SpawnDiagnostic"),
		TEXT("Authority: spawn transient AGP_MainBase. Usage: gp.Storage.SpawnDiagnostic [TeamId=1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StorageSpawnDiagnostic));

	static FAutoConsoleCommandWithWorldAndArgs GStorageInspect(
		TEXT("gp.Storage.Inspect"),
		TEXT("Inspect storage. Usage: gp.Storage.Inspect [Name]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StorageInspect));

	static FAutoConsoleCommandWithWorldAndArgs GStorageAdd(
		TEXT("gp.Storage.Add"),
		TEXT("Authority AddPlanetaryFerronite. Usage: gp.Storage.Add <Name> <Amount>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StorageAdd));

	static FAutoConsoleCommandWithWorldAndArgs GStorageContract(
		TEXT("gp.Storage.RunContractTest"),
		TEXT("Staged Storage contract test (GP-S28)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StorageRunContractTest));
}

void UGP_StorageContractTestRunner::BeginDestroy()
{
	Finish();
	Super::BeginDestroy();
}

void UGP_StorageContractTestRunner::UnbindWorldCleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
}

void UGP_StorageContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;
	if (World == nullptr || World == WorldWeak.Get() || !WorldWeak.IsValid())
	{
		Finish();
	}
}

void UGP_StorageContractTestRunner::Start(UWorld* InWorld)
{
	bFinished = false;
	WorldWeak = InWorld;
	StageIndex = 0;
	Failures = 0;
	UnbindWorldCleanup();
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UGP_StorageContractTestRunner::OnWorldCleanup);
	UE_LOG(LogGPStorage, Log, TEXT("GP Storage.RunContractTest Stage=Start"));
	ScheduleNext();
}

void UGP_StorageContractTestRunner::ScheduleNext()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalid"));
		return;
	}
	StageTimerHandle = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UGP_StorageContractTestRunner::AdvanceStage));
}

bool UGP_StorageContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	if (!bOk)
	{
		++Failures;
		UE_LOG(LogGPStorage, Error, TEXT("GP Storage.RunContractTest FAIL: %s"), Label);
		return false;
	}
	UE_LOG(LogGPStorage, Log, TEXT("GP Storage.RunContractTest PASS: %s"), Label);
	return true;
}

void UGP_StorageContractTestRunner::Abort(const TCHAR* Reason)
{
	if (bFinished)
	{
		return;
	}
	UE_LOG(LogGPStorage, Error, TEXT("GP Storage.RunContractTest ABORT: %s"), Reason);
	++Failures;
	Finish();
}

void UGP_StorageContractTestRunner::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;
	UnbindWorldCleanup();
	if (UWorld* World = WorldWeak.Get())
	{
		World->GetTimerManager().ClearTimer(StageTimerHandle);
	}
	if (AGP_MainBase* Base = MainBaseWeak.Get())
	{
		if (IsValid(Base))
		{
			Base->Destroy();
		}
	}
	MainBaseWeak.Reset();
	UE_LOG(LogGPStorage, Log, TEXT("GP Storage.RunContractTest: Complete Failures=%d"), Failures);
	RemoveFromRoot();
	GPStorageDebug::GActiveStorageContractTest.Reset();
	WorldWeak.Reset();
}

void UGP_StorageContractTestRunner::AdvanceStage()
{
	UWorld* World = WorldWeak.Get();
	if (!IsValid(World))
	{
		Abort(TEXT("WorldInvalidDuringStage"));
		return;
	}

	switch (StageIndex)
	{
	case 0:
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		AGP_MainBase* Base = World->SpawnActor<AGP_MainBase>(
			AGP_MainBase::StaticClass(),
			FVector(-45500.0f, 0.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		if (!Expect(IsValid(Base), TEXT("SpawnMainBase")))
		{
			Finish();
			return;
		}
		Base->SetTeamId(1);
		MainBaseWeak = Base;
		UGP_StorageComponent* Storage = Base->GetStorageComponent();
		Expect(IsValid(Storage), TEXT("HasStorage"));
		Expect(Storage->GetContainerCount() == 5, TEXT("ContainerCount5"));
		Expect(FMath::IsNearlyEqual(Storage->GetContainerCapacity(), 100.0f), TEXT("ContainerCap100"));
		Expect(FMath::IsNearlyEqual(Storage->GetTotalCapacity(), 500.0f), TEXT("TotalCap500"));
		Expect(FMath::IsNearlyEqual(Storage->GetTotalStored(), 0.0f), TEXT("EmptyStart"));
		Expect(Storage->IsComponentTickEnabled() == false, TEXT("NoTick"));
		Expect(Storage->PrimaryComponentTick.bCanEverTick == false, TEXT("CanEverTickFalse"));
		Expect(FMath::IsNearlyEqual(Storage->GetThreatPerStoredUnit(), 0.5f), TEXT("ThreatPerUnit0_5"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 1:
	{
		UGP_StorageComponent* Storage = MainBaseWeak.IsValid() ? MainBaseWeak->GetStorageComponent() : nullptr;
		if (!Expect(IsValid(Storage), TEXT("AddObjects")))
		{
			Finish();
			return;
		}
		const FGP_StorageAddResult Partial = Storage->AddPlanetaryFerronite(40.0f);
		Expect(FMath::IsNearlyEqual(Partial.Accepted, 40.0f), TEXT("PartialAccepted40"));
		Expect(Partial.Rejected <= KINDA_SMALL_NUMBER, TEXT("PartialNoReject"));
		Expect(Storage->GetContainers()[0].State == EGP_StorageContainerState::Filling, TEXT("FillingState"));
		const FGP_StorageAddResult Ready = Storage->AddPlanetaryFerronite(60.0f);
		Expect(FMath::IsNearlyEqual(Ready.Accepted, 60.0f), TEXT("FillToReadyAccepted"));
		Expect(Ready.bReadyContainerCreated, TEXT("ReadyCreated"));
		Expect(Storage->GetReadyCount() == 1, TEXT("ReadyCount1"));
		Expect(Storage->GetContainers()[0].State == EGP_StorageContainerState::Ready, TEXT("ReadyState"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 2:
	{
		UGP_StorageComponent* Storage = MainBaseWeak.IsValid() ? MainBaseWeak->GetStorageComponent() : nullptr;
		if (!Expect(IsValid(Storage), TEXT("FullObjects")))
		{
			Finish();
			return;
		}
		const FGP_StorageAddResult FillRest = Storage->AddPlanetaryFerronite(10000.0f);
		Expect(FillRest.Accepted > KINDA_SMALL_NUMBER, TEXT("FillRestAccepted"));
		Expect(Storage->IsStorageFull(), TEXT("StorageFull"));
		Expect(FillRest.bStorageFullAfter, TEXT("FullAfterFlag"));
		const FGP_StorageAddResult Overflow = Storage->AddPlanetaryFerronite(25.0f);
		Expect(Overflow.Accepted <= KINDA_SMALL_NUMBER, TEXT("FullRejectAccepted0"));
		Expect(FMath::IsNearlyEqual(Overflow.Rejected, 25.0f), TEXT("FullRejected25"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 3:
	{
		UGP_StorageComponent* Storage = MainBaseWeak.IsValid() ? MainBaseWeak->GetStorageComponent() : nullptr;
		if (!Expect(IsValid(Storage), TEXT("InvalidObjects")))
		{
			Finish();
			return;
		}
		const float Before = Storage->GetTotalStored();
		Expect(Storage->AddPlanetaryFerronite(-1.0f).bRejectedInvalidInput, TEXT("RejectNegative"));
		Expect(Storage->AddPlanetaryFerronite(0.0f).bRejectedInvalidInput, TEXT("RejectZero"));
		Expect(Storage->AddPlanetaryFerronite(std::numeric_limits<float>::quiet_NaN()).bRejectedInvalidInput, TEXT("RejectNan"));
		Expect(FMath::IsNearlyEqual(Storage->GetTotalStored(), Before), TEXT("NoMutationOnInvalid"));
		const float Removed = Storage->RemovePlanetaryFerronite(50.0f);
		Expect(Removed > KINDA_SMALL_NUMBER, TEXT("RemovePartial"));
		Expect(!Storage->IsStorageFull(), TEXT("NotFullAfterRemove"));
		++StageIndex;
		ScheduleNext();
		break;
	}
	case 4:
	{
		UGP_StorageComponent* Storage = MainBaseWeak.IsValid() ? MainBaseWeak->GetStorageComponent() : nullptr;
		if (!Expect(IsValid(Storage), TEXT("LaunchObjects")))
		{
			Finish();
			return;
		}
		Storage->DebugForceContainerLaunching(0, true);
		Expect(Storage->GetContainers()[0].State == EGP_StorageContainerState::Launching, TEXT("LaunchingState"));
		Expect(Storage->GetLaunchingCount() >= 1, TEXT("LaunchingCount"));
		const float RemainingBefore = Storage->GetTotalRemaining();
		Storage->AddPlanetaryFerronite(10.0f);
		// Launching container must not accept fill.
		Expect(Storage->GetContainers()[0].State == EGP_StorageContainerState::Launching, TEXT("LaunchingPreserved"));
		Storage->DebugForceContainerLaunching(0, false);
		TArray<FText> Errors;
		TArray<FText> Warnings;
		Expect(Storage->ValidateStorageContract(Errors, Warnings), TEXT("ValidateOk"));
		Expect(FMath::IsNearlyEqual(Storage->GetThreatPerStoredUnit(), 0.5f), TEXT("ThreatStill0_5"));
		(void)RemainingBefore;
		Finish();
		break;
	}
	default:
		Abort(TEXT("UnknownStage"));
		break;
	}
}

#else
void UGP_StorageContractTestRunner::BeginDestroy()
{
	bFinished = true;
	Super::BeginDestroy();
}
void UGP_StorageContractTestRunner::Start(UWorld* InWorld) { (void)InWorld; }
void UGP_StorageContractTestRunner::ScheduleNext() {}
void UGP_StorageContractTestRunner::AdvanceStage() {}
bool UGP_StorageContractTestRunner::Expect(bool bOk, const TCHAR* Label)
{
	(void)bOk;
	(void)Label;
	return false;
}
void UGP_StorageContractTestRunner::Abort(const TCHAR* Reason) { (void)Reason; }
void UGP_StorageContractTestRunner::Finish() { bFinished = true; }
void UGP_StorageContractTestRunner::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	(void)World;
	(void)bSessionEnded;
	(void)bCleanupResources;
}
void UGP_StorageContractTestRunner::UnbindWorldCleanup() {}
#endif
