// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GPMinimapWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TextureResource.h"
#include "Presentation/GPFoWPresentationRaster.h"
#include "Rendering/DrawElements.h"
#include "Settings/GPGameplayPresentationSettings.h"
#include "Settings/GPUIPresentationSettings.h"
#include "Styling/SlateBrush.h"
#include "ViewModels/GPHUDViewModelSubsystem.h"
#include "ViewModels/GPMinimapPresenter.h"
#include "Widgets/SLeafWidget.h"
#include "Blueprint/UserWidget.h"

namespace GPMinimapWidgetPrivate
{
	static constexpr int32 DefaultFoWResolution = 128;
	static const FLinearColor FallbackColor(0.02f, 0.02f, 0.025f, 1.0f);
	static constexpr float UnitBlipHalfExtentPx = 1.75f;
	static constexpr float BuildingBlipHalfExtentPx = 2.75f;

	static FColor OverlayColor(EGP_FoWState State)
	{
		return GPFoWPresentationRaster::OverlayColorForState(State).ToFColor(false);
	}
}

struct FGPMinimapSurfaceBlipDraw
{
	FVector2D PresenterNormalized = FVector2D::ZeroVector;
	int32 TeamId = -1;
	bool bIsBuilding = false;
};

class SGPMinimapSurface : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGPMinimapSurface) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		(void)InArgs;
		SetCanTick(false);
		SetVisibility(EVisibility::Visible);
	}

	void SetFallbackColor(const FLinearColor& InColor)
	{
		FallbackColor = InColor;
	}

	void SetBackground(const FSlateBrush& InBrush, bool bInHasTexture, const FVector2D& InTextureSize)
	{
		BackgroundBrush = InBrush;
		bHasBackgroundTexture = bInHasTexture;
		TextureSize = InTextureSize;
	}

	void SetFoW(const FSlateBrush& InBrush, bool bInHasFoW)
	{
		FoWBrush = InBrush;
		bHasFoW = bInHasFoW;
	}

	void SetBlips(const TArray<FGPMinimapSurfaceBlipDraw>& InBlips)
	{
		Blips = InBlips;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(128.0f, 128.0f);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		(void)Args;
		(void)MyCullingRect;
		(void)InWidgetStyle;
		(void)bParentEnabled;

		const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		const float Side = FMath::Max(0.0f, FMath::Min(LocalSize.X, LocalSize.Y));
		const FVector2D SquareOffset(
			(LocalSize.X - Side) * 0.5f,
			(LocalSize.Y - Side) * 0.5f);
		const FPaintGeometry SquareGeometry = AllottedGeometry.ToPaintGeometry(
			FVector2D(Side, Side),
			FSlateLayoutTransform(SquareOffset));

		const FSlateBrush FillBrush = FSlateColorBrush(FallbackColor);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			SquareGeometry,
			&FillBrush,
			ESlateDrawEffect::None,
			FallbackColor);

		const FBox2D MapDest = UGP_MinimapWidget::ComputeSharedMapDestLocal(
			LocalSize,
			bHasBackgroundTexture,
			TextureSize);
		const FVector2D DestSize = MapDest.GetSize();
		if (DestSize.X <= KINDA_SMALL_NUMBER || DestSize.Y <= KINDA_SMALL_NUMBER)
		{
			return LayerId + 1;
		}

		const FPaintGeometry MapGeometry = AllottedGeometry.ToPaintGeometry(
			DestSize,
			FSlateLayoutTransform(MapDest.Min));

		if (bHasBackgroundTexture)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				MapGeometry,
				&BackgroundBrush,
				ESlateDrawEffect::None,
				FLinearColor::White);
		}

		if (bHasFoW)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				MapGeometry,
				&FoWBrush,
				ESlateDrawEffect::None,
				FLinearColor::White);
		}

		int32 NextLayer = LayerId + 3;
		const FSlateBrush BlipBrush = FSlateColorBrush(FLinearColor::White);
		for (const FGPMinimapSurfaceBlipDraw& Blip : Blips)
		{
			const FVector2D SurfaceUV =
				UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(Blip.PresenterNormalized);
			const FVector2D Center = MapDest.Min + SurfaceUV * DestSize;
			const float HalfExtent = Blip.bIsBuilding
				? GPMinimapWidgetPrivate::BuildingBlipHalfExtentPx
				: GPMinimapWidgetPrivate::UnitBlipHalfExtentPx;
			const FVector2D BlipSize(HalfExtent * 2.0f, HalfExtent * 2.0f);
			const FVector2D BlipOffset(Center.X - HalfExtent, Center.Y - HalfExtent);
			const FPaintGeometry BlipGeometry = AllottedGeometry.ToPaintGeometry(
				BlipSize,
				FSlateLayoutTransform(BlipOffset));
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				NextLayer,
				BlipGeometry,
				&BlipBrush,
				ESlateDrawEffect::None,
				UGP_MinimapWidget::ResolveBlipColor(Blip.TeamId));
		}

		return NextLayer;
	}

private:
	FSlateBrush BackgroundBrush;
	FSlateBrush FoWBrush;
	TArray<FGPMinimapSurfaceBlipDraw> Blips;
	FLinearColor FallbackColor = GPMinimapWidgetPrivate::FallbackColor;
	FVector2D TextureSize = FVector2D::ZeroVector;
	bool bHasBackgroundTexture = false;
	bool bHasFoW = false;
};

UGP_MinimapWidget::UGP_MinimapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;
	SetVisibility(ESlateVisibility::Visible);
	BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
	FoWBrush.DrawAs = ESlateBrushDrawType::Image;
	FoWBrush.Tiling = ESlateBrushTileType::NoTile;
	BackgroundBrush.Tiling = ESlateBrushTileType::NoTile;
}

#if WITH_EDITOR
const FText UGP_MinimapWidget::GetPaletteCategory()
{
	return NSLOCTEXT("GPMinimapWidget", "PaletteCategory", "GP");
}
#endif

FVector2D UGP_MinimapWidget::PresenterNormalizedToSurfaceUV(const FVector2D& Normalized)
{
	return FVector2D(1.0f - Normalized.X, 1.0f - Normalized.Y);
}

FVector2D UGP_MinimapWidget::SurfaceUVToPresenterNormalized(const FVector2D& SurfaceUV)
{
	return FVector2D(1.0f - SurfaceUV.X, 1.0f - SurfaceUV.Y);
}

FLinearColor UGP_MinimapWidget::ResolveBlipColor(int32 TeamId)
{
	if (const UGP_GameplayPresentationSettings* Settings = UGP_GameplayPresentationSettings::Get())
	{
		return Settings->GetTeamColor(TeamId);
	}

	return FLinearColor::White;
}

float UGP_MinimapWidget::GetUnitBlipHalfExtentPx()
{
	return GPMinimapWidgetPrivate::UnitBlipHalfExtentPx;
}

float UGP_MinimapWidget::GetBuildingBlipHalfExtentPx()
{
	return GPMinimapWidgetPrivate::BuildingBlipHalfExtentPx;
}

int32 UGP_MinimapWidget::ClampFoWPresentationResolution(int32 Requested)
{
	return FMath::Clamp(Requested, 32, 256);
}

FBox2D UGP_MinimapWidget::ComputeSharedMapDestLocal(
	const FVector2D& AllottedSize,
	bool bHasTexture,
	const FVector2D& TextureSize)
{
	const float Side = FMath::Max(0.0f, FMath::Min(AllottedSize.X, AllottedSize.Y));
	const FVector2D SquareOffset(
		(AllottedSize.X - Side) * 0.5f,
		(AllottedSize.Y - Side) * 0.5f);

	FVector2D DrawSize(Side, Side);
	if (bHasTexture && TextureSize.X > KINDA_SMALL_NUMBER && TextureSize.Y > KINDA_SMALL_NUMBER)
	{
		const float Aspect = TextureSize.X / TextureSize.Y;
		if (Aspect > 1.0f)
		{
			DrawSize.X = Side;
			DrawSize.Y = Side / Aspect;
		}
		else
		{
			DrawSize.Y = Side;
			DrawSize.X = Side * Aspect;
		}
	}

	const FVector2D DrawOffset(
		SquareOffset.X + (Side - DrawSize.X) * 0.5f,
		SquareOffset.Y + (Side - DrawSize.Y) * 0.5f);
	return FBox2D(DrawOffset, DrawOffset + DrawSize);
}

void UGP_MinimapWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	FoWResolution = ResolveConfiguredFoWResolution();
	RequestBackgroundLoad();
	BindPresenter(ResolvePresenter());
	RebuildFoWOverlay();
	RebuildBlipDrawCache();
	PushBrushesToSlate();
}

void UGP_MinimapWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	UnbindPresenter();
	CancelBackgroundLoad();
	MySurface.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UGP_MinimapWidget::BeginDestroy()
{
	UnbindPresenter();
	CancelBackgroundLoad();
	Super::BeginDestroy();
}

TSharedRef<SWidget> UGP_MinimapWidget::RebuildWidget()
{
	MySurface = SNew(SGPMinimapSurface);
	MySurface->SetFallbackColor(GPMinimapWidgetPrivate::FallbackColor);
	PushBrushesToSlate();
	return MySurface.ToSharedRef();
}

ULocalPlayer* UGP_MinimapWidget::ResolveOwningLocalPlayer() const
{
	for (const UObject* Outer = GetOuter(); Outer != nullptr; Outer = Outer->GetOuter())
	{
		if (const UUserWidget* UserWidget = Cast<UUserWidget>(Outer))
		{
			if (ULocalPlayer* LocalPlayer = UserWidget->GetOwningLocalPlayer())
			{
				return LocalPlayer;
			}
		}
	}

	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetFirstGamePlayer();
		}
	}

	return nullptr;
}

UGP_MinimapPresenter* UGP_MinimapWidget::ResolvePresenter() const
{
	ULocalPlayer* LocalPlayer = ResolveOwningLocalPlayer();
	UGP_HUDViewModelSubsystem* Subsystem =
		LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<UGP_HUDViewModelSubsystem>() : nullptr;
	return Subsystem != nullptr ? Subsystem->GetMinimapPresenter() : nullptr;
}

void UGP_MinimapWidget::BindPresenter(UGP_MinimapPresenter* Presenter)
{
	if (BoundPresenter.Get() == Presenter
		&& PresentationChangedHandle.IsValid()
		&& BlipsChangedHandle.IsValid())
	{
		return;
	}

	UnbindPresenter();
	if (!IsValid(Presenter))
	{
		RebuildFoWOverlay();
		RebuildBlipDrawCache();
		return;
	}

	BoundPresenter = Presenter;
	PresentationChangedHandle = Presenter->OnMinimapPresentationChanged.AddUObject(
		this,
		&ThisClass::HandleMinimapPresentationChanged);
	BlipsChangedHandle = Presenter->OnMinimapBlipsChanged.AddUObject(
		this,
		&ThisClass::HandleMinimapBlipsChanged);
	RebuildBlipDrawCache();
}

void UGP_MinimapWidget::UnbindPresenter()
{
	if (UGP_MinimapPresenter* Presenter = BoundPresenter.Get())
	{
		if (PresentationChangedHandle.IsValid())
		{
			Presenter->OnMinimapPresentationChanged.Remove(PresentationChangedHandle);
		}
		if (BlipsChangedHandle.IsValid())
		{
			Presenter->OnMinimapBlipsChanged.Remove(BlipsChangedHandle);
		}
	}

	PresentationChangedHandle.Reset();
	BlipsChangedHandle.Reset();
	BoundPresenter.Reset();
	BlipDrawList.Reset();
}

void UGP_MinimapWidget::HandleMinimapPresentationChanged()
{
	RebuildFoWOverlay();
	RebuildBlipDrawCache();
	PushBrushesToSlate();
}

void UGP_MinimapWidget::HandleMinimapBlipsChanged()
{
	RebuildBlipDrawCache();
	if (MySurface.IsValid())
	{
		TArray<FGPMinimapSurfaceBlipDraw> SurfaceBlips;
		SurfaceBlips.Reserve(BlipDrawList.Num());
		for (const FGPMinimapBlipDraw& Blip : BlipDrawList)
		{
			FGPMinimapSurfaceBlipDraw SurfaceBlip;
			SurfaceBlip.PresenterNormalized = Blip.PresenterNormalized;
			SurfaceBlip.TeamId = Blip.TeamId;
			SurfaceBlip.bIsBuilding = Blip.bIsBuilding;
			SurfaceBlips.Add(SurfaceBlip);
		}
		MySurface->SetBlips(SurfaceBlips);
		MySurface->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FSoftObjectPath UGP_MinimapWidget::GetConfiguredBackgroundPath() const
{
	const UGP_UIPresentationSettings* Settings = UGP_UIPresentationSettings::Get();
	return Settings != nullptr
		? Settings->MinimapBackgroundTexture.ToSoftObjectPath()
		: FSoftObjectPath();
}

int32 UGP_MinimapWidget::ResolveConfiguredFoWResolution() const
{
	const UGP_UIPresentationSettings* Settings = UGP_UIPresentationSettings::Get();
	const int32 Requested = Settings != nullptr
		? Settings->MinimapFoWPresentationResolution
		: GPMinimapWidgetPrivate::DefaultFoWResolution;
	return ClampFoWPresentationResolution(Requested);
}

void UGP_MinimapWidget::RequestBackgroundLoad()
{
	StartBackgroundLoad(GetConfiguredBackgroundPath());
}

void UGP_MinimapWidget::StartBackgroundLoad(const FSoftObjectPath& Path)
{
	bDebugRequestedAsyncLoad = false;
	PendingBackgroundPath = Path;
	if (!Path.IsValid())
	{
		CancelBackgroundLoad();
		ApplyResidentBackground(nullptr);
		return;
	}

	if (UTexture2D* Resident = Cast<UTexture2D>(Path.ResolveObject()))
	{
		CancelBackgroundLoad();
		ApplyResidentBackground(Resident);
		return;
	}

	CancelBackgroundLoad();
	ApplyResidentBackground(nullptr);

	TWeakObjectPtr<UGP_MinimapWidget> WeakThis(this);
	BackgroundLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Path,
		FStreamableDelegate::CreateLambda(
			[WeakThis]()
			{
				if (UGP_MinimapWidget* Widget = WeakThis.Get())
				{
					Widget->HandleBackgroundLoaded();
				}
			}));
	bDebugRequestedAsyncLoad = BackgroundLoadHandle.IsValid();
}

void UGP_MinimapWidget::CancelBackgroundLoad()
{
	if (BackgroundLoadHandle.IsValid())
	{
		BackgroundLoadHandle->CancelHandle();
	}
	BackgroundLoadHandle.Reset();
}

void UGP_MinimapWidget::HandleBackgroundLoaded()
{
	BackgroundLoadHandle.Reset();
	UTexture2D* Loaded =
		PendingBackgroundPath.IsValid() ? Cast<UTexture2D>(PendingBackgroundPath.ResolveObject()) : nullptr;
	ApplyResidentBackground(Loaded);
	PushBrushesToSlate();
}

void UGP_MinimapWidget::ApplyResidentBackground(UTexture2D* Texture)
{
	BackgroundTexture = Texture;
	BackgroundBrush.SetResourceObject(Texture);
	if (Texture != nullptr)
	{
		BackgroundBrush.ImageSize = FVector2D(
			static_cast<float>(Texture->GetSizeX()),
			static_cast<float>(Texture->GetSizeY()));
	}
	else
	{
		BackgroundBrush.ImageSize = FVector2D::ZeroVector;
	}
}

void UGP_MinimapWidget::EnsureFoWTexture()
{
	const int32 Size = FoWResolution;
	if (FoWTexture != nullptr
		&& FoWTexture->GetSizeX() == Size
		&& FoWTexture->GetSizeY() == Size)
	{
		return;
	}

	FoWTexture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (FoWTexture == nullptr)
	{
		return;
	}

	FoWTexture->NeverStream = true;
	FoWTexture->SRGB = false;
	FoWTexture->Filter = TF_Bilinear;
	FoWTexture->AddressX = TA_Clamp;
	FoWTexture->AddressY = TA_Clamp;
	FoWTexture->CompressionSettings = TC_VectorDisplacementmap;
	FoWTexture->UpdateResource();
	FoWBrush.SetResourceObject(FoWTexture);
	FoWBrush.ImageSize = FVector2D(static_cast<float>(Size), static_cast<float>(Size));
}

void UGP_MinimapWidget::RebuildFoWOverlay()
{
	FoWResolution = ResolveConfiguredFoWResolution();
	const int32 SampleCount = FoWResolution * FoWResolution;
	FoWSamples.SetNum(SampleCount);
	FoWPixels.SetNum(SampleCount);

	const UGP_MinimapPresenter* Presenter = BoundPresenter.Get();
	const bool bReady = Presenter != nullptr && Presenter->IsMinimapReady();
	const int64 NewRevision = bReady ? Presenter->GetMinimapPresentation().Revision : -1;
	ConsumedRevision = NewRevision;
	bFoWReady = bReady;

	const float Inv = 1.0f / static_cast<float>(FoWResolution);
	for (int32 SurfaceY = 0; SurfaceY < FoWResolution; ++SurfaceY)
	{
		const float SurfaceV = (static_cast<float>(SurfaceY) + 0.5f) * Inv;
		for (int32 SurfaceX = 0; SurfaceX < FoWResolution; ++SurfaceX)
		{
			const float SurfaceU = (static_cast<float>(SurfaceX) + 0.5f) * Inv;
			const FVector2D PresenterNormalized =
				SurfaceUVToPresenterNormalized(FVector2D(SurfaceU, SurfaceV));
			const int32 Index = SurfaceY * FoWResolution + SurfaceX;
			const EGP_FoWState State = bReady
				? Presenter->GetMinimapFoWStateNormalized(PresenterNormalized)
				: EGP_FoWState::Unexplored;
			FoWSamples[Index] = State;
			FoWPixels[Index] = GPMinimapWidgetPrivate::OverlayColor(State);
		}
	}

	EnsureFoWTexture();
	UploadFoWTexture();
}

void UGP_MinimapWidget::UploadFoWTexture()
{
	if (FoWTexture == nullptr || FoWTexture->GetPlatformData() == nullptr
		|| FoWTexture->GetPlatformData()->Mips.Num() == 0)
	{
		return;
	}

	FTexture2DMipMap& Mip = FoWTexture->GetPlatformData()->Mips[0];
	const int32 ExpectedBytes = FoWPixels.Num() * sizeof(FColor);
	if (Mip.BulkData.GetBulkDataSize() < ExpectedBytes)
	{
		return;
	}

	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (Data != nullptr)
	{
		FMemory::Memcpy(Data, FoWPixels.GetData(), ExpectedBytes);
	}
	Mip.BulkData.Unlock();
	FoWTexture->UpdateResource();
}

void UGP_MinimapWidget::PushBrushesToSlate()
{
	if (!MySurface.IsValid())
	{
		return;
	}

	const bool bHasTexture = BackgroundTexture != nullptr;
	const FVector2D TextureSize = bHasTexture
		? FVector2D(
			static_cast<float>(BackgroundTexture->GetSizeX()),
			static_cast<float>(BackgroundTexture->GetSizeY()))
		: FVector2D::ZeroVector;
	MySurface->SetFallbackColor(GPMinimapWidgetPrivate::FallbackColor);
	MySurface->SetBackground(BackgroundBrush, bHasTexture, TextureSize);
	MySurface->SetFoW(FoWBrush, FoWTexture != nullptr);
	TArray<FGPMinimapSurfaceBlipDraw> SurfaceBlips;
	SurfaceBlips.Reserve(BlipDrawList.Num());
	for (const FGPMinimapBlipDraw& Blip : BlipDrawList)
	{
		FGPMinimapSurfaceBlipDraw SurfaceBlip;
		SurfaceBlip.PresenterNormalized = Blip.PresenterNormalized;
		SurfaceBlip.TeamId = Blip.TeamId;
		SurfaceBlip.bIsBuilding = Blip.bIsBuilding;
		SurfaceBlips.Add(SurfaceBlip);
	}
	MySurface->SetBlips(SurfaceBlips);
	MySurface->Invalidate(EInvalidateWidgetReason::Paint);
}

void UGP_MinimapWidget::RebuildBlipDrawCache()
{
	BlipDrawList.Reset();
	const UGP_MinimapPresenter* Presenter = BoundPresenter.Get();
	if (Presenter == nullptr || !Presenter->IsMinimapReady())
	{
		return;
	}

	for (const FGP_MinimapBlip& Blip : Presenter->GetBlips())
	{
		FGPMinimapBlipDraw Draw;
		Draw.PresenterNormalized = Blip.NormalizedPosition;
		Draw.TeamId = Blip.TeamId;
		Draw.bIsBuilding = Blip.Kind == EGP_MinimapBlipKind::Building;
		BlipDrawList.Add(Draw);
	}
}

FBox2D UGP_MinimapWidget::ComputeMapDestLocal(const FVector2D& AllottedSize) const
{
	const bool bHasTexture = BackgroundTexture != nullptr;
	const FVector2D TextureSize = bHasTexture
		? FVector2D(
			static_cast<float>(BackgroundTexture->GetSizeX()),
			static_cast<float>(BackgroundTexture->GetSizeY()))
		: FVector2D::ZeroVector;
	return ComputeSharedMapDestLocal(AllottedSize, bHasTexture, TextureSize);
}

#if !UE_BUILD_SHIPPING
void UGP_MinimapWidget::ContractBindPresenter(UGP_MinimapPresenter* Presenter)
{
	BindPresenter(Presenter);
	RebuildFoWOverlay();
	PushBrushesToSlate();
}

void UGP_MinimapWidget::ContractUnbindPresenter()
{
	UnbindPresenter();
	RebuildFoWOverlay();
	PushBrushesToSlate();
}

void UGP_MinimapWidget::ContractRequestBackgroundLoad()
{
	RequestBackgroundLoad();
}

void UGP_MinimapWidget::ContractRequestBackgroundLoadForPath(const FSoftObjectPath& Path)
{
	StartBackgroundLoad(Path);
}

int32 UGP_MinimapWidget::GetBoundPresenterListenerCount() const
{
	return PresentationChangedHandle.IsValid() ? 1 : 0;
}

bool UGP_MinimapWidget::IsUsingFallbackBackground() const
{
	return BackgroundTexture == nullptr;
}

EGP_FoWState UGP_MinimapWidget::GetFoWPresentationSample(int32 SurfaceX, int32 SurfaceY) const
{
	if (SurfaceX < 0 || SurfaceY < 0 || SurfaceX >= FoWResolution || SurfaceY >= FoWResolution
		|| FoWSamples.Num() != FoWResolution * FoWResolution)
	{
		return EGP_FoWState::Unexplored;
	}

	return FoWSamples[SurfaceY * FoWResolution + SurfaceX];
}

FBox2D UGP_MinimapWidget::ContractComputeMapDestLocal(const FVector2D& AllottedSize) const
{
	return ComputeMapDestLocal(AllottedSize);
}

int32 UGP_MinimapWidget::ContractGetBlipDrawTeamId(int32 Index) const
{
	return BlipDrawList.IsValidIndex(Index) ? BlipDrawList[Index].TeamId : -1;
}

bool UGP_MinimapWidget::ContractGetBlipDrawIsBuilding(int32 Index) const
{
	return BlipDrawList.IsValidIndex(Index) && BlipDrawList[Index].bIsBuilding;
}

FVector2D UGP_MinimapWidget::ContractWorldToSurfaceUV(const FVector& WorldLocation) const
{
	const UGP_MinimapPresenter* Presenter = BoundPresenter.Get();
	if (Presenter == nullptr)
	{
		return FVector2D::ZeroVector;
	}

	FVector2D Unclamped = FVector2D::ZeroVector;
	if (!Presenter->TryWorldToMinimapNormalizedUnclamped(WorldLocation, Unclamped))
	{
		return FVector2D(-1.0f, -1.0f);
	}

	return PresenterNormalizedToSurfaceUV(Unclamped);
}
#endif
