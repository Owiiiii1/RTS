// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/GPMinimapPresenter.h"

#include "Camera/GPCameraPawn.h"
#include "FogOfWar/GPLocalFoWComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/GPPlayerController.h"
#include "Presentation/GPLocalFoWUnitPresentationSubsystem.h"
#include "SceneView.h"
#include "Units/GPUnitBase.h"

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

	static bool BlipsEqual(const TArray<FGP_MinimapBlip>& A, const TArray<FGP_MinimapBlip>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			const FGP_MinimapBlip& Left = A[Index];
			const FGP_MinimapBlip& Right = B[Index];
			if (Left.Kind != Right.Kind
				|| Left.TeamId != Right.TeamId
				|| Left.SourceActor.Get() != Right.SourceActor.Get()
				|| !Left.NormalizedPosition.Equals(Right.NormalizedPosition, 0.0001f))
			{
				return false;
			}
		}

		return true;
	}

	static bool CameraFootprintsEqual(
		const FGP_MinimapCameraFootprint& A,
		const FGP_MinimapCameraFootprint& B)
	{
		if (A.bIsValid != B.bIsValid || A.NormalizedCorners.Num() != B.NormalizedCorners.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.NormalizedCorners.Num(); ++Index)
		{
			if (!A.NormalizedCorners[Index].Equals(B.NormalizedCorners[Index], 0.0001f))
			{
				return false;
			}
		}

		return true;
	}

	enum class EUnitSquareEdge : uint8
	{
		Left,
		Right,
		Bottom,
		Top
	};

	static bool IsInsideUnitSquare(const FVector2D& Point, EUnitSquareEdge Edge)
	{
		switch (Edge)
		{
		case EUnitSquareEdge::Left:
			return Point.X >= 0.0f;
		case EUnitSquareEdge::Right:
			return Point.X <= 1.0f;
		case EUnitSquareEdge::Bottom:
			return Point.Y >= 0.0f;
		case EUnitSquareEdge::Top:
			return Point.Y <= 1.0f;
		default:
			return false;
		}
	}

	static FVector2D IntersectUnitSquareEdge(
		const FVector2D& Start,
		const FVector2D& End,
		EUnitSquareEdge Edge)
	{
		const FVector2D Delta = End - Start;
		switch (Edge)
		{
		case EUnitSquareEdge::Left:
		{
			const float T = FMath::IsNearlyZero(Delta.X) ? 0.0f : (0.0f - Start.X) / Delta.X;
			return FVector2D(0.0f, Start.Y + T * Delta.Y);
		}
		case EUnitSquareEdge::Right:
		{
			const float T = FMath::IsNearlyZero(Delta.X) ? 0.0f : (1.0f - Start.X) / Delta.X;
			return FVector2D(1.0f, Start.Y + T * Delta.Y);
		}
		case EUnitSquareEdge::Bottom:
		{
			const float T = FMath::IsNearlyZero(Delta.Y) ? 0.0f : (0.0f - Start.Y) / Delta.Y;
			return FVector2D(Start.X + T * Delta.X, 0.0f);
		}
		case EUnitSquareEdge::Top:
		{
			const float T = FMath::IsNearlyZero(Delta.Y) ? 0.0f : (1.0f - Start.Y) / Delta.Y;
			return FVector2D(Start.X + T * Delta.X, 1.0f);
		}
		default:
			return Start;
		}
	}

	static void ClipPolygonAgainstUnitSquareEdge(
		const TArray<FVector2D>& InPolygon,
		TArray<FVector2D>& OutPolygon,
		EUnitSquareEdge Edge)
	{
		OutPolygon.Reset();
		if (InPolygon.Num() == 0)
		{
			return;
		}

		FVector2D Previous = InPolygon.Last();
		for (const FVector2D& Current : InPolygon)
		{
			const bool bCurrentInside = IsInsideUnitSquare(Current, Edge);
			const bool bPreviousInside = IsInsideUnitSquare(Previous, Edge);
			if (bCurrentInside)
			{
				if (!bPreviousInside)
				{
					OutPolygon.Add(IntersectUnitSquareEdge(Previous, Current, Edge));
				}
				OutPolygon.Add(Current);
			}
			else if (bPreviousInside)
			{
				OutPolygon.Add(IntersectUnitSquareEdge(Previous, Current, Edge));
			}

			Previous = Current;
		}
	}

	static bool ClipConvexPolygonToUnitSquare(TArray<FVector2D>& Polygon)
	{
		TArray<FVector2D> Temp;
		ClipPolygonAgainstUnitSquareEdge(Polygon, Temp, EUnitSquareEdge::Left);
		ClipPolygonAgainstUnitSquareEdge(Temp, Polygon, EUnitSquareEdge::Right);
		ClipPolygonAgainstUnitSquareEdge(Polygon, Temp, EUnitSquareEdge::Bottom);
		ClipPolygonAgainstUnitSquareEdge(Temp, Polygon, EUnitSquareEdge::Top);

		for (const FVector2D& Point : Polygon)
		{
			if (!IsFiniteVector2D(Point))
			{
				Polygon.Reset();
				return false;
			}
		}

		return Polygon.Num() >= 3;
	}

	static bool TryIntersectRayWithHorizontalPlane(
		const FVector& Origin,
		const FVector& Direction,
		float PlaneZ,
		FVector& OutPoint)
	{
		if (!IsFiniteVector(Origin)
			|| !IsFiniteVector(Direction)
			|| !FMath::IsFinite(PlaneZ)
			|| FMath::Abs(Direction.Z) <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float T = (PlaneZ - Origin.Z) / Direction.Z;
		if (T <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		OutPoint = Origin + Direction * T;
		return IsFiniteVector(OutPoint);
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
	BindPlayerController(InPlayerController);
	BindCameraPawn(CameraPawn);
	BindUnitRegistry(IsValid(InPlayerController) ? InPlayerController->GetWorld() : nullptr);
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
	BindUnitRegistry(Mirror->GetWorld());
	RebuildPresentation(true);
	return true;
}

void UGP_MinimapPresenter::Shutdown()
{
	UnbindUnitRegistry();
	UnbindCameraPawn();
	UnbindPlayerController();
	UnbindMirror();
	bHasExplicitDisplayedBounds = false;
	ExplicitDisplayedBounds = FBox(ForceInit);
	bHasViewportSizeOverride = false;
	ViewportSizeOverride = FIntPoint::ZeroValue;
	Blips.Reset();
	BlipRevision = 0;
	CameraFootprint = FGP_MinimapCameraFootprint();
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

int32 UGP_MinimapPresenter::GetBoundCameraPresentationDelegateCount() const
{
	return CameraPresentationChangedHandle.IsValid() ? 1 : 0;
}

int32 UGP_MinimapPresenter::GetBoundUnitRegistryDelegateCount() const
{
	int32 Count = 0;
	if (UnitRegistryChangedHandle.IsValid())
	{
		++Count;
	}
	if (RegisteredUnitsEvaluatedHandle.IsValid())
	{
		++Count;
	}
	return Count;
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

bool UGP_MinimapPresenter::TryWorldToMinimapNormalizedUnclamped(
	const FVector& WorldLocation,
	FVector2D& OutNormalized) const
{
	if (!HasUsableBounds() || !GPMinimapPresenterPrivate::IsFiniteVector(WorldLocation))
	{
		return false;
	}

	const FVector2D WorldSize = GetDisplayedWorldSizeCm();
	const float NormalizedX =
		(WorldLocation.X - Presentation.MapWorldMin.X) / WorldSize.X;
	const float NormalizedY =
		(WorldLocation.Y - Presentation.MapWorldMin.Y) / WorldSize.Y;
	if (NormalizedX < 0.0f || NormalizedX > 1.0f || NormalizedY < 0.0f || NormalizedY > 1.0f)
	{
		return false;
	}

	OutNormalized = FVector2D(NormalizedX, NormalizedY);
	return true;
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

bool UGP_MinimapPresenter::PanCameraToMinimapNormalized(const FVector2D& PresenterNormalized)
{
	if (!IsMinimapReady()
		|| !HasUsableBounds()
		|| !GPMinimapPresenterPrivate::IsFiniteVector2D(PresenterNormalized)
		|| PresenterNormalized.X < 0.0f
		|| PresenterNormalized.X > 1.0f
		|| PresenterNormalized.Y < 0.0f
		|| PresenterNormalized.Y > 1.0f)
	{
		return false;
	}

	AGP_CameraPawn* CameraPawn = BoundCameraPawn.Get();
	if (!IsValid(CameraPawn))
	{
		return false;
	}

	const FVector World = MinimapNormalizedToWorld(
		PresenterNormalized,
		CameraPawn->GetGroundReferencePlaneZ());
	if (!GPMinimapPresenterPrivate::IsFiniteVector(World))
	{
		return false;
	}

	return CameraPawn->SetCameraAnchorWorldXY(FVector2D(World.X, World.Y));
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
	const bool bMetadataChanged =
		!GPMinimapPresenterPrivate::PresentationsEqual(Presentation, NewPresentation);
	if (bMetadataChanged)
	{
		Presentation = NewPresentation;
		if (bBroadcast)
		{
			OnMinimapPresentationChanged.Broadcast();
		}
	}

	RebuildBlips(bBroadcast);
	RebuildCameraFootprint(bBroadcast);
}

void UGP_MinimapPresenter::RebuildBlips(bool bBroadcast)
{
	TArray<FGP_MinimapBlip> NewBlips;
	const int32 LocalTeamId = Presentation.LocalTeamId;
	const UGP_LocalFoWComponent* Mirror = BoundMirror.Get();
	if (Presentation.bIsReady && LocalTeamId >= 1)
	{
		if (UGP_LocalFoWUnitPresentationSubsystem* Registry = BoundUnitRegistry.Get())
		{
			Registry->ForEachRegisteredUnit(
				[this, LocalTeamId, Mirror, &NewBlips](AGP_UnitBase* Unit)
				{
					if (!IsValid(Unit))
					{
						return;
					}

					const int32 UnitTeamId = Unit->GetTeamId();
					if (UnitTeamId < 1)
					{
						return;
					}

					if (!Unit->IsSelectionTypeUnit() && !Unit->IsSelectionTypeBuilding())
					{
						return;
					}

					if (!UGP_LocalFoWUnitPresentationSubsystem::ShouldPresentUnitForLocalPlayer(
							Unit,
							LocalTeamId,
							Mirror))
					{
						return;
					}

					FVector2D Normalized = FVector2D::ZeroVector;
					if (!TryWorldToMinimapNormalizedUnclamped(Unit->GetActorLocation(), Normalized))
					{
						return;
					}

					FGP_MinimapBlip Blip;
					Blip.NormalizedPosition = Normalized;
					Blip.Kind = Unit->IsSelectionTypeBuilding()
						? EGP_MinimapBlipKind::Building
						: EGP_MinimapBlipKind::Unit;
					Blip.TeamId = UnitTeamId;
					Blip.SourceActor = Unit;
					NewBlips.Add(Blip);
				});
		}
	}

	if (GPMinimapPresenterPrivate::BlipsEqual(Blips, NewBlips))
	{
		return;
	}

	Blips = MoveTemp(NewBlips);
	++BlipRevision;
	if (bBroadcast)
	{
		OnMinimapBlipsChanged.Broadcast();
	}
}

void UGP_MinimapPresenter::RebuildCameraFootprint(bool bBroadcast)
{
	FGP_MinimapCameraFootprint NewFootprint;
	if (!TryBuildCameraFootprint(NewFootprint))
	{
		NewFootprint.bIsValid = false;
		NewFootprint.NormalizedCorners.Reset();
	}

	if (GPMinimapPresenterPrivate::CameraFootprintsEqual(CameraFootprint, NewFootprint))
	{
		return;
	}

	NewFootprint.Revision = CameraFootprint.Revision + 1;
	CameraFootprint = MoveTemp(NewFootprint);
	if (bBroadcast)
	{
		OnMinimapCameraFootprintChanged.Broadcast();
	}
}

bool UGP_MinimapPresenter::TryBuildCameraFootprint(FGP_MinimapCameraFootprint& OutFootprint) const
{
	OutFootprint = FGP_MinimapCameraFootprint();
	TArray<FVector2D> NormalizedCorners;
	if (!TryCollectUnclampedNormalizedCorners(NormalizedCorners)
		|| !GPMinimapPresenterPrivate::ClipConvexPolygonToUnitSquare(NormalizedCorners))
	{
		return false;
	}

	OutFootprint.bIsValid = true;
	OutFootprint.NormalizedCorners = MoveTemp(NormalizedCorners);
	return true;
}

bool UGP_MinimapPresenter::TryCollectUnclampedNormalizedCorners(TArray<FVector2D>& OutCorners) const
{
	OutCorners.Reset();
	if (!HasUsableBounds())
	{
		return false;
	}

	AGP_PlayerController* PlayerController = BoundPlayerController.Get();
	AGP_CameraPawn* CameraPawn = BoundCameraPawn.Get();
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController() || !IsValid(CameraPawn))
	{
		return false;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	if (bHasViewportSizeOverride)
	{
		ViewportX = ViewportSizeOverride.X;
		ViewportY = ViewportSizeOverride.Y;
	}
	else
	{
		PlayerController->GetViewportSize(ViewportX, ViewportY);
	}

	if (ViewportX < 2 || ViewportY < 2)
	{
		return false;
	}

	const float PlaneZ = CameraPawn->GetGroundReferencePlaneZ();
	const float ScreenCorners[4][2] = {
		{ 0.0f, 0.0f },
		{ static_cast<float>(ViewportX - 1), 0.0f },
		{ static_cast<float>(ViewportX - 1), static_cast<float>(ViewportY - 1) },
		{ 0.0f, static_cast<float>(ViewportY - 1) }
	};

	OutCorners.Reserve(4);
	for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
	{
		FVector WorldPoint = FVector::ZeroVector;
		if (!TryDeprojectViewportCornerToGround(
				ScreenCorners[CornerIndex][0],
				ScreenCorners[CornerIndex][1],
				ViewportX,
				ViewportY,
				PlaneZ,
				WorldPoint))
		{
			OutCorners.Reset();
			return false;
		}

		const FVector2D Normalized = WorldToMinimapNormalizedUnclamped(WorldPoint);
		if (!GPMinimapPresenterPrivate::IsFiniteVector2D(Normalized))
		{
			OutCorners.Reset();
			return false;
		}

		OutCorners.Add(Normalized);
	}

	return OutCorners.Num() == 4;
}

bool UGP_MinimapPresenter::TryDeprojectViewportCornerToGround(
	float ScreenX,
	float ScreenY,
	int32 SizeX,
	int32 SizeY,
	float PlaneZ,
	FVector& OutWorld) const
{
	OutWorld = FVector::ZeroVector;
	if (SizeX < 2
		|| SizeY < 2
		|| !FMath::IsFinite(ScreenX)
		|| !FMath::IsFinite(ScreenY)
		|| !FMath::IsFinite(PlaneZ))
	{
		return false;
	}

	AGP_CameraPawn* CameraPawn = BoundCameraPawn.Get();
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	float FieldOfView = 90.0f;
	if (!IsValid(CameraPawn)
		|| !CameraPawn->GetPresentationView(ViewLocation, ViewRotation, FieldOfView))
	{
		return false;
	}

	FMinimalViewInfo ViewInfo;
	ViewInfo.Location = ViewLocation;
	ViewInfo.Rotation = ViewRotation;
	ViewInfo.FOV = FieldOfView;
	ViewInfo.AspectRatio = static_cast<float>(SizeX) / static_cast<float>(SizeY);
	ViewInfo.bConstrainAspectRatio = false;
	ViewInfo.ProjectionMode = ECameraProjectionMode::Perspective;

	FMatrix ViewMatrix = FMatrix::Identity;
	FMatrix ProjectionMatrix = FMatrix::Identity;
	FMatrix ViewProjectionMatrix = FMatrix::Identity;
	UGameplayStatics::GetViewProjectionMatrix(
		ViewInfo,
		ViewMatrix,
		ProjectionMatrix,
		ViewProjectionMatrix);

	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
	FSceneView::DeprojectScreenToWorld(
		FVector2D(ScreenX, ScreenY),
		FIntRect(0, 0, SizeX, SizeY),
		ViewProjectionMatrix.Inverse(),
		Origin,
		Direction);

	return GPMinimapPresenterPrivate::TryIntersectRayWithHorizontalPlane(
		Origin,
		Direction,
		PlaneZ,
		OutWorld);
}

FVector2D UGP_MinimapPresenter::WorldToMinimapNormalizedUnclamped(const FVector& WorldLocation) const
{
	if (!HasUsableBounds() || !GPMinimapPresenterPrivate::IsFiniteVector(WorldLocation))
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D WorldSize = GetDisplayedWorldSizeCm();
	return FVector2D(
		(WorldLocation.X - Presentation.MapWorldMin.X) / WorldSize.X,
		(WorldLocation.Y - Presentation.MapWorldMin.Y) / WorldSize.Y);
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

void UGP_MinimapPresenter::HandleCameraPresentationChanged()
{
	RebuildCameraFootprint(true);
}

void UGP_MinimapPresenter::HandleUnitRegistryChanged()
{
	RebuildBlips(true);
}

void UGP_MinimapPresenter::HandleRegisteredUnitsEvaluated()
{
	RebuildBlips(true);
}

void UGP_MinimapPresenter::BindCameraPawn(AGP_CameraPawn* CameraPawn)
{
	if (IsValid(CameraPawn)
		&& BoundCameraPawn.Get() == CameraPawn
		&& CameraBoundsChangedHandle.IsValid()
		&& CameraPresentationChangedHandle.IsValid())
	{
		return;
	}

	UnbindCameraPawn();
	if (!IsValid(CameraPawn))
	{
		RebuildCameraFootprint(true);
		return;
	}

	BoundCameraPawn = CameraPawn;
	CameraBoundsChangedHandle = CameraPawn->OnResolvedCameraBoundsChanged.AddUObject(
		this,
		&ThisClass::HandleResolvedCameraBoundsChanged);
	CameraPresentationChangedHandle = CameraPawn->OnCameraPresentationChanged.AddUObject(
		this,
		&ThisClass::HandleCameraPresentationChanged);
	RebuildCameraFootprint(false);
}

void UGP_MinimapPresenter::UnbindCameraPawn()
{
	if (AGP_CameraPawn* CameraPawn = BoundCameraPawn.Get())
	{
		if (CameraBoundsChangedHandle.IsValid())
		{
			CameraPawn->OnResolvedCameraBoundsChanged.Remove(CameraBoundsChangedHandle);
		}
		if (CameraPresentationChangedHandle.IsValid())
		{
			CameraPawn->OnCameraPresentationChanged.Remove(CameraPresentationChangedHandle);
		}
	}
	CameraBoundsChangedHandle.Reset();
	CameraPresentationChangedHandle.Reset();
	BoundCameraPawn.Reset();
}

void UGP_MinimapPresenter::BindPlayerController(AGP_PlayerController* InPlayerController)
{
	BoundPlayerController = IsValid(InPlayerController) ? InPlayerController : nullptr;
}

void UGP_MinimapPresenter::UnbindPlayerController()
{
	BoundPlayerController.Reset();
}

void UGP_MinimapPresenter::BindUnitRegistry(UWorld* World)
{
	if (BoundUnitRegistry.IsValid()
		&& UnitRegistryChangedHandle.IsValid()
		&& RegisteredUnitsEvaluatedHandle.IsValid()
		&& BoundUnitRegistry->GetWorld() == World)
	{
		return;
	}

	UnbindUnitRegistry();
	if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UGP_LocalFoWUnitPresentationSubsystem* Registry =
		World->GetSubsystem<UGP_LocalFoWUnitPresentationSubsystem>();
	if (Registry == nullptr)
	{
		return;
	}

	BoundUnitRegistry = Registry;
	UnitRegistryChangedHandle = Registry->OnUnitRegistryChanged.AddUObject(
		this,
		&ThisClass::HandleUnitRegistryChanged);
	RegisteredUnitsEvaluatedHandle = Registry->OnRegisteredUnitsEvaluated.AddUObject(
		this,
		&ThisClass::HandleRegisteredUnitsEvaluated);
	RebuildBlips(false);
}

void UGP_MinimapPresenter::UnbindUnitRegistry()
{
	if (UGP_LocalFoWUnitPresentationSubsystem* Registry = BoundUnitRegistry.Get())
	{
		if (UnitRegistryChangedHandle.IsValid())
		{
			Registry->OnUnitRegistryChanged.Remove(UnitRegistryChangedHandle);
		}
		if (RegisteredUnitsEvaluatedHandle.IsValid())
		{
			Registry->OnRegisteredUnitsEvaluated.Remove(RegisteredUnitsEvaluatedHandle);
		}
	}

	UnitRegistryChangedHandle.Reset();
	RegisteredUnitsEvaluatedHandle.Reset();
	BoundUnitRegistry.Reset();
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

void UGP_MinimapPresenter::ContractRebuildFriendlyBlips()
{
	RebuildBlips(true);
}

void UGP_MinimapPresenter::ContractBindUnitRegistry(UWorld* World)
{
	BindUnitRegistry(World);
}

void UGP_MinimapPresenter::ContractRebuildCameraFootprint()
{
	RebuildCameraFootprint(true);
}

void UGP_MinimapPresenter::ContractSetViewportSizeOverride(int32 SizeX, int32 SizeY)
{
	bHasViewportSizeOverride = true;
	ViewportSizeOverride = FIntPoint(SizeX, SizeY);
	RebuildCameraFootprint(true);
}

void UGP_MinimapPresenter::ContractClearViewportSizeOverride()
{
	bHasViewportSizeOverride = false;
	ViewportSizeOverride = FIntPoint::ZeroValue;
	RebuildCameraFootprint(true);
}

bool UGP_MinimapPresenter::ContractTryGetUnclampedNormalizedCorners(TArray<FVector2D>& OutCorners) const
{
	return TryCollectUnclampedNormalizedCorners(OutCorners);
}

const FGP_MinimapBlip* UGP_MinimapPresenter::ContractFindFriendlyBlipForActor(
	const AGP_UnitBase* Unit) const
{
	if (Unit == nullptr)
	{
		return nullptr;
	}

	for (const FGP_MinimapBlip& Blip : Blips)
	{
		if (Blip.SourceActor.Get() == Unit)
		{
			return &Blip;
		}
	}

	return nullptr;
}

const FGP_MinimapBlip* UGP_MinimapPresenter::ContractFindBlipForActor(
	const AGP_UnitBase* Unit) const
{
	return ContractFindFriendlyBlipForActor(Unit);
}
#endif
