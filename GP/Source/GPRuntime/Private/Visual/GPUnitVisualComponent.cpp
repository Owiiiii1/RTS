// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPUnitVisualComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Units/GPUnitBase.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR
#include "Misc/App.h"
#endif

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Resources/GPResourceNode.h"
#include "Units/GPUnit.h"
#include "Visual/GPResourceNodeVisualComponent.h"
#endif

DEFINE_LOG_CATEGORY(LogGPUnitVisual);

UGP_UnitVisualComponent::UGP_UnitVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	VisualProfile = TSoftObjectPtr<UGP_PrimitiveVisualProfile>(
		FSoftObjectPath(GPPrimitiveVisualDefaults::DefaultInfantryMeleeProfilePath()));
}

void UGP_UnitVisualComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	if (!IsTemplate() && GetWorld() != nullptr && !GetWorld()->IsGameWorld())
	{
		RebuildVisual();
	}
#endif
}

void UGP_UnitVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	RebuildVisual();
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
	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(UGP_UnitVisualComponent, VisualProfile)
		|| PropName == GET_MEMBER_NAME_CHECKED(UGP_UnitVisualComponent, VisualArchetype))
	{
		RebuildVisual();
	}
}
#endif

EGP_VisualArchetype UGP_UnitVisualComponent::GetVisualArchetype() const
{
	return VisualArchetype;
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

bool UGP_UnitVisualComponent::IsUsingFallback() const
{
	return bUsingFallback;
}

EGP_VisualDefinitionSource UGP_UnitVisualComponent::GetActiveVisualSource() const
{
	return ActiveVisualSource;
}

FString UGP_UnitVisualComponent::GetVisualProfilePath() const
{
	return VisualProfile.ToSoftObjectPath().IsValid()
		? VisualProfile.ToSoftObjectPath().ToString()
		: FString(TEXT("None"));
}

bool UGP_UnitVisualComponent::IsProfileValid() const
{
	return bProfileValid;
}

int32 UGP_UnitVisualComponent::GetProfileValidationErrorCount() const
{
	return ProfileValidationErrorCount;
}

int32 UGP_UnitVisualComponent::GetProfilePartCount() const
{
	return ProfilePartCount;
}

bool UGP_UnitVisualComponent::IsHierarchyValid() const
{
	return bHierarchyValid;
}

int32 UGP_UnitVisualComponent::GetDuplicatePartNameCount() const
{
	return DuplicatePartNameCount;
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

void UGP_UnitVisualComponent::SetVisualProfile(TSoftObjectPtr<UGP_PrimitiveVisualProfile> NewProfile)
{
	VisualProfile = NewProfile;
	RebuildVisual();
}

bool UGP_UnitVisualComponent::ShouldSuppressVisualConstruction() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() == NM_DedicatedServer;
}

FGP_PrimitiveVisualDefinition UGP_UnitVisualComponent::ResolveDefinition()
{
	bUsingFallback = true;
	bProfileValid = false;
	bHierarchyValid = true;
	ProfileValidationErrorCount = 0;
	ProfilePartCount = 0;
	DuplicatePartNameCount = 0;
	ActiveVisualSource = EGP_VisualDefinitionSource::NativeFallback;

	UGP_PrimitiveVisualProfile* Profile = VisualProfile.IsNull() ? nullptr : VisualProfile.LoadSynchronous();
	if (Profile != nullptr)
	{
		ProfilePartCount = Profile->Parts.Num();
		TArray<FString> Errors;
		FGP_PrimitiveVisualDefinition FromAsset;
		if (Profile->GetValidatedDefinition(FromAsset, Errors))
		{
			FGP_PrimitiveVisualDefinition Check = FromAsset;
			Check.Archetype = VisualArchetype;
			const FGP_PrimitiveVisualValidationResult Validation =
				GPPrimitiveVisualDefaults::ValidateAndSanitizeDefinition(Check);
			bProfileValid = Validation.bValid;
			bHierarchyValid = Validation.bHierarchyValid;
			DuplicatePartNameCount = Validation.DuplicatePartNameCount;
			ProfileValidationErrorCount = Validation.Errors.Num();
			if (Validation.bValid)
			{
				bUsingFallback = false;
				ActiveVisualSource = EGP_VisualDefinitionSource::DataAsset;
				FromAsset.Archetype = VisualArchetype;
				return Validation.SanitizedDefinition.Parts.Num() > 0
					? Validation.SanitizedDefinition
					: FromAsset;
			}
		}
		else
		{
			ProfileValidationErrorCount = Errors.Num();
			bHierarchyValid = false;
		}

		UE_LOG(LogGPUnitVisual, Warning,
			TEXT("GP UnitVisual: Owner=%s invalid profile=%s — native InfantryMelee fallback"),
			*GetNameSafe(GetOwner()),
			*GetVisualProfilePath());
	}

	FGP_PrimitiveVisualDefinition Native =
		GPPrimitiveVisualDefaults::MakeDefinitionForArchetype(VisualArchetype);
	const FGP_PrimitiveVisualValidationResult NativeValidation =
		GPPrimitiveVisualDefaults::ValidateAndSanitizeDefinition(Native);
	bHierarchyValid = NativeValidation.bHierarchyValid;
	DuplicatePartNameCount = NativeValidation.DuplicatePartNameCount;
	return NativeValidation.bValid ? NativeValidation.SanitizedDefinition : Native;
}

void UGP_UnitVisualComponent::RebuildVisual()
{
	ClearVisual();

	if (ShouldSuppressVisualConstruction())
	{
		bDedicatedVisualSuppressed = true;
		UE_LOG(LogGPUnitVisual, Verbose,
			TEXT("GP UnitVisual: Owner=%s DedicatedVisualSuppressed=true"),
			*GetNameSafe(GetOwner()));
		return;
	}

	bDedicatedVisualSuppressed = false;
	AActor* Owner = GetOwner();
	USceneComponent* OwnerRoot = Owner != nullptr ? Owner->GetRootComponent() : nullptr;
	CachedDefinition = ResolveDefinition();
	bVisualBuilt = GPPrimitiveVisualBuilder::BuildFromDefinition(
		Owner,
		OwnerRoot,
		CachedDefinition,
		BuiltVisual,
		*GetNameSafe(Owner));
	ApplyTeamColorFallback();
}

void UGP_UnitVisualComponent::ClearVisual()
{
	GPPrimitiveVisualBuilder::DestroyBuiltParts(BuiltVisual);
	bVisualBuilt = false;
}

void UGP_UnitVisualComponent::ApplyTeamColorFallback()
{
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

	TSet<FName> TintEligible;
	for (const FGP_PrimitiveVisualPart& Part : CachedDefinition.Parts)
	{
		if (Part.bTeamTintEligible)
		{
			TintEligible.Add(Part.PartName);
		}
	}

	bool bAnyParameterApplied = false;
	for (TObjectPtr<UStaticMeshComponent>& Part : BuiltVisual.PartComponents)
	{
		if (Part == nullptr || !TintEligible.Contains(Part->GetFName()))
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
			TEXT("GP UnitVisual.Inspect: Source=%s VisualComponent=%s Archetype=%s Parts=%d PartNames=[%s] PresentationRoot=%s Role=%s NetMode=%s DedicatedVisualSuppressed=%s TickEnabled=%s VisualCollisionDisabled=%s LegacyVisualMesh=%s HasBuiltVisual=%s VisualProfile=%s VisualSource=%s ProfileValid=%s ProfileValidationErrors=%d ProfilePartCount=%d BuiltPartCount=%d IsUsingFallback=%s DuplicatePartNames=%d HierarchyValid=%s"),
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
			Visual != nullptr ? *Visual->GetVisualProfilePath() : TEXT("n/a"),
			Visual != nullptr
				? GPPrimitiveVisualDefaults::VisualSourceToString(Visual->GetActiveVisualSource())
				: TEXT("n/a"),
			(Visual != nullptr && Visual->IsProfileValid()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr ? Visual->GetProfileValidationErrorCount() : 0,
			Visual != nullptr ? Visual->GetProfilePartCount() : 0,
			Visual != nullptr ? Visual->GetPartCount() : 0,
			(Visual != nullptr && Visual->IsUsingFallback()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr ? Visual->GetDuplicatePartNameCount() : 0,
			(Visual == nullptr || Visual->IsHierarchyValid()) ? TEXT("true") : TEXT("false"));
	}

	static void VisualRebuild(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPUnitVisual, Warning, TEXT("GP Visual.Rebuild: missing world"));
			return;
		}

		FString Target = Args.Num() > 0 ? Args[0] : TEXT("All");
		const bool bUnit = Target.Equals(TEXT("Unit"), ESearchCase::IgnoreCase) || Target.Equals(TEXT("All"), ESearchCase::IgnoreCase);
		const bool bResource = Target.Equals(TEXT("Resource"), ESearchCase::IgnoreCase) || Target.Equals(TEXT("All"), ESearchCase::IgnoreCase);
		if (!bUnit && !bResource)
		{
			UE_LOG(LogGPUnitVisual, Warning, TEXT("GP Visual.Rebuild: usage gp.Visual.Rebuild [Unit|Resource|All]"));
			return;
		}

		if (bUnit)
		{
			if (AGP_Unit* Unit = FindInspectUnit(World))
			{
				if (UGP_UnitVisualComponent* Visual = Unit->GetUnitVisualComponent())
				{
					Visual->RebuildVisual();
					UE_LOG(LogGPUnitVisual, Log, TEXT("GP Visual.Rebuild: Unit=%s Source=%s Fallback=%s"),
						*Unit->GetName(),
						GPPrimitiveVisualDefaults::VisualSourceToString(Visual->GetActiveVisualSource()),
						Visual->IsUsingFallback() ? TEXT("true") : TEXT("false"));
				}
			}
			else
			{
				UE_LOG(LogGPUnitVisual, Warning, TEXT("GP Visual.Rebuild: no AGP_Unit found"));
			}
		}

		if (bResource)
		{
			AGP_ResourceNode* Best = nullptr;
			for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
			{
				if (IsValid(*It) && (Best == nullptr || (*It)->GetName() < Best->GetName()))
				{
					Best = *It;
				}
			}

			if (Best != nullptr)
			{
				if (UGP_ResourceNodeVisualComponent* Visual = Best->GetResourceNodeVisualComponent())
				{
					Visual->RebuildVisual();
					UE_LOG(LogGPUnitVisual, Log, TEXT("GP Visual.Rebuild: Resource=%s Source=%s Fallback=%s"),
						*Best->GetName(),
						GPPrimitiveVisualDefaults::VisualSourceToString(Visual->GetActiveVisualSource()),
						Visual->IsUsingFallback() ? TEXT("true") : TEXT("false"));
				}
			}
			else
			{
				UE_LOG(LogGPUnitVisual, Warning, TEXT("GP Visual.Rebuild: no AGP_ResourceNode found"));
			}
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs GUnitVisualInspectCommand(
		TEXT("gp.UnitVisual.Inspect"),
		TEXT("Inspect primitive unit visual composition for the first AGP_Unit in the world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UnitVisualInspect));

	static FAutoConsoleCommandWithWorldAndArgs GVisualRebuildCommand(
		TEXT("gp.Visual.Rebuild"),
		TEXT("Local cosmetic rebuild of first Unit and/or ResourceNode visual. Usage: gp.Visual.Rebuild [Unit|Resource|All]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&VisualRebuild));
}
#endif
