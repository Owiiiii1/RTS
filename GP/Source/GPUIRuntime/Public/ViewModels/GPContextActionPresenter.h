// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GPContextActionPresenter.generated.h"

class AGP_PlayerController;
class AGP_UnitBase;
class UGP_SelectionComponent;

UENUM(BlueprintType)
enum class EGP_ContextActionMode : uint8
{
	None UMETA(DisplayName = "None"),
	Unit UMETA(DisplayName = "Unit"),
	UnitGroup UMETA(DisplayName = "Unit Group"),
	Building UMETA(DisplayName = "Building"),
	MainBase UMETA(DisplayName = "Main Base")
};

UENUM(BlueprintType)
enum class EGP_ContextActionId : uint8
{
	None UMETA(DisplayName = "None"),
	Move UMETA(DisplayName = "Move"),
	Stop UMETA(DisplayName = "Stop"),
	AttackMove UMETA(DisplayName = "Attack-Move"),
	Patrol UMETA(DisplayName = "Patrol"),
	Purchase UMETA(DisplayName = "Purchase")
};

UENUM(BlueprintType)
enum class EGP_ContextActionPanelState : uint8
{
	Actions UMETA(DisplayName = "Actions"),
	PurchaseRoot UMETA(DisplayName = "Purchase Root")
};

USTRUCT(BlueprintType)
struct GPUIRUNTIME_API FGP_ContextActionPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|ContextActions")
	EGP_ContextActionId ActionId = EGP_ContextActionId::None;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|ContextActions")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|ContextActions")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|ContextActions")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|ContextActions")
	FText DisabledReason;
};

DECLARE_MULTICAST_DELEGATE(FOnGPContextActionsChanged);

/**
 * LocalPlayer-owned, event-driven Context Action Grid presentation.
 * Reads local selection + existing PC command seams. Does not tick, scan the world,
 * or own gameplay authority.
 */
UCLASS()
class GPUIRUNTIME_API UGP_ContextActionPresenter : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(AGP_PlayerController* InPlayerController);
	void Shutdown();

	EGP_ContextActionMode GetMode() const { return Mode; }
	EGP_ContextActionPanelState GetPanelState() const { return PanelState; }
	const TArray<FGP_ContextActionPresentation>& GetActions() const { return Actions; }
	int32 GetBoundDelegateCount() const;

	void RequestContextAction(EGP_ContextActionId ActionId);
	void RequestOpenMainBasePurchase();

	FOnGPContextActionsChanged OnContextActionsChanged;

protected:
	virtual void BeginDestroy() override;

private:
	struct FBoundSelectedUnit
	{
		TWeakObjectPtr<AGP_UnitBase> Unit;
		FDelegateHandle DiedHandle;
	};

	void HandleSelectionChanged();
	void RebuildPresentation();
	void UnbindSelectedUnits();
	void BindSelectedUnits(const TArray<AGP_UnitBase*>& Units);
	void HandleUnitDied(AGP_UnitBase* Unit);

	UFUNCTION()
	void HandleBoundActorDestroyed(AActor* DestroyedActor);

	static void CollectLiveSelectedUnits(
		const UGP_SelectionComponent* Selection,
		TArray<AGP_UnitBase*>& OutUnits);
	static EGP_ContextActionMode ResolveMode(
		const TArray<AGP_UnitBase*>& LiveSelected,
		int32 LocalTeamId);
	static FGP_ContextActionPresentation MakeAction(
		EGP_ContextActionId ActionId,
		const FText& DisplayName,
		bool bEnabled,
		const FText& DisabledReason);
	bool IsActionEnabled(EGP_ContextActionId ActionId) const;

	TWeakObjectPtr<AGP_PlayerController> BoundPlayerController;
	TWeakObjectPtr<UGP_SelectionComponent> BoundSelection;
	FDelegateHandle SelectionChangedHandle;
	TArray<FBoundSelectedUnit> BoundUnits;
	TArray<FGP_ContextActionPresentation> Actions;
	EGP_ContextActionMode Mode = EGP_ContextActionMode::None;
	EGP_ContextActionPanelState PanelState = EGP_ContextActionPanelState::Actions;
};
