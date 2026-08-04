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

			// Unresolved explicit parent → actor root (not presentation root) to avoid surprise scale inherit.
			UE_LOG(LogGPPrimitiveVisualBuilder, Warning,
				TEXT("GP PrimitiveVisualBuild: unresolved ParentPartName=%s for Part=%s; attach actor root"),
				*Part.ParentPartName.ToString(),
				*Part.PartName.ToString());
			return FallbackRoot;
		}

		if (!Part.bPresentationRoot && PresentationRoot != nullptr)
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

		const FGP_PrimitiveVisualValidationResult Validation =
			GPPrimitiveVisualDefaults::ValidateAndSanitizeDefinition(Definition);
		const FGP_PrimitiveVisualDefinition& Def =
			Validation.bValid ? Validation.SanitizedDefinition : Definition;

		if (!Validation.bValid)
		{
			UE_LOG(LogGPPrimitiveVisualBuilder, Warning,
				TEXT("GP PrimitiveVisualBuild: Owner=%s definition failed validation; building best-effort"),
				LogOwnerLabel != nullptr ? LogOwnerLabel : TEXT("null"));
		}

		struct FPendingPart
		{
			FGP_PrimitiveVisualPart Def;
			TObjectPtr<UStaticMeshComponent> Component;
		};
		TArray<FPendingPart> Pending;

		// Pass 1: create all parts attached to actor root so order-independent parents can resolve.
		for (const FGP_PrimitiveVisualPart& PartDef : Def.Parts)
		{
			if (PartDef.PartName.IsNone() || OutResult.PartLookup.Contains(PartDef.PartName))
			{
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
			PartComp->SetupAttachment(AttachRoot);
			PartComp->SetRelativeLocation(PartDef.RelativeLocation);
			PartComp->SetRelativeRotation(PartDef.RelativeRotation);
			PartComp->SetRelativeScale3D(PartDef.RelativeScale);
			PartComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PartComp->SetGenerateOverlapEvents(false);
			PartComp->SetCanEverAffectNavigation(false);
			PartComp->SetCastShadow(PartDef.bCastShadow);
			PartComp->SetVisibility(PartDef.bVisible, true);
			PartComp->SetHiddenInGame(!PartDef.bVisible);
			PartComp->SetStaticMesh(Mesh);
			PartComp->RegisterComponent();

			OutResult.PartComponents.Add(PartComp);
			OutResult.PartLookup.Add(PartDef.PartName, PartComp);

			if (PartDef.bPresentationRoot || OutResult.PresentationRootComponent == nullptr)
			{
				OutResult.PresentationRootComponent = PartComp;
				OutResult.PresentationRootPartName = PartDef.PartName;
			}

			FPendingPart PendingPart;
			PendingPart.Def = PartDef;
			PendingPart.Component = PartComp;
			Pending.Add(PendingPart);
		}

		// Pass 2: reparent according to hierarchy / presentation-root sibling policy.
		for (FPendingPart& PendingPart : Pending)
		{
			UStaticMeshComponent* PartComp = PendingPart.Component.Get();
			if (PartComp == nullptr)
			{
				continue;
			}

			USceneComponent* DesiredParent = ResolveAttachParent(
				PendingPart.Def,
				AttachRoot,
				OutResult.PresentationRootComponent.Get(),
				OutResult.PartLookup);
			if (DesiredParent == nullptr || DesiredParent == PartComp)
			{
				continue;
			}

			if (PartComp->GetAttachParent() != DesiredParent)
			{
				PartComp->AttachToComponent(DesiredParent, FAttachmentTransformRules::KeepRelativeTransform);
			}
			// Relative transforms are authored vs intended parent — re-apply after hierarchy resolve.
			PartComp->SetRelativeLocation(PendingPart.Def.RelativeLocation);
			PartComp->SetRelativeRotation(PendingPart.Def.RelativeRotation);
			PartComp->SetRelativeScale3D(PendingPart.Def.RelativeScale);
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
