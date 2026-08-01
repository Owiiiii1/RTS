// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/GPMatchAssetLoader.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

void UGP_MatchAssetLoader::Deinitialize()
{
	++LoadGeneration;
	CancelAndReleaseActiveHandle();
	ClearRequestedAndFailedPaths();
	MatchAssetsLoadCompleted.Clear();
	LoadState = EGPMatchAssetLoadState::Idle;

	Super::Deinitialize();
}

void UGP_MatchAssetLoader::PreloadForMatch(const TArray<FSoftObjectPath>& AssetPaths)
{
	TArray<FSoftObjectPath> NormalizedPaths;
	TArray<FSoftObjectPath> InvalidPaths;
	const bool bNormalizationOk = NormalizeAssetPaths(AssetPaths, NormalizedPaths, InvalidPaths);

	if (!bNormalizationOk)
	{
		CancelAndReleaseActiveHandle();
		++LoadGeneration;

		ClearRequestedAndFailedPaths();
		FailedAssetPaths = MoveTemp(InvalidPaths);
		LoadState = EGPMatchAssetLoadState::Failed;

		UE_LOG(LogTemp, Error,
			TEXT("UGP_MatchAssetLoader::PreloadForMatch: invalid required path count=%d generation=%u"),
			FailedAssetPaths.Num(),
			LoadGeneration);

		MatchAssetsLoadCompleted.Broadcast(false, LoadGeneration);
		return;
	}

	if (NormalizedPaths.Num() == 0)
	{
		CancelAndReleaseActiveHandle();
		++LoadGeneration;

		ClearRequestedAndFailedPaths();
		LoadState = EGPMatchAssetLoadState::Loaded;

		UE_LOG(LogTemp, Verbose,
			TEXT("UGP_MatchAssetLoader::PreloadForMatch: empty request -> Loaded generation=%u"),
			LoadGeneration);

		MatchAssetsLoadCompleted.Broadcast(true, LoadGeneration);
		return;
	}

	const bool bSameNormalizedSet = (NormalizedPaths == RequestedAssetPaths);
	if (bSameNormalizedSet
		&& (LoadState == EGPMatchAssetLoadState::Loading || LoadState == EGPMatchAssetLoadState::Loaded))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("UGP_MatchAssetLoader::PreloadForMatch: same-set no-op state=%d generation=%u"),
			static_cast<int32>(LoadState),
			LoadGeneration);
		return;
	}

	CancelAndReleaseActiveHandle();
	++LoadGeneration;

	RequestedAssetPaths = MoveTemp(NormalizedPaths);
	RequestedAssetPathSet.Reset();
	RequestedAssetPathSet.Append(RequestedAssetPaths);
	FailedAssetPaths.Reset();
	LoadState = EGPMatchAssetLoadState::Loading;

	const uint32 CurrentGeneration = LoadGeneration;
	ActiveLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		RequestedAssetPaths,
		FStreamableDelegate::CreateUObject(this, &UGP_MatchAssetLoader::HandleLoadCompleted, CurrentGeneration));

	if (!ActiveLoadHandle.IsValid())
	{
		LoadState = EGPMatchAssetLoadState::Failed;
		FailedAssetPaths = RequestedAssetPaths;
		ActiveLoadHandle.Reset();

		UE_LOG(LogTemp, Error,
			TEXT("UGP_MatchAssetLoader::PreloadForMatch: RequestAsyncLoad returned null handle generation=%u count=%d"),
			LoadGeneration,
			RequestedAssetPaths.Num());

		MatchAssetsLoadCompleted.Broadcast(false, LoadGeneration);
		return;
	}

	UE_LOG(LogTemp, Verbose,
		TEXT("UGP_MatchAssetLoader::PreloadForMatch: Loading generation=%u count=%d"),
		LoadGeneration,
		RequestedAssetPaths.Num());
}

void UGP_MatchAssetLoader::ReleaseMatchAssets()
{
	++LoadGeneration;
	CancelAndReleaseActiveHandle();
	ClearRequestedAndFailedPaths();
	LoadState = EGPMatchAssetLoadState::Idle;

	UE_LOG(LogTemp, Verbose,
		TEXT("UGP_MatchAssetLoader::ReleaseMatchAssets: Idle generation=%u"),
		LoadGeneration);
}

EGPMatchAssetLoadState UGP_MatchAssetLoader::GetLoadState() const
{
	return LoadState;
}

bool UGP_MatchAssetLoader::IsMatchAssetSetReady() const
{
	return LoadState == EGPMatchAssetLoadState::Loaded;
}

uint32 UGP_MatchAssetLoader::GetLoadGeneration() const
{
	return LoadGeneration;
}

const TArray<FSoftObjectPath>& UGP_MatchAssetLoader::GetRequestedAssetPaths() const
{
	return RequestedAssetPaths;
}

const TArray<FSoftObjectPath>& UGP_MatchAssetLoader::GetFailedAssetPaths() const
{
	return FailedAssetPaths;
}

UObject* UGP_MatchAssetLoader::ResolveObject(const FSoftObjectPath& AssetPath) const
{
	if (LoadState != EGPMatchAssetLoadState::Loaded)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("UGP_MatchAssetLoader::ResolveObject: rejected (state=%d) path=%s"),
			static_cast<int32>(LoadState),
			*AssetPath.ToString());
		return nullptr;
	}

	if (!AssetPath.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UGP_MatchAssetLoader::ResolveObject: invalid path"));
		return nullptr;
	}

	if (!RequestedAssetPathSet.Contains(AssetPath))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UGP_MatchAssetLoader::ResolveObject: path not in active set %s"),
			*AssetPath.ToString());
		return nullptr;
	}

	return AssetPath.ResolveObject();
}

FGPOnMatchAssetsLoadCompleted& UGP_MatchAssetLoader::OnMatchAssetsLoadCompleted()
{
	return MatchAssetsLoadCompleted;
}

void UGP_MatchAssetLoader::HandleLoadCompleted(uint32 CompletedGeneration)
{
	if (CompletedGeneration != LoadGeneration)
	{
		return;
	}

	if (LoadState != EGPMatchAssetLoadState::Loading)
	{
		return;
	}

	FailedAssetPaths.Reset();
	for (const FSoftObjectPath& AssetPath : RequestedAssetPaths)
	{
		if (AssetPath.ResolveObject() == nullptr)
		{
			FailedAssetPaths.Add(AssetPath);
		}
	}

	if (FailedAssetPaths.Num() == 0)
	{
		LoadState = EGPMatchAssetLoadState::Loaded;

		UE_LOG(LogTemp, Verbose,
			TEXT("UGP_MatchAssetLoader::HandleLoadCompleted: Loaded generation=%u count=%d"),
			LoadGeneration,
			RequestedAssetPaths.Num());

		MatchAssetsLoadCompleted.Broadcast(true, LoadGeneration);
		return;
	}

	LoadState = EGPMatchAssetLoadState::Failed;
	CancelAndReleaseActiveHandle();

	UE_LOG(LogTemp, Error,
		TEXT("UGP_MatchAssetLoader::HandleLoadCompleted: missing required assets count=%d generation=%u"),
		FailedAssetPaths.Num(),
		LoadGeneration);

	MatchAssetsLoadCompleted.Broadcast(false, LoadGeneration);
}

bool UGP_MatchAssetLoader::NormalizeAssetPaths(
	const TArray<FSoftObjectPath>& InputPaths,
	TArray<FSoftObjectPath>& OutNormalizedPaths,
	TArray<FSoftObjectPath>& OutInvalidPaths) const
{
	OutNormalizedPaths.Reset();
	OutInvalidPaths.Reset();

	OutNormalizedPaths.Reserve(InputPaths.Num());
	for (const FSoftObjectPath& AssetPath : InputPaths)
	{
		if (!AssetPath.IsValid())
		{
			OutInvalidPaths.Add(AssetPath);
			continue;
		}

		OutNormalizedPaths.Add(AssetPath);
	}

	if (OutInvalidPaths.Num() > 0)
	{
		OutNormalizedPaths.Reset();
		return false;
	}

	OutNormalizedPaths.Sort(FSoftObjectPathLexicalLess());

	for (int32 Index = OutNormalizedPaths.Num() - 1; Index > 0; --Index)
	{
		if (OutNormalizedPaths[Index] == OutNormalizedPaths[Index - 1])
		{
			OutNormalizedPaths.RemoveAt(Index, 1, EAllowShrinking::No);
		}
	}

	return true;
}

void UGP_MatchAssetLoader::CancelAndReleaseActiveHandle()
{
	if (!ActiveLoadHandle.IsValid())
	{
		return;
	}

	if (ActiveLoadHandle->IsLoadingInProgress())
	{
		ActiveLoadHandle->CancelHandle();
	}

	ActiveLoadHandle->ReleaseHandle();
	ActiveLoadHandle.Reset();
}

void UGP_MatchAssetLoader::ClearRequestedAndFailedPaths()
{
	RequestedAssetPaths.Reset();
	RequestedAssetPathSet.Reset();
	FailedAssetPaths.Reset();
}
