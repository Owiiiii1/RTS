// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPResourceTypes.generated.h"

UENUM()
enum class EGP_ResourceType : uint8
{
	None UMETA(DisplayName = "None"),
	Ore UMETA(DisplayName = "Ore")
};

namespace GPResourceTypePrivate
{
	inline const TCHAR* ToString(EGP_ResourceType Type)
	{
		switch (Type)
		{
		case EGP_ResourceType::Ore:
			return TEXT("Ore");
		case EGP_ResourceType::None:
		default:
			return TEXT("None");
		}
	}
}
