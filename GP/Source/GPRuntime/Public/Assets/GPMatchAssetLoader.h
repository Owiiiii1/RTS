// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/StreamableManager.h"
#include "GPMatchAssetLoader.generated.h"

UENUM()
enum class EGPMatchAssetLoadState : uint8
{
	Idle,
	Loading,
	Loaded,
	Failed
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FGPOnMatchAssetsLoadCompleted,
	bool /*bSuccess*/,
	uint32 /*Generation*/);

/**
 * Local GameInstance match asset preload/resolve helper.
 * Accepts an explicit soft-path list; does not own faction/map/manifest semantics.
 */
UCLASS()
class GPRUNTIME_API UGP_MatchAssetLoader : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void PreloadForMatch(const TArray<FSoftObjectPath>& AssetPaths);
	void ReleaseMatchAssets();

	EGPMatchAssetLoadState GetLoadState() const;
	bool IsMatchAssetSetReady() const;
	uint32 GetLoadGeneration() const;

	const TArray<FSoftObjectPath>& GetRequestedAssetPaths() const;
	const TArray<FSoftObjectPath>& GetFailedAssetPaths() const;

	UObject* ResolveObject(const FSoftObjectPath& AssetPath) const;

	template <typename TObjectType>
	TObjectType* Resolve(const TSoftObjectPtr<TObjectType>& SoftReference) const
	{
		return Cast<TObjectType>(ResolveObject(SoftReference.ToSoftObjectPath()));
	}

	template <typename TObjectType>
	TSubclassOf<TObjectType> ResolveClass(const TSoftClassPtr<TObjectType>& SoftClass) const
	{
		UObject* ResolvedObject = ResolveObject(SoftClass.ToSoftObjectPath());
		UClass* ResolvedClass = Cast<UClass>(ResolvedObject);
		if (ResolvedClass == nullptr || !ResolvedClass->IsChildOf(TObjectType::StaticClass()))
		{
			return TSubclassOf<TObjectType>();
		}

		return TSubclassOf<TObjectType>(ResolvedClass);
	}

	FGPOnMatchAssetsLoadCompleted& OnMatchAssetsLoadCompleted();

private:
	void HandleLoadCompleted(uint32 CompletedGeneration);

	bool NormalizeAssetPaths(
		const TArray<FSoftObjectPath>& InputPaths,
		TArray<FSoftObjectPath>& OutNormalizedPaths,
		TArray<FSoftObjectPath>& OutInvalidPaths) const;

	void CancelAndReleaseActiveHandle();
	void ClearRequestedAndFailedPaths();

	EGPMatchAssetLoadState LoadState = EGPMatchAssetLoadState::Idle;
	uint32 LoadGeneration = 0;

	TArray<FSoftObjectPath> RequestedAssetPaths;
	TSet<FSoftObjectPath> RequestedAssetPathSet;
	TArray<FSoftObjectPath> FailedAssetPaths;

	TSharedPtr<FStreamableHandle> ActiveLoadHandle;

	FGPOnMatchAssetsLoadCompleted MatchAssetsLoadCompleted;
};
