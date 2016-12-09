// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/Geometry.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SCanvas.h"

class FPaintArgs;
class FSceneViewport;
class FSlateWindowElementList;
class SOverlay;
class STooltipPresenter;
class UGameViewportClient;
class ULocalPlayer;

/**
 * Allows you to provide a custom layer that multiple sources can contribute to.  Unlike
 * adding widgets directly to the layer manager.  First registering a layer with a name
 * allows multiple widgets to be added.
 */
class IGameLayer : public TSharedFromThis<IGameLayer>
{
public:
	/** Get the layer as a widget. */
	virtual TSharedRef<SWidget> AsWidget() = 0;

public:

	/** Virtual destructor. */
	virtual ~IGameLayer() { }
};

/**
 * Allows widgets to be managed for different users.
 */
class IGameLayerManager
{
public:
	virtual void NotifyPlayerAdded(int32 PlayerIndex, ULocalPlayer* AddedPlayer) = 0;
	virtual void NotifyPlayerRemoved(int32 PlayerIndex, ULocalPlayer* RemovedPlayer) = 0;

	virtual void AddWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, int32 ZOrder) = 0;
	virtual void RemoveWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent) = 0;
	virtual void ClearWidgetsForPlayer(ULocalPlayer* Player) = 0;

	virtual TSharedPtr<IGameLayer> FindLayerForPlayer(ULocalPlayer* Player, const FName& LayerName) = 0;
	virtual bool AddLayerForPlayer(ULocalPlayer* Player, const FName& LayerName, TSharedRef<IGameLayer> Layer, int32 ZOrder) = 0;

	virtual void ClearWidgets() = 0;
};

/**
 * 
 */
class ENGINE_API SGameLayerManager : public SCompoundWidget, public IGameLayerManager
{
public:

	SLATE_BEGIN_ARGS(SGameLayerManager)
		: _UseScissor(true)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		/** Slot for this content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_ATTRIBUTE(const FSceneViewport*, SceneViewport)
		SLATE_ARGUMENT(bool, UseScissor)

	SLATE_END_ARGS()

	SGameLayerManager();

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );

	// Begin IGameLayerManager
	virtual void NotifyPlayerAdded(int32 PlayerIndex, ULocalPlayer* AddedPlayer) override;
	virtual void NotifyPlayerRemoved(int32 PlayerIndex, ULocalPlayer* RemovedPlayer) override;

	virtual void AddWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent, int32 ZOrder) override;
	virtual void RemoveWidgetForPlayer(ULocalPlayer* Player, TSharedRef<SWidget> ViewportContent) override;
	virtual void ClearWidgetsForPlayer(ULocalPlayer* Player) override;

	virtual TSharedPtr<IGameLayer> FindLayerForPlayer(ULocalPlayer* Player, const FName& LayerName) override;
	virtual bool AddLayerForPlayer(ULocalPlayer* Player, const FName& LayerName, TSharedRef<IGameLayer> Layer, int32 ZOrder) override;

	virtual void ClearWidgets() override;
	// End IGameLayerManager

public:

	// Begin SWidget overrides
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual bool OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent) override;
	// End SWidget overrides

private:
	float GetGameViewportDPIScale() const;

private:

	struct FPlayerLayer : TSharedFromThis<FPlayerLayer>
	{
		TSharedPtr<SOverlay> Widget;
		SCanvas::FSlot* Slot;

		TMap< FName, TSharedPtr<IGameLayer> > Layers;

		FPlayerLayer()
			: Slot(nullptr)
		{
		}
	};

	void UpdateLayout();

	TSharedPtr<FPlayerLayer> FindOrCreatePlayerLayer(ULocalPlayer* LocalPlayer);
	void RemoveMissingPlayerLayers(const TArray<ULocalPlayer*>& GamePlayers);
	void RemovePlayerWidgets(ULocalPlayer* LocalPlayer);
	void AddOrUpdatePlayerLayers(const FGeometry& AllottedGeometry, UGameViewportClient* ViewportClient, const TArray<ULocalPlayer*>& GamePlayers);
	FVector2D GetAspectRatioInset(ULocalPlayer* LocalPlayer) const;

private:
	FGeometry CachedGeometry;

	TMap < ULocalPlayer*, TSharedPtr<FPlayerLayer> > PlayerLayers;

	TAttribute<const FSceneViewport*> SceneViewport;
	TSharedPtr<SCanvas> PlayerCanvas;
	TSharedPtr<class STooltipPresenter> TooltipPresenter;
};
