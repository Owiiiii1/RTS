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

	FOnGPResolvedCameraBoundsChanged OnResolvedCameraBoundsChanged;

#if !UE_BUILD_SHIPPING
	void ContractSetCameraBoundsVolume(AGP_CameraBoundsVolume* Volume);
	void ContractClearCameraBoundsVolume();
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

	void FindCameraBoundsVolume();
	FBox ResolveCameraBounds(const UGP_CameraConfigDataAsset& Config) const;
	void NotifyResolvedCameraBoundsChanged();

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
};
