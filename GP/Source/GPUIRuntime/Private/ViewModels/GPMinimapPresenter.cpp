// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPMinimapPresenter.h"

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
}

bool UGP_MinimapPresenter::Initialize(AGP_PlayerController* InPlayerController)
{
	UGP_LocalFoWComponent* Mirror =
		IsValid(InPlayerController) && InPlayerController->IsLocalController()
			? InPlayerController->GetLocalFogOfWarComponent()
			: nullptr;
	return InitializeWithMirror(Mirror);
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
	UnbindMirror();
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

FVector2D UGP_MinimapPresenter::WorldToMinimapNormalized(const FVector& WorldLocation) const
{
	if (!HasUsableBounds() || !GPMinimapPresenterPrivate::IsFiniteVector(WorldLocation))
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D WorldSize = GetWorldSizeCm();
	const float NormalizedX =
		(WorldLocation.X - Presentation.GridOrigin.X) / WorldSize.X;
	const float NormalizedY =
		(WorldLocation.Y - Presentation.GridOrigin.Y) / WorldSize.Y;
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
	const FVector2D WorldSize = GetWorldSizeCm();
	return FVector(
		Presentation.GridOrigin.X + ClampedX * WorldSize.X,
		Presentation.GridOrigin.Y + ClampedY * WorldSize.Y,
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
	return Result;
}

bool UGP_MinimapPresenter::HasUsableBounds() const
{
	return Presentation.bIsReady
		&& Presentation.GridDimensions.X > 0
		&& Presentation.GridDimensions.Y > 0
		&& Presentation.WorldSizeCm.X > KINDA_SMALL_NUMBER
		&& Presentation.WorldSizeCm.Y > KINDA_SMALL_NUMBER;
}

FVector2D UGP_MinimapPresenter::GetWorldSizeCm() const
{
	return Presentation.WorldSizeCm;
}

FVector UGP_MinimapPresenter::ResolveFoWQueryWorldLocation(const FVector2D& Normalized) const
{
	const int32 DimX = Presentation.GridDimensions.X;
	const int32 DimY = Presentation.GridDimensions.Y;
	const int32 CellX = Normalized.X >= 1.0f
		? DimX - 1
		: FMath::Clamp(FMath::FloorToInt(Normalized.X * static_cast<float>(DimX)), 0, DimX - 1);
	const int32 CellY = Normalized.Y >= 1.0f
		? DimY - 1
		: FMath::Clamp(FMath::FloorToInt(Normalized.Y * static_cast<float>(DimY)), 0, DimY - 1);
	return FVector(
		Presentation.GridOrigin.X + (static_cast<float>(CellX) + 0.5f) * Presentation.CellSizeCm,
		Presentation.GridOrigin.Y + (static_cast<float>(CellY) + 0.5f) * Presentation.CellSizeCm,
		0.0f);
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
