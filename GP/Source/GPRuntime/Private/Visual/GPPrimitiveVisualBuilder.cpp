// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPPrimitiveVisualBuilder.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Visual/GPPrimitiveVisualMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPPrimitiveVisualBuilder, Log, All);

namespace GPPrimitiveVisualBuilder
{
	static USceneComponent* ResolveAttachParent(
		const FGP_PrimitiveVisualPart& Part,
		USceneComponent* FallbackRoot,
		USceneComponent* PresentationRoot,
		const TMap<FName, TObjectPtr<UStaticMeshComponent>>& PartLookup)
	{
		if (!Part.ParentPartName.IsNone())
		{
			if (const TObjectPtr<UStaticMeshComponent>* Found = PartLookup.Find(Part.ParentPartName))
			{
				if (Found->Get() != nullptr)
				{
					return Found->Get();
				}
			}
		}

		if (PresentationRoot != nullptr && !Part.bPresentationRoot)
		{
			return PresentationRoot;
		}

		return FallbackRoot;
	}

	bool BuildFromDefinition(
		AActor* Owner,
		USceneComponent* AttachRoot,
		const FGP_PrimitiveVisualDefinition& Definition,
		FBuildResult& OutResult,
		const TCHAR* LogOwnerLabel)
	{
		OutResult = FBuildResult();

		if (Owner == nullptr || AttachRoot == nullptr)
		{
			UE_LOG(LogGPPrimitiveVisualBuilder, Warning,
				TEXT("GP PrimitiveVisualBuild: Owner=%s missing owner/root"),
				LogOwnerLabel != nullptr ? LogOwnerLabel : TEXT("null"));
			return false;
		}

		for (const FGP_PrimitiveVisualPart& PartDef : Definition.Parts)
		{
			if (PartDef.PartName.IsNone())
			{
				UE_LOG(LogGPPrimitiveVisualBuilder, Warning,
					TEXT("GP PrimitiveVisualBuild: skip empty PartName Owner=%s"),
					LogOwnerLabel != nullptr ? LogOwnerLabel : TEXT("null"));
				continue;
			}

			if (OutResult.PartLookup.Contains(PartDef.PartName))
			{
				UE_LOG(LogGPPrimitiveVisualBuilder, Warning,
					TEXT("GP PrimitiveVisualBuild: duplicate PartName=%s Owner=%s"),
					*PartDef.PartName.ToString(),
					LogOwnerLabel != nullptr ? LogOwnerLabel : TEXT("null"));
				continue;
			}

			UStaticMesh* Mesh = GPPrimitiveVisualMesh::ResolveShapeMesh(PartDef.Shape);
			if (Mesh == nullptr)
			{
				UE_LOG(LogGPPrimitiveVisualBuilder, Warning,
					TEXT("GP PrimitiveVisualBuild: failed shape=%s Part=%s Owner=%s"),
					GPPrimitiveVisualDefaults::ShapeToString(PartDef.Shape),
					*PartDef.PartName.ToString(),
					LogOwnerLabel != nullptr ? LogOwnerLabel : TEXT("null"));
				continue;
			}

			USceneComponent* AttachParent = ResolveAttachParent(
				PartDef,
				AttachRoot,
				OutResult.PresentationRootComponent.Get(),
				OutResult.PartLookup);
			if (AttachParent == nullptr)
			{
				continue;
			}

			UStaticMeshComponent* PartComp = NewObject<UStaticMeshComponent>(
				Owner,
				UStaticMeshComponent::StaticClass(),
				PartDef.PartName,
				RF_Transient);
			if (PartComp == nullptr)
			{
				continue;
			}

			PartComp->SetMobility(EComponentMobility::Movable);
			PartComp->SetupAttachment(AttachParent);
			PartComp->SetRelativeLocation(PartDef.RelativeLocation);
			PartComp->SetRelativeRotation(PartDef.RelativeRotation);
			PartComp->SetRelativeScale3D(PartDef.RelativeScale);
			PartComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PartComp->SetGenerateOverlapEvents(false);
			PartComp->SetCanEverAffectNavigation(false);
			PartComp->SetCastShadow(false);
			PartComp->SetStaticMesh(Mesh);
			PartComp->RegisterComponent();

			OutResult.PartComponents.Add(PartComp);
			OutResult.PartLookup.Add(PartDef.PartName, PartComp);

			if (PartDef.bPresentationRoot || OutResult.PresentationRootComponent == nullptr)
			{
				OutResult.PresentationRootComponent = PartComp;
				OutResult.PresentationRootPartName = PartDef.PartName;
			}
		}

		const bool bBuilt = OutResult.PartComponents.Num() > 0;
		UE_LOG(LogGPPrimitiveVisualBuilder, Log,
			TEXT("GP PrimitiveVisualBuilt: Owner=%s Parts=%d PresentationRoot=%s"),
			LogOwnerLabel != nullptr ? LogOwnerLabel : TEXT("null"),
			OutResult.PartComponents.Num(),
			*OutResult.PresentationRootPartName.ToString());
		return bBuilt;
	}

	void DestroyBuiltParts(FBuildResult& InOutResult)
	{
		for (TObjectPtr<UStaticMeshComponent>& Part : InOutResult.PartComponents)
		{
			if (Part != nullptr)
			{
				Part->DestroyComponent();
				Part = nullptr;
			}
		}

		InOutResult = FBuildResult();
	}
}
