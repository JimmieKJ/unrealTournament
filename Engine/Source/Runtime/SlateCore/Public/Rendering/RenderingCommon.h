// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FVector2D;
class FSlateRect;
enum class EPopupMethod : uint8;


#define SLATE_USE_32BIT_INDICES !PLATFORM_USES_ES2
#define SLATE_USE_FLOAT16 !PLATFORM_USES_ES2

#if SLATE_USE_32BIT_INDICES
typedef uint32 SlateIndex;
#else
typedef uint16 SlateIndex;
#endif

/**
 * Draw primitive types                   
 */
namespace ESlateDrawPrimitive
{
	typedef uint8 Type;

	const Type LineList = 0;
	const Type TriangleList = 1;
};

/**
 * Shader types. NOTE: mirrored in the shader file   
 * If you add a type here you must also implement the proper shader type (TSlateElementPS).  See SlateShaders.h
 */
namespace ESlateShader
{
	typedef uint8 Type;
	/** The default shader type.  Simple texture lookup */
	const Type Default = 0;
	/** Border shader */
	const Type Border = 1;
	/** Font shader, same as default except uses an alpha only texture */
	const Type Font = 2;
	/** Line segment shader. For drawing anti-aliased lines **/
	const Type LineSegment = 3;
};

/**
 * Effects that can be applied to elements when rendered.
 * Note: New effects added should be in bit mask form
 * * If you add a type here you must also implement the proper shader type (TSlateElementPS).  See SlateShaders.h
 */
namespace ESlateDrawEffect
{
	typedef uint8 Type;
	/** No effect applied */
	const Type None = 0;
	/** Draw the element with a disabled effect */
	const Type DisabledEffect = 1<<0;
	/** Don't read from texture alpha channel */
	const Type IgnoreTextureAlpha = 1<<2;
};


/** Flags for drawing a batch */
namespace ESlateBatchDrawFlag
{
	typedef uint8 Type;
	/** No draw flags */
	const Type None					=		0;
	/** Draw the element with no blending */
	const Type NoBlending			=		0x01;
	/** Draw the element as wireframe */
	const Type Wireframe			=		0x02;
	/** The element should be tiled horizontally */
	const Type TileU				=		0x04;
	/** The element should be tiled vertically */
	const Type TileV				=		0x08;
	/** No gamma correction should be done */
	const Type NoGamma				=		0x10;
};

namespace ESlateLineJoinType
{
	enum Type
	{
		// Joins line segments with a sharp edge (miter)
		Sharp =	0,
		// Simply stitches together line segments
		Simple = 1,
	};
};

/**
 * Stores a rectangle that has been transformed by an arbitrary render transform. 
 * We provide a ctor that does the work common to slate drawing, but you could technically 
 * create this any way you want.
 */
struct FSlateRotatedRect
{
	/** Default ctor. */
	FSlateRotatedRect();
	/** Construct a rotated rect from a given aligned rect. */
	explicit FSlateRotatedRect(const FSlateRect& AlignedRect);
	/** Per-element constructor. */
	FSlateRotatedRect(const FVector2D& InTopLeft, const FVector2D& InExtentX, const FVector2D& InExtentY);
	/** transformed Top-left corner. */
	FVector2D TopLeft;
	/** transformed X extent (right-left). */
	FVector2D ExtentX;
	/** transformed Y extent (bottom-top). */
	FVector2D ExtentY;

	/** Convert to a bounding, aligned rect. */
	FSlateRect ToBoundingRect() const;
	/** Point-in-rect test. */
	bool IsUnderLocation(const FVector2D& Location) const;
};

/**
 * Transforms a rect by the given transform.
 */
template <typename TransformType>
FSlateRotatedRect TransformRect(const TransformType& Transform, const FSlateRotatedRect& Rect)
{
	return FSlateRotatedRect
	(
		TransformPoint(Transform, Rect.TopLeft),
		TransformVector(Transform, Rect.ExtentX),
		TransformVector(Transform, Rect.ExtentY)
	);
}


/**
 * Stores a Rotated rect as float16 (for rendering).
 */
struct FSlateRotatedRectHalf
{
	/** Default ctor. */
	FSlateRotatedRectHalf();
	/** Construct a float16 version of a rotated rect from a full-float version. */
	explicit FSlateRotatedRectHalf(const FSlateRotatedRect& RotatedRect);
	/** Per-element constructor. */
	FSlateRotatedRectHalf(const FVector2D& InTopLeft, const FVector2D& InExtentX, const FVector2D& InExtentY);
	/** transformed Top-left corner. */
	FVector2DHalf TopLeft;
	/** transformed X extent (right-left). */
	FVector2DHalf ExtentX;
	/** transformed Y extent (bottom-top). */
	FVector2DHalf ExtentY;
};

/**
 * Not all platforms support Float16, so we have to be tricky here and declare the proper vertex type.
 */
#if SLATE_USE_FLOAT16
typedef FSlateRotatedRectHalf FSlateRotatedClipRectType;
#else
typedef FSlateRotatedRect FSlateRotatedClipRectType;
#endif


/** 
 * A struct which defines a basic vertex seen by the Slate vertex buffers and shaders
 */
struct FSlateVertex
{
	/** Texture coordinates.  The first 2 are in xy and the 2nd are in zw */
	float TexCoords[4]; 
	/** Position of the vertex in window space */
	int16 Position[2];
	/** clip center/extents in render window space (window space with render transforms applied) */
	FSlateRotatedClipRectType ClipRect;
	/** Vertex color */
	FColor Color;
	
	FSlateVertex();
	FSlateVertex( const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FVector2D& InTexCoord, const FVector2D& InTexCoord2, const FColor& InColor, const FSlateRotatedClipRectType& InClipRect );
	FSlateVertex( const FSlateRenderTransform& RenderTransform, const FVector2D& InLocalPosition, const FVector2D& InTexCoord, const FColor& InColor, const FSlateRotatedClipRectType& InClipRect );
};

template<> struct TIsPODType<FSlateVertex> { enum { Value = true }; };

/** Stores an aligned rect as shorts. */
struct FShortRect
{
	FShortRect() : Left(0), Top(0), Right(0), Bottom(0) {}
	FShortRect(uint16 InLeft, uint16 InTop, uint16 InRight, uint16 InBottom) : Left(InLeft), Top(InTop), Right(InRight), Bottom(InBottom) {}
	explicit FShortRect(const FSlateRect& Rect) : Left((uint16)Rect.Left), Top((uint16)Rect.Top), Right((uint16)Rect.Right), Bottom((uint16)Rect.Bottom) {}
	bool operator==(const FShortRect& RHS) const { return Left == RHS.Left && Top == RHS.Top && Right == RHS.Right && Bottom == RHS.Bottom; }
	bool operator!=(const FShortRect& RHS) const { return !(*this == RHS); }
	bool DoesIntersect( const FShortRect& B ) const
	{
		const bool bDoNotOverlap =
			B.Right < Left || Right < B.Left ||
			B.Bottom < Top || Bottom < B.Top;

		return ! bDoNotOverlap;
	}

	uint16 Left;
	uint16 Top;
	uint16 Right;
	uint16 Bottom;
};

/**
 * Viewport implementation interface that is used by SViewport when it needs to draw and processes input.                   
 */
class ISlateViewport
{
public:
	virtual ~ISlateViewport() {}

	/**
	 * Called by Slate when the viewport widget is drawn
	 * Implementers of this interface can use this method to perform custom
	 * per draw functionality.  This is only called if the widget is visible
	 *
	 * @param AllottedGeometry	The geometry of the viewport widget
	 */
	virtual void OnDrawViewport( const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, class FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) { }
	
	/**
	 * Returns the size of the viewport                   
	 */
	virtual FIntPoint GetSize() const = 0;

	/**
	 * Returns a slate texture used to draw the rendered viewport in Slate.                   
	 */
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const = 0;

	/**
	 * Does the texture returned by GetViewportRenderTargetTexture only have an alpha channel?
	 */
	virtual bool IsViewportTextureAlphaOnly() const
	{
		return false;
	}

	/**
	 * Performs any ticking necessary by this handle                   
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime )
	{
	}

	/**
	 * Returns true if the viewport should be vsynced.
	 */
	virtual bool RequiresVsync() const = 0;

	/**
	 * Called when Slate needs to know what the mouse cursor should be.
	 * 
	 * @return FCursorReply::Unhandled() if the event is not handled; FCursorReply::Cursor() otherwise.
	 */
	virtual FCursorReply OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent )
	{
		return FCursorReply::Unhandled();
	}

	/**
	 * After OnCursorQuery has specified a cursor type the system asks each widget under the mouse to map that cursor to a widget. This event is bubbled.
	 * 
	 * @return TOptional<TSharedRef<SWidget>>() if you don't have a mapping otherwise return the Widget to show.
	 */
	virtual TOptional<TSharedRef<SWidget>> OnMapCursor(const FCursorReply& CursorReply)
	{
		return TOptional<TSharedRef<SWidget>>();
	}

	/**
	 *	Returns whether the software cursor is currently visible
	 */
	virtual bool IsSoftwareCursorVisible() const
	{
		return false;
	}

	/**
	 *	Returns the current position of the software cursor
	 */
	virtual FVector2D GetSoftwareCursorPosition() const
	{
		return FVector2D::ZeroVector;
	}

	/**
	 * Called by Slate when a mouse button is pressed inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when a mouse button is released inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		return FReply::Unhandled();
	}


	virtual void OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{

	}

	virtual void OnMouseLeave( const FPointerEvent& MouseEvent )
	{
		
	}

	/**
	 * Called by Slate when a mouse button is released inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when the mouse wheel is used inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when the mouse wheel is used inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the mouse event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when a key is pressed inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the key event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when a key is released inside the viewport
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the key event
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when an analog value changes on a button that supports analog
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param InAnalogInputEvent Analog input event
	 * @return Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnAnalogValueChanged( const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called by Slate when a character key is pressed while the viewport has focus
	 *
	 * @param MyGeometry	Information about the location and size of the viewport
	 * @param MouseEvent	Information about the character that was pressed
	 *
	 * @return Whether the event was handled along with possible requests for the system to take action.
	 */
	virtual FReply OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when the viewport gains keyboard focus.  
	 *
	 * @param InFocusEvent	Information about what caused the viewport to gain focus
	 */
	virtual FReply OnFocusReceived( const FFocusEvent& InFocusEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when a touchpad touch is started (finger down)
	 * 
	 * @param ControllerEvent	The controller event generated
	 */
	virtual FReply OnTouchStarted( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when a touchpad touch is moved  (finger moved)
	 * 
	 * @param ControllerEvent	The controller event generated
	 */
	virtual FReply OnTouchMoved( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called when a touchpad touch is ended (finger lifted)
	 * 
	 * @param ControllerEvent	The controller event generated
	 */
	virtual FReply OnTouchEnded( const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called on a touchpad gesture event
	 *
	 * @param InGestureEvent	The touch event generated
	 */
	virtual FReply OnTouchGesture( const FGeometry& MyGeometry, const FPointerEvent& InGestureEvent )
	{
		return FReply::Unhandled();
	}
	
	/**
	 * Called when motion is detected (controller or device)
	 * e.g. Someone tilts or shakes their controller.
	 * 
	 * @param InMotionEvent	The motion event generated
	 */
	virtual FReply OnMotionDetected( const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent )
	{
		return FReply::Unhandled();
	}

	/**
	 * Called to determine if we should render the focus brush.
	 *
	 * @param InFocusCause	The cause of focus
	 */
	virtual TOptional<bool> OnQueryShowFocus(const EFocusCause InFocusCause) const
	{
		return TOptional<bool>();
	}

	/**
	 * Called after all input for this frame is processed.
	 */
	virtual void OnFinishedPointerInput()
	{
	}

	/**
	 * Called to figure out whether we can make new windows for popups within this viewport.
	 * Making windows allows us to have popups that go outside the parent window, but cannot
	 * be used in fullscreen and do not have per-pixel alpha.
	 */
	virtual TOptional<EPopupMethod> OnQueryPopupMethod() const
	{
		return TOptional<EPopupMethod>();
	}

	/**
	 * Called when navigation is requested
	 * e.g. Left Joystick, Direction Pad, Arrow Keys can generate navigation events.
	 * 
	 * @param InNavigationEvent	The navigation event generated
	 */
	virtual FNavigationReply OnNavigation( const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent )
	{
		return FNavigationReply::Stop();
	}

	/**
	 * Called when the viewport loses keyboard focus.  
	 *
	 * @param InFocusEvent	Information about what caused the viewport to lose focus
	 */
	virtual void OnFocusLost( const FFocusEvent& InFocusEvent )
	{
	}

	/**
	 * Called when the viewports top level window is being closed
	 */
	virtual void OnViewportClosed()
	{
	}
};

/**
 * An interface for a custom slate drawing element
 * Implementers of this interface are expected to handle destroying this interface properly when a separate 
 * rendering thread may have access to it. (I.E this cannot be destroyed from a different thread if the rendering thread is using it)
 */
class ICustomSlateElement
{
public:
	virtual ~ICustomSlateElement() {}

	/** 
	 * Called from the rendering thread when it is time to render the element
	 *
	 * @param RenderTarget	handle to the platform specific render target implementation.  Note this is already bound by Slate initially 
	 */
	virtual void DrawRenderThread(class FRHICommandListImmediate& RHICmdList, const void* RenderTarget) = 0;
};
