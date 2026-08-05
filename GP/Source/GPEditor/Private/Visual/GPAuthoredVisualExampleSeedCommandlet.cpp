// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visual/GPAuthoredVisualExampleSeedCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Resources/GPResourceDefinition.h"
#include "Resources/GPResourceNode.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Units/GPUnit.h"
#include "Visual/GPPrimitiveVisualMesh.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "Visual/GPResourceNodeVisualComponent.h"
#include "Visual/GPUnitVisualComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPAuthoredVisualSeed, Log, All);

namespace GPAuthoredVisualSeedPrivate
{
	static constexpr const TCHAR* UnitPackagePath = TEXT("/Game/GrimProtocol/Units/BP_Unit_AuthoredExample");
	static constexpr const TCHAR* UnitAssetPath =
		TEXT("/Game/GrimProtocol/Units/BP_Unit_AuthoredExample.BP_Unit_AuthoredExample");
	static constexpr const TCHAR* ResourcePackagePath =
		TEXT("/Game/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample");
	static constexpr const TCHAR* ResourceAssetPath =
		TEXT("/Game/GrimProtocol/Resources/BP_ResourceNode_AuthoredExample.BP_ResourceNode_AuthoredExample");

	static void ConfigurePresentationMesh(UStaticMeshComponent* MeshComp, UStaticMesh* Mesh, const FTransform& Relative)
	{
		if (MeshComp == nullptr)
		{
			return;
		}

		MeshComp->SetStaticMesh(Mesh);
		MeshComp->SetRelativeTransform(Relative);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetGenerateOverlapEvents(false);
		MeshComp->SetCanEverAffectNavigation(false);
		MeshComp->SetCastShadow(true);
		MeshComp->SetVisibility(true);
		MeshComp->SetHiddenInGame(false);
		MeshComp->SetMobility(EComponentMobility::Movable);
	}

	static USCS_Node* AddSceneNode(USimpleConstructionScript* SCS, const FName Name)
	{
		USCS_Node* Node = SCS->CreateNode(USceneComponent::StaticClass(), Name);
		SCS->AddNode(Node);
		if (USceneComponent* Template = Cast<USceneComponent>(Node->ComponentTemplate))
		{
			Template->SetMobility(EComponentMobility::Movable);
		}
		return Node;
	}

	static USCS_Node* AddMeshChild(
		USCS_Node* Parent,
		USimpleConstructionScript* SCS,
		const FName Name,
		UStaticMesh* Mesh,
		const FTransform& Relative)
	{
		USCS_Node* Node = SCS->CreateNode(UStaticMeshComponent::StaticClass(), Name);
		Parent->AddChildNode(Node);
		ConfigurePresentationMesh(Cast<UStaticMeshComponent>(Node->ComponentTemplate), Mesh, Relative);
		return Node;
	}

	static bool SaveBlueprintPackage(UBlueprint* Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return false;
		}

		UPackage* Package = Blueprint->GetOutermost();
		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Blueprint);

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GError;
		SaveArgs.SaveFlags = SAVE_None;

		const bool bSaved = UPackage::SavePackage(Package, Blueprint, *PackageFilename, SaveArgs);
		UE_LOG(LogGPAuthoredVisualSeed, Log, TEXT("SavePackage %s Result=%s"),
			*Blueprint->GetPathName(),
			bSaved ? TEXT("OK") : TEXT("FAIL"));
		return bSaved;
	}

	static void ClearScsNodes(USimpleConstructionScript* SCS)
	{
		if (SCS == nullptr)
		{
			return;
		}

		const TArray<USCS_Node*> AllNodes = SCS->GetAllNodes();
		for (USCS_Node* Node : AllNodes)
		{
			if (Node != nullptr)
			{
				SCS->RemoveNode(Node);
			}
		}
	}

	static UBlueprint* GetOrCreateBlueprint(const TCHAR* AssetPath, const TCHAR* PackagePath, UClass* ParentClass, const FName AssetName)
	{
		if (UBlueprint* Existing = LoadObject<UBlueprint>(nullptr, AssetPath))
		{
			UE_LOG(LogGPAuthoredVisualSeed, Log, TEXT("Updating existing Blueprint %s"), AssetPath);
			ClearScsNodes(Existing->SimpleConstructionScript);
			return Existing;
		}

		UPackage* Package = CreatePackage(PackagePath);
		Package->FullyLoad();

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			Package,
			AssetName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			NAME_None);

		if (Blueprint == nullptr)
		{
			UE_LOG(LogGPAuthoredVisualSeed, Error, TEXT("CreateBlueprint failed: %s"), PackagePath);
		}

		return Blueprint;
	}

	template <typename TVisualComponent>
	static bool ApplyAuthoredModeToNativeVisualComponent(UBlueprint* Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return false;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass);
		if (BPGC == nullptr)
		{
			return false;
		}

		AActor* GeneratedCDO = Cast<AActor>(BPGC->GetDefaultObject());
		if (GeneratedCDO == nullptr)
		{
			return false;
		}

		TInlineComponentArray<TVisualComponent*> GeneratedVisuals(GeneratedCDO);
		if (GeneratedVisuals.Num() == 0 || GeneratedVisuals[0] == nullptr)
		{
			UE_LOG(LogGPAuthoredVisualSeed, Error, TEXT("Generated visual component missing on %s"),
				*Blueprint->GetName());
			return false;
		}

		GeneratedVisuals[0]->Modify();
		GeneratedVisuals[0]->SetVisualSourceMode(EGP_VisualSourceMode::AuthoredComponents);

		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		// Re-apply after compile (compile can refresh native subobject defaults).
		if (UBlueprintGeneratedClass* CompiledBPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
		{
			if (AActor* CompiledCDO = Cast<AActor>(CompiledBPGC->GetDefaultObject()))
			{
				TInlineComponentArray<TVisualComponent*> CompiledVisuals(CompiledCDO);
				if (CompiledVisuals.Num() > 0 && CompiledVisuals[0] != nullptr)
				{
					CompiledVisuals[0]->Modify();
					CompiledVisuals[0]->SetVisualSourceMode(EGP_VisualSourceMode::AuthoredComponents);
				}
			}
		}

		UE_LOG(LogGPAuthoredVisualSeed, Log, TEXT("Applied AuthoredComponents on %s"), *Blueprint->GetName());
		return true;
	}

	static bool SeedUnitExample()
	{
		UBlueprint* Blueprint = GetOrCreateBlueprint(
			UnitAssetPath,
			UnitPackagePath,
			AGP_Unit::StaticClass(),
			TEXT("BP_Unit_AuthoredExample"));
		if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
		{
			return false;
		}

		USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
		USCS_Node* VisualRoot = AddSceneNode(SCS, TEXT("VisualRoot"));

		UStaticMesh* Cylinder = GPPrimitiveVisualMesh::ResolveShapeMesh(EGP_PrimitiveShape::Cylinder);
		UStaticMesh* Cube = GPPrimitiveVisualMesh::ResolveShapeMesh(EGP_PrimitiveShape::Cube);
		if (Cylinder == nullptr || Cube == nullptr)
		{
			UE_LOG(LogGPAuthoredVisualSeed, Error, TEXT("Failed to resolve Engine BasicShapes for unit example"));
			return false;
		}

		USCS_Node* Body = AddMeshChild(
			VisualRoot,
			SCS,
			TEXT("Body"),
			Cylinder,
			FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -8.0f), FVector(0.72f, 0.72f, 1.45f)));

		AddMeshChild(
			Body,
			SCS,
			TEXT("Forward"),
			Cube,
			FTransform(FRotator::ZeroRotator, FVector(52.0f, 0.0f, 16.0f), FVector(0.58f, 0.20f, 0.16f)));

		AddMeshChild(
			Body,
			SCS,
			TEXT("Weapon"),
			Cube,
			FTransform(FRotator(0.0f, 10.0f, 0.0f), FVector(42.0f, 30.0f, 14.0f), FVector(0.90f, 0.16f, 0.16f)));

		if (!ApplyAuthoredModeToNativeVisualComponent<UGP_UnitVisualComponent>(Blueprint))
		{
			return false;
		}

		return SaveBlueprintPackage(Blueprint);
	}

	static bool SeedResourceExample()
	{
		UBlueprint* Blueprint = GetOrCreateBlueprint(
			ResourceAssetPath,
			ResourcePackagePath,
			AGP_ResourceNode::StaticClass(),
			TEXT("BP_ResourceNode_AuthoredExample"));
		if (Blueprint == nullptr || Blueprint->SimpleConstructionScript == nullptr)
		{
			return false;
		}

		USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
		USCS_Node* VisualRoot = AddSceneNode(SCS, TEXT("VisualRoot"));

		UStaticMesh* Cylinder = GPPrimitiveVisualMesh::ResolveShapeMesh(EGP_PrimitiveShape::Cylinder);
		UStaticMesh* Cone = GPPrimitiveVisualMesh::ResolveShapeMesh(EGP_PrimitiveShape::Cone);
		if (Cylinder == nullptr || Cone == nullptr)
		{
			UE_LOG(LogGPAuthoredVisualSeed, Error, TEXT("Failed to resolve Engine BasicShapes for resource example"));
			return false;
		}

		USCS_Node* Base = AddMeshChild(
			VisualRoot,
			SCS,
			TEXT("Base"),
			Cylinder,
			FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, -40.0f), FVector(0.56f, 0.56f, 0.56f)));

		AddMeshChild(
			Base,
			SCS,
			TEXT("Core"),
			Cone,
			FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 98.0f), FVector(0.52f, 0.52f, 3.85f)));

		AddMeshChild(
			Base,
			SCS,
			TEXT("AccentA"),
			Cone,
			FTransform(FRotator(28.0f, 0.0f, 0.0f), FVector(52.0f, 8.0f, 82.0f), FVector(0.36f, 0.36f, 2.75f)));

		AddMeshChild(
			Base,
			SCS,
			TEXT("AccentB"),
			Cone,
			FTransform(FRotator(0.0f, 0.0f, -30.0f), FVector(-10.0f, -54.0f, 76.0f), FVector(0.32f, 0.32f, 2.45f)));

		AddMeshChild(
			Base,
			SCS,
			TEXT("AccentC"),
			Cone,
			FTransform(FRotator(-18.0f, 12.0f, 24.0f), FVector(-46.0f, 40.0f, 68.0f), FVector(0.28f, 0.28f, 1.95f)));

		if (!ApplyAuthoredModeToNativeVisualComponent<UGP_ResourceNodeVisualComponent>(Blueprint))
		{
			return false;
		}

		if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
		{
			if (AGP_ResourceNode* CDO = Cast<AGP_ResourceNode>(BPGC->GetDefaultObject()))
			{
				const FSoftObjectPath FerroniteSoftPath(UGP_ResourceDefinition::DefaultFerroniteAssetPath());
				if (CDO->GetResourceDefinitionSoft().ToSoftObjectPath() != FerroniteSoftPath)
				{
					CDO->SetResourceDefinitionSoft(TSoftObjectPtr<UGP_ResourceDefinition>(FerroniteSoftPath));
					CDO->MarkPackageDirty();
				}
			}
		}

		return SaveBlueprintPackage(Blueprint);
	}

	static bool VerifyExamples()
	{
		bool bOk = true;

		if (UBlueprint* UnitBP = LoadObject<UBlueprint>(nullptr, UnitAssetPath))
		{
			const int32 ScsNodes = UnitBP->SimpleConstructionScript != nullptr
				? UnitBP->SimpleConstructionScript->GetAllNodes().Num()
				: 0;
			EGP_VisualSourceMode Mode = EGP_VisualSourceMode::NativeFallback;
			if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(UnitBP->GeneratedClass))
			{
				if (AGP_Unit* CDO = Cast<AGP_Unit>(BPGC->GetDefaultObject()))
				{
					if (UGP_UnitVisualComponent* Visual = CDO->GetUnitVisualComponent())
					{
						Mode = Visual->GetVisualSourceMode();
					}
				}
			}

			UE_LOG(LogGPAuthoredVisualSeed, Log,
				TEXT("Verify Unit BP=%s SCSNodes=%d VisualSourceMode=%s Parent=%s"),
				*UnitBP->GetPathName(),
				ScsNodes,
				GPPrimitiveVisualDefaults::VisualSourceModeToString(Mode),
				*GetNameSafe(UnitBP->ParentClass));

			if (Mode != EGP_VisualSourceMode::AuthoredComponents || ScsNodes < 4)
			{
				bOk = false;
			}
		}
		else
		{
			UE_LOG(LogGPAuthoredVisualSeed, Error, TEXT("Verify missing unit BP"));
			bOk = false;
		}

		if (UBlueprint* ResourceBP = LoadObject<UBlueprint>(nullptr, ResourceAssetPath))
		{
			const int32 ScsNodes = ResourceBP->SimpleConstructionScript != nullptr
				? ResourceBP->SimpleConstructionScript->GetAllNodes().Num()
				: 0;
			EGP_VisualSourceMode Mode = EGP_VisualSourceMode::NativeFallback;
			FString SoftDefinitionPath;
			bool bDefinitionOk = false;
			const FString ExpectedDefinition = UGP_ResourceDefinition::DefaultFerroniteAssetPath();
			if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ResourceBP->GeneratedClass))
			{
				if (AGP_ResourceNode* CDO = Cast<AGP_ResourceNode>(BPGC->GetDefaultObject()))
				{
					if (UGP_ResourceNodeVisualComponent* Visual = CDO->GetResourceNodeVisualComponent())
					{
						Mode = Visual->GetVisualSourceMode();
					}
					SoftDefinitionPath = CDO->GetResourceDefinitionSoft().ToSoftObjectPath().ToString();
					bDefinitionOk = SoftDefinitionPath.Equals(ExpectedDefinition, ESearchCase::IgnoreCase)
						|| SoftDefinitionPath.Contains(TEXT("DA_GP_Resource_Ferronite"));
				}
			}

			UE_LOG(LogGPAuthoredVisualSeed, Log,
				TEXT("Verify Resource BP=%s SCSNodes=%d VisualSourceMode=%s Parent=%s SoftDefinition=%s DefinitionOk=%s"),
				*ResourceBP->GetPathName(),
				ScsNodes,
				GPPrimitiveVisualDefaults::VisualSourceModeToString(Mode),
				*GetNameSafe(ResourceBP->ParentClass),
				*SoftDefinitionPath,
				bDefinitionOk ? TEXT("true") : TEXT("false"));

			if (Mode != EGP_VisualSourceMode::AuthoredComponents || ScsNodes < 4 || !bDefinitionOk)
			{
				bOk = false;
			}
		}
		else
		{
			UE_LOG(LogGPAuthoredVisualSeed, Error, TEXT("Verify missing resource BP"));
			bOk = false;
		}

		return bOk;
	}
}

UGPAuthoredVisualExampleSeedCommandlet::UGPAuthoredVisualExampleSeedCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UGPAuthoredVisualExampleSeedCommandlet::Main(const FString& Params)
{
	UE_LOG(LogGPAuthoredVisualSeed, Log, TEXT("GPAuthoredVisualExampleSeedCommandlet starting Params=%s"), *Params);

	if (FParse::Param(*Params, TEXT("VerifyOnly")))
	{
		return GPAuthoredVisualSeedPrivate::VerifyExamples() ? 0 : 2;
	}

	const bool bUnitOk = GPAuthoredVisualSeedPrivate::SeedUnitExample();
	const bool bResourceOk = GPAuthoredVisualSeedPrivate::SeedResourceExample();
	if (!bUnitOk || !bResourceOk)
	{
		UE_LOG(LogGPAuthoredVisualSeed, Error, TEXT("Seed failed Unit=%s Resource=%s"),
			bUnitOk ? TEXT("OK") : TEXT("FAIL"),
			bResourceOk ? TEXT("OK") : TEXT("FAIL"));
		return 1;
	}

	const bool bVerified = GPAuthoredVisualSeedPrivate::VerifyExamples();
	UE_LOG(LogGPAuthoredVisualSeed, Log, TEXT("Seed OK verified=%s"), bVerified ? TEXT("true") : TEXT("false"));
	return bVerified ? 0 : 2;
}
