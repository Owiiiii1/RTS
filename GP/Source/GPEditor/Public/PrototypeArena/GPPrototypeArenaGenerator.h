// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ANavMeshBoundsVolume;

struct FGPPrototypeArenaGenerateResult
{
	bool bSuccess = false;
	bool bExistingMapAbort = false;
	bool bNavigationBuildAttempted = false;
	bool bNavigationBuildSucceeded = false;
	bool bOperatorMustBuildPaths = false;
	FString FailureStage;
	FString Message;
	FString MapPackagePath;
	FString MapFilename;
	int32 SpawnedActorCount = 0;
	int32 RecastNavMeshCount = 0;
	int32 NavDataCount = 0;
};

struct FGPPrototypeArenaInspectResult
{
	FString MapPath;
	bool bExists = false;
	bool bLoaded = false;
	bool bWorldPartition = false;
	FString GameModeOverride;
	int32 GeneratorTagActors = 0;
	int32 FloorCount = 0;
	int32 WallsCount = 0;
	int32 DirectionalLightCount = 0;
	int32 SkyLightCount = 0;
	int32 SkyAtmosphereCount = 0;
	int32 PlayerStartCount = 0;
	int32 NavMeshBoundsCount = 0;
	int32 DuplicateLabelsCount = 0;
	int32 UnexpectedGeneratedActorsCount = 0;
	bool bNavBoundsValid = false;
	FVector NavBoundsOrigin = FVector::ZeroVector;
	FVector NavBoundsExtent = FVector::ZeroVector;
	float NavBoundsSphereRadius = 0.0f;
	int32 RecastNavMeshCount = 0;
	int32 NavDataCount = 0;
	FString NavigationBuildStatus;
	bool bReadyForPopulation = false;
};

/** One-shot editor generator for L_PrototypeArena infrastructure (GP-S27A2). */
class FGPPrototypeArenaGenerator
{
public:
	static constexpr int32 PrototypeArenaGeneratorVersion = 1;
	static const TCHAR* MapPackagePath;
	static const TCHAR* GeneratedActorTag;
	static const TCHAR* ManifestRelativePath;

	static bool IsEditorGenerationAllowed(FString& OutReason);
	static bool DoesMapPackageExist();
	static FGPPrototypeArenaGenerateResult Generate();
	static FGPPrototypeArenaInspectResult Inspect(UWorld* OptionalWorld = nullptr);
	static void LogInspectResult(const FGPPrototypeArenaInspectResult& Result);

private:
	struct FSpawnedActorRecord
	{
		FString Label;
		FString ClassName;
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FVector Scale = FVector::OneVector;
		FString Extra;
	};

	struct FNavBoundsValidation
	{
		bool bValid = false;
		FVector Origin = FVector::ZeroVector;
		FVector BoxExtent = FVector::ZeroVector;
		float SphereRadius = 0.0f;
		FString Detail;
	};

	static void LogStage(const TCHAR* Stage, const FString& Detail);
	static AActor* SpawnStaticCube(
		UWorld* World,
		const FName& Label,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& Scale,
		TArray<FSpawnedActorRecord>& OutRecords);
	static void ApplyGeneratedMetadata(AActor* Actor, const FName& Label);
	static bool ConfigureWorldSettings(UWorld* World, FString& OutError);
	static bool SpawnInfrastructure(UWorld* World, TArray<FSpawnedActorRecord>& OutRecords, FString& OutError);
	static FNavBoundsValidation ValidateNavMeshBounds(const ANavMeshBoundsVolume* NavBounds);
	static bool ValidatePrimaryNavBoundsInWorld(UWorld* World, FNavBoundsValidation& OutValidation, FString& OutError);
	static bool TryBuildNavigation(UWorld* World, FGPPrototypeArenaGenerateResult& InOutResult);
	static void CountNavigationActors(UWorld* World, int32& OutRecastCount, int32& OutNavDataCount);
	static bool WriteManifest(const TArray<FSpawnedActorRecord>& Records, const FGPPrototypeArenaGenerateResult& Result, FString& OutError);
	static FString ResolveManifestAbsolutePath();
};
