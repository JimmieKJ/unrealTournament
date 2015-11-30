// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/HittestGrid.h"
#include "WidgetRenderer.h"


DECLARE_MULTICAST_DELEGATE( FOnRetainedModeChanged );

/**
 * 
 */
class UMG_API SRetainerWidget : public ISlateViewport, public SCompoundWidget, public FGCObject, public ICustomHitTestPath
{
public:
	SLATE_BEGIN_ARGS(SRetainerWidget)
	{
		_Visibility = EVisibility::Visible;
		_Phase = 0;
		_PhaseCount = 1;
	}
	SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(int32, Phase)
		SLATE_ARGUMENT(int32, PhaseCount)
	SLATE_END_ARGS()

	SRetainerWidget();
	~SRetainerWidget();

	void Construct(const FArguments& Args);

	// ISlateViewport
	using ISlateViewport::Tick;
	virtual FIntPoint GetSize() const override;
	virtual bool RequiresVsync() const override;
	virtual FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	bool IsViewportTextureAlphaOnly() const override;
	// ISlateViewport

	void SetRetainedRendering(bool bRetainRendering);

	void SetContent(const TSharedRef< SWidget >& InContent);

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// FGCObject

	// SWidget
	using SWidget::Tick;
	virtual FChildren* GetChildren() override;
	virtual bool ComputeVolatility() const override;
	// SWidget

	virtual void OnTickRetainers(float DeltaTime);

	// ICustomHitTestPath
	virtual TArray<FWidgetAndPointer> GetBubblePathAndVirtualCursors(const FGeometry& InGeometry, FVector2D DesktopSpaceCoordinate, bool bIgnoreEnabledStatus) const override;
	virtual void ArrangeChildren(FArrangedChildren& ArrangedChildren) const override;
	virtual TSharedPtr<struct FVirtualPointerPosition> TranslateMouseCoordinateFor3DChild(const TSharedRef<SWidget>& ChildWidget, const FGeometry& ViewportGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate) const override;
	// ICustomHitTestPath

protected:
	// BEGIN SLeafWidget interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float Scale) const override;
	// END SLeafWidget interface

	void RefreshRenderingMode();
	bool ShouldBeRenderingOffscreen() const;
	void OnRetainerModeChanged();
private:
#if !UE_BUILD_SHIPPING
	static void OnRetainerModeCVarChanged( IConsoleVariable* CVar );
	static FOnRetainedModeChanged OnRetainerModeChangedDelegate;
#endif

	mutable FGeometry CachedAllottedGeometry;
	mutable FSlateRect CachedClippingRect;

	FSimpleSlot EmptyChildSlot;

	FSlateBrush RenderTargetBrush;

	mutable FWidgetRenderer WidgetRenderer;
	mutable class UTextureRenderTarget2D* RenderTarget;
	mutable class FSlateRenderTargetRHI* RenderTargetResource;
	mutable TSharedPtr<SWidget> MyWidget;

	bool bRenderingOffscreenDesire;
	bool bRenderingOffscreen;

	int32 Phase;
	int32 PhaseCount;

	double LastDrawTime;
	int64 LastTickedFrame;

	TSharedPtr<SVirtualWindow> Window;
	TSharedPtr<FHittestGrid> HitTestGrid;
};