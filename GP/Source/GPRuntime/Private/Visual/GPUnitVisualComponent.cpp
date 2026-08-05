// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPUnitVisualComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Units/GPUnitBase.h"

#if WITH_EDITOR
#include "Misc/App.h"
#endif

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Units/GPUnit.h"
#endif

DEFINE_LOG_CATEGORY(LogGPUnitVisual);

UGP_UnitVisualComponent::UGP_UnitVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UGP_UnitVisualComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	if (!IsTemplate() && GetWorld() != nullptr && !GetWorld()->IsGameWorld())
	{
		RefreshVisualMode();
	}
#endif
}

void UGP_UnitVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisualMode();
}

void UGP_UnitVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearVisual();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UGP_UnitVisualComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UGP_UnitVisualComponent, VisualSourceMode)
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UGP_UnitVisualComponent, VisualArchetype))
	{
		RefreshVisualMode();
	}
}
#endif

EGP_VisualArchetype UGP_UnitVisualComponent::GetVisualArchetype() const
{
	return VisualArchetype;
}

EGP_VisualSourceMode UGP_UnitVisualComponent::GetVisualSourceMode() const
{
	return VisualSourceMode;
}

bool UGP_UnitVisualComponent::UsesAuthoredComponents() const
{
	return VisualSourceMode == EGP_VisualSourceMode::AuthoredComponents;
}

int32 UGP_UnitVisualComponent::GetPartCount() const
{
	return BuiltVisual.PartComponents.Num();
}

bool UGP_UnitVisualComponent::IsDedicatedVisualSuppressed() const
{
	return bDedicatedVisualSuppressed;
}

bool UGP_UnitVisualComponent::HasBuiltVisual() const
{
	return bVisualBuilt;
}

int32 UGP_UnitVisualComponent::GetGeneratedPartCount() const
{
	return BuiltVisual.PartComponents.Num();
}

int32 UGP_UnitVisualComponent::GetAuthoredPrimitiveComponentCount() const
{
	RefreshAuthoredDiagnostics();
	return CachedAuthoredSnapshot.AuthoredPrimitiveComponentCount;
}

int32 UGP_UnitVisualComponent::GetAuthoredCollisionWarningCount() const
{
	RefreshAuthoredDiagnostics();
	return CachedAuthoredSnapshot.AuthoredCollisionWarnings;
}

int32 UGP_UnitVisualComponent::GetAuthoredNavigationWarningCount() const
{
	RefreshAuthoredDiagnostics();
	return CachedAuthoredSnapshot.AuthoredNavigationWarnings;
}

int32 UGP_UnitVisualComponent::GetDuplicateGeneratedPartCount() const
{
	return FMath::Max(0, BuiltVisual.PartComponents.Num() - BuiltVisual.PartLookup.Num());
}

bool UGP_UnitVisualComponent::AreGeneratedCollisionsDisabled() const
{
	return AreVisualPartCollisionsDisabled();
}

FName UGP_UnitVisualComponent::GetPresentationRootPartName() const
{
	return BuiltVisual.PresentationRootPartName;
}

void UGP_UnitVisualComponent::GetPartNames(TArray<FName>& OutNames) const
{
	OutNames.Reset();
	BuiltVisual.PartLookup.GetKeys(OutNames);
}

bool UGP_UnitVisualComponent::AreVisualPartCollisionsDisabled() const
{
	if (BuiltVisual.PartComponents.Num() == 0)
	{
		return true;
	}

	for (const TObjectPtr<UStaticMeshComponent>& Part : BuiltVisual.PartComponents)
	{
		if (Part != nullptr && Part->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			return false;
		}
	}

	return true;
}

void UGP_UnitVisualComponent::RefreshAuthoredDiagnostics() const
{
	if (!bAuthoredSnapshotDirty)
	{
		return;
	}

	TSet<const UActorComponent*> Generated;
	for (const TObjectPtr<UStaticMeshComponent>& Part : BuiltVisual.PartComponents)
	{
		if (Part != nullptr)
		{
			Generated.Add(Part.Get());
		}
	}

	GPAuthoredVisualDiagnostics::Collect(GetOwner(), Generated, CachedAuthoredSnapshot);
	bAuthoredSnapshotDirty = false;
}

bool UGP_UnitVisualComponent::ShouldSuppressVisualConstruction() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() == NM_DedicatedServer;
}

void UGP_UnitVisualComponent::SetVisualSourceMode(EGP_VisualSourceMode NewMode)
{
	VisualSourceMode = NewMode;
	RefreshVisualMode();
}

void UGP_UnitVisualComponent::RefreshVisualMode()
{
	bAuthoredSnapshotDirty = true;

	if (UsesAuthoredComponents())
	{
		ClearVisual();
		bDedicatedVisualSuppressed = ShouldSuppressVisualConstruction();
		UE_LOG(LogGPUnitVisual, Verbose,
			TEXT("GP UnitVisual: Owner=%s VisualSourceMode=AuthoredComponents GeneratedPartsCleared"),
			*GetNameSafe(GetOwner()));
		return;
	}

	RebuildVisual();
}

void UGP_UnitVisualComponent::RebuildVisual()
{
	ClearVisual();
	bAuthoredSnapshotDirty = true;

	if (UsesAuthoredComponents())
	{
		bDedicatedVisualSuppressed = ShouldSuppressVisualConstruction();
		return;
	}

	if (ShouldSuppressVisualConstruction())
	{
		bDedicatedVisualSuppressed = true;
		UE_LOG(LogGPUnitVisual, Verbose,
			TEXT("GP UnitVisual: Owner=%s Archetype=%s DedicatedVisualSuppressed=true (no parts)"),
			*GetNameSafe(GetOwner()),
			GPPrimitiveVisualDefaults::ArchetypeToString(VisualArchetype));
		return;
	}

	bDedicatedVisualSuppressed = false;
	AActor* Owner = GetOwner();
	USceneComponent* OwnerRoot = Owner != nullptr ? Owner->GetRootComponent() : nullptr;
	const FGP_PrimitiveVisualDefinition Definition =
		GPPrimitiveVisualDefaults::MakeDefinitionForArchetype(VisualArchetype);
	bVisualBuilt = GPPrimitiveVisualBuilder::BuildFromDefinition(
		Owner,
		OwnerRoot,
		Definition,
		BuiltVisual,
		*GetNameSafe(Owner));
	ApplyTeamColorFallback();
}

void UGP_UnitVisualComponent::ClearVisual()
{
	// Ownership guarantee: destroy only BuiltVisual generated parts.
	GPPrimitiveVisualBuilder::DestroyBuiltParts(BuiltVisual);
	bVisualBuilt = false;
	bAuthoredSnapshotDirty = true;
}

void UGP_UnitVisualComponent::ApplyTeamColorFallback()
{
	if (UsesAuthoredComponents())
	{
		return;
	}

	const AGP_UnitBase* Unit = Cast<AGP_UnitBase>(GetOwner());
	if (Unit == nullptr || BuiltVisual.PartComponents.Num() == 0)
	{
		return;
	}

	const int32 TeamId = Unit->GetTeamId();
	FLinearColor Tint = FLinearColor(0.75f, 0.75f, 0.78f, 1.0f);
	if (TeamId == 1)
	{
		Tint = FLinearColor(0.20f, 0.45f, 0.85f, 1.0f);
	}
	else if (TeamId == 2)
	{
		Tint = FLinearColor(0.85f, 0.30f, 0.20f, 1.0f);
	}
	else if (TeamId >= 3)
	{
		Tint = FLinearColor(0.25f, 0.75f, 0.35f, 1.0f);
	}

	bool bAnyParameterApplied = false;
	for (TObjectPtr<UStaticMeshComponent>& Part : BuiltVisual.PartComponents)
	{
		if (Part == nullptr)
		{
			continue;
		}

		UMaterialInstanceDynamic* MID = Part->CreateAndSetMaterialInstanceDynamic(0);
		if (MID == nullptr)
		{
			continue;
		}

		static const FName ParamNames[] = {
			TEXT("BaseColor"),
			TEXT("Color"),
			TEXT("Tint"),
			TEXT("BaseColorTint")
		};

		for (const FName& ParamName : ParamNames)
		{
			MID->SetVectorParameterValue(ParamName, Tint);
		}
		bAnyParameterApplied = true;
	}

	UE_LOG(LogGPUnitVisual, Verbose,
		TEXT("GP UnitVisualTeamTintAttempt: Owner=%s TeamId=%d DMICreated=%s Note=EngineBasicMaterialParamsUnverified"),
		*GetNameSafe(GetOwner()),
		TeamId,
		bAnyParameterApplied ? TEXT("true") : TEXT("false"));
}

#if !UE_BUILD_SHIPPING
namespace GPUnitVisualDebug
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

	static AGP_Unit* FindInspectUnit(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		AGP_Unit* Best = nullptr;
		for (TActorIterator<AGP_Unit> It(World); It; ++It)
		{
			AGP_Unit* Unit = *It;
			if (!IsValid(Unit))
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

	static void UnitVisualInspect(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitVisual, Warning, TEXT("GP UnitVisual.Inspect: missing world"));
			return;
		}

		AGP_Unit* Unit = FindInspectUnit(World);
		if (Unit == nullptr)
		{
			UE_LOG(LogGPUnitVisual, Warning, TEXT("GP UnitVisual.Inspect: no AGP_Unit found"));
			return;
		}

		const UGP_UnitVisualComponent* Visual = Unit->GetUnitVisualComponent();
		TArray<FName> PartNames;
		if (Visual != nullptr)
		{
			Visual->GetPartNames(PartNames);
		}

		FString PartNamesJoined = TEXT("none");
		if (PartNames.Num() > 0)
		{
			PartNamesJoined.Reset();
			for (int32 Index = 0; Index < PartNames.Num(); ++Index)
			{
				if (Index > 0)
				{
					PartNamesJoined += TEXT(",");
				}
				PartNamesJoined += PartNames[Index].ToString();
			}
		}

		UE_LOG(LogGPUnitVisual, Log,
			TEXT("GP UnitVisual.Inspect: Source=%s VisualComponent=%s Archetype=%s Parts=%d PartNames=[%s] PresentationRoot=%s Role=%s NetMode=%s DedicatedVisualSuppressed=%s TickEnabled=%s VisualPartCollisionDisabled=%s LegacyVisualMesh=%s HasBuiltVisual=%s VisualSourceMode=%s GeneratedPartCount=%d AuthoredPrimitiveComponentCount=%d NativeVisualBuilt=%s UsesAuthoredComponents=%s GeneratedCollisionDisabled=%s AuthoredCollisionWarnings=%d AuthoredNavigationWarnings=%d DuplicateGeneratedParts=%d"),
			*Unit->GetName(),
			Visual != nullptr ? TEXT("present") : TEXT("missing"),
			Visual != nullptr
				? GPPrimitiveVisualDefaults::ArchetypeToString(Visual->GetVisualArchetype())
				: TEXT("n/a"),
			Visual != nullptr ? Visual->GetPartCount() : 0,
			*PartNamesJoined,
			Visual != nullptr ? *Visual->GetPresentationRootPartName().ToString() : TEXT("n/a"),
			RoleToString(Unit->GetLocalRole()),
			NetModeToString(World->GetNetMode()),
			(Visual != nullptr && Visual->IsDedicatedVisualSuppressed()) ? TEXT("true") : TEXT("false"),
			(Visual != nullptr && Visual->IsComponentTickEnabled()) ? TEXT("true") : TEXT("false"),
			(Visual == nullptr || Visual->AreVisualPartCollisionsDisabled()) ? TEXT("true") : TEXT("false"),
			Unit->HasLegacyVisualMesh() ? TEXT("present") : TEXT("absent"),
			(Visual != nullptr && Visual->HasBuiltVisual()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr
				? GPPrimitiveVisualDefaults::VisualSourceModeToString(Visual->GetVisualSourceMode())
				: TEXT("n/a"),
			Visual != nullptr ? Visual->GetGeneratedPartCount() : 0,
			Visual != nullptr ? Visual->GetAuthoredPrimitiveComponentCount() : 0,
			(Visual != nullptr && Visual->HasBuiltVisual()) ? TEXT("true") : TEXT("false"),
			(Visual != nullptr && Visual->UsesAuthoredComponents()) ? TEXT("true") : TEXT("false"),
			(Visual == nullptr || Visual->AreGeneratedCollisionsDisabled()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr ? Visual->GetAuthoredCollisionWarningCount() : 0,
			Visual != nullptr ? Visual->GetAuthoredNavigationWarningCount() : 0,
			Visual != nullptr ? Visual->GetDuplicateGeneratedPartCount() : 0);
	}

	static FAutoConsoleCommandWithWorldAndArgs GUnitVisualInspectCommand(
		TEXT("gp.UnitVisual.Inspect"),
		TEXT("Inspect primitive unit visual composition for the first AGP_Unit in the world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UnitVisualInspect));
}
#endif
