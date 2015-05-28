// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __LevelViewportLayout_h__
#define __LevelViewportLayout_h__

#pragma once

#include "LevelEditor.h"
#include "LevelViewportActions.h"

/**
 * Base class for level viewport layout configurations
 * Handles maximizing and restoring well as visibility of specific viewports.
 */
class FLevelViewportLayout : public TSharedFromThis<FLevelViewportLayout>, public FTickableEditorObject
{
public:
	/**
	 * Constructor
	 */
	FLevelViewportLayout();

	/**
	 * Destructor
	 */
	virtual ~FLevelViewportLayout();

	/**
	 * Builds a viewport layout and returns the widget containing the layout
	 * 
	 * @param InParentDockTab		The parent dock tab widget of this viewport configuration
	 * @param InParentTab			The parent tab object
	 * @param LayoutString			The layout string loaded from file to custom build the layout with
	 * @param InParentLevelEditor	Optional level editor parent to use for new viewports
	 */
	TSharedRef<SWidget> BuildViewportLayout( TSharedPtr<SDockTab> InParentDockTab, TSharedPtr<class FLevelViewportTabContent> InParentTab, const FString& LayoutString, TWeakPtr<class SLevelEditor> InParentLevelEditor = NULL );

	/**
	 * Makes a request to maximize a specific viewport and hide the others in this layout
	 * 
	 * @param	ViewportToMaximize	The viewport that should be maximized
	 * @param	bWantMaximize		True to maximize or false to "restore"
	 * @param	bWantImmersive		True to perform an "immersive" maximize, which transitions the viewport to fill the entire application window
	 * @param	bAllowAnimation		True if an animated transition should be used
	 */
	void RequestMaximizeViewport( TSharedRef<class SLevelViewport> ViewportToMaximize, const bool bWantMaximize, const bool bWantImmersive, const bool bAllowAnimation = true );

	/**
	 * @return true if this layout is visible.  It is not visible if its parent tab is not active
	 */
	bool IsVisible() const;

	/**
	 * Checks to see the specified level viewport is visible in this layout
	 * A viewport is visible in a layout if the layout is visible and the viewport is the maximized viewport or there is no maximized viewport
	 *
	 * @param InViewport	The viewport within this layout that should be checked
	 * @return true if the viewport is visible.  
	 */
	bool IsLevelViewportVisible( const class SLevelViewport& InViewport ) const;

	/**
	* Checks to see if the specified level viewport supports maximizing one pane
	*
	* @return true if the viewport supports maximizing
	*/
	bool IsMaximizeSupported() const { return bIsMaximizeSupported; }

	/** 
	 * Checks to see if the specified level viewport is maximized
	 *
	 * @param InViewport	The viewport to check
	 * @return true if the viewport is maximized, false otherwise
	 */
	bool IsViewportMaximized( const SLevelViewport& InViewport ) const;

	/** 
	 * Checks to see if the specified level viewport is in immersive mode
	 *
	 * @param InViewport	The viewport to check
	 * @return true if the viewport is immersive, false otherwise
	 */
	bool IsViewportImmersive( const SLevelViewport& InViewport ) const;

	/**
	 * @return All the viewports in this configuration
	 */
	const TArray< TSharedPtr<class SLevelViewport> >& GetViewports() const { return Viewports; }

	/**
	 * Saves viewport layout information between editor sessions
	 */
	virtual void SaveLayoutString(const FString& LayoutString) const = 0;

	virtual const FName& GetLayoutTypeName() const = 0;


	/** FTickableEditorObject interface */
	virtual void Tick( float DeltaTime ) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override { return TStatId(); }


	/** Tells this layout whether it was the intial layout or replaced an existing one when the user switched layouts */
	void SetIsReplacement(bool bInIsReplacement) { bIsReplacement = bInIsReplacement; }

	/** Returns the parent tab content object */
	TWeakPtr< class FLevelViewportTabContent > GetParentTabContent() const { return ParentTabContent; }

	/** Returns whether a viewport animation is currently taking place */
	bool IsTransitioning() const { return bIsTransitioning; }

protected:

	/**
	 * Maximizes a specific viewport and hides the others in this layout
	 * 
	 * @param	ViewportToMaximize	The viewport that should be maximized
	 * @param	bWantMaximize		True to maximize or false to "restore"
	 * @param	bWantImmersive		True to perform an "immersive" maximize, which transitions the viewport to fill the entire application window
	 * @param	bAllowAnimation		True if an animated transition should be used
	 */
	void MaximizeViewport( TSharedRef<class SLevelViewport> ViewportToMaximize, const bool bWantMaximize, const bool bWantImmersive, const bool bAllowAnimation );

	/**
	 * Overridden in derived classes to set up custom layouts  
	 *
	 * @param LayoutString		The layout string loaded from a file
	 * @return The base widget representing the layout.  Usually a splitter
	 */
	virtual TSharedRef<SWidget> MakeViewportLayout(const FString& LayoutString) = 0;

	/**
	 * Delegate called to get the visibility of the non-maximized viewports
	 * The non-maximized viewports are not visible if there is a maximized viewport on top of them
	 *
	 * @param EVisibility::Visible when visible, EVisibility::Collapsed otherwise
	 */
	EVisibility OnGetNonMaximizedVisibility() const;

	/**
	 * Returns the widget position for viewport transition animations
	 *
	 * @return	Viewport position on canvas
	 */
	FVector2D GetMaximizedViewportPositionOnCanvas() const;

	/**
	 * Returns the widget size for viewport transition animations
	 *
	 * @return	Viewport size on canvas
	 */
	FVector2D GetMaximizedViewportSizeOnCanvas() const;

	/**
	 * Inline replaces a viewport content widget within this layout
	 *
	 * @param	Source	The widget to replace
	 * @param	Replacement	The widget to replace the source widget with
	 */
	virtual void ReplaceWidget( TSharedRef< SWidget > Source, TSharedRef< SWidget > Replacement ) = 0;

	/** If a viewport animation is in progress, finishes that transition immediately */
	void FinishMaximizeTransition();

	/**
	 * Begins a draw throttle for responsiveness when animating a viewports size
	 */
	void BeginThrottleForAnimatedResize();

	/**
	 * Ends a draw throttle for responsiveness when animating a viewports size
	 */
	void EndThrottleForAnimatedResize();

	/** Generates a layout string for persisting settings for this layout based on the runtime type of layout */
	FString GetTypeSpecificLayoutString(const FString& LayoutString) const
	{ 
		if (LayoutString.IsEmpty())
		{
			return LayoutString;
		}
		return FString::Printf(TEXT("%s.%s"), *GetLayoutTypeName().ToString(), *LayoutString);
	}

	/** Called in MakeViewportLayout() functions for derived types of layout to init values common to all types */
	void InitCommonLayoutFromString( const FString& SpecificLayoutString );

	/** Called in SaveLayoutString() functions for derived types of layout to save values common to all types */
	void SaveCommonLayoutString( const FString& SpecificLayoutString ) const;

protected:
	/** True if we've started an animation and are waiting for it to finish */
	bool bIsTransitioning;

	/** Curve for animating from a "restored" state to a maximized state */
	FCurveSequence MaximizeAnimation;

	/** List of all of the viewports in this layout */
	TArray< TSharedPtr< SLevelViewport > > Viewports;
	
	/** The parent tab where this layout resides */
	TWeakPtr< SDockTab > ParentTab;

	/** The parent tab content object where this layout resides */
	TWeakPtr< class FLevelViewportTabContent > ParentTabContent;

	/** The optional parent level editor for this layout */
	TWeakPtr< class SLevelEditor > ParentLevelEditor;

	/** The current maximized viewport if any */
	TSharedPtr< SLevelViewport > MaximizedViewport;

	/** True if the user selected this layout, false if it's the initial layout loaded */
	bool bIsReplacement;

	/** Temporarily set to true while we are querying layout metrics and want all widgets to be visible */
	bool bIsQueryingLayoutMetrics;

	/** True if the layout supports maximizing one viewport, false if the feature is disabled  */
	bool bIsMaximizeSupported;

	/** True if we're currently maximized */
	bool bIsMaximized;

	/** True if we're currently in immersive mode */
	bool bIsImmersive;

	/** True when transitioning from a maximized state */
	bool bWasMaximized;

	/** True when transitioning from an immersive state */
	bool bWasImmersive;

	/** Window-space start position of the viewport that's currently being maximized */
	FVector2D MaximizedViewportStartPosition;

	/** Window-space start size of the viewport that's currently being maximized */
	FVector2D MaximizedViewportStartSize;

	/** The overlay widget that handles what viewports should be on top (non-maximized or maximized) */
	TWeakPtr< class SViewportsOverlay > ViewportsOverlayPtr;

	/** When maximizing viewports (or making them immersive), this stores the widget we create to wrap the viewport */
	TSharedPtr< SWidget > ViewportsOverlayWidget;

	/** Dummy widget that we'll inline-replace viewport widgets with while a view is maximized (or made immersive) */
	TSharedPtr< SWidget > ViewportReplacementWidget;
	
	/** Caches the window that our widgets are contained within */
	TWeakPtr< SWindow > CachedOwnerWindow;

	/** Viewport resize draw throttle request */
	FThrottleRequest ViewportResizeThrottleRequest;

	/**
	 * Maximize/immersive commands can be queued up at startup to be executed on the first tick.
	 * This is necessary, because these commands can't be executed until the viewport has a parent window,
	 * which might not be there upon viewport initialization
	 */
	struct FMaximizeViewportCommand
	{
		FMaximizeViewportCommand(TSharedRef<SLevelViewport> InViewport, bool bInMaximize, bool bInImmersive, bool bInToggle=true, bool bInAllowAnimation=true)
			: Viewport(InViewport)
			, bMaximize(bInMaximize)
			, bImmersive(bInImmersive) 
			, bToggle(bInToggle)
			, bAllowAnimation(bInAllowAnimation)
		{}

		TSharedRef<SLevelViewport> Viewport;
		bool bMaximize;
		bool bImmersive;
		bool bToggle;
		bool bAllowAnimation;
	};
	TArray<FMaximizeViewportCommand> DeferredMaximizeCommands;
};

#endif
