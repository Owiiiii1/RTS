// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Orbital/GPUnitDropManifest.h"
#include "UObject/Object.h"
#include "GPContextActionPresenter.generated.h"

class AGP_PlayerController;
class AGP_UnitBase;
class UAbilitySystemComponent;
class UGP_SelectionComponent;
class UGP_WallSegmentInventoryComponent;
class UTexture2D;
struct FOnAttributeChangeData;

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
	PurchaseRoot UMETA(DisplayName = "Purchase Root"),
	PurchaseUnits UMETA(DisplayName = "Purchase Units"),
	PurchaseBuildings UMETA(DisplayName = "Purchase Buildings"),
	PurchaseDefense UMETA(DisplayName = "Purchase Defense"),
	PurchaseBuildingSelected UMETA(DisplayName = "Purchase Building Selected"),
	PurchaseDefenseSelected UMETA(DisplayName = "Purchase Defense Selected")
};

UENUM(BlueprintType)
enum class EGP_PurchaseCategory : uint8
{
	Units UMETA(DisplayName = "Units"),
	Buildings UMETA(DisplayName = "Buildings"),
	Defense UMETA(DisplayName = "Defense")
};

UENUM(BlueprintType)
enum class EGP_PurchaseCatalogItemKind : uint8
{
	Unit UMETA(DisplayName = "Unit"),
	Building UMETA(DisplayName = "Building"),
	DefensiveBuilding UMETA(DisplayName = "Defensive Building"),
	WallPackage UMETA(DisplayName = "Wall Package")
};

/**
 * Factual MainBase PURCHASE catalog row. Presentation only — no spend / RPC / manifest.
 * ItemId is the product PrimaryAssetId. Icon is already-loaded only (null allowed).
 */
USTRUCT(BlueprintType)
struct GPUIRUNTIME_API FGP_PurchaseCatalogRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	FPrimaryAssetId ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	EGP_PurchaseCatalogItemKind ItemKind = EGP_PurchaseCatalogItemKind::Unit;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	EGP_PurchaseCategory Category = EGP_PurchaseCategory::Units;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	float Cost = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	FText DisabledReason;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	int32 TransportSlotCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	int32 SegmentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct GPUIRUNTIME_API FGP_PurchaseUnitManifestPresentation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	int32 WorkerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	int32 SalvageWalkerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	int32 UnitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	int32 UsedSlots = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	int32 MaxSlots = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	float TotalCost = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	bool bCanLaunch = false;

	UPROPERTY(BlueprintReadOnly, Category = "GP|HUD|PurchaseCatalog")
	FText DisabledReason;
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
	const TArray<FGP_PurchaseCatalogRow>& GetPurchaseCatalogRows() const { return PurchaseCatalogRows; }
	FGP_PurchaseCatalogRow GetSelectedPurchaseItem() const;
	FGP_PurchaseUnitManifestPresentation GetPurchaseUnitManifestPresentation() const;
	FText GetContextMessage() const;
	int32 GetBoundDelegateCount() const;

	FText GetTargetingPrompt() const;

	void RequestContextAction(EGP_ContextActionId ActionId);
	void RequestOpenMainBasePurchase();
	void RequestOpenPurchaseCategory(EGP_PurchaseCategory Category);
	void RequestPurchaseBack();
	void RequestPurchaseRowPrimary(FPrimaryAssetId ItemId);
	void RequestPurchaseRowSecondary(FPrimaryAssetId ItemId);
	void RequestLaunchUnitShuttle();
	void RequestLaunchSelectedPurchaseItem();

#if !UE_BUILD_SHIPPING
	int32 DebugGetUnitDropRequestCount() const { return DebugUnitDropRequestCount; }
	FGP_UnitDropManifest DebugGetLastRequestedUnitManifest() const { return DebugLastRequestedUnitManifest; }
	int32 DebugGetBuildingPurchaseRequestCount() const { return DebugBuildingPurchaseRequestCount; }
	FPrimaryAssetId DebugGetLastRequestedBuildingPurchaseId() const { return DebugLastRequestedBuildingPurchaseId; }
	int32 DebugGetWallPackagePurchaseRequestCount() const { return DebugWallPackagePurchaseRequestCount; }
	FPrimaryAssetId DebugGetPendingAutoDeployItemId() const { return PendingAutoDeployItemId; }
#endif

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
	void HandleCommandTargetingModeChanged();
	void RebuildPresentation();
	void RebuildPurchaseCatalogRows();
	void UnbindSelectedUnits();
	void BindSelectedUnits(const TArray<AGP_UnitBase*>& Units);
	void HandleUnitDied(AGP_UnitBase* Unit);
	void EnsureOrbitalFerroniteBinding();
	void UnbindOrbitalFerronite();
	void BindWallInventory(class AGP_MainBase* MainBase);
	void UnbindWallInventory();
	void HandleOrbitalFerroniteChanged(const FOnAttributeChangeData& Data);
	void RefreshPurchaseCatalogIfCategoryActive();
	void ClearPurchaseSession();
	void ClearSelectedPurchaseItem();
	void RebuildSelectedPurchaseRow();
	bool TryAddUnitToManifest(FPrimaryAssetId ItemId);
	bool TryRemoveUnitFromManifest(FPrimaryAssetId ItemId);
	void SelectPurchaseItem(FPrimaryAssetId ItemId);
	void LaunchSelectedBuildingOrDefense();
	void EnsureBuildingInventoryBinding();
	void UnbindBuildingInventory();
	void HandleBuildingReadyChanged(FPrimaryAssetId DropDefinitionId, int32 NewCount);
	FGP_PurchaseUnitManifestPresentation BuildUnitManifestPresentation() const;
	float GetLocalOrbitalFerronite() const;
	static int32 GetShuttleSlotCapacity();

	UFUNCTION()
	void HandleBoundActorDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleWallInventoryChanged(int32 NewCount);

	UFUNCTION()
	void HandleWallPackagePendingChanged(bool bPending);

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
	void SetPanelState(EGP_ContextActionPanelState NewState);
	static bool IsPurchaseCategoryState(EGP_ContextActionPanelState State);
	static bool IsPurchaseSelectedState(EGP_ContextActionPanelState State);
	static bool IsPurchasePresentationState(EGP_ContextActionPanelState State);
	static EGP_ContextActionPanelState PanelStateForCategory(EGP_PurchaseCategory Category);

	TWeakObjectPtr<AGP_PlayerController> BoundPlayerController;
	TWeakObjectPtr<UGP_SelectionComponent> BoundSelection;
	FDelegateHandle SelectionChangedHandle;
	FDelegateHandle CommandTargetingChangedHandle;
	TArray<FBoundSelectedUnit> BoundUnits;
	TArray<FGP_ContextActionPresentation> Actions;
	TArray<FGP_PurchaseCatalogRow> PurchaseCatalogRows;
	FGP_PurchaseCatalogRow SelectedPurchaseRow;
	FGP_UnitDropManifest PendingUnitManifest;
	FPrimaryAssetId SelectedPurchaseItemId;
	EGP_PurchaseCatalogItemKind SelectedPurchaseItemKind = EGP_PurchaseCatalogItemKind::Unit;
	FPrimaryAssetId PendingAutoDeployItemId;
	int32 PendingAutoDeployReadyBefore = 0;
	FText PurchaseContextMessage;
	TWeakObjectPtr<UAbilitySystemComponent> BoundOrbitalASC;
	TWeakObjectPtr<UGP_WallSegmentInventoryComponent> BoundWallInventory;
	TWeakObjectPtr<class UGP_OrbitalBuildingInventoryComponent> BoundBuildingInventory;
	FDelegateHandle OrbitalFerroniteHandle;
	FDelegateHandle BuildingReadyChangedHandle;
	EGP_ContextActionMode Mode = EGP_ContextActionMode::None;
	EGP_ContextActionPanelState PanelState = EGP_ContextActionPanelState::Actions;
#if !UE_BUILD_SHIPPING
	int32 DebugUnitDropRequestCount = 0;
	FGP_UnitDropManifest DebugLastRequestedUnitManifest;
	int32 DebugBuildingPurchaseRequestCount = 0;
	FPrimaryAssetId DebugLastRequestedBuildingPurchaseId;
	int32 DebugWallPackagePurchaseRequestCount = 0;
#endif
};
