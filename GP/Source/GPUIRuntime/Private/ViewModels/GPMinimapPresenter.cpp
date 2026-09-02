// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPMinimapPresenter.h"

#include "Camera/GPCameraPawn.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "Player/GPPlayerController.h"

namespace GPMinimapPresenterPrivate
{
	static bool PresentationsEqual(const FGP_MinimapPresentation& A, const FGP_MinimapPresentation& B)
	{
		return A.bIsReady == B.bIsReady
			&& A.LocalTeamId == B.LocalTeamId
			&& A.Revision == B.Revision
			&& A.GridDimensions == B.GridDimensions
			&& A.WorldOrigin.Equals(B.WorldOrigin, KINDA_SMALL_NUMBER)
			&& A.GridOrigin.Equals(B.GridOrigin, KINDA_SMALL_NUMBER)
			&& A.WorldSizeCm.Equals(B.WorldSizeCm, KINDA_SMALL_NUMBER)
			&& A.MapWorldMin.Equals(B.MapWorldMin, KINDA_SMALL_NUMBER)
			&& A.MapWorldSizeCm.Equals(B.MapWorldSizeCm, KINDA_SMALL_NUMBER)
			&& FMath::IsNearlyEqual(A.CellSizeCm, B.CellSizeCm);
	}

	static bool IsFiniteVector(const FVector& Location)
	{
		return !Location.ContainsNaN()
			&& FMath::IsFinite(Location.X)
			&& FMath::IsFinite(Location.Y)
			&& FMath::IsFinite(Location.Z);
	}

	static bool IsFiniteVector2D(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}

	static bool HasUsableDisplayedSize(const FVector2D& Size)
	{
		return FMath::IsFinite(Size.X)
			&& FMath::IsFinite(Size.Y)
			&& Size.X > KINDA_SMALL_NUMBER
			&& Size.Y > KINDA_SMALL_NUMBER;
	}
}

bool UGP_MinimapPresenter::Initialize(AGP_PlayerController* InPlayerController)
{
	UGP_LocalFoWComponent* Mirror =
		IsValid(InPlayerController) && InPlayerController->IsLocalController()
			? InPlayerController->GetLocalFogOfWarComponent()
			: nullptr;
	AGP_CameraPawn* CameraPawn =
		IsValid(InPlayerController) ? Cast<AGP_CameraPawn>(InPlayerController->GetPawn()) : nullptr;
	const bool bMirrorBound = InitializeWithMirror(Mirror);
	BindCameraPawn(CameraPawn);
	RebuildPresentation(true);
	return bMirrorBound;
}

bool UGP_MinimapPresenter::InitializeWithMirror(UGP_LocalFoWComponent* Mirror)
{
	Shutdown();
	if (!IsValid(Mirror))
	{
		return false;
	}

	BoundMirror = Mirror;
	MirrorUpdatedHandle = Mirror->OnLocalFoWUpdated.AddUObject(
		this,
		&ThisClass::HandleMirrorUpdated);
	RebuildPresentation(true);
	return true;
}

void UGP_MinimapPresenter::Shutdown()
{
	UnbindCameraPawn();
	UnbindMirror();
	bHasExplicitDisplayedBounds = false;
	ExplicitDisplayedBounds = FBox(ForceInit);
	RebuildPresentation(true);
}

void UGP_MinimapPresenter::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

int32 UGP_MinimapPresenter::GetBoundDelegateCount() const
{
	return MirrorUpdatedHandle.IsValid() ? 1 : 0;
}

int32 UGP_MinimapPresenter::GetBoundCameraBoundsDelegateCount() const
{
	return CameraBoundsChangedHandle.IsValid() ? 1 : 0;
}

FVector2D UGP_MinimapPresenter::WorldToMinimapNormalized(const FVector& WorldLocation) const
{
	if (!HasUsableBounds() || !GPMinimapPresenterPrivate::IsFiniteVector(WorldLocation))
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D WorldSize = GetDisplayedWorldSizeCm();
	const float NormalizedX =
		(WorldLocation.X - Presentation.MapWorldMin.X) / WorldSize.X;
	const float NormalizedY =
		(WorldLocation.Y - Presentation.MapWorldMin.Y) / WorldSize.Y;
	return FVector2D(
		FMath::Clamp(NormalizedX, 0.0f, 1.0f),
		FMath::Clamp(NormalizedY, 0.0f, 1.0f));
}

FVector UGP_MinimapPresenter::MinimapNormalizedToWorld(
	const FVector2D& Normalized,
	float WorldZ) const
{
	const float SafeZ = FMath::IsFinite(WorldZ) ? WorldZ : 0.0f;
	if (!HasUsableBounds() || !GPMinimapPresenterPrivate::IsFiniteVector2D(Normalized))
	{
		return FVector(0.0f, 0.0f, SafeZ);
	}

	const float ClampedX = FMath::Clamp(Normalized.X, 0.0f, 1.0f);
	const float ClampedY = FMath::Clamp(Normalized.Y, 0.0f, 1.0f);
	const FVector2D WorldSize = GetDisplayedWorldSizeCm();
	return FVector(
		Presentation.MapWorldMin.X + ClampedX * WorldSize.X,
		Presentation.MapWorldMin.Y + ClampedY * WorldSize.Y,
		SafeZ);
}

EGP_FoWState UGP_MinimapPresenter::GetMinimapFoWStateNormalized(const FVector2D& Normalized) const
{
	const UGP_LocalFoWComponent* Mirror = BoundMirror.Get();
	if (!Presentation.bIsReady
		|| Mirror == nullptr
		|| !GPMinimapPresenterPrivate::IsFiniteVector2D(Normalized)
		|| Normalized.X < 0.0f
		|| Normalized.X > 1.0f
		|| Normalized.Y < 0.0f
		|| Normalized.Y > 1.0f)
	{
		return EGP_FoWState::Unexplored;
	}

	return Mirror->GetStateAtWorldLocation(ResolveFoWQueryWorldLocation(Normalized));
}

void UGP_MinimapPresenter::HandleMirrorUpdated(UGP_LocalFoWComponent* Mirror)
{
	if (Mirror == nullptr || Mirror != BoundMirror.Get())
	{
		return;
	}

	RebuildPresentation(true);
}

void UGP_MinimapPresenter::RebuildPresentation(bool bBroadcast)
{
	const FGP_MinimapPresentation NewPresentation =
		BuildPresentationFromMirror(BoundMirror.Get());
	if (GPMinimapPresenterPrivate::PresentationsEqual(Presentation, NewPresentation))
	{
		return;
	}

	Presentation = NewPresentation;
	if (bBroadcast)
	{
		OnMinimapPresentationChanged.Broadcast();
	}
}

FGP_MinimapPresentation UGP_MinimapPresenter::BuildPresentationFromMirror(
	const UGP_LocalFoWComponent* Mirror) const
{
	FGP_MinimapPresentation Result;
	if (Mirror == nullptr || !Mirror->IsReady())
	{
		return Result;
	}

	const FIntPoint Dimensions = Mirror->GetGridDimensions();
	const float CellSizeCm = Mirror->GetCellSizeCm();
	if (Dimensions.X <= 0 || Dimensions.Y <= 0
		|| !FMath::IsFinite(CellSizeCm)
		|| CellSizeCm <= KINDA_SMALL_NUMBER)
	{
		return Result;
	}

	const FVector2D GridOrigin = Mirror->GetGridOriginWorldXY();
	if (!FMath::IsFinite(GridOrigin.X) || !FMath::IsFinite(GridOrigin.Y))
	{
		return Result;
	}

	Result.bIsReady = true;
	Result.LocalTeamId = Mirror->GetLocalTeamId();
	Result.GridOrigin = GridOrigin;
	Result.WorldOrigin = FVector(GridOrigin.X, GridOrigin.Y, 0.0f);
	Result.WorldSizeCm = FVector2D(
		static_cast<float>(Dimensions.X) * CellSizeCm,
		static_cast<float>(Dimensions.Y) * CellSizeCm);
	Result.GridDimensions = Dimensions;
	Result.CellSizeCm = CellSizeCm;
	Result.Revision = Mirror->GetRevision();

	FVector2D DisplayedMin = GridOrigin;
	FVector2D DisplayedSize = Result.WorldSizeCm;
	if (!TryResolveDisplayedWorldBounds(DisplayedMin, DisplayedSize)
		|| !GPMinimapPresenterPrivate::HasUsableDisplayedSize(DisplayedSize))
	{
		DisplayedMin = GridOrigin;
		DisplayedSize = Result.WorldSizeCm;
	}
	Result.MapWorldMin = DisplayedMin;
	Result.MapWorldSizeCm = DisplayedSize;
	return Result;
}

bool UGP_MinimapPresenter::TryResolveDisplayedWorldBounds(FVector2D& OutMin, FVector2D& OutSize) const
{
	auto TryBoxXY = [&OutMin, &OutSize](const FBox& Bounds) -> bool
	{
		if (!Bounds.IsValid
			|| !FMath::IsFinite(Bounds.Min.X) || !FMath::IsFinite(Bounds.Max.X)
			|| !FMath::IsFinite(Bounds.Min.Y) || !FMath::IsFinite(Bounds.Max.Y)
			|| Bounds.Min.X >= Bounds.Max.X
			|| Bounds.Min.Y >= Bounds.Max.Y)
		{
			return false;
		}

		OutMin = FVector2D(Bounds.Min.X, Bounds.Min.Y);
		OutSize = FVector2D(Bounds.Max.X - Bounds.Min.X, Bounds.Max.Y - Bounds.Min.Y);
		return GPMinimapPresenterPrivate::HasUsableDisplayedSize(OutSize);
	};

	if (bHasExplicitDisplayedBounds && TryBoxXY(ExplicitDisplayedBounds))
	{
		return true;
	}

	if (const AGP_CameraPawn* CameraPawn = BoundCameraPawn.Get())
	{
		FBox CameraBounds(ForceInit);
		if (CameraPawn->GetResolvedCameraBounds(CameraBounds) && TryBoxXY(CameraBounds))
		{
			return true;
		}
	}

	return false;
}

bool UGP_MinimapPresenter::HasUsableBounds() const
{
	return Presentation.bIsReady
		&& FMath::IsFinite(Presentation.MapWorldMin.X)
		&& FMath::IsFinite(Presentation.MapWorldMin.Y)
		&& GPMinimapPresenterPrivate::HasUsableDisplayedSize(Presentation.MapWorldSizeCm);
}

FVector2D UGP_MinimapPresenter::GetDisplayedWorldSizeCm() const
{
	return Presentation.MapWorldSizeCm;
}

FVector UGP_MinimapPresenter::ResolveFoWQueryWorldLocation(const FVector2D& Normalized) const
{
	return MinimapNormalizedToWorld(Normalized, 0.0f);
}

void UGP_MinimapPresenter::HandleResolvedCameraBoundsChanged()
{
	RebuildPresentation(true);
}

void UGP_MinimapPresenter::BindCameraPawn(AGP_CameraPawn* CameraPawn)
{
	if (IsValid(CameraPawn)
		&& BoundCameraPawn.Get() == CameraPawn
		&& CameraBoundsChangedHandle.IsValid())
	{
		return;
	}

	UnbindCameraPawn();
	if (!IsValid(CameraPawn))
	{
		return;
	}

	BoundCameraPawn = CameraPawn;
	CameraBoundsChangedHandle = CameraPawn->OnResolvedCameraBoundsChanged.AddUObject(
		this,
		&ThisClass::HandleResolvedCameraBoundsChanged);
}

void UGP_MinimapPresenter::UnbindCameraPawn()
{
	if (AGP_CameraPawn* CameraPawn = BoundCameraPawn.Get())
	{
		if (CameraBoundsChangedHandle.IsValid())
		{
			CameraPawn->OnResolvedCameraBoundsChanged.Remove(CameraBoundsChangedHandle);
		}
	}
	CameraBoundsChangedHandle.Reset();
	BoundCameraPawn.Reset();
}

void UGP_MinimapPresenter::UnbindMirror()
{
	if (UGP_LocalFoWComponent* Mirror = BoundMirror.Get())
	{
		if (MirrorUpdatedHandle.IsValid())
		{
			Mirror->OnLocalFoWUpdated.Remove(MirrorUpdatedHandle);
		}
	}
	MirrorUpdatedHandle.Reset();
	BoundMirror.Reset();
}

#if !UE_BUILD_SHIPPING
void UGP_MinimapPresenter::ContractBindCameraPawn(AGP_CameraPawn* CameraPawn)
{
	BindCameraPawn(CameraPawn);
	RebuildPresentation(true);
}

void UGP_MinimapPresenter::ContractApplyDisplayedWorldBounds(const FBox& DisplayedBounds)
{
	bHasExplicitDisplayedBounds = true;
	ExplicitDisplayedBounds = DisplayedBounds;
	RebuildPresentation(true);
}

void UGP_MinimapPresenter::ContractClearDisplayedWorldBounds()
{
	bHasExplicitDisplayedBounds = false;
	ExplicitDisplayedBounds = FBox(ForceInit);
	RebuildPresentation(true);
}
#endif
