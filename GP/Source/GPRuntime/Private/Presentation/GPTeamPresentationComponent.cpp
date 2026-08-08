// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presentation/GPTeamPresentationComponent.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Settings/GPGameplayPresentationSettings.h"
#include "Units/GPUnitBase.h"
#include "Visual/GPUnitVisualComponent.h"

UGP_TeamPresentationComponent::UGP_TeamPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UGP_TeamPresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshTeamPresentation();
}

FLinearColor UGP_TeamPresentationComponent::GetTeamPresentationColor() const
{
	const AGP_UnitBase* Unit = Cast<AGP_UnitBase>(GetOwner());
	const int32 TeamId = Unit != nullptr ? Unit->GetTeamId() : -1;
	if (const UGP_GameplayPresentationSettings* Settings = UGP_GameplayPresentationSettings::Get())
	{
		return Settings->GetTeamColor(TeamId);
	}
	return FLinearColor::White;
}

void UGP_TeamPresentationComponent::RefreshTeamPresentation()
{
	const FLinearColor Color = GetTeamPresentationColor();
	AppliedTeamColor = Color;
	bHasAppliedOnce = true;
	ApplyColorToMeshComponents(Color);

	if (AGP_UnitBase* Unit = Cast<AGP_UnitBase>(GetOwner()))
	{
		if (UGP_UnitVisualComponent* Visual = Unit->FindComponentByClass<UGP_UnitVisualComponent>())
		{
			Visual->RefreshTeamColorFromPresentation();
		}
	}
}

void UGP_TeamPresentationComponent::ApplyColorToMeshComponents(const FLinearColor& Color)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	const UGP_GameplayPresentationSettings* Settings = UGP_GameplayPresentationSettings::Get();
	const FName PreferredParam =
		Settings != nullptr ? Settings->TeamColorParameterName : FName(TEXT("TeamColor"));

	static const FName FallbackParams[] = {
		TEXT("TeamColor"),
		TEXT("BaseColor"),
		TEXT("Color"),
		TEXT("Tint"),
		TEXT("BaseColorTint")
	};

	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents<UMeshComponent>(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh == nullptr)
		{
			continue;
		}

		const int32 NumMaterials = Mesh->GetNumMaterials();
		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(Index);
			if (MID == nullptr)
			{
				continue;
			}

			if (!PreferredParam.IsNone())
			{
				MID->SetVectorParameterValue(PreferredParam, Color);
			}
			for (const FName& ParamName : FallbackParams)
			{
				MID->SetVectorParameterValue(ParamName, Color);
			}
		}
	}
}
