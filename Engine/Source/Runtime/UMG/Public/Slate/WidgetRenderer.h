// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class ISlate3DRenderer;
class FHittestGrid;
class SWindow;

class UMG_API SVirtualWindow : public SWindow
{
	SLATE_BEGIN_ARGS(SVirtualWindow)
		: _Size(FVector2D(100, 100))
	{}

	SLATE_ARGUMENT(FVector2D, Size)
	
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);

	/**
	 * We allow users to control the resolve policy for deferred content.  When a virtual window is used in a retainer, we don't want it trying to resolve
	 * the deferred content, we instead need it to pass that content up to the non-retained window drawing the retainer.  If a virtual window is used in 
	 * a case like the WidgetComponent, we always want to resolve the deferred content because there won't be another opportunity since the 3D widget has
	 * no owner window that can draw it.
	 */
	void SetShouldResolveDeferred(bool bResolve);

	/**  */
	void SetIsFocusable(bool bFocusable);
	
	virtual FPopupMethodReply OnQueryPopupMethod() const override;
	virtual bool OnVisualizeTooltip(const TSharedPtr<SWidget>& TooltipContent) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual bool SupportsKeyboardFocus() const override;

private:
	TSharedPtr<class STooltipPresenter> TooltipPresenter;
	bool bIsFocusable;
	bool bShouldResolveDeferred;
};

/**
 * 
 */
class UMG_API FWidgetRenderer
{
public:
	FWidgetRenderer(bool bUseGammaCorrection = false, bool bInClearTarget = true);
	~FWidgetRenderer();

	bool GetIsPrepassNeeded() const { return bPrepassNeeded; }
	void SetIsPrepassNeeded(bool bInPrepassNeeded) { bPrepassNeeded = bInPrepassNeeded; }
	void SetShouldClearTarget(bool bShouldClear) { bClearTarget = bShouldClear; }

	ISlate3DRenderer* GetSlateRenderer();

	static UTextureRenderTarget2D* CreateTargetFor(FVector2D DrawSize, TextureFilter InFilter, bool bUseGammaCorrection);

	UTextureRenderTarget2D* DrawWidget(const TSharedRef<SWidget>& Widget, FVector2D DrawSize);

	void DrawWidget(
		UTextureRenderTarget2D* RenderTarget,
		const TSharedRef<SWidget>& Widget,
		FVector2D DrawSize,
		float DeltaTime);

	void DrawWindow(
		UTextureRenderTarget2D* RenderTarget,
		TSharedRef<FHittestGrid> HitTestGrid,
		TSharedRef<SWindow> Window,
		float Scale,
		FVector2D DrawSize,
		float DeltaTime);

	void DrawWindow(
		UTextureRenderTarget2D* RenderTarget,
		TSharedRef<FHittestGrid> HitTestGrid,
		TSharedRef<SWindow> Window,
		FGeometry WindowGeometry,
		FSlateRect WindowClipRect,
		float DeltaTime);

	TArray< TSharedPtr<FSlateWindowElementList::FDeferredPaint> > DeferredPaints;

private:
	void PrepassWindowAndChildren(TSharedRef<SWindow> Window, float Scale);

	void DrawWindowAndChildren(
		UTextureRenderTarget2D* RenderTarget,
		FSlateDrawBuffer& DrawBuffer,
		TSharedRef<FHittestGrid> HitTestGrid,
		TSharedRef<SWindow> Window,
		FGeometry WindowGeometry,
		FSlateRect WindowClipRect,
		float DeltaTime);

private:
	/** The slate 3D renderer used to render the user slate widget */
	TSharedPtr<ISlate3DRenderer, ESPMode::ThreadSafe> Renderer;
	/** Prepass Needed when drawing the widget? */
	bool bPrepassNeeded;
	/** Is gamma space needed? */
	bool bUseGammaSpace;
	/** Should we clear the render target before rendering. */
	bool bClearTarget;

public:
	FVector2D ViewOffset;
};
