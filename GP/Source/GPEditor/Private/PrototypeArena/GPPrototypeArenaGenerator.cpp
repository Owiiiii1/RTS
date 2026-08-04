// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrototypeArena/GPPrototypeArenaGenerator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Builders/CubeBuilder.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Game/GPGameMode.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Editor.h"
#include "Engine/Brush.h"
#include "Components/SkyAtmosphereComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPPrototypeArenaGenerator, Log, All);

const TCHAR* FGPPrototypeArenaGenerator::MapPackagePath = TEXT("/Game/GrimProtocol/Maps/L_PrototypeArena");
const TCHAR* FGPPrototypeArenaGenerator::GeneratedActorTag = TEXT("GP.GeneratedPrototypeArena");
const TCHAR* FGPPrototypeArenaGenerator::ManifestRelativePath = TEXT("Docs/Development/Generated/GP_PrototypeArena_Layout.md");

namespace GPPrototypeArenaPrivate
{
	static const FName TagName(TEXT("GP.GeneratedPrototypeArena"));

	static const TCHAR* ExpectedLabels[] = {
		TEXT("GP_Arena_Floor"),
		TEXT("GP_Arena_Wall_North"),
		TEXT("GP_Arena_Wall_South"),
		TEXT("GP_Arena_Wall_East"),
		TEXT("GP_Arena_Wall_West"),
		TEXT("GP_Arena_DirectionalLight"),
		TEXT("GP_Arena_SkyLight"),
		TEXT("GP_Arena_SkyAtmosphere"),
		TEXT("GP_Arena_PlayerStart"),
		TEXT("GP_Arena_NavMeshBounds")
	};

	static bool IsExpectedLabel(const FString& Label)
	{
		for (const TCHAR* Expected : ExpectedLabels)
		{
			if (Label.Equals(Expected, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	static FString ClassDisplayName(const AActor* Actor)
	{
		return Actor != nullptr && Actor->GetClass() != nullptr
			? Actor->GetClass()->GetName()
			: TEXT("None");
	}
}

void FGPPrototypeArenaGenerator::LogStage(const TCHAR* Stage, const FString& Detail)
{
	UE_LOG(LogGPPrototypeArenaGenerator, Log, TEXT("GP PrototypeArena[%s]: %s"), Stage, *Detail);
}

bool FGPPrototypeArenaGenerator::IsEditorGenerationAllowed(FString& OutReason)
{
	if (GEditor == nullptr)
	{
		OutReason = TEXT("GEditor is null (not an Editor process)");
		return false;
	}

	if (GEditor->PlayWorld != nullptr)
	{
		OutReason = TEXT("PIE is active; refuse generation");
		return false;
	}

	if (GEditor->bIsSimulatingInEditor)
	{
		OutReason = TEXT("SIE is active; refuse generation");
		return false;
	}

	return true;
}

bool FGPPrototypeArenaGenerator::DoesMapPackageExist()
{
	const FString PackageName = MapPackagePath;
	if (FPackageName::DoesPackageExist(PackageName))
	{
		return true;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(
		PackageName,
		FPackageName::GetMapPackageExtension());
	return IFileManager::Get().FileExists(*Filename);
}

void FGPPrototypeArenaGenerator::ApplyGeneratedMetadata(AActor* Actor, const FName& Label)
{
	if (Actor == nullptr)
	{
		return;
	}

	Actor->SetActorLabel(Label.ToString(), true);
	Actor->Tags.AddUnique(GPPrototypeArenaPrivate::TagName);
	Actor->SetFolderPath(FName(TEXT("GP/PrototypeArena")));
}

AActor* FGPPrototypeArenaGenerator::SpawnStaticCube(
	UWorld* World,
	const FName& Label,
	const FVector& Location,
	const FRotator& Rotation,
	const FVector& Scale,
	TArray<FSpawnedActorRecord>& OutRecords)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
	if (MeshActor == nullptr)
	{
		return nullptr;
	}

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* MeshComp = MeshActor->GetStaticMeshComponent();
	if (Cube == nullptr || MeshComp == nullptr)
	{
		MeshActor->Destroy();
		return nullptr;
	}

	MeshActor->SetMobility(EComponentMobility::Static);
	MeshComp->SetMobility(EComponentMobility::Static);
	MeshComp->SetStaticMesh(Cube);
	MeshComp->SetCollisionProfileName(TEXT("BlockAll"));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetGenerateOverlapEvents(false);
	MeshComp->SetCanEverAffectNavigation(true);
	MeshActor->SetActorScale3D(Scale);

	ApplyGeneratedMetadata(MeshActor, Label);

	FSpawnedActorRecord Record;
	Record.Label = Label.ToString();
	Record.ClassName = TEXT("StaticMeshActor");
	Record.Location = Location;
	Record.Rotation = Rotation;
	Record.Scale = Scale;
	OutRecords.Add(Record);

	return MeshActor;
}

bool FGPPrototypeArenaGenerator::ConfigureWorldSettings(UWorld* World, FString& OutError)
{
	if (World == nullptr)
	{
		OutError = TEXT("World is null");
		return false;
	}

	AWorldSettings* Settings = World->GetWorldSettings();
	if (Settings == nullptr)
	{
		OutError = TEXT("WorldSettings missing");
		return false;
	}

	if (Settings->GetWorldPartition() != nullptr)
	{
		OutError = TEXT("Blank map unexpectedly enabled World Partition");
		return false;
	}

	Settings->DefaultGameMode = AGP_GameMode::StaticClass();
	Settings->SetFlags(RF_Transactional);
	World->MarkPackageDirty();
	return true;
}

bool FGPPrototypeArenaGenerator::SpawnInfrastructure(
	UWorld* World,
	TArray<FSpawnedActorRecord>& OutRecords,
	FString& OutError)
{
	if (World == nullptr)
	{
		OutError = TEXT("World is null");
		return false;
	}

	// Engine Cube = 100uu. Floor 40x40x1 → 4000×4000×100; Z=-50 → top near Z=0.
	if (SpawnStaticCube(
			World,
			FName(TEXT("GP_Arena_Floor")),
			FVector(0.0f, 0.0f, -50.0f),
			FRotator::ZeroRotator,
			FVector(40.0f, 40.0f, 1.0f),
			OutRecords) == nullptr)
	{
		OutError = TEXT("Failed to spawn floor");
		return false;
	}

	const float WallHeightScale = 3.0f; // 300uu
	const float WallThicknessScale = 1.0f; // 100uu
	const float WallLengthScale = 42.0f; // 4200uu covers 4000 floor + margin
	const float WallEdge = 2050.0f;
	const float WallZ = 150.0f;

	if (SpawnStaticCube(
			World,
			FName(TEXT("GP_Arena_Wall_North")),
			FVector(0.0f, WallEdge, WallZ),
			FRotator::ZeroRotator,
			FVector(WallLengthScale, WallThicknessScale, WallHeightScale),
			OutRecords) == nullptr
		|| SpawnStaticCube(
			World,
			FName(TEXT("GP_Arena_Wall_South")),
			FVector(0.0f, -WallEdge, WallZ),
			FRotator::ZeroRotator,
			FVector(WallLengthScale, WallThicknessScale, WallHeightScale),
			OutRecords) == nullptr
		|| SpawnStaticCube(
			World,
			FName(TEXT("GP_Arena_Wall_East")),
			FVector(WallEdge, 0.0f, WallZ),
			FRotator::ZeroRotator,
			FVector(WallThicknessScale, WallLengthScale, WallHeightScale),
			OutRecords) == nullptr
		|| SpawnStaticCube(
			World,
			FName(TEXT("GP_Arena_Wall_West")),
			FVector(-WallEdge, 0.0f, WallZ),
			FRotator::ZeroRotator,
			FVector(WallThicknessScale, WallLengthScale, WallHeightScale),
			OutRecords) == nullptr)
	{
		OutError = TEXT("Failed to spawn walls");
		return false;
	}

	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADirectionalLight* DirLight = World->SpawnActor<ADirectionalLight>(
			FVector(0.0f, 0.0f, 800.0f),
			FRotator(-40.0f, 35.0f, 0.0f),
			Params);
		if (DirLight == nullptr)
		{
			OutError = TEXT("Failed to spawn DirectionalLight");
			return false;
		}

		if (UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(DirLight->GetLightComponent()))
		{
			LightComp->SetMobility(EComponentMobility::Stationary);
			LightComp->SetIntensity(5.0f);
			LightComp->SetAtmosphereSunLight(true);
		}
		ApplyGeneratedMetadata(DirLight, FName(TEXT("GP_Arena_DirectionalLight")));

		FSpawnedActorRecord Record;
		Record.Label = TEXT("GP_Arena_DirectionalLight");
		Record.ClassName = TEXT("DirectionalLight");
		Record.Location = FVector(0.0f, 0.0f, 800.0f);
		Record.Rotation = FRotator(-40.0f, 35.0f, 0.0f);
		Record.Scale = FVector::OneVector;
		OutRecords.Add(Record);
	}

	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (SkyLight == nullptr)
		{
			OutError = TEXT("Failed to spawn SkyLight");
			return false;
		}

		if (USkyLightComponent* SkyComp = SkyLight->GetLightComponent())
		{
			SkyComp->SetMobility(EComponentMobility::Stationary);
			SkyComp->bRealTimeCapture = false;
			SkyComp->SourceType = ESkyLightSourceType::SLS_CapturedScene;
			SkyComp->SetIntensity(1.0f);
			SkyComp->bLowerHemisphereIsBlack = false;
			SkyComp->RecaptureSky();
		}
		ApplyGeneratedMetadata(SkyLight, FName(TEXT("GP_Arena_SkyLight")));

		FSpawnedActorRecord Record;
		Record.Label = TEXT("GP_Arena_SkyLight");
		Record.ClassName = TEXT("SkyLight");
		Record.Location = FVector::ZeroVector;
		Record.Rotation = FRotator::ZeroRotator;
		Record.Scale = FVector::OneVector;
		OutRecords.Add(Record);
	}

	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkyAtmosphere* Atmosphere = World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (Atmosphere != nullptr)
		{
			ApplyGeneratedMetadata(Atmosphere, FName(TEXT("GP_Arena_SkyAtmosphere")));
			FSpawnedActorRecord Record;
			Record.Label = TEXT("GP_Arena_SkyAtmosphere");
			Record.ClassName = TEXT("SkyAtmosphere");
			Record.Location = FVector::ZeroVector;
			Record.Rotation = FRotator::ZeroRotator;
			Record.Scale = FVector::OneVector;
			OutRecords.Add(Record);
		}
	}

	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Face +Y into arena center from south staging edge.
		APlayerStart* PlayerStart = World->SpawnActor<APlayerStart>(
			FVector(0.0f, -1500.0f, 100.0f),
			FRotator(0.0f, 90.0f, 0.0f),
			Params);
		if (PlayerStart == nullptr)
		{
			OutError = TEXT("Failed to spawn PlayerStart");
			return false;
		}
		ApplyGeneratedMetadata(PlayerStart, FName(TEXT("GP_Arena_PlayerStart")));

		FSpawnedActorRecord Record;
		Record.Label = TEXT("GP_Arena_PlayerStart");
		Record.ClassName = TEXT("PlayerStart");
		Record.Location = FVector(0.0f, -1500.0f, 100.0f);
		Record.Rotation = FRotator(0.0f, 90.0f, 0.0f);
		Record.Scale = FVector::OneVector;
		OutRecords.Add(Record);
	}

	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ANavMeshBoundsVolume* NavBounds = World->SpawnActor<ANavMeshBoundsVolume>(
			FVector(0.0f, 0.0f, 100.0f),
			FRotator::ZeroRotator,
			Params);
		if (NavBounds == nullptr)
		{
			OutError = TEXT("Failed to spawn NavMeshBoundsVolume");
			return false;
		}

		UCubeBuilder* CubeBuilder = NewObject<UCubeBuilder>();
		CubeBuilder->X = 4500.0f;
		CubeBuilder->Y = 4500.0f;
		CubeBuilder->Z = 500.0f;
		NavBounds->BrushBuilder = CubeBuilder;
		CubeBuilder->Build(World, NavBounds);
		NavBounds->SetActorLabel(TEXT("GP_Arena_NavMeshBounds"), true);
		NavBounds->Tags.AddUnique(GPPrototypeArenaPrivate::TagName);
		NavBounds->SetFolderPath(FName(TEXT("GP/PrototypeArena")));

		FSpawnedActorRecord Record;
		Record.Label = TEXT("GP_Arena_NavMeshBounds");
		Record.ClassName = TEXT("NavMeshBoundsVolume");
		Record.Location = FVector(0.0f, 0.0f, 100.0f);
		Record.Rotation = FRotator::ZeroRotator;
		Record.Scale = FVector::OneVector;
		OutRecords.Add(Record);
	}

	return true;
}

bool FGPPrototypeArenaGenerator::TryBuildNavigation(UWorld* World, FGPPrototypeArenaGenerateResult& InOutResult)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (NavSys == nullptr)
	{
		InOutResult.bOperatorMustBuildPaths = true;
		InOutResult.Message += TEXT(" NavigationSystem missing; operator must press Build Paths.");
		UE_LOG(LogGPPrototypeArenaGenerator, Warning,
			TEXT("GP PrototypeArena[Navigation]: NavigationSystem missing — operator must press Build Paths"));
		return false;
	}

	NavSys->ReleaseInitialBuildingLock();
	InOutResult.bNavigationBuildAttempted = true;
	NavSys->Build();

	// Async/editor nav build completion is not always synchronously observable.
	InOutResult.bNavigationBuildSucceeded = false;
	InOutResult.bOperatorMustBuildPaths = true;
	InOutResult.Message += TEXT(" Nav bounds created; verify nav with P / Build Paths if green mesh missing.");
	UE_LOG(LogGPPrototypeArenaGenerator, Warning,
		TEXT("GP PrototypeArena[Navigation]: bounds created; operator must confirm Build Paths if nav data not yet green"));
	return false;
}

FString FGPPrototypeArenaGenerator::ResolveManifestAbsolutePath()
{
	// ProjectDir = .../GP/ ; Docs lives at repo root sibling.
	const FString RepoRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("..")));
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(RepoRoot, ManifestRelativePath));
}

bool FGPPrototypeArenaGenerator::WriteManifest(
	const TArray<FSpawnedActorRecord>& Records,
	const FGPPrototypeArenaGenerateResult& Result,
	FString& OutError)
{
	const FString AbsolutePath = ResolveManifestAbsolutePath();
	const FString Directory = FPaths::GetPath(AbsolutePath);
	IFileManager::Get().MakeDirectory(*Directory, true);

	FString Body;
	Body += TEXT("# GP Prototype Arena Layout Manifest\n\n");
	Body += FString::Printf(TEXT("- GeneratorVersion: %d\n"), PrototypeArenaGeneratorVersion);
	Body += FString::Printf(TEXT("- MapPath: `%s`\n"), MapPackagePath);
	Body += TEXT("- MapType: non-World-Partition compact umap\n");
	Body += TEXT("- FloorDimensions: 4000 x 4000 uu (Engine Cube scale 40x40x1)\n");
	Body += TEXT("- NavPolicy: NavMeshBoundsVolume spawned; editor Build Paths may be required\n");
	Body += TEXT("- GameplayPopulation: none (GP-S27A2 infrastructure only)\n");
	Body += FString::Printf(TEXT("- ExistingMapAbort: %s\n"), Result.bExistingMapAbort ? TEXT("true") : TEXT("false"));
	Body += FString::Printf(TEXT("- GenerationTimestampUTC: %s\n"), *FDateTime::UtcNow().ToIso8601());
	Body += TEXT("- SourceCommit: (filled by documentation commit)\n\n");
	Body += TEXT("| Label | Class | Location | Rotation | Scale | Tag |\n");
	Body += TEXT("| --- | --- | --- | --- | --- | --- |\n");

	for (const FSpawnedActorRecord& Record : Records)
	{
		Body += FString::Printf(
			TEXT("| %s | %s | (%.1f, %.1f, %.1f) | (P%.1f Y%.1f R%.1f) | (%.2f, %.2f, %.2f) | `%s` |\n"),
			*Record.Label,
			*Record.ClassName,
			Record.Location.X, Record.Location.Y, Record.Location.Z,
			Record.Rotation.Pitch, Record.Rotation.Yaw, Record.Rotation.Roll,
			Record.Scale.X, Record.Scale.Y, Record.Scale.Z,
			GeneratedActorTag);
	}

	Body += TEXT("\n## Notes\n");
	Body += TEXT("- No AGP_Unit / AGP_ResourceNode / combat pairs in A2.\n");
	Body += TEXT("- Global GameDefaultMap / DefaultGameMode intentionally unchanged.\n");
	Body += TEXT("- World GameMode override: AGP_GameMode.\n");

	if (!FFileHelper::SaveStringToFile(Body, *AbsolutePath))
	{
		OutError = FString::Printf(TEXT("Failed to write manifest: %s"), *AbsolutePath);
		return false;
	}

	LogStage(TEXT("Complete"), FString::Printf(TEXT("Manifest written: %s"), *AbsolutePath));
	return true;
}

FGPPrototypeArenaGenerateResult FGPPrototypeArenaGenerator::Generate()
{
	FGPPrototypeArenaGenerateResult Result;
	Result.MapPackagePath = MapPackagePath;

	LogStage(TEXT("Validate"), TEXT("Begin"));

	FString AllowReason;
	if (!IsEditorGenerationAllowed(AllowReason))
	{
		Result.FailureStage = TEXT("Validate");
		Result.Message = AllowReason;
		UE_LOG(LogGPPrototypeArenaGenerator, Warning, TEXT("GP PrototypeArena generate refused: %s"), *AllowReason);
		return Result;
	}

	if (DoesMapPackageExist())
	{
		Result.bExistingMapAbort = true;
		Result.FailureStage = TEXT("Validate");
		Result.Message = FString::Printf(
			TEXT("Map already exists at %s — abort (ExistingMapAbort=true). No overwrite."),
			MapPackagePath);
		UE_LOG(LogGPPrototypeArenaGenerator, Warning, TEXT("%s"), *Result.Message);
		return Result;
	}

	LogStage(TEXT("CreateMap"), TEXT("NewBlankMap"));
	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(/*bSaveExistingMap*/ false);
	if (World == nullptr)
	{
		Result.FailureStage = TEXT("CreateMap");
		Result.Message = TEXT("NewBlankMap returned null");
		return Result;
	}

	FString Error;
	LogStage(TEXT("ConfigureWorld"), TEXT("GameMode + non-WP check"));
	if (!ConfigureWorldSettings(World, Error))
	{
		Result.FailureStage = TEXT("ConfigureWorld");
		Result.Message = Error;
		UE_LOG(LogGPPrototypeArenaGenerator, Error, TEXT("GP PrototypeArena incomplete (not saved): %s"), *Error);
		return Result;
	}

	TArray<FSpawnedActorRecord> Records;
	LogStage(TEXT("SpawnInfrastructure"), TEXT("Floor/Walls/Lights/PlayerStart/NavBounds"));
	if (!SpawnInfrastructure(World, Records, Error))
	{
		Result.FailureStage = TEXT("SpawnInfrastructure");
		Result.Message = Error;
		UE_LOG(LogGPPrototypeArenaGenerator, Error, TEXT("GP PrototypeArena incomplete (not saved): %s"), *Error);
		return Result;
	}
	Result.SpawnedActorCount = Records.Num();

	LogStage(TEXT("Navigation"), TEXT("Update/build attempt"));
	TryBuildNavigation(World, Result);

	LogStage(TEXT("Save"), MapPackagePath);
	const bool bSaved = UEditorLoadingAndSavingUtils::SaveMap(World, MapPackagePath);
	if (!bSaved)
	{
		Result.FailureStage = TEXT("Save");
		Result.Message = TEXT("SaveMap failed; package may be incomplete — do not treat as success");
		UE_LOG(LogGPPrototypeArenaGenerator, Error, TEXT("%s"), *Result.Message);
		return Result;
	}

	Result.MapFilename = FPackageName::LongPackageNameToFilename(
		MapPackagePath,
		FPackageName::GetMapPackageExtension());

	if (!IFileManager::Get().FileExists(*Result.MapFilename))
	{
		Result.FailureStage = TEXT("Save");
		Result.Message = FString::Printf(TEXT("SaveMap reported success but file missing: %s"), *Result.MapFilename);
		UE_LOG(LogGPPrototypeArenaGenerator, Error, TEXT("%s"), *Result.Message);
		return Result;
	}

	FAssetRegistryModule::AssetCreated(World);

	LogStage(TEXT("Open"), TEXT("LoadMap after save"));
	UEditorLoadingAndSavingUtils::LoadMap(MapPackagePath);

	if (!WriteManifest(Records, Result, Error))
	{
		// Map saved successfully; manifest failure is non-fatal for umap but report it.
		Result.Message += TEXT(" ");
		Result.Message += Error;
		UE_LOG(LogGPPrototypeArenaGenerator, Warning, TEXT("%s"), *Error);
	}

	Result.bSuccess = true;
	Result.Message = FString::Printf(
		TEXT("Generated %s actors=%d ExistingMapAbort=false file=%s"),
		MapPackagePath,
		Result.SpawnedActorCount,
		*Result.MapFilename);
	LogStage(TEXT("Complete"), Result.Message);
	return Result;
}

FGPPrototypeArenaInspectResult FGPPrototypeArenaGenerator::Inspect(UWorld* OptionalWorld)
{
	FGPPrototypeArenaInspectResult Result;
	Result.MapPath = MapPackagePath;
	Result.bExists = DoesMapPackageExist();
	Result.NavigationBuildStatus = TEXT("Unknown");

	UWorld* World = OptionalWorld;
	if (World == nullptr && GEditor != nullptr)
	{
		World = GEditor->GetEditorWorldContext().World();
	}

	const FString CurrentPackage = World != nullptr && World->GetOutermost() != nullptr
		? World->GetOutermost()->GetName()
		: FString();
	Result.bLoaded = Result.bExists && CurrentPackage.Equals(MapPackagePath, ESearchCase::IgnoreCase);

	if (World == nullptr || !Result.bLoaded)
	{
		Result.bReadyForPopulation = false;
		if (Result.bExists && !Result.bLoaded)
		{
			Result.NavigationBuildStatus = TEXT("MapExistsButNotLoaded");
		}
		else if (!Result.bExists)
		{
			Result.NavigationBuildStatus = TEXT("MapMissing");
		}
		return Result;
	}

	AWorldSettings* Settings = World->GetWorldSettings();
	Result.bWorldPartition = Settings != nullptr && Settings->GetWorldPartition() != nullptr;
	if (Settings != nullptr && Settings->DefaultGameMode != nullptr)
	{
		Result.GameModeOverride = Settings->DefaultGameMode->GetName();
	}
	else
	{
		Result.GameModeOverride = TEXT("None");
	}

	TMap<FString, int32> LabelCounts;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || !Actor->Tags.Contains(GPPrototypeArenaPrivate::TagName))
		{
			continue;
		}

		++Result.GeneratorTagActors;
		const FString Label = Actor->GetActorLabel();
		LabelCounts.FindOrAdd(Label)++;

		if (!GPPrototypeArenaPrivate::IsExpectedLabel(Label))
		{
			++Result.UnexpectedGeneratedActorsCount;
		}

		if (Label == TEXT("GP_Arena_Floor"))
		{
			++Result.FloorCount;
		}
		else if (Label.StartsWith(TEXT("GP_Arena_Wall_")))
		{
			++Result.WallsCount;
		}
		else if (Label == TEXT("GP_Arena_DirectionalLight"))
		{
			++Result.DirectionalLightCount;
		}
		else if (Label == TEXT("GP_Arena_SkyLight"))
		{
			++Result.SkyLightCount;
		}
		else if (Label == TEXT("GP_Arena_SkyAtmosphere"))
		{
			++Result.SkyAtmosphereCount;
		}
		else if (Label == TEXT("GP_Arena_PlayerStart"))
		{
			++Result.PlayerStartCount;
		}
		else if (Label == TEXT("GP_Arena_NavMeshBounds"))
		{
			++Result.NavMeshBoundsCount;
		}
	}

	for (const TPair<FString, int32>& Pair : LabelCounts)
	{
		if (Pair.Value > 1)
		{
			Result.DuplicateLabelsCount += Pair.Value - 1;
		}
	}

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		Result.NavigationBuildStatus = NavSys->IsNavigationBuilt(World->GetWorldSettings())
			? TEXT("NavigationBuiltOrDefaultPresent")
			: TEXT("NavSysPresent_BuildUnconfirmed_OperatorMayNeedBuildPaths");
	}
	else
	{
		Result.NavigationBuildStatus = TEXT("NavigationSystemMissing");
	}

	Result.bReadyForPopulation =
		Result.bExists
		&& Result.bLoaded
		&& !Result.bWorldPartition
		&& Result.FloorCount == 1
		&& Result.WallsCount == 4
		&& Result.DirectionalLightCount == 1
		&& Result.SkyLightCount == 1
		&& Result.PlayerStartCount == 1
		&& Result.NavMeshBoundsCount == 1
		&& Result.DuplicateLabelsCount == 0
		&& Result.UnexpectedGeneratedActorsCount == 0
		&& Result.GameModeOverride.Contains(TEXT("GP_GameMode"));

	return Result;
}

void FGPPrototypeArenaGenerator::LogInspectResult(const FGPPrototypeArenaInspectResult& Result)
{
	UE_LOG(LogGPPrototypeArenaGenerator, Log,
		TEXT("GP Editor.InspectPrototypeArena: MapPath=%s Exists=%s Loaded=%s WorldPartition=%s GameModeOverride=%s GeneratorTagActors=%d Floor=%d Walls=%d DirectionalLight=%d SkyLight=%d SkyAtmosphere=%d PlayerStart=%d NavMeshBounds=%d DuplicateLabels=%d UnexpectedGeneratedActors=%d NavigationBuildStatus=%s ReadyForPopulation=%s"),
		*Result.MapPath,
		Result.bExists ? TEXT("true") : TEXT("false"),
		Result.bLoaded ? TEXT("true") : TEXT("false"),
		Result.bWorldPartition ? TEXT("true") : TEXT("false"),
		*Result.GameModeOverride,
		Result.GeneratorTagActors,
		Result.FloorCount,
		Result.WallsCount,
		Result.DirectionalLightCount,
		Result.SkyLightCount,
		Result.SkyAtmosphereCount,
		Result.PlayerStartCount,
		Result.NavMeshBoundsCount,
		Result.DuplicateLabelsCount,
		Result.UnexpectedGeneratedActorsCount,
		*Result.NavigationBuildStatus,
		Result.bReadyForPopulation ? TEXT("true") : TEXT("false"));
}
