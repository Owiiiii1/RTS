// Copyright Epic Games, Inc. All Rights Reserved.

#include "Orbital/GPWallPackageDefinition.h"

#include "Tags/GPGameplayTags.h"

namespace GPWallPackageDefinitionPrivate
{
	static constexpr const TCHAR* PrimaryType = TEXT("GPWallPackageDefinition");
}

UGP_WallPackageDefinition::UGP_WallPackageDefinition()
{
	DisplayName = NSLOCTEXT("GPWallPackage", "DisplayName", "Wall Package");
	Cost = NativeBootstrapCost;
	SegmentCount = NativeBootstrapSegmentCount;
	DeliveryDescentSeconds = 2.5f;
	PayloadDeployDelaySeconds = 2.0f;

	const FGPGameplayTags& Tags = FGPGameplayTags::Get();
	if (Tags.Drop_Type_WallPackage.IsValid())
	{
		DropTags.AddTag(Tags.Drop_Type_WallPackage);
	}
}

const TCHAR* UGP_WallPackageDefinition::PrimaryAssetTypeName()
{
	return GPWallPackageDefinitionPrivate::PrimaryType;
}

FPrimaryAssetId UGP_WallPackageDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(PrimaryAssetTypeName()), GetFName());
}

bool UGP_WallPackageDefinition::IsValidForDelivery(int32 InventoryCapacity, TArray<FText>* OutErrors) const
{
	bool bOk = true;
	auto AddError = [&](const FText& Text)
	{
		bOk = false;
		if (OutErrors != nullptr)
		{
			OutErrors->Add(Text);
		}
	};

	if (!FMath::IsFinite(Cost) || Cost < 0.0f)
	{
		AddError(NSLOCTEXT("GPWallPackage", "BadCost", "Cost must be finite and >= 0."));
	}
	if (SegmentCount <= 0)
	{
		AddError(NSLOCTEXT("GPWallPackage", "BadSegments", "SegmentCount must be > 0."));
	}
	if (InventoryCapacity > 0 && SegmentCount > InventoryCapacity)
	{
		AddError(NSLOCTEXT("GPWallPackage", "OverCapacity", "SegmentCount exceeds MainBase wall inventory capacity."));
	}
	if (!FMath::IsFinite(DeliveryDescentSeconds) || DeliveryDescentSeconds < 0.0f)
	{
		AddError(NSLOCTEXT("GPWallPackage", "BadDescent", "DeliveryDescentSeconds must be finite and >= 0."));
	}
	if (!FMath::IsFinite(PayloadDeployDelaySeconds) || PayloadDeployDelaySeconds < 0.0f)
	{
		AddError(NSLOCTEXT("GPWallPackage", "BadDeploy", "PayloadDeployDelaySeconds must be finite and >= 0."));
	}

	const FGPGameplayTags& Tags = FGPGameplayTags::Get();
	if (Tags.Drop_Type_WallPackage.IsValid() && !DropTags.HasTagExact(Tags.Drop_Type_WallPackage))
	{
		AddError(NSLOCTEXT("GPWallPackage", "MissingTag", "DropTags must include GP.Drop.Type.WallPackage."));
	}

	return bOk;
}
