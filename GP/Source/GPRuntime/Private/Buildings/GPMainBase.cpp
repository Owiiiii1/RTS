// Copyright Epic Games, Inc. All Rights Reserved.

#include "Buildings/GPMainBase.h"

#include "Components/CapsuleComponent.h"
#include "Game/GPGameState.h"
#include "Net/UnrealNetwork.h"
#include "Resources/GPStorageComponent.h"
#include "Tags/GPGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

AGP_MainBase::AGP_MainBase()
{
	CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
	SetRootComponent(CapsuleComponent);
	CapsuleComponent->InitCapsuleSize(80.0f, 120.0f);
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
	CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CapsuleComponent->SetGenerateOverlapEvents(false);
	CapsuleComponent->SetCanEverAffectNavigation(false);
	CapsuleComponent->SetSimulatePhysics(false);

	StorageComponent = CreateDefaultSubobject<UGP_StorageComponent>(TEXT("StorageComponent"));
	DropOffRangeCm = 400.0f;

	const FGPGameplayTags& GPTags = FGPGameplayTags::Get();
	if (GPTags.Building_Type_MainBase.IsValid())
	{
		CapabilityTags.AddTag(GPTags.Building_Type_MainBase);
	}
}

void AGP_MainBase::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithGameState();
}

void AGP_MainBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromGameState();
	Super::EndPlay(EndPlayReason);
}

void AGP_MainBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGP_MainBase, DropOffRangeCm);
}

UGP_StorageComponent* AGP_MainBase::GetStorageComponent() const
{
	return StorageComponent;
}

UCapsuleComponent* AGP_MainBase::GetCapsuleComponent() const
{
	return CapsuleComponent;
}

void AGP_MainBase::RegisterWithGameState()
{
	if (!HasAuthority() || bRegisteredWithGameState)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			GS->RegisterMainBase(this);
			bRegisteredWithGameState = true;
		}
	}
}

void AGP_MainBase::UnregisterFromGameState()
{
	if (!bRegisteredWithGameState)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (AGP_GameState* GS = World->GetGameState<AGP_GameState>())
		{
			GS->UnregisterMainBase(this);
		}
	}
	bRegisteredWithGameState = false;
}

bool AGP_MainBase::ValidateMainBaseContract(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	OutErrors.Reset();
	OutWarnings.Reset();

	if (!IsValid(StorageComponent))
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrStorage", "MainBase requires StorageComponent."));
	}
	else if (StorageComponent->GetOwner() != this)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrStorageOwner", "StorageComponent owner must be MainBase."));
	}

	if (!FMath::IsFinite(DropOffRangeCm) || DropOffRangeCm <= 0.0f)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrDropOff", "DropOffRangeCm must be finite and > 0."));
	}

	if (PrimaryActorTick.bCanEverTick)
	{
		OutErrors.Add(NSLOCTEXT("GPMainBase", "ErrTick", "MainBase must not enable permanent Tick."));
	}

	if (!GetCapabilityTags().HasTagExact(FGPGameplayTags::Get().Building_Type_MainBase))
	{
		OutWarnings.Add(NSLOCTEXT("GPMainBase", "WarnTag", "MainBase missing GP.Building.Type.MainBase tag."));
	}

	OutWarnings.Add(NSLOCTEXT("GPMainBase", "WarnNoBuildingDefinition",
		"No UGP_BuildingDefinition yet — DropOffRange/containers use GP-S28 placeholders."));

	if (IsValid(StorageComponent))
	{
		TArray<FText> StorageErrors;
		TArray<FText> StorageWarnings;
		StorageComponent->ValidateStorageContract(StorageErrors, StorageWarnings);
		OutErrors.Append(StorageErrors);
		OutWarnings.Append(StorageWarnings);
	}

	return OutErrors.Num() == 0;
}

#if WITH_EDITOR
EDataValidationResult AGP_MainBase::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	TArray<FText> Warnings;
	const bool bOk = ValidateMainBaseContract(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}
	return bOk ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
