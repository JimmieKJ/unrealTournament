// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "CocoaWindow.h"
#include "MacApplication.h"
#include "CocoaTextView.h"
#include "CocoaThread.h"
#include "MacCursor.h"

NSString* NSDraggingExited = @"NSDraggingExited";
NSString* NSDraggingUpdated = @"NSDraggingUpdated";
NSString* NSPrepareForDragOperation = @"NSPrepareForDragOperation";
NSString* NSPerformDragOperation = @"NSPerformDragOperation";

/**
 * Custom window class used for input handling
 */
@implementation FCocoaWindow

@synthesize TargetWindowMode;
@synthesize PreFullScreenRect;

- (id)initWithContentRect:(NSRect)ContentRect styleMask:(NSUInteger)Style backing:(NSBackingStoreType)BufferingType defer:(BOOL)Flag
{
	WindowMode = EWindowMode::Windowed;
	bAcceptsInput = false;
	bDisplayReconfiguring = false;
	bRenderInitialized = false;
	Opacity = 0.0f;

	id NewSelf = [super initWithContentRect:ContentRect styleMask:Style backing:BufferingType defer:Flag];
	if (NewSelf)
	{
		bZoomed = [super isZoomed];
		self.TargetWindowMode = EWindowMode::Windowed;
		[super setAlphaValue:Opacity];
		self.PreFullScreenRect = [super frame];
	}
	return NewSelf;
}

- (NSRect)openGLFrame
{
	SCOPED_AUTORELEASE_POOL;
	if (self.TargetWindowMode == EWindowMode::Fullscreen || WindowMode == EWindowMode::Fullscreen)
	{
		return {{0, 0}, self.PreFullScreenRect.size};
	}
	else if ([self styleMask] & NSTexturedBackgroundWindowMask)
	{
		return [self frame];
	}
	else
	{
		return [[self contentView] frame];
	}
}

- (NSView*)openGLView
{
	SCOPED_AUTORELEASE_POOL;
	if (FPlatformMisc::IsRunningOnMavericks() && [self styleMask] & (NSTexturedBackgroundWindowMask))
	{
		NSView* SuperView = [[self contentView] superview];
		for (NSView* View in [SuperView subviews])
		{
			if ([View isKindOfClass:[FCocoaTextView class]])
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

- (void)setAcceptsInput:(bool)InAcceptsInput
{
	bAcceptsInput = InAcceptsInput;
}

- (void)setWindowMode:(EWindowMode::Type)NewWindowMode
{
	WindowMode = NewWindowMode;
	
	NSView* OpenGLView = [self openGLView];
	[[NSNotificationCenter defaultCenter] postNotificationName:NSViewGlobalFrameDidChangeNotification object:OpenGLView];
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
	return bAcceptsInput && ![self ignoresMouseEvents];
}

- (BOOL)canBecomeKeyWindow
{
	SCOPED_AUTORELEASE_POOL;
	return bAcceptsInput && ([self styleMask] != NSBorderlessWindowMask);
}

- (BOOL)validateMenuItem:(NSMenuItem*)MenuItem
{
	SCOPED_AUTORELEASE_POOL;
	// Borderless windows we use do not automatically handle first responder's actions, so we force it here
	return ([MenuItem action] == @selector(performClose:) || [MenuItem action] == @selector(miniaturize:) || [MenuItem action] == @selector(zoom:)) ? YES : [super validateMenuItem:MenuItem];
}

- (void)setAlphaValue:(CGFloat)WindowAlpha
{
	Opacity = WindowAlpha;
	if (bRenderInitialized)
	{
		[super setAlphaValue:WindowAlpha];
	}
}

- (void)startRendering
{
	if (!bRenderInitialized)
	{
		bRenderInitialized = true;
		[super setAlphaValue:Opacity];
	}
}

- (bool)isRenderInitialized
{
	return bRenderInitialized;
}

- (void)performClose:(id)Sender
{
	GameThreadCall(^{
		if (MacApplication)
		{
			TSharedPtr<FMacWindow> Window = MacApplication->FindWindowByNSWindow(self);
			if (Window.IsValid())
			{
				MacApplication->CloseWindow(Window.ToSharedRef());
			}
		}
	}, @[ NSDefaultRunLoopMode ], false);
}

- (void)performZoom:(id)Sender
{
}

- (void)zoom:(id)Sender
{
	SCOPED_AUTORELEASE_POOL;
	bZoomed = !bZoomed;
	[super zoom:Sender];
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
	if (self.TargetWindowMode == EWindowMode::Windowed)
	{
		self.PreFullScreenRect.origin = [self frame].origin;
		self.PreFullScreenRect.size = [self openGLFrame].size;
		
		// Use the current default fullscreen mode when switching via the OS button
		auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.FullScreenMode"));
		check(CVar);
		self.TargetWindowMode = CVar->GetValueOnAnyThread() == 0 ? EWindowMode::Fullscreen : EWindowMode::WindowedFullscreen;
		
#if WITH_EDITORONLY_DATA // Always use WindowedFullscreen for the Editor or bad things happen
		if (GIsEditor)
		{
			self.TargetWindowMode = EWindowMode::WindowedFullscreen;
		}
#endif
	}
}

- (void)windowDidEnterFullScreen:(NSNotification*)Notification
{
	WindowMode = self.TargetWindowMode;

	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}

	FMacCursor* MacCursor = (FMacCursor*)MacApplication->Cursor.Get();
	if (MacCursor)
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
	if (self.TargetWindowMode != EWindowMode::Windowed)
	{
		self.TargetWindowMode = EWindowMode::Windowed;
	}
}

- (void)windowDidExitFullScreen:(NSNotification*)Notification
{
	WindowMode = EWindowMode::Windowed;
	self.TargetWindowMode = EWindowMode::Windowed;
	
	// Remove any primary fullscreen behaviour here, we don't support it properly.
	NSWindowCollectionBehavior Behaviour = [self collectionBehavior];
	Behaviour &= ~(NSWindowCollectionBehaviorFullScreenPrimary);
	Behaviour |= NSWindowCollectionBehaviorFullScreenAuxiliary;
	[self setCollectionBehavior: Behaviour];

	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}

	FMacCursor* MacCursor = (FMacCursor*)MacApplication->Cursor.Get();
	if (MacCursor)
	{
		MacCursor->SetMouseScaling(FVector2D(1.0f, 1.0f));
	}
}

- (void)windowDidBecomeMain:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	if ([NSApp isHidden] == NO)
	{
		[self orderFrontAndMakeMain:false andKey:false];
	}

	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
}

- (void)windowDidResignMain:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	[self setMovable: YES];
	[self setMovableByWindowBackground: NO];

	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
}

- (void)windowWillMove:(NSNotification*)Notification
{
	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
}

- (void)windowDidMove:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	bZoomed = [self isZoomed];
	
	NSView* OpenGLView = [self openGLView];
	[[NSNotificationCenter defaultCenter] postNotificationName:NSViewGlobalFrameDidChangeNotification object:OpenGLView];
	
	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
}

- (void)windowDidChangeScreen:(NSNotification*)Notification
{
	// The windowdidChangeScreen notification only comes after you finish dragging.
	// It does however, work fine for handling display arrangement changes that cause a window to go offscreen.
	if (bDisplayReconfiguring)
	{
		SCOPED_AUTORELEASE_POOL;
		NSScreen* Screen = [self screen];
		NSRect Frame = [self frame];
		NSRect VisibleFrame = [Screen visibleFrame];
		if (NSContainsRect(VisibleFrame, Frame) == NO)
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
				if (Intersection.size.width > 0 && Intersection.size.height > 0)
				{
					CGFloat X = Frame.size.width - Intersection.size.width;
					CGFloat Y = Frame.size.height - Intersection.size.height;
					
					if (Intersection.size.width+Intersection.origin.x >= VisibleFrame.size.width+VisibleFrame.origin.x)
					{
						Origin.x -= X;
					}
					else if (Origin.x < VisibleFrame.origin.x)
					{
						Origin.x += X;
					}
					
					if (Intersection.size.height+Intersection.origin.y >= VisibleFrame.size.height+VisibleFrame.origin.y)
					{
						Origin.y -= Y;
					}
					else if (Origin.y < VisibleFrame.origin.y)
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

- (void)windowWillStartLiveResize:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
}

- (void)windowDidEndLiveResize:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
}

- (void)windowDidResize:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	bZoomed = [self isZoomed];
	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
}

- (void)windowWillClose:(NSNotification*)Notification
{
	SCOPED_AUTORELEASE_POOL;
	[self setDelegate:nil];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)Sender
{
	return NSDragOperationGeneric;
}

- (void)draggingExited:(id <NSDraggingInfo>)Sender
{
	SCOPED_AUTORELEASE_POOL;
	NSNotification* Notification = [NSNotification notificationWithName:NSDraggingExited object:Sender];
	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
}

- (NSDragOperation)draggingUpdated:(id <NSDraggingInfo>)Sender
{
	SCOPED_AUTORELEASE_POOL;
	NSNotification* Notification = [NSNotification notificationWithName:NSDraggingUpdated object:Sender];
	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
	return NSDragOperationGeneric;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)Sender
{
	SCOPED_AUTORELEASE_POOL;
	NSNotification* Notification = [NSNotification notificationWithName:NSPrepareForDragOperation object:Sender];
	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
	return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)Sender
{
	SCOPED_AUTORELEASE_POOL;
	NSNotification* Notification = [NSNotification notificationWithName:NSPerformDragOperation object:Sender];
	if (MacApplication)
	{
		MacApplication->DeferEvent(Notification);
	}
	return YES;
}

@end
