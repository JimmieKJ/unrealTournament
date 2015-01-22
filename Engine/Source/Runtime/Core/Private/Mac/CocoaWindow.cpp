// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "CocoaWindow.h"
#include "MacApplication.h"
#include "CocoaTextView.h"
#include "MacEvent.h"
#include "CocoaThread.h"
#include "MacCursor.h"

NSString* NSWindowRedrawContents = @"NSWindowRedrawContents";
NSString* NSDraggingExited = @"NSDraggingExited";
NSString* NSDraggingUpdated = @"NSDraggingUpdated";
NSString* NSPrepareForDragOperation = @"NSPrepareForDragOperation";
NSString* NSPerformDragOperation = @"NSPerformDragOperation";

/**
 * Custom window class used for input handling
 */
@implementation FCocoaWindow

@synthesize bForwardEvents;
@synthesize TargetWindowMode;
@synthesize PreFullScreenRect;

- (id)initWithContentRect:(NSRect)ContentRect styleMask:(NSUInteger)Style backing:(NSBackingStoreType)BufferingType defer:(BOOL)Flag
{
	WindowMode = EWindowMode::Windowed;
	bAcceptsInput = false;
	bRoundedCorners = false;
	bDisplayReconfiguring = false;
	bDeferOrderFront = false;
	DeferOpacity = 0.0f;
	bRenderInitialised = false;
	self.bDeferSetFrame = false;
	self.bDeferSetOrigin = false;

	id NewSelf = [super initWithContentRect:ContentRect styleMask:Style backing:BufferingType defer:Flag];
	if(NewSelf)
	{
		bZoomed = [super isZoomed];
		self.bForwardEvents = true;
		self.TargetWindowMode = EWindowMode::Windowed;
		[super setAlphaValue:DeferOpacity];
		self.DeferFrame = [super frame];
		self.PreFullScreenRect = self.DeferFrame;
	}
	return NewSelf;
}

- (void)dealloc
{
	[super dealloc];
}

- (NSRect)openGLFrame
{
	SCOPED_AUTORELEASE_POOL;
	if(self.TargetWindowMode == EWindowMode::Fullscreen || WindowMode == EWindowMode::Fullscreen)
	{
		return {{0, 0}, self.PreFullScreenRect.size};
	}
	else if([self styleMask] & (NSTexturedBackgroundWindowMask))
	{
		return (!self.bDeferSetFrame ? [self frame] : self.DeferFrame);
	}
	else
	{
		return (!self.bDeferSetFrame ? [[self contentView] frame] : [self contentRectForFrameRect:self.DeferFrame]);
	}
}


- (NSView*)openGLView
{
	SCOPED_AUTORELEASE_POOL;
	if([self styleMask] & (NSTexturedBackgroundWindowMask))
	{
		NSView* SuperView = [[self contentView] superview];
		for(NSView* View in [SuperView subviews])
		{
			if([View isKindOfClass:[FCocoaTextView class]])
			{
				return View;
			}
		}
		return nil;
	}
	else
	{
		return [self contentView];
	}
}

- (void)performDeferredOrderFront
{
	if(!bRenderInitialised)
	{
		bRenderInitialised = true;
	}
	
	if(bDeferOrderFront)
	{
		SCOPED_AUTORELEASE_POOL;
		if(!(self.bDeferSetFrame || self.bDeferSetOrigin))
		{
			bDeferOrderFront = false;
			[super setAlphaValue:DeferOpacity];
		}
		else
		{
			[self performDeferredSetFrame];
		}
	}
}

- (void)performDeferredSetFrame
{
	if(bRenderInitialised && (self.bDeferSetFrame || self.bDeferSetOrigin))
	{
		SCOPED_AUTORELEASE_POOL;
		dispatch_block_t Block = ^{
			SCOPED_AUTORELEASE_POOL;
			if(!self.bDeferSetFrame && self.bDeferSetOrigin)
			{
				self.DeferFrame.size = [self frame].size;
			}
			
			[super setFrame:self.DeferFrame display:YES];
		};
		
		if([NSThread isMainThread])
		{
			Block();
		}
		else
		{
			dispatch_async(dispatch_get_main_queue(), Block);
		}
		
		self.bDeferSetFrame = false;
		self.bDeferSetOrigin = false;
	}
}

- (void)orderWindow:(NSWindowOrderingMode)OrderingMode relativeTo:(NSInteger)OtherWindowNumber
{
	SCOPED_AUTORELEASE_POOL;
	if([self alphaValue] > 0.0f)
	{
		[self performDeferredSetFrame];
	}
	[super orderWindow:OrderingMode relativeTo:OtherWindowNumber];
}

- (bool)roundedCorners
{
    return bRoundedCorners;
}

- (void)setRoundedCorners:(bool)bUseRoundedCorners
{
	bRoundedCorners = bUseRoundedCorners;
}

- (void)setAcceptsInput:(bool)InAcceptsInput
{
	bAcceptsInput = InAcceptsInput;
}

- (void)redrawContents
{
	SCOPED_AUTORELEASE_POOL;
	if(bNeedsRedraw && bForwardEvents && ([self isVisible] && [super alphaValue] > 0.0f))
	{
		NSNotification* Notification = [NSNotification notificationWithName:NSWindowRedrawContents object:self];
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Sync, @[ NSDefaultRunLoopMode, UE4NilEventMode, UE4ShowEventMode, UE4ResizeEventMode, UE4FullscreenEventMode, UE4CloseEventMode, UE4IMEEventMode ]);
	}
	bNeedsRedraw = false;
}

- (void)setWindowMode:(EWindowMode::Type)NewWindowMode
{
	WindowMode = NewWindowMode;
}

- (EWindowMode::Type)windowMode
{
	return WindowMode;
}

- (void)setDisplayReconfiguring:(bool)bIsDisplayReconfiguring
{
	bDisplayReconfiguring = bIsDisplayReconfiguring;
}

- (void)orderFrontAndMakeMain:(bool)bMain andKey:(bool)bKey
{
	SCOPED_AUTORELEASE_POOL;
	if ([NSApp isHidden] == NO)
	{
		[self orderFront:nil];

		if (bMain && [self canBecomeMainWindow] && self != [NSApp mainWindow])
		{
			[self makeMainWindow];
		}
		if (bKey && [self canBecomeKeyWindow] && self != [NSApp keyWindow])
		{
			[self makeKeyWindow];
		}
	}
}

// Following few methods overload NSWindow's methods from Cocoa API, so have to use Cocoa's BOOL (signed char), not bool (unsigned int)
- (BOOL)canBecomeMainWindow
{
	SCOPED_AUTORELEASE_POOL;
	return bAcceptsInput && ([self styleMask] != NSBorderlessWindowMask);
}

- (BOOL)canBecomeKeyWindow
{
	SCOPED_AUTORELEASE_POOL;
	return bAcceptsInput && ![self ignoresMouseEvents];
}

- (BOOL)validateMenuItem:(NSMenuItem*)MenuItem
{
	SCOPED_AUTORELEASE_POOL;
	// Borderless windows we use do not automatically handle first responder's actions, so we force it here
	return ([MenuItem action] == @selector(performClose:) || [MenuItem action] == @selector(performMiniaturize:) || [MenuItem action] == @selector(performZoom:)) ? YES : [super validateMenuItem:MenuItem];
}

- (void)setAlphaValue:(CGFloat)WindowAlpha
{
	if(!bRenderInitialised)
	{
		DeferOpacity = WindowAlpha;
		bDeferOrderFront = true;
	}
	else
	{
		SCOPED_AUTORELEASE_POOL;
		if([self isVisible] && WindowAlpha > 0.0f)
		{
			[self performDeferredSetFrame];
		}
		[super setAlphaValue:WindowAlpha];
	}
}

- (void)orderOut:(id)Sender
{
	SCOPED_AUTORELEASE_POOL;
	bDeferOrderFront = false;
	
	[super orderOut:Sender];
}

- (void)performClose:(id)Sender
{
	GameThreadCall(^{
		if (MacApplication)
		{
			MacApplication->OnWindowClose(self);
		}
	}, @[ NSDefaultRunLoopMode ], false);
}

- (void)performMiniaturize:(id)Sender
{
	SCOPED_AUTORELEASE_POOL;
	[self miniaturize: self];
}

- (void)performZoom:(id)Sender
{
	SCOPED_AUTORELEASE_POOL;
	bZoomed = !bZoomed;
	[self zoom: self];
}

- (void)destroy
{
	SCOPED_AUTORELEASE_POOL;
	bDeferOrderFront = false;
	[self close];
}

- (void)setFrame:(NSRect)FrameRect display:(BOOL)Flag
{
	SCOPED_AUTORELEASE_POOL;
	NSSize Size = [self frame].size;
	
	if (TargetWindowMode != EWindowMode::Windowed)
	{
		FrameRect.size = [[self screen] frame].size;
	}
	
	NSSize NewSize = FrameRect.size;
	if(!bRenderInitialised || ([self isVisible] && [super alphaValue] > 0.0f && (Size.width > 1 || Size.height > 1 || NewSize.width > 1 || NewSize.height > 1)))
	{
		[super setFrame:FrameRect display:Flag];
		self.bDeferSetFrame = false;
	}
	else
	{
		self.bDeferSetFrame = true;
		self.DeferFrame = FrameRect;
		if(self.bForwardEvents)
		{
			NSNotification* Notification = [NSNotification notificationWithName:NSWindowDidResizeNotification object:self];
			FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async, @[ NSDefaultRunLoopMode, UE4ResizeEventMode, UE4ShowEventMode ]);
		}
	}
}

- (void)setFrameOrigin:(NSPoint)Point
{
	SCOPED_AUTORELEASE_POOL;
	NSSize Size = [self frame].size;
	if(!bRenderInitialised || ([self isVisible] && [super alphaValue] > 0.0f && (Size.width > 1 || Size.height > 1)))
	{
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[super setFrameOrigin:Point];
		});
		self.bDeferSetOrigin = false;
	}
	else
	{
		self.bDeferSetOrigin = true;
		self.DeferFrame = NSMakeRect(Point.x, Point.y, self.DeferFrame.size.width, self.DeferFrame.size.height);
		NSNotification* Notification = [NSNotification notificationWithName:NSWindowDidMoveNotification object:self];
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async, @[ NSDefaultRunLoopMode, UE4ResizeEventMode, UE4ShowEventMode ]);
	}
}

- (void)keyDown:(NSEvent*)Event
{
	// @note Deliberately empty - we don't want OS X to handle keyboard input as it will recursively re-add events we aren't handling
}

- (void)keyUp:(NSEvent*)Event
{
	// @note Deliberately empty - we don't want OS X to handle keyboard input as it will recursively re-add events we aren't handling
}

- (void)windowWillEnterFullScreen:(NSNotification*)Notification
{
	// Handle clicking on the titlebar fullscreen item
	if(self.TargetWindowMode == EWindowMode::Windowed)
	{
		self.TargetWindowMode = EWindowMode::Fullscreen;
#if WITH_EDITORONLY_DATA
		if(GIsEditor)
		{
			self.TargetWindowMode = EWindowMode::WindowedFullscreen;
		}
#endif
		self.PreFullScreenRect = [self openGLFrame];
	}
}

- (void)windowDidEnterFullScreen:(NSNotification*)Notification
{
	WindowMode = self.TargetWindowMode;
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async, @[ NSDefaultRunLoopMode, UE4FullscreenEventMode ]);
	}
	FMacCursor* MacCursor = (FMacCursor*)MacApplication->Cursor.Get();
	if ( MacCursor )
	{
		NSSize WindowSize = [self frame].size;
		NSSize ViewSize = [self openGLFrame].size;
		float WidthScale = ViewSize.width / WindowSize.width;
		float HeightScale = ViewSize.height / WindowSize.height;
		MacCursor->SetMouseScaling(FVector2D(WidthScale, HeightScale));
	}
}

- (void)windowWillExitFullScreen:(NSNotification *)Notification
{
	if(self.TargetWindowMode != EWindowMode::Windowed)
	{
		self.TargetWindowMode = EWindowMode::Windowed;
	}
}

- (void)windowDidExitFullScreen:(NSNotification*)Notification
{
	WindowMode = EWindowMode::Windowed;
	self.TargetWindowMode = EWindowMode::Windowed;
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async, @[ NSDefaultRunLoopMode, UE4FullscreenEventMode ]);
	}
	FMacCursor* MacCursor = (FMacCursor*)MacApplication->Cursor.Get();
	if ( MacCursor )
	{
		MacCursor->SetMouseScaling(FVector2D(1.0f, 1.0f));
	}
}

- (void)windowDidBecomeKey:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	if([NSApp isHidden] == NO)
	{
		[self orderFrontAndMakeMain:false andKey:false];
	}

	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async, @[ NSDefaultRunLoopMode, UE4ShowEventMode, UE4CloseEventMode, UE4FullscreenEventMode ]);
	}
}

- (void)windowDidResignKey:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	[self setMovable: YES];
	[self setMovableByWindowBackground: NO];
	
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async, @[ NSDefaultRunLoopMode, UE4ShowEventMode, UE4CloseEventMode, UE4FullscreenEventMode ]);
	}
}

- (void)windowWillMove:(NSNotification*)Notification
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async, @[ NSDefaultRunLoopMode, UE4ResizeEventMode, UE4ShowEventMode, UE4FullscreenEventMode ]);
	}
}

- (void)windowDidMove:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	bZoomed = [self isZoomed];
	
	NSView* OpenGLView = [self openGLView];
	[[NSNotificationCenter defaultCenter] postNotificationName:NSViewGlobalFrameDidChangeNotification object:OpenGLView];
	
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async, @[ NSDefaultRunLoopMode, UE4ResizeEventMode, UE4ShowEventMode, UE4FullscreenEventMode ]);
	}
}

- (void)windowDidChangeScreen:(NSNotification*)Notification
{
	// The windowdidChangeScreen notification only comes after you finish dragging.
	// It does however, work fine for handling display arrangement changes that cause a window to go offscreen.
	if(bDisplayReconfiguring)
	{
		SCOPED_AUTORELEASE_POOL;
		NSScreen* Screen = [self screen];
		NSRect Frame = [self frame];
		NSRect VisibleFrame = [Screen visibleFrame];
		if(NSContainsRect(VisibleFrame, Frame) == NO)
		{
			// May need to scale the window to fit if it is larger than the new display.
			if (Frame.size.width > VisibleFrame.size.width || Frame.size.height > VisibleFrame.size.height)
			{
				NSRect NewFrame;
				NewFrame.size.width = Frame.size.width > VisibleFrame.size.width ? VisibleFrame.size.width : Frame.size.width;
				NewFrame.size.height = Frame.size.height > VisibleFrame.size.height ? VisibleFrame.size.height : Frame.size.height;
				NewFrame.origin = VisibleFrame.origin;
				
				[self setFrame:NewFrame display:NO];
			}
			else
			{
				NSRect Intersection = NSIntersectionRect(VisibleFrame, Frame);
				NSPoint Origin = Frame.origin;
				
				// If there's at least something on screen, try shifting it entirely on screen.
				if(Intersection.size.width > 0 && Intersection.size.height > 0)
				{
					CGFloat X = Frame.size.width - Intersection.size.width;
					CGFloat Y = Frame.size.height - Intersection.size.height;
					
					if(Intersection.size.width+Intersection.origin.x >= VisibleFrame.size.width+VisibleFrame.origin.x)
					{
						Origin.x -= X;
					}
					else if(Origin.x < VisibleFrame.origin.x)
					{
						Origin.x += X;
					}
					
					if(Intersection.size.height+Intersection.origin.y >= VisibleFrame.size.height+VisibleFrame.origin.y)
					{
						Origin.y -= Y;
					}
					else if(Origin.y < VisibleFrame.origin.y)
					{
						Origin.y += Y;
					}
				}
				else
				{
					Origin = VisibleFrame.origin;
				}
				
				[self setFrameOrigin:Origin];
			}
		}
	}
}

- (void)windowDidResize:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	bZoomed = [self isZoomed];
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async, @[ NSDefaultRunLoopMode, UE4ResizeEventMode, UE4ShowEventMode, UE4FullscreenEventMode ]);
	}
	bNeedsRedraw = true;
}

- (void)windowWillClose:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	if(self.bForwardEvents && MacApplication)
	{
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Sync, @[ NSDefaultRunLoopMode, UE4CloseEventMode ]);
	}
	self.bForwardEvents = false;
	[self setDelegate:nil];
}

- (void)mouseDown:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)rightMouseDown:(NSEvent*)Event
{
	// Really we shouldn't be doing this - on OS X only left-click changes focus,
	// but for the moment it is easier than changing Slate.
	SCOPED_AUTORELEASE_POOL;
	if([self canBecomeKeyWindow])
	{
		[self makeKeyWindow];
	}
	
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)otherMouseDown:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)mouseUp:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)rightMouseUp:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (void)otherMouseUp:(NSEvent*)Event
{
	if(self.bForwardEvents)
	{
		FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
	}
}

- (NSDragOperation)draggingEntered:(id < NSDraggingInfo >)Sender
{
	return NSDragOperationGeneric;
}

- (void)draggingExited:(id < NSDraggingInfo >)Sender
{
	if(self.bForwardEvents)
	{
		SCOPED_AUTORELEASE_POOL;
		NSNotification* Notification = [NSNotification notificationWithName:NSDraggingExited object:Sender];
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async);
	}
}

- (NSDragOperation)draggingUpdated:(id < NSDraggingInfo >)Sender
{
	if(self.bForwardEvents)
	{
		SCOPED_AUTORELEASE_POOL;
		NSNotification* Notification = [NSNotification notificationWithName:NSDraggingUpdated object:Sender];
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async);
	}
	return NSDragOperationGeneric;
}

- (BOOL)prepareForDragOperation:(id < NSDraggingInfo >)Sender
{
	if(self.bForwardEvents)
	{
		SCOPED_AUTORELEASE_POOL;
		NSNotification* Notification = [NSNotification notificationWithName:NSPrepareForDragOperation object:Sender];
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async);
	}
	return YES;
}

- (BOOL)performDragOperation:(id < NSDraggingInfo >)Sender
{
	if(self.bForwardEvents)
	{
		SCOPED_AUTORELEASE_POOL;
		NSNotification* Notification = [NSNotification notificationWithName:NSPerformDragOperation object:Sender];
		FMacEvent::SendToGameRunLoop(Notification, EMacEventSendMethod::Async);
	}
	return YES;
}

- (BOOL)isMovable
{
	SCOPED_AUTORELEASE_POOL;
	BOOL Movable = [super isMovable];
	if(Movable && bRenderInitialised && MacApplication)
	{
		Movable &= (BOOL)(GameThreadReturn(^{ return MacApplication->IsWindowMovable(self, NULL); }, @[ NSDefaultRunLoopMode, UE4NilEventMode, UE4ShowEventMode, UE4ResizeEventMode, UE4FullscreenEventMode, UE4CloseEventMode, UE4IMEEventMode ]));
	}
	return Movable;
}

@end

/**
 * Custom window class used for mouse capture
 */
@implementation FMouseCaptureWindow

- (id)initWithTargetWindow: (FCocoaWindow*)Window
{
	self = [super initWithContentRect: [[Window screen] frame] styleMask: NSBorderlessWindowMask backing: NSBackingStoreBuffered defer: NO];
	[self setBackgroundColor: [NSColor clearColor]];
	[self setOpaque: NO];
	[self setLevel: NSMainMenuWindowLevel + 1];
	[self setIgnoresMouseEvents: NO];
	[self setAcceptsMouseMovedEvents: YES];
	[self setHidesOnDeactivate: YES];
	
	TargetWindow = Window;
	
	return self;
}

- (FCocoaWindow*)targetWindow
{
	return TargetWindow;
}

- (void)setTargetWindow: (FCocoaWindow*)Window
{
	TargetWindow = Window;
}

- (void)mouseDown:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)rightMouseDown:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)otherMouseDown:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)mouseUp:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)rightMouseUp:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

- (void)otherMouseUp:(NSEvent*)Event
{
	FMacEvent::SendToGameRunLoop(Event, EMacEventSendMethod::Async);
}

@end
