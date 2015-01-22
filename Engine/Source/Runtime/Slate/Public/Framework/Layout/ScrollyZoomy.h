// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Have your widget implement this interface to allow FScrollyZoomy to interface with your widget
 */
class IScrollableZoomable
{
public:

	/**
	 * Override this to scroll your widget's content.
	 *
	 * @param Offset The 2D offset to scroll by.
	 * @return True if anything was scrolled.
	 */
	virtual bool ScrollBy( const FVector2D& Offset ) = 0;

	/**
	 * Override this to zoom your widget's content.
	 *
	 * @param Amount The amount to zoom by.
	 * @return True if anything was zoomed.
	 */
	virtual bool ZoomBy( const float Amount ) = 0;
};


/**
 * Utility class that adds scrolling and zooming functionality to a widget.  Derived your widget class from IScrollableZoomable, then embed an
 * instance of FScrollyZoomy as a widget member variable, and call this class's event handlers from your own widget's event handler callbacks.
 */
class SLATE_API FScrollyZoomy
{
public:

	/** Default constructor for FScrollyZoomy */
	FScrollyZoomy( const bool bInUseIntertialScrolling = true );

	/** 
	 * Should be called every frame to update simulation state
	 *
	 * @param	DeltaTime			Time that's passed
	 * @param	ScrollableZoomable	Interface to the widget to scroll/zoom
	 */
	void Tick( const float DeltaTime, IScrollableZoomable& ScrollableZoomable );

	/**
	 * Should be called when a mouse button is pressed
	 *
	 * @param	MouseEvent	The mouse event passed to a widget's OnMouseButtonDown() function
	 * @return	If FReply::Handled is returned, that should be passed as the result of the calling function
	 */
	FReply OnMouseButtonDown( const FPointerEvent& MouseEvent );

	/**
	 * Should be called when a mouse button is released
	 *
	 * @param	MyWidget	Pointer to the widget that owns this object
	 * @param	MyGeometry	Geometry of the widget we're scrolling
	 * @param	MouseEvent	The mouse event passed to a widget's OnMouseButtonUp() function
	 * @return	If FReply::Handled is returned, that should be passed as the result of the calling function
	 */
	FReply OnMouseButtonUp( const TSharedRef<SWidget> MyWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );
		
	/**
	 * Should be called when a mouse move event occurs
	 *
	 * @param	MyWidget	Pointer to the widget that owns this object
	 * @param	ScrollableZoomable	Interface to the widget to scroll/zoom
	 * @param	MyGeometry	Geometry of the widget we're scrolling
	 * @param	MouseEvent	The mouse event passed to a widget's OnMouseMove() function
	 * @return	If FReply::Handled is returned, that should be passed as the result of the calling function
	 */
	FReply OnMouseMove( const TSharedRef<SWidget> MyWidget, IScrollableZoomable& ScrollableZoomable, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

	/**
	 * Should be called from your widget's OnMouseLeave() override
	 *
	 * @param	MyWidget	Pointer to the widget that owns this object
	 * @param	MouseEvent	The mouse leave event that was passed to the widget's OnMouseLeave() function
	 */
	void OnMouseLeave( const TSharedRef<SWidget> MyWidget, const FPointerEvent& MouseEvent );
		
	/**
	 * Should be called by your widget when the mouse wheel is used
	 *
	 * @param	MouseEvent	The event passed to your widget's OnMouseWheel function
	 * @param	ScrollableZoomable	Interface to the widget to scroll/zoom
	 * @return	If FReply::Handled is returned, that should be passed as the result of the calling function
	 */
	FReply OnMouseWheel( const FPointerEvent& MouseEvent, IScrollableZoomable& ScrollableZoomable );

	/**
	 * Call this from your widget's OnCursorQuery() function
	 *
	 * @return	The cursor reply to pass back
	 */
	FCursorReply OnCursorQuery( ) const;

	/** @return Returns true if the user is actively scrolling */
	bool IsRightClickScrolling( ) const;

	/** @return Returns whether we need a software cursor to be rendered right now.  Should be called from your widget's OnPaint() function! */
	bool NeedsSoftwareCursor( ) const;

	/** @return Returns the position of the software cursor when NeedsSoftwareCursor() == true.  Should be called from your widget's OnPaint() function! */
	const FVector2D& GetSoftwareCursorPosition( ) const;

	/**
	 * Call this from your widget's OnPaint to paint a software cursor, if needed 
	 *
	 * @param	AllottedGeometry		Widget geometry passed into OnPaint()
	 * @param	MyClippingRect			Widget clipping rect passed into OnPaint()
	 * @param	OutDrawElements			The draw element list
	 * @param	LayerId					Layer Id
	 * @return	New layer Id
	 */
	int32 PaintSoftwareCursorIfNeeded( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId ) const;

private:

	// How much we scrolled while the right mouse button has been held.
	float AmountScrolledWhileRightMouseDown;

	// Whether the software cursor should be drawn in the view port.
	bool bShowSoftwareCursor;

	// The current position of the software cursor.
	FVector2D SoftwareCursorPosition;

	// Is intertial scrolling enabled?
	bool bUseIntertialScrolling;

	// Tracks simulation state for horizontal scrolling.
	FInertialScrollManager HorizontalIntertia;

	// Tracks simulation state for vertical scrolling.
	FInertialScrollManager VerticalIntertia;
};
