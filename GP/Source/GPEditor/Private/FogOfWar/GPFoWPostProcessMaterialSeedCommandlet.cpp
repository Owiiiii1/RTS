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
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
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

	static void ConfigureTextureObject(
		UMaterialExpressionTextureObjectParameter* TextureObject,
		const FName ParameterName)
	{
		TextureObject->ParameterName = ParameterName;
		TextureObject->SamplerType = SAMPLERTYPE_LinearColor;
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

		UMaterialExpressionSceneTexture* SceneDepth = Cast<UMaterialExpressionSceneTexture>(
			CreateExpr(Material, UMaterialExpressionSceneTexture::StaticClass(), -1600, -40));
		SceneDepth->SceneTextureId = PPI_SceneDepth;

		UMaterialExpressionCustom* ReconstructWorld = Cast<UMaterialExpressionCustom>(
			CreateExpr(Material, UMaterialExpressionCustom::StaticClass(), -1320, 0));
		ReconstructWorld->Description = TEXT("GPFoWReconstructSceneWorld");
		ReconstructWorld->OutputType = CMOT_Float3;
		ReconstructWorld->Code = TEXT(
			"float DeviceZ = ConvertToDeviceZ(max(SceneDepth, 0.0));\n"
			"float3 TranslatedWorld = SvPositionToTranslatedWorld(float4(Parameters.SvPosition.xy, DeviceZ, 1.0));\n"
			"float3 WorldPosition = TranslatedWorld - ResolvedView.PreViewTranslation;\n"
			"return WorldPosition;\n");
		ReconstructWorld->Inputs.Reset();
		{
			FCustomInput& Input = ReconstructWorld->Inputs.AddDefaulted_GetRef();
			Input.InputName = TEXT("SceneDepth");
		}
		UMaterialExpressionComponentMask* SceneDepthR = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -1460, -40));
		SceneDepthR->R = true;
		SceneDepthR->G = false;
		SceneDepthR->B = false;
		SceneDepthR->A = false;
		Connect(SceneDepth, TEXT("Color"), SceneDepthR, TEXT(""));
		Connect(SceneDepthR, TEXT(""), ReconstructWorld, TEXT("SceneDepth"));

		UMaterialExpressionComponentMask* WorldXY = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -1040, 0));
		WorldXY->R = true;
		WorldXY->G = true;
		WorldXY->B = false;
		WorldXY->A = false;
		Connect(ReconstructWorld, TEXT(""), WorldXY, TEXT(""));

		UMaterialExpressionVectorParameter* OriginParam = Cast<UMaterialExpressionVectorParameter>(
			CreateExpr(Material, UMaterialExpressionVectorParameter::StaticClass(), -1600, 220));
		OriginParam->ParameterName = TEXT("FoWOriginXY");
		OriginParam->DefaultValue = FLinearColor(-100000.0f, -100000.0f, 0.0f, 0.0f);
		UMaterialExpressionComponentMask* OriginXY = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -1320, 220));
		OriginXY->R = true;
		OriginXY->G = true;
		OriginXY->B = false;
		OriginXY->A = false;
		Connect(OriginParam, TEXT(""), OriginXY, TEXT(""));

		UMaterialExpressionVectorParameter* InvExtentParam = Cast<UMaterialExpressionVectorParameter>(
			CreateExpr(Material, UMaterialExpressionVectorParameter::StaticClass(), -1600, 440));
		InvExtentParam->ParameterName = TEXT("FoWInvExtentXY");
		InvExtentParam->DefaultValue = FLinearColor(1.0f / 200000.0f, 1.0f / 200000.0f, 0.0f, 0.0f);
		UMaterialExpressionComponentMask* InvExtentXY = Cast<UMaterialExpressionComponentMask>(
			CreateExpr(Material, UMaterialExpressionComponentMask::StaticClass(), -1320, 440));
		InvExtentXY->R = true;
		InvExtentXY->G = true;
		InvExtentXY->B = false;
		InvExtentXY->A = false;
		Connect(InvExtentParam, TEXT(""), InvExtentXY, TEXT(""));

		UMaterialExpressionSubtract* WorldMinusOrigin = Cast<UMaterialExpressionSubtract>(
			CreateExpr(Material, UMaterialExpressionSubtract::StaticClass(), -860, 40));
		Connect(WorldXY, TEXT(""), WorldMinusOrigin, TEXT("A"));
		Connect(OriginXY, TEXT(""), WorldMinusOrigin, TEXT("B"));

		UMaterialExpressionMultiply* MaskUV = Cast<UMaterialExpressionMultiply>(
			CreateExpr(Material, UMaterialExpressionMultiply::StaticClass(), -680, 40));
		Connect(WorldMinusOrigin, TEXT(""), MaskUV, TEXT("A"));
		Connect(InvExtentXY, TEXT(""), MaskUV, TEXT("B"));

		UMaterialExpressionTextureObjectParameter* PreviousMask =
			Cast<UMaterialExpressionTextureObjectParameter>(
				CreateExpr(Material, UMaterialExpressionTextureObjectParameter::StaticClass(), -680, -220));
		ConfigureTextureObject(PreviousMask, TEXT("FoWPreviousMask"));

		UMaterialExpressionTextureObjectParameter* TargetMask =
			Cast<UMaterialExpressionTextureObjectParameter>(
				CreateExpr(Material, UMaterialExpressionTextureObjectParameter::StaticClass(), -680, 160));
		ConfigureTextureObject(TargetMask, TEXT("FoWTargetMask"));

		UMaterialExpressionSceneTexture* SceneColor = Cast<UMaterialExpressionSceneTexture>(
			CreateExpr(Material, UMaterialExpressionSceneTexture::StaticClass(), -680, 460));
		SceneColor->SceneTextureId = PPI_PostProcessInput0;

		UMaterialExpressionScalarParameter* BlendAlpha = Cast<UMaterialExpressionScalarParameter>(
			CreateExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -680, 640));
		BlendAlpha->ParameterName = TEXT("FoWBlendAlpha");
		BlendAlpha->DefaultValue = 1.0f;

		UMaterialExpressionScalarParameter* Ready = Cast<UMaterialExpressionScalarParameter>(
			CreateExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -680, 760));
		Ready->ParameterName = TEXT("FoWReady");
		Ready->DefaultValue = 0.0f;

		UMaterialExpressionScalarParameter* ExploredDim = Cast<UMaterialExpressionScalarParameter>(
			CreateExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -680, 880));
		ExploredDim->ParameterName = TEXT("FoWExploredDim");
		ExploredDim->DefaultValue = 0.35f;

		UMaterialExpressionScalarParameter* TexelSize = Cast<UMaterialExpressionScalarParameter>(
			CreateExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -680, 1000));
		TexelSize->ParameterName = TEXT("FoWMaskTexelSize");
		TexelSize->DefaultValue = 0.001f;

		UMaterialExpressionScalarParameter* BlurRadius = Cast<UMaterialExpressionScalarParameter>(
			CreateExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -680, 1120));
		BlurRadius->ParameterName = TEXT("FoWBlurRadiusTexels");
		BlurRadius->DefaultValue = 1.0f;

		UMaterialExpressionScalarParameter* DebugMode = Cast<UMaterialExpressionScalarParameter>(
			CreateExpr(Material, UMaterialExpressionScalarParameter::StaticClass(), -680, 1240));
		DebugMode->ParameterName = TEXT("FoWDebugMode");
		DebugMode->DefaultValue = 0.0f;

		UMaterialExpressionCustom* Compose = Cast<UMaterialExpressionCustom>(
			CreateExpr(Material, UMaterialExpressionCustom::StaticClass(), -120, 80));
		Compose->Description = TEXT("GPFoWComposeGPU");
		Compose->OutputType = CMOT_Float3;
		Compose->Code = TEXT(
			"if (DebugMode > 0.5) return float3(1.0, 0.0, 1.0);\n"
			"float2 UV = MaskUV;\n"
			"float InBounds = (UV.x >= 0.0 && UV.x <= 1.0 && UV.y >= 0.0 && UV.y <= 1.0) ? 1.0 : 0.0;\n"
			"float2 Offset = float2(max(TexelSize, 1e-6), max(TexelSize, 1e-6)) * max(BlurRadiusTexels, 0.0);\n"
			"float4 PrevMask = Texture2DSample(PrevTex, PrevTexSampler, UV);\n"
			"PrevMask += Texture2DSample(PrevTex, PrevTexSampler, UV + float2(Offset.x, 0.0));\n"
			"PrevMask += Texture2DSample(PrevTex, PrevTexSampler, UV - float2(Offset.x, 0.0));\n"
			"PrevMask += Texture2DSample(PrevTex, PrevTexSampler, UV + float2(0.0, Offset.y));\n"
			"PrevMask += Texture2DSample(PrevTex, PrevTexSampler, UV - float2(0.0, Offset.y));\n"
			"PrevMask += Texture2DSample(PrevTex, PrevTexSampler, UV + float2(Offset.x, Offset.y));\n"
			"PrevMask += Texture2DSample(PrevTex, PrevTexSampler, UV + float2(-Offset.x, Offset.y));\n"
			"PrevMask += Texture2DSample(PrevTex, PrevTexSampler, UV + float2(Offset.x, -Offset.y));\n"
			"PrevMask += Texture2DSample(PrevTex, PrevTexSampler, UV + float2(-Offset.x, -Offset.y));\n"
			"PrevMask /= 9.0;\n"
			"float4 TargetMask = Texture2DSample(TargetTex, TargetTexSampler, UV);\n"
			"TargetMask += Texture2DSample(TargetTex, TargetTexSampler, UV + float2(Offset.x, 0.0));\n"
			"TargetMask += Texture2DSample(TargetTex, TargetTexSampler, UV - float2(Offset.x, 0.0));\n"
			"TargetMask += Texture2DSample(TargetTex, TargetTexSampler, UV + float2(0.0, Offset.y));\n"
			"TargetMask += Texture2DSample(TargetTex, TargetTexSampler, UV - float2(0.0, Offset.y));\n"
			"TargetMask += Texture2DSample(TargetTex, TargetTexSampler, UV + float2(Offset.x, Offset.y));\n"
			"TargetMask += Texture2DSample(TargetTex, TargetTexSampler, UV + float2(-Offset.x, Offset.y));\n"
			"TargetMask += Texture2DSample(TargetTex, TargetTexSampler, UV + float2(Offset.x, -Offset.y));\n"
			"TargetMask += Texture2DSample(TargetTex, TargetTexSampler, UV + float2(-Offset.x, -Offset.y));\n"
			"TargetMask /= 9.0;\n"
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
		AddInput(TEXT("PrevTex"));
		AddInput(TEXT("TargetTex"));
		AddInput(TEXT("MaskUV"));
		AddInput(TEXT("BlendAlpha"));
		AddInput(TEXT("Ready"));
		AddInput(TEXT("ExploredDim"));
		AddInput(TEXT("TexelSize"));
		AddInput(TEXT("BlurRadiusTexels"));
		AddInput(TEXT("DebugMode"));

		Connect(SceneColor, TEXT("Color"), Compose, TEXT("SceneColor"));
		Connect(PreviousMask, TEXT(""), Compose, TEXT("PrevTex"));
		Connect(TargetMask, TEXT(""), Compose, TEXT("TargetTex"));
		Connect(MaskUV, TEXT(""), Compose, TEXT("MaskUV"));
		Connect(BlendAlpha, TEXT(""), Compose, TEXT("BlendAlpha"));
		Connect(Ready, TEXT(""), Compose, TEXT("Ready"));
		Connect(ExploredDim, TEXT(""), Compose, TEXT("ExploredDim"));
		Connect(TexelSize, TEXT(""), Compose, TEXT("TexelSize"));
		Connect(BlurRadius, TEXT(""), Compose, TEXT("BlurRadiusTexels"));
		Connect(DebugMode, TEXT(""), Compose, TEXT("DebugMode"));

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
