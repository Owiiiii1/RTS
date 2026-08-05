// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/GPResourceNode.h"

#include "Components/BoxComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Visual/GPPrimitiveVisualTypes.h"
#include "Visual/GPResourceNodeVisualComponent.h"

#if !UE_BUILD_SHIPPING
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#endif

DEFINE_LOG_CATEGORY(LogGPResourceNode);

namespace GPResourceNodePrivate
{
	/** Documented collision policy for S27A1 ore piles. */
	static const FName CollisionProfileName(TEXT("BlockAll"));

	static const TCHAR* NetModeToString(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Standalone:
			return TEXT("Standalone");
		case NM_DedicatedServer:
			return TEXT("DedicatedServer");
		case NM_ListenServer:
			return TEXT("ListenServer");
		case NM_Client:
			return TEXT("Client");
		default:
			return TEXT("Unknown");
		}
	}

	static const TCHAR* RoleToString(ENetRole Role)
	{
		switch (Role)
		{
		case ROLE_None:
			return TEXT("None");
		case ROLE_SimulatedProxy:
			return TEXT("SimulatedProxy");
		case ROLE_AutonomousProxy:
			return TEXT("AutonomousProxy");
		case ROLE_Authority:
			return TEXT("Authority");
		default:
			return TEXT("Unknown");
		}
	}
}

AGP_ResourceNode::AGP_ResourceNode()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);
	bAlwaysRelevant = true;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	// Pile-sized gameplay volume (~120×120×80 uu), not a single crystal.
	CollisionBox->InitBoxExtent(FVector(60.0f, 60.0f, 40.0f));
	CollisionBox->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	CollisionBox->SetCollisionProfileName(GPResourceNodePrivate::CollisionProfileName);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetGenerateOverlapEvents(false);
	CollisionBox->SetCanEverAffectNavigation(true);
	CollisionBox->SetSimulatePhysics(false);

	ResourceNodeVisualComponent = CreateDefaultSubobject<UGP_ResourceNodeVisualComponent>(TEXT("ResourceNodeVisualComponent"));

	ResourceType = EGP_ResourceType::Ore;
	MaxAmount = 5000;
	CurrentAmount = 5000;
}

void AGP_ResourceNode::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		NormalizeAmountsOnConstruction();
	}
}

void AGP_ResourceNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGP_ResourceNode, ResourceType);
	DOREPLIFETIME(AGP_ResourceNode, MaxAmount);
	DOREPLIFETIME(AGP_ResourceNode, CurrentAmount);
}

void AGP_ResourceNode::NormalizeAmountsOnConstruction()
{
	if (MaxAmount < 0)
	{
		MaxAmount = 0;
	}
	ClampCurrentAmountToMax();
}

void AGP_ResourceNode::ClampCurrentAmountToMax()
{
	CurrentAmount = FMath::Clamp(CurrentAmount, 0, MaxAmount);
}

EGP_ResourceType AGP_ResourceNode::GetResourceType() const
{
	return ResourceType;
}

int32 AGP_ResourceNode::GetMaxAmount() const
{
	return MaxAmount;
}

int32 AGP_ResourceNode::GetCurrentAmount() const
{
	return CurrentAmount;
}

bool AGP_ResourceNode::IsDepleted() const
{
	return CurrentAmount <= 0;
}

int32 AGP_ResourceNode::ConsumeResource(int32 RequestedAmount)
{
	if (!HasAuthority())
	{
		UE_LOG(LogGPResourceNode, Warning,
			TEXT("GP ResourceNode.ConsumeResource rejected (no authority): Actor=%s Requested=%d"),
			*GetName(),
			RequestedAmount);
		return 0;
	}

	if (RequestedAmount <= 0)
	{
		return 0;
	}

	NormalizeAmountsOnConstruction();
	const int32 Consumed = FMath::Min(RequestedAmount, CurrentAmount);
	if (Consumed <= 0)
	{
		return 0;
	}

	CurrentAmount -= Consumed;
	ClampCurrentAmountToMax();
	return Consumed;
}

UGP_ResourceNodeVisualComponent* AGP_ResourceNode::GetResourceNodeVisualComponent() const
{
	return ResourceNodeVisualComponent;
}

UBoxComponent* AGP_ResourceNode::GetCollisionBox() const
{
	return CollisionBox;
}

void AGP_ResourceNode::OnRep_CurrentAmount()
{
	ClampCurrentAmountToMax();
	UE_LOG(LogGPResourceNode, Verbose,
		TEXT("GP ResourceNode.OnRep_CurrentAmount: Actor=%s Current=%d Max=%d"),
		*GetName(),
		CurrentAmount,
		MaxAmount);
}

#if !UE_BUILD_SHIPPING
namespace GPResourceNodeDebug
{
	static AGP_ResourceNode* FindFirstNode(UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		AGP_ResourceNode* Best = nullptr;
		for (TActorIterator<AGP_ResourceNode> It(World); It; ++It)
		{
			AGP_ResourceNode* Node = *It;
			if (!IsValid(Node))
			{
				continue;
			}

			if (Best == nullptr || Node->GetName() < Best->GetName())
			{
				Best = Node;
			}
		}

		return Best;
	}

	static void ResourceNodeInspect(const TArray<FString>& Args, UWorld* World)
	{
		(void)Args;
		if (World == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Inspect: missing world"));
			return;
		}

		AGP_ResourceNode* Node = FindFirstNode(World);
		if (Node == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Inspect: no AGP_ResourceNode found"));
			return;
		}

		const UBoxComponent* Box = Node->GetCollisionBox();
		const UGP_ResourceNodeVisualComponent* Visual = Node->GetResourceNodeVisualComponent();
		TArray<FName> PartNames;
		if (Visual != nullptr)
		{
			Visual->GetPartNames(PartNames);
		}

		FString PartNamesJoined = TEXT("none");
		if (PartNames.Num() > 0)
		{
			PartNamesJoined.Reset();
			for (int32 Index = 0; Index < PartNames.Num(); ++Index)
			{
				if (Index > 0)
				{
					PartNamesJoined += TEXT(",");
				}
				PartNamesJoined += PartNames[Index].ToString();
			}
		}

		UE_LOG(LogGPResourceNode, Log,
			TEXT("GP ResourceNode.Inspect: Actor=%s ResourceType=%s MaxAmount=%d CurrentAmount=%d Depleted=%s Role=%s NetMode=%s Replicates=%s AlwaysRelevant=%s CollisionComponent=%s CollisionEnabled=%s CollisionProfile=%s AffectsNavigation=%s VisualComponent=%s VisualBuilt=%s Parts=%d PartNames=[%s] DedicatedVisualSuppressed=%s TickEnabled=%s VisualCollisionDisabled=%s VisualSourceMode=%s GeneratedPartCount=%d AuthoredPrimitiveComponentCount=%d NativeVisualBuilt=%s UsesAuthoredComponents=%s GeneratedCollisionDisabled=%s AuthoredCollisionWarnings=%d AuthoredNavigationWarnings=%d DuplicateGeneratedParts=%d"),
			*Node->GetName(),
			GPResourceTypePrivate::ToString(Node->GetResourceType()),
			Node->GetMaxAmount(),
			Node->GetCurrentAmount(),
			Node->IsDepleted() ? TEXT("true") : TEXT("false"),
			GPResourceNodePrivate::RoleToString(Node->GetLocalRole()),
			GPResourceNodePrivate::NetModeToString(World->GetNetMode()),
			Node->GetIsReplicated() ? TEXT("true") : TEXT("false"),
			Node->bAlwaysRelevant ? TEXT("true") : TEXT("false"),
			Box != nullptr ? *Box->GetName() : TEXT("missing"),
			Box != nullptr
				? (Box->GetCollisionEnabled() == ECollisionEnabled::NoCollision
					? TEXT("NoCollision")
					: (Box->GetCollisionEnabled() == ECollisionEnabled::QueryOnly
						? TEXT("QueryOnly")
						: (Box->GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly
							? TEXT("PhysicsOnly")
							: TEXT("QueryAndPhysics"))))
				: TEXT("n/a"),
			Box != nullptr ? *Box->GetCollisionProfileName().ToString() : TEXT("n/a"),
			(Box != nullptr && Box->CanEverAffectNavigation()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr ? TEXT("present") : TEXT("missing"),
			(Visual != nullptr && Visual->HasBuiltVisual()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr ? Visual->GetPartCount() : 0,
			*PartNamesJoined,
			(Visual != nullptr && Visual->IsDedicatedVisualSuppressed()) ? TEXT("true") : TEXT("false"),
			Node->IsActorTickEnabled() ? TEXT("true") : TEXT("false"),
			(Visual == nullptr || Visual->AreVisualPartCollisionsDisabled()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr
				? GPPrimitiveVisualDefaults::VisualSourceModeToString(Visual->GetVisualSourceMode())
				: TEXT("n/a"),
			Visual != nullptr ? Visual->GetGeneratedPartCount() : 0,
			Visual != nullptr ? Visual->GetAuthoredPrimitiveComponentCount() : 0,
			(Visual != nullptr && Visual->HasBuiltVisual()) ? TEXT("true") : TEXT("false"),
			(Visual != nullptr && Visual->UsesAuthoredComponents()) ? TEXT("true") : TEXT("false"),
			(Visual == nullptr || Visual->AreGeneratedCollisionsDisabled()) ? TEXT("true") : TEXT("false"),
			Visual != nullptr ? Visual->GetAuthoredCollisionWarningCount() : 0,
			Visual != nullptr ? Visual->GetAuthoredNavigationWarningCount() : 0,
			Visual != nullptr ? Visual->GetDuplicateGeneratedPartCount() : 0);
	}

	static void ResourceNodeConsume(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Consume: missing world"));
			return;
		}

		if (World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogGPResourceNode, Warning,
				TEXT("GP ResourceNode.Consume: rejected on client (authority required)"));
			return;
		}

		int32 Requested = 0;
		if (Args.Num() < 1 || !LexTryParseString(Requested, *Args[0]))
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Consume: usage gp.ResourceNode.Consume <Amount>"));
			return;
		}

		AGP_ResourceNode* Node = FindFirstNode(World);
		if (Node == nullptr)
		{
			UE_LOG(LogGPResourceNode, Warning, TEXT("GP ResourceNode.Consume: no AGP_ResourceNode found"));
			return;
		}

		if (!Node->HasAuthority())
		{
			UE_LOG(LogGPResourceNode, Warning,
				TEXT("GP ResourceNode.Consume: rejected (node has no authority) Actor=%s"),
				*Node->GetName());
			return;
		}

		const int32 Before = Node->GetCurrentAmount();
		const int32 Consumed = Node->ConsumeResource(Requested);
		const int32 After = Node->GetCurrentAmount();

		UE_LOG(LogGPResourceNode, Log,
			TEXT("GP ResourceNode.Consume: Actor=%s Requested=%d Consumed=%d Before=%d After=%d Depleted=%s"),
			*Node->GetName(),
			Requested,
			Consumed,
			Before,
			After,
			Node->IsDepleted() ? TEXT("true") : TEXT("false"));
	}

	static FAutoConsoleCommandWithWorldAndArgs GResourceNodeInspectCommand(
		TEXT("gp.ResourceNode.Inspect"),
		TEXT("Inspect the first AGP_ResourceNode in the world (non-shipping)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceNodeInspect));

	static FAutoConsoleCommandWithWorldAndArgs GResourceNodeConsumeCommand(
		TEXT("gp.ResourceNode.Consume"),
		TEXT("Authority-only: ConsumeResource on the first AGP_ResourceNode. Usage: gp.ResourceNode.Consume <Amount>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ResourceNodeConsume));
}
#endif
