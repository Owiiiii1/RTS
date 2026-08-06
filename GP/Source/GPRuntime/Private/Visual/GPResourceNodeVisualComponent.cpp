// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPResourceNodeVisualComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Resources/GPResourceNode.h"

#if WITH_EDITOR
#include "Misc/App.h"
#endif

DEFINE_LOG_CATEGORY(LogGPResourceNodeVisual);

UGP_ResourceNodeVisualComponent::UGP_ResourceNodeVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UGP_ResourceNodeVisualComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	if (!IsTemplate() && GetWorld() != nullptr && !GetWorld()->IsGameWorld())
	{
		RefreshVisualMode();
	}
#endif
}

void UGP_ResourceNodeVisualComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisualMode();
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
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UGP_ResourceNodeVisualComponent, VisualSourceMode))
	{
		RefreshVisualMode();
	}
}
#endif

EGP_VisualSourceMode UGP_ResourceNodeVisualComponent::GetVisualSourceMode() const
{
	return VisualSourceMode;
}

bool UGP_ResourceNodeVisualComponent::UsesAuthoredComponents() const
{
	return VisualSourceMode == EGP_VisualSourceMode::AuthoredComponents;
}

bool UGP_ResourceNodeVisualComponent::ShouldUseGeneratedPrototypeVisual() const
{
	if (UsesAuthoredComponents())
	{
		return false;
	}

	if (const AGP_ResourceNode* Node = Cast<AGP_ResourceNode>(GetOwner()))
	{
		return Node->GetUseGeneratedPrototypeVisual();
	}

	// Non-ResourceNode owners (if any) keep NativeFallback behavior.
	return true;
}

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

int32 UGP_ResourceNodeVisualComponent::GetGeneratedPartCount() const
{
	return BuiltVisual.PartComponents.Num();
}

int32 UGP_ResourceNodeVisualComponent::GetAuthoredPrimitiveComponentCount() const
{
	RefreshAuthoredDiagnostics();
	return CachedAuthoredSnapshot.AuthoredPrimitiveComponentCount;
}

int32 UGP_ResourceNodeVisualComponent::GetAuthoredCollisionWarningCount() const
{
	RefreshAuthoredDiagnostics();
	return CachedAuthoredSnapshot.AuthoredCollisionWarnings;
}

int32 UGP_ResourceNodeVisualComponent::GetAuthoredNavigationWarningCount() const
{
	RefreshAuthoredDiagnostics();
	return CachedAuthoredSnapshot.AuthoredNavigationWarnings;
}

int32 UGP_ResourceNodeVisualComponent::GetDuplicateGeneratedPartCount() const
{
	return FMath::Max(0, BuiltVisual.PartComponents.Num() - BuiltVisual.PartLookup.Num());
}

bool UGP_ResourceNodeVisualComponent::AreGeneratedCollisionsDisabled() const
{
	return AreVisualPartCollisionsDisabled();
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

void UGP_ResourceNodeVisualComponent::RefreshAuthoredDiagnostics() const
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

bool UGP_ResourceNodeVisualComponent::ShouldSuppressVisualConstruction() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() == NM_DedicatedServer;
}

void UGP_ResourceNodeVisualComponent::SetVisualSourceMode(EGP_VisualSourceMode NewMode)
{
	VisualSourceMode = NewMode;
	RefreshVisualMode();
}

void UGP_ResourceNodeVisualComponent::RefreshVisualMode()
{
	bAuthoredSnapshotDirty = true;

	if (!ShouldUseGeneratedPrototypeVisual())
	{
		ClearVisual();
		bDedicatedVisualSuppressed = ShouldSuppressVisualConstruction();
		UE_LOG(LogGPResourceNodeVisual, Verbose,
			TEXT("GP ResourceNodeVisual: Owner=%s GeneratedPrototypeVisual=false GeneratedPartsCleared Mode=%s"),
			*GetNameSafe(GetOwner()),
			UsesAuthoredComponents() ? TEXT("AuthoredComponents") : TEXT("NativeFallback"));
		return;
	}

	RebuildVisual();
}

void UGP_ResourceNodeVisualComponent::RebuildVisual()
{
	ClearVisual();
	bAuthoredSnapshotDirty = true;

	if (!ShouldUseGeneratedPrototypeVisual())
	{
		bDedicatedVisualSuppressed = ShouldSuppressVisualConstruction();
		return;
	}

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
	const FGP_PrimitiveVisualDefinition Definition = GPPrimitiveVisualDefaults::MakeOreNodeDefinition();
	bVisualBuilt = GPPrimitiveVisualBuilder::BuildFromDefinition(
		Owner,
		OwnerRoot,
		Definition,
		BuiltVisual,
		*GetNameSafe(Owner));
}

void UGP_ResourceNodeVisualComponent::ClearVisual()
{
	// Ownership guarantee: destroy only BuiltVisual generated parts.
	GPPrimitiveVisualBuilder::DestroyBuiltParts(BuiltVisual);
	bVisualBuilt = false;
	bAuthoredSnapshotDirty = true;
}
