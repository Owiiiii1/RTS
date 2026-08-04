// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPResourceNodeVisualComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR
#include "Misc/App.h"
#endif

DEFINE_LOG_CATEGORY(LogGPResourceNodeVisual);

UGP_ResourceNodeVisualComponent::UGP_ResourceNodeVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	VisualProfile = TSoftObjectPtr<UGP_PrimitiveVisualProfile>(
		FSoftObjectPath(GPPrimitiveVisualDefaults::DefaultOreProfilePath()));
}

void UGP_ResourceNodeVisualComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	if (!IsTemplate() && GetWorld() != nullptr && !GetWorld()->IsGameWorld())
	{
		RebuildVisual();
	}
#endif
}

void UGP_ResourceNodeVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	RebuildVisual();
}

void UGP_ResourceNodeVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearVisual();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UGP_ResourceNodeVisualComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UGP_ResourceNodeVisualComponent, VisualProfile))
	{
		RebuildVisual();
	}
}
#endif

int32 UGP_ResourceNodeVisualComponent::GetPartCount() const
{
	return BuiltVisual.PartComponents.Num();
}

bool UGP_ResourceNodeVisualComponent::IsDedicatedVisualSuppressed() const
{
	return bDedicatedVisualSuppressed;
}

bool UGP_ResourceNodeVisualComponent::HasBuiltVisual() const
{
	return bVisualBuilt;
}

bool UGP_ResourceNodeVisualComponent::IsUsingFallback() const
{
	return bUsingFallback;
}

EGP_VisualDefinitionSource UGP_ResourceNodeVisualComponent::GetActiveVisualSource() const
{
	return ActiveVisualSource;
}

FString UGP_ResourceNodeVisualComponent::GetVisualProfilePath() const
{
	return VisualProfile.ToSoftObjectPath().IsValid()
		? VisualProfile.ToSoftObjectPath().ToString()
		: FString(TEXT("None"));
}

bool UGP_ResourceNodeVisualComponent::IsProfileValid() const
{
	return bProfileValid;
}

int32 UGP_ResourceNodeVisualComponent::GetProfileValidationErrorCount() const
{
	return ProfileValidationErrorCount;
}

int32 UGP_ResourceNodeVisualComponent::GetProfilePartCount() const
{
	return ProfilePartCount;
}

bool UGP_ResourceNodeVisualComponent::IsHierarchyValid() const
{
	return bHierarchyValid;
}

int32 UGP_ResourceNodeVisualComponent::GetDuplicatePartNameCount() const
{
	return DuplicatePartNameCount;
}

FName UGP_ResourceNodeVisualComponent::GetPresentationRootPartName() const
{
	return BuiltVisual.PresentationRootPartName;
}

void UGP_ResourceNodeVisualComponent::GetPartNames(TArray<FName>& OutNames) const
{
	OutNames.Reset();
	BuiltVisual.PartLookup.GetKeys(OutNames);
}

bool UGP_ResourceNodeVisualComponent::AreVisualPartCollisionsDisabled() const
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

void UGP_ResourceNodeVisualComponent::SetVisualProfile(TSoftObjectPtr<UGP_PrimitiveVisualProfile> NewProfile)
{
	VisualProfile = NewProfile;
	RebuildVisual();
}

bool UGP_ResourceNodeVisualComponent::ShouldSuppressVisualConstruction() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() == NM_DedicatedServer;
}

FGP_PrimitiveVisualDefinition UGP_ResourceNodeVisualComponent::ResolveDefinition()
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
			const FGP_PrimitiveVisualValidationResult Validation =
				GPPrimitiveVisualDefaults::ValidateAndSanitizeDefinition(FromAsset);
			bProfileValid = Validation.bValid;
			bHierarchyValid = Validation.bHierarchyValid;
			DuplicatePartNameCount = Validation.DuplicatePartNameCount;
			ProfileValidationErrorCount = Validation.Errors.Num();
			if (Validation.bValid)
			{
				bUsingFallback = false;
				ActiveVisualSource = EGP_VisualDefinitionSource::DataAsset;
				return Validation.SanitizedDefinition;
			}
		}
		else
		{
			ProfileValidationErrorCount = Errors.Num();
			bHierarchyValid = false;
		}

		UE_LOG(LogGPResourceNodeVisual, Warning,
			TEXT("GP ResourceNodeVisual: Owner=%s invalid profile=%s — native Ore fallback"),
			*GetNameSafe(GetOwner()),
			*GetVisualProfilePath());
	}

	FGP_PrimitiveVisualDefinition Native = GPPrimitiveVisualDefaults::MakeOreNodeDefinition();
	const FGP_PrimitiveVisualValidationResult NativeValidation =
		GPPrimitiveVisualDefaults::ValidateAndSanitizeDefinition(Native);
	bHierarchyValid = NativeValidation.bHierarchyValid;
	DuplicatePartNameCount = NativeValidation.DuplicatePartNameCount;
	return NativeValidation.bValid ? NativeValidation.SanitizedDefinition : Native;
}

void UGP_ResourceNodeVisualComponent::RebuildVisual()
{
	ClearVisual();

	if (ShouldSuppressVisualConstruction())
	{
		bDedicatedVisualSuppressed = true;
		UE_LOG(LogGPResourceNodeVisual, Verbose,
			TEXT("GP ResourceNodeVisual: Owner=%s DedicatedVisualSuppressed=true"),
			*GetNameSafe(GetOwner()));
		return;
	}

	bDedicatedVisualSuppressed = false;
	AActor* Owner = GetOwner();
	USceneComponent* OwnerRoot = Owner != nullptr ? Owner->GetRootComponent() : nullptr;
	const FGP_PrimitiveVisualDefinition Definition = ResolveDefinition();
	bVisualBuilt = GPPrimitiveVisualBuilder::BuildFromDefinition(
		Owner,
		OwnerRoot,
		Definition,
		BuiltVisual,
		*GetNameSafe(Owner));
}

void UGP_ResourceNodeVisualComponent::ClearVisual()
{
	GPPrimitiveVisualBuilder::DestroyBuiltParts(BuiltVisual);
	bVisualBuilt = false;
}
