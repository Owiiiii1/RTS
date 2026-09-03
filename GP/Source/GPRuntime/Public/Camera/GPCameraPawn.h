// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GPCameraPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class USpringArmComponent;
class UGP_CameraConfigDataAsset;
class AGP_CameraBoundsVolume;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE(FOnGPResolvedCameraBoundsChanged);
DECLARE_MULTICAST_DELEGATE(FOnGPCameraPresentationChanged);

/**
 * Local RTS camera pawn: pan / edge-scroll / zoom / pitch / yaw.
 * Presentation only — no replication, RPC, GAS, selection, or input binding.
 */
UCLASS()
class GPRUNTIME_API AGP_CameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AGP_CameraPawn();
	virtual ~AGP_CameraPawn() override;

	virtual void Tick(float DeltaSeconds) override;

	void SetPanInput(const FVector2D& AxisXY);
	void AddZoomInput(float WheelDelta);
	void AddRotateInput(float MouseDeltaX);
	void SetRotateActive(bool bActive);

	/** Same FBox ClampToBounds uses: valid volume AABB, else Config.FallbackBounds. */
	bool GetResolvedCameraBounds(FBox& OutBounds) const;
	static bool IsUsableResolvedCameraBounds(const FBox& Bounds);

	/** XY-anchor / pan plane Z. Pan preserves this height; not terrain sampling. */
	float GetGroundReferencePlaneZ() const;

	/** Camera component view used by the minimap footprint (not the pawn root). */
	bool GetPresentationView(FVector& OutLocation, FRotator& OutRotation, float& OutFOV) const;

	FOnGPResolvedCameraBoundsChanged OnResolvedCameraBoundsChanged;
	FOnGPCameraPresentationChanged OnCameraPresentationChanged;

#if !UE_BUILD_SHIPPING
	void ContractSetCameraBoundsVolume(AGP_CameraBoundsVolume* Volume);
	void ContractClearCameraBoundsVolume();
	void ContractNotifyCameraPresentationChanged();
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BeginLoadConfig();
	void HandleConfigLoaded();

	const UGP_CameraConfigDataAsset* GetActiveConfig() const;

	FVector2D CalculateEdgeScrollInput() const;
	float CalculateEdgeStrength(float DistanceToEdge, int32 ThresholdPx, int32 FalloffPx) const;
	float CalculateZoomFraction(const UGP_CameraConfigDataAsset& Config, float ArmLength) const;

	void ApplyZoom(const UGP_CameraConfigDataAsset& Config, float DeltaSeconds);
	void ApplyPitch(const UGP_CameraConfigDataAsset& Config);
	void ApplyRotation(const UGP_CameraConfigDataAsset& Config);
	void ApplyPan(const UGP_CameraConfigDataAsset& Config, const FVector2D& EdgeInput, float DeltaSeconds);
	void ClampToBounds(const UGP_CameraConfigDataAsset& Config);
	void ResetFrameInput();
	void SyncSpringArmPresentation(float DeltaSeconds);

	void FindCameraBoundsVolume();
	FBox ResolveCameraBounds(const UGP_CameraConfigDataAsset& Config) const;
	void NotifyResolvedCameraBoundsChanged();
	void NotifyCameraPresentationChangedIfNeeded();
	bool CaptureCameraPresentationFingerprint(
		FVector& OutActorLocation,
		float& OutYaw,
		float& OutArmLength,
		float& OutPitch,
		FVector& OutCameraLocation,
		FRotator& OutCameraRotation,
		int32& OutViewportX,
		int32& OutViewportY) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GP|Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GP|Camera", meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<UGP_CameraConfigDataAsset> ConfigRef;

	UPROPERTY(Transient)
	TObjectPtr<UGP_CameraConfigDataAsset> CachedConfig;

	TSharedPtr<FStreamableHandle> ConfigLoadHandle;

	FVector2D PendingPanInput = FVector2D::ZeroVector;
	FVector2D SmoothedPanInput = FVector2D::ZeroVector;
	float PendingZoomInput = 0.0f;
	float PendingRotateInput = 0.0f;
	bool bRotateActive = false;

	float CurrentArmLength = 0.0f;
	float TargetArmLength = 0.0f;
	float CurrentYaw = 0.0f;

	TWeakObjectPtr<AGP_CameraBoundsVolume> CameraBoundsVolume;
	mutable bool bInvalidCameraBoundsWarningLogged = false;

	bool bHasCameraPresentationFingerprint = false;
	FVector LastPresentationActorLocation = FVector::ZeroVector;
	FVector LastPresentationCameraLocation = FVector::ZeroVector;
	FRotator LastPresentationCameraRotation = FRotator::ZeroRotator;
	float LastPresentationYaw = 0.0f;
	float LastPresentationArmLength = 0.0f;
	float LastPresentationPitch = 0.0f;
	int32 LastPresentationViewportX = 0;
	int32 LastPresentationViewportY = 0;
};
