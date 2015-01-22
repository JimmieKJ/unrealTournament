// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef __OBJC__

/**
 * Custom window class used for input handling
 */
@interface FCocoaWindow : NSWindow <NSWindowDelegate, NSDraggingDestination>
{
	EWindowMode::Type WindowMode;
	CGFloat DeferOpacity;
	bool bAcceptsInput;
	bool bRoundedCorners;
	bool bDisplayReconfiguring;
	bool bDeferOrderFront;
	bool bRenderInitialised;
	bool bNeedsRedraw;
@public
	bool bZoomed;
}

@property (assign) NSRect PreFullScreenRect;
@property (assign) EWindowMode::Type TargetWindowMode;
@property (assign) bool bForwardEvents;
@property (assign) bool bDeferSetFrame;
@property (assign) bool bDeferSetOrigin;
@property (assign) NSRect DeferFrame;

/** Get the frame filled by a child OpenGL view, which may cover the window or fill the content view depending upon the window style.
 @return The NSRect for a child OpenGL view. */
- (NSRect)openGLFrame;

/** Get the view used for OpenGL rendering. @return The OpenGL view for rendering. */
- (NSView*)openGLView;

/** Perform render thread deferred order front operation. */
- (void)performDeferredOrderFront;

/** Perform deferred set frame operation */
- (void)performDeferredSetFrame;

/** Set whether the window should display with rounded corners. */
- (void)setRoundedCorners:(bool)bUseRoundedCorners;

/** Get whether the window should display with rounded corners.
 @return True when window corners should be rounded, else false. */
- (bool)roundedCorners;

/** Lets window know if its owner (SWindow) accepts input */
- (void)setAcceptsInput:(bool)InAcceptsInput;

/** Redraws window's contents. */
- (void)redrawContents;

/** Set the initial window mode. */
- (void)setWindowMode:(EWindowMode::Type)WindowMode;

/**	@return The current mode for this Cocoa window. */
- (EWindowMode::Type)windowMode;

/** Mutator that specifies that the display arrangement is being reconfigured when bIsDisplayReconfiguring is true. */
- (void)setDisplayReconfiguring:(bool)bIsDisplayReconfiguring;

/** Order window to the front. */
- (void)orderFrontAndMakeMain:(bool)bMain andKey:(bool)bKey;

/** Destroy the window */
- (void)destroy;

@end

/**
 * Custom window class used for mouse capture
 */
@interface FMouseCaptureWindow : NSWindow <NSWindowDelegate>
{
	FCocoaWindow*	TargetWindow;
}

- (id)initWithTargetWindow: (FCocoaWindow*)Window;
- (FCocoaWindow*)targetWindow;
- (void)setTargetWindow: (FCocoaWindow*)Window;

@end

extern NSString* NSWindowRedrawContents;
extern NSString* NSDraggingExited;
extern NSString* NSDraggingUpdated;
extern NSString* NSPrepareForDragOperation;
extern NSString* NSPerformDragOperation;

#else // __OBJC__

class FCocoaWindow;
class FMouseCaptureWindow;
class NSWindow;
class NSEvent;
class NSScreen;

#endif // __OBJC__
