// Copyright Epic Games, Inc. All Rights Reserved.

#include "FogOfWar/GPFoWPostProcessMaterialSeedCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/PackageName.h"
#include "Misc/FeedbackContext.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPFoWPostProcessMaterialSeed, Log, All);

namespace GPFoWPostProcessMaterialSeedPrivate
{
	static constexpr const TCHAR* PackagePath = TEXT("/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess");
	static constexpr const TCHAR* AssetPath =
		TEXT("/Game/GrimProtocol/FogOfWar/M_GP_FoW_PostProcess.M_GP_FoW_PostProcess");
	static constexpr const TCHAR* AssetName = TEXT("M_GP_FoW_PostProcess");

	static UMaterialExpression* CreateExpr(
		UMaterial* Material,
		UClass* Class,
		int32 PosX,
		int32 PosY)
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, PosX, PosY);
	}

	static bool Connect(
		UMaterialExpression* From,
		const FString& FromOutput,
		UMaterialExpression* To,
		const FString& ToInput)
	{
		return UMaterialEditingLibrary::ConnectMaterialExpressions(From, FromOutput, To, ToInput);
	}

	static void ConfigureTextureParameter(
		UMaterialExpressionTextureSampleParameter2D* Sample,
		const FName ParameterName)
	{
		Sample->ParameterName = ParameterName;
		Sample->SamplerType = SAMPLERTYPE_LinearColor;
		Sample->ConstCoordinate = 0;
	}

	static UMaterial* GetOrCreateMaterial()
	{
		if (UMaterial* Existing = LoadObject<UMaterial>(nullptr, AssetPath))
		{
			UE_LOG(LogGPFoWPostProcessMaterialSeed, Log, TEXT("Updating existing material %s"), AssetPath);
			UMaterialEditingLibrary::DeleteAllMaterialExpressions(Existing);
			return Existing;
		}

		UPackage* Package = CreatePackage(PackagePath);
		Package->FullyLoad();

		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		UMaterial* Material = Cast<UMaterial>(Factory->FactoryCreateNew(
			UMaterial::StaticClass(),
			Package,
			FName(AssetName),
			RF_Public | RF_Standalone,
			nullptr,
			GWarn));
		if (Material == nullptr)
		{
			UE_LOG(LogGPFoWPostProcessMaterialSeed, Error, TEXT("FactoryCreateNew failed for %s"), PackagePath);
		}
		return Material;
	}

	static bool BuildGraph(UMaterial* Material)
	{
		if (Material == nullptr)
		{
			return false;
		}

		Material->MaterialDomain = MD_PostProcess;
		Material->BlendableLocation = BL_SceneColorAfterTonemapping;
		Material->SetShadingModel(MSM_Unlit);

		UMaterialExpressionWorldPosition* WorldPosition = Cast<UMaterialExpressionWorldPosition>(
			CreateExpr(Material, UMaterialExpressionWorldPosition::StaticClass(), -1400, 0));
		UMaterialExpressionComponentMask* WorldXY = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -1180, 0));
		WorldXY->R = true;
		WorldXY->G = true;
		WorldXY->B = false;
		WorldXY->A = false;
		Connect(WorldPosition, TEXT(""), WorldXY, TEXT(""));

		UMaterialExpressionVectorParameter* OriginParam = Cast<UMaterialExpressionVectorParameter>(
			CreateExpr(Material, UMaterialExpressionVectorParameter::StaticClass(), -1400, 220));
		OriginParam->ParameterName = TEXT("FoWOriginXY");
		OriginParam->DefaultValue = FLinearColor(-100000.0f, -100000.0f, 0.0f, 0.0f);
		UMaterialExpressionComponentMask* OriginXY = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -1180, 220));
		OriginXY->R = true;
		OriginXY->G = true;
		OriginXY->B = false;
		OriginXY->A = false;
		Connect(OriginParam, TEXT(""), OriginXY, TEXT(""));

		UMaterialExpressionVectorParameter* InvExtentParam = Cast<UMaterialExpressionVectorParameter>(
			CreateExpr(Material, UMaterialExpressionVectorParameter::StaticClass(), -1400, 440));
		InvExtentParam->ParameterName = TEXT("FoWInvExtentXY");
		InvExtentParam->DefaultValue = FLinearColor(1.0f / 200000.0f, 1.0f / 200000.0f, 0.0f, 0.0f);
		UMaterialExpressionComponentMask* InvExtentXY = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -1180, 440));
		InvExtentXY->R = true;
		InvExtentXY->G = true;
		InvExtentXY->B = false;
		InvExtentXY->A = false;
		Connect(InvExtentParam, TEXT(""), InvExtentXY, TEXT(""));

		UMaterialExpressionSubtract* WorldMinusOrigin = Cast<UMaterialExpressionSubtract>(
			CreateExpr(Material, UMaterialExpressionSubtract::StaticClass(), -960, 40));
		Connect(WorldXY, TEXT(""), WorldMinusOrigin, TEXT("A"));
		Connect(OriginXY, TEXT(""), WorldMinusOrigin, TEXT("B"));

		UMaterialExpressionMultiply* UV = Cast<UMaterialExpressionMultiply>(
			CreateExpr(Material, UMaterialExpressionMultiply::StaticClass(), -760, 40));
		Connect(WorldMinusOrigin, TEXT(""), UV, TEXT("A"));
		Connect(InvExtentXY, TEXT(""), UV, TEXT("B"));

		UMaterialExpressionTextureSampleParameter2D* PreviousMask =
			Cast<UMaterialExpressionTextureSampleParameter2D>(
				CreateExpr(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -540, -180));
		ConfigureTextureParameter(PreviousMask, TEXT("FoWPreviousMask"));
		Connect(UV, TEXT(""), PreviousMask, TEXT("UVs"));

		UMaterialExpressionTextureSampleParameter2D* TargetMask =
			Cast<UMaterialExpressionTextureSampleParameter2D>(
				CreateExpr(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -540, 160));
		ConfigureTextureParameter(TargetMask, TEXT("FoWTargetMask"));
		Connect(UV, TEXT(""), TargetMask, TEXT("UVs"));

		UMaterialExpressionSceneTexture* SceneColor = Cast<UMaterialExpressionSceneTexture>(
			CreateExpr(Material, UMaterialExpressionSceneTexture::StaticClass(), -540, 460));
		SceneColor->SceneTextureId = PPI_PostProcessInput0;

		UMaterialExpressionScalarParameter* BlendAlpha = Cast<UMaterialExpressionScalarParameter>(
			CreateExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -540, 640));
		BlendAlpha->ParameterName = TEXT("FoWBlendAlpha");
		BlendAlpha->DefaultValue = 1.0f;

		UMaterialExpressionScalarParameter* Ready = Cast<UMaterialExpressionScalarParameter>(
			CreateExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -540, 760));
		Ready->ParameterName = TEXT("FoWReady");
		Ready->DefaultValue = 0.0f;

		UMaterialExpressionScalarParameter* ExploredDim = Cast<UMaterialExpressionScalarParameter>(
			CreateExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -540, 880));
		ExploredDim->ParameterName = TEXT("FoWExploredDim");
		ExploredDim->DefaultValue = 0.35f;

		UMaterialExpressionCustom* Compose = Cast<UMaterialExpressionCustom>(
			CreateExpr(Material, UMaterialExpressionCustom::StaticClass(), -120, 80));
		Compose->Description = TEXT("GPFoWCompose");
		Compose->OutputType = CMOT_Float3;
		Compose->Code = TEXT(
			"float2 UV = (float2(WorldX, WorldY) - float2(OriginX, OriginY)) * float2(InvExtentX, InvExtentY);\n"
			"float InBounds = (UV.x >= 0.0 && UV.x <= 1.0 && UV.y >= 0.0 && UV.y <= 1.0) ? 1.0 : 0.0;\n"
			"float4 Mask = lerp(PrevMask, TargetMask, saturate(BlendAlpha));\n"
			"float Known = saturate(Mask.r) * InBounds;\n"
			"float Visible = saturate(Mask.g) * InBounds;\n"
			"float3 Black = float3(0.0, 0.0, 0.0);\n"
			"float3 ExploredCol = SceneColor * ExploredDim;\n"
			"float3 Lit = lerp(ExploredCol, SceneColor, Visible);\n"
			"float3 Result = lerp(Black, Lit, Known);\n"
			"if (Ready < 0.5) Result = Black;\n"
			"return Result;\n");

		Compose->Inputs.Reset();
		auto AddInput = [Compose](const TCHAR* Name)
		{
			FCustomInput& Input = Compose->Inputs.AddDefaulted_GetRef();
			Input.InputName = Name;
		};
		AddInput(TEXT("SceneColor"));
		AddInput(TEXT("PrevMask"));
		AddInput(TEXT("TargetMask"));
		AddInput(TEXT("BlendAlpha"));
		AddInput(TEXT("WorldX"));
		AddInput(TEXT("WorldY"));
		AddInput(TEXT("OriginX"));
		AddInput(TEXT("OriginY"));
		AddInput(TEXT("InvExtentX"));
		AddInput(TEXT("InvExtentY"));
		AddInput(TEXT("Ready"));
		AddInput(TEXT("ExploredDim"));

		UMaterialExpressionComponentMask* WorldX = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -760, 280));
		WorldX->R = true;
		WorldX->G = false;
		WorldX->B = false;
		WorldX->A = false;
		Connect(WorldXY, TEXT(""), WorldX, TEXT(""));

		UMaterialExpressionComponentMask* WorldY = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -760, 380));
		WorldY->R = false;
		WorldY->G = true;
		WorldY->B = false;
		WorldY->A = false;
		Connect(WorldXY, TEXT(""), WorldY, TEXT(""));

		UMaterialExpressionComponentMask* OriginX = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -760, 480));
		OriginX->R = true;
		OriginX->G = false;
		OriginX->B = false;
		OriginX->A = false;
		Connect(OriginXY, TEXT(""), OriginX, TEXT(""));

		UMaterialExpressionComponentMask* OriginY = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -760, 580));
		OriginY->R = false;
		OriginY->G = true;
		OriginY->B = false;
		OriginY->A = false;
		Connect(OriginXY, TEXT(""), OriginY, TEXT(""));

		UMaterialExpressionComponentMask* InvX = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -760, 680));
		InvX->R = true;
		InvX->G = false;
		InvX->B = false;
		InvX->A = false;
		Connect(InvExtentXY, TEXT(""), InvX, TEXT(""));

		UMaterialExpressionComponentMask* InvY = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -760, 780));
		InvY->R = false;
		InvY->G = true;
		InvY->B = false;
		InvY->A = false;
		Connect(InvExtentXY, TEXT(""), InvY, TEXT(""));

		Connect(SceneColor, TEXT("Color"), Compose, TEXT("SceneColor"));
		Connect(PreviousMask, TEXT("RGBA"), Compose, TEXT("PrevMask"));
		Connect(TargetMask, TEXT("RGBA"), Compose, TEXT("TargetMask"));
		Connect(BlendAlpha, TEXT(""), Compose, TEXT("BlendAlpha"));
		Connect(WorldX, TEXT(""), Compose, TEXT("WorldX"));
		Connect(WorldY, TEXT(""), Compose, TEXT("WorldY"));
		Connect(OriginX, TEXT(""), Compose, TEXT("OriginX"));
		Connect(OriginY, TEXT(""), Compose, TEXT("OriginY"));
		Connect(InvX, TEXT(""), Compose, TEXT("InvExtentX"));
		Connect(InvY, TEXT(""), Compose, TEXT("InvExtentY"));
		Connect(Ready, TEXT(""), Compose, TEXT("Ready"));
		Connect(ExploredDim, TEXT(""), Compose, TEXT("ExploredDim"));

		const bool bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(
			Compose,
			TEXT(""),
			MP_EmissiveColor);
		UMaterialEditingLibrary::LayoutMaterialExpressions(Material);
		UMaterialEditingLibrary::RecompileMaterial(Material);
		return bConnected;
	}

	static bool SaveMaterial(UMaterial* Material)
	{
		if (Material == nullptr)
		{
			return false;
		}

		UPackage* Package = Material->GetOutermost();
		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(Material);

		const FString PackageFilename = FPackageName::LongPackageNameToFilename(
			Package->GetName(),
			FPackageName::GetAssetPackageExtension());

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GError;
		SaveArgs.SaveFlags = SAVE_None;
		const bool bSaved = UPackage::SavePackage(Package, Material, *PackageFilename, SaveArgs);
		UE_LOG(LogGPFoWPostProcessMaterialSeed, Log,
			TEXT("SavePackage %s Result=%s"),
			*Material->GetPathName(),
			bSaved ? TEXT("OK") : TEXT("FAIL"));
		return bSaved;
	}
}

UGPFoWPostProcessMaterialSeedCommandlet::UGPFoWPostProcessMaterialSeedCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UGPFoWPostProcessMaterialSeedCommandlet::Main(const FString& Params)
{
	UE_LOG(LogGPFoWPostProcessMaterialSeed, Log,
		TEXT("GPFoWPostProcessMaterialSeedCommandlet Params=%s"),
		*Params);

	UMaterial* Material = GPFoWPostProcessMaterialSeedPrivate::GetOrCreateMaterial();
	if (Material == nullptr
		|| !GPFoWPostProcessMaterialSeedPrivate::BuildGraph(Material)
		|| !GPFoWPostProcessMaterialSeedPrivate::SaveMaterial(Material))
	{
		return 2;
	}

	UE_LOG(LogGPFoWPostProcessMaterialSeed, Display,
		TEXT("Seeded FoW post-process material %s"),
		GPFoWPostProcessMaterialSeedPrivate::AssetPath);
	return 0;
}
