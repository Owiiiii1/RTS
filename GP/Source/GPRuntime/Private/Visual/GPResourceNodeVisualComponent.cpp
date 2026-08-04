// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPResourceNodeVisualComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Visual/GPPrimitiveVisualTypes.h"

DEFINE_LOG_CATEGORY(LogGPResourceNodeVisual);

UGP_ResourceNodeVisualComponent::UGP_ResourceNodeVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
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

bool UGP_ResourceNodeVisualComponent::ShouldSuppressVisualConstruction() const
{
	const UWorld* World = GetWorld();
	return World != nullptr && World->GetNetMode() == NM_DedicatedServer;
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
	GPPrimitiveVisualBuilder::DestroyBuiltParts(BuiltVisual);
	bVisualBuilt = false;
}
