// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "MacWindow.h"
#include "MacApplication.h"
#include "MacCursor.h"
#include "MacEvent.h"
#include "CocoaTextView.h"
#include "CocoaThread.h"

FMacWindow::~FMacWindow()
{
	// While on Windows invalid HWNDs fail silently, accessing an invalid NSWindow is fatal.
	// So instead we release the window here.
	if(WindowHandle)
	{
		NSWindow* Window = WindowHandle;
		dispatch_async(dispatch_get_main_queue(), ^{
			SCOPED_AUTORELEASE_POOL;
			[Window release];
		});
		WindowHandle = nil;
	}
}

TSharedRef<FMacWindow> FMacWindow::Make()
{
	// First, allocate the new native window object.  This doesn't actually create a native window or anything,
	// we're simply instantiating the object so that we can keep shared references to it.
	return MakeShareable( new FMacWindow() );
}

void FMacWindow::Initialize( FMacApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FMacWindow >& InParent, const bool bShowImmediately )
{
	SCOPED_AUTORELEASE_POOL;

	OwningApplication = Application;
	Definition = InDefinition;

	// Finally, let's initialize the new native window object.  Calling this function will often cause OS
	// window messages to be sent! (such as activation messages)

	const int32 X = FMath::TruncToInt( Definition->XDesiredPositionOnScreen );

	NSScreen* TargetScreen = Application->FindScreenByPoint( X, Definition->YDesiredPositionOnScreen );

	int32 Y = FMath::TruncToInt( Definition->YDesiredPositionOnScreen );

	// Make sure it's not under the menu bar on whatever display being targeted
	const int32 ScreenHeight = FMath::TruncToInt( [TargetScreen frame].size.height );
	const int32 VisibleHeight = FMath::TruncToInt( [TargetScreen visibleFrame].origin.y + [TargetScreen visibleFrame].size.height );
	Y = FMath::Max( Y, ScreenHeight - VisibleHeight );

	const int32 SizeX = FMath::Max(FMath::TruncToInt( Definition->WidthDesiredOnScreen ), 1);
	const int32 SizeY = FMath::Max(FMath::TruncToInt( Definition->HeightDesiredOnScreen ), 1);

	PositionX = X;
	PositionY = Y;

	const int32 InvertedY = FPlatformMisc::ConvertSlateYPositionToCocoa(Y) - SizeY + 1;
	const NSRect ViewRect = NSMakeRect(X, InvertedY, SizeX, SizeY);

	uint32 WindowStyle = 0;
	if( Definition->IsRegularWindow )
	{
		WindowStyle = NSClosableWindowMask;
		
		// In order to support rounded, shadowed windows set the window to be
		// titled - we'll set the OpenGL view to cover the whole window
		WindowStyle |= NSTitledWindowMask | NSTexturedBackgroundWindowMask;
		
		if( Definition->SupportsMinimize )
		{
			WindowStyle |= NSMiniaturizableWindowMask;
		}
		if( Definition->SupportsMaximize )
		{
			WindowStyle |= NSResizableWindowMask;
		}
	}
	else
	{
		WindowStyle = NSBorderlessWindowMask;
	}

	if( Definition->HasOSWindowBorder )
	{
		WindowStyle |= NSTitledWindowMask;
		WindowStyle &= ~(NSTexturedBackgroundWindowMask);
	}

	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		WindowHandle = [[FCocoaWindow alloc] initWithContentRect: ViewRect styleMask: WindowStyle backing: NSBackingStoreBuffered defer: NO];
		
		if( WindowHandle != nullptr )
		{
			[WindowHandle setReleasedWhenClosed:NO];
			[WindowHandle setWindowMode: EWindowMode::Windowed];
			[WindowHandle setAcceptsInput: Definition->AcceptsInput];
			[WindowHandle setDisplayReconfiguring: false];
			[WindowHandle setAcceptsMouseMovedEvents: YES];
			[WindowHandle setDelegate: WindowHandle];

			// @todo: We really need a window type in tab manager to know what window level to use and whether or not a window should hide on deactivate
			if (Definition->IsModalWindow)
			{
				NSInteger Level = [NSApp isActive] ? NSModalPanelWindowLevel : NSNormalWindowLevel;
				[WindowHandle setLevel: Level];
			}
			else if (Definition->IsRegularWindow)
			{
				if (InParent.IsValid())
				{
					[WindowHandle setLevel: NSFloatingWindowLevel];
				}
				else
				{
					[WindowHandle setLevel: NSNormalWindowLevel];
				}
			}
			else if (!Definition->SupportsMaximize && !Definition->SupportsMinimize)
			{
				[WindowHandle setLevel: NSFloatingWindowLevel];
			}
			else
			{
				[WindowHandle setLevel: NSModalPanelWindowLevel];
				[WindowHandle setHidesOnDeactivate: YES];
			}
			
			// Use of rounded corners will always render with system values for rounding
			[WindowHandle setRoundedCorners: (Definition->CornerRadius != 0)];
			
			if( !Definition->HasOSWindowBorder )
			{
				[WindowHandle setBackgroundColor: [NSColor darkGrayColor]];
				[WindowHandle setHasShadow: YES];
			}

			[WindowHandle setOpaque: NO];

			[WindowHandle setMinSize:NSMakeSize(Definition->SizeLimits.GetMinWidth().Get(10.0f), Definition->SizeLimits.GetMinHeight().Get(10.0f))];
			[WindowHandle setMaxSize:NSMakeSize(Definition->SizeLimits.GetMaxWidth().Get(10000.0f), Definition->SizeLimits.GetMaxHeight().Get(10000.0f))];

			ReshapeWindow( X, Y, SizeX, SizeY );

			if( Definition->IsRegularWindow )
			{
				CFStringRef CFName = FPlatformString::TCHARToCFString( *Definition->Title );
				[WindowHandle setTitle: ( NSString *)CFName];
				CFRelease( CFName );

				[NSApp addWindowsItem: WindowHandle title: [WindowHandle title] filename: NO];

				// Tell Cocoa that we are opting into drag and drop.
				// Only makes sense for regular windows (windows that last a while.)
				[WindowHandle registerForDraggedTypes:@[NSFilenamesPboardType, NSPasteboardTypeString]];

				if( Definition->HasOSWindowBorder )
				{
					[WindowHandle setCollectionBehavior: NSWindowCollectionBehaviorFullScreenPrimary|NSWindowCollectionBehaviorDefault|NSWindowCollectionBehaviorManaged|NSWindowCollectionBehaviorParticipatesInCycle];
				}
				else
				{
					[WindowHandle setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary|NSWindowCollectionBehaviorDefault|NSWindowCollectionBehaviorManaged|NSWindowCollectionBehaviorParticipatesInCycle];
				}
			}
			else if(Definition->AppearsInTaskbar)
			{
				if ( !Definition->Title.IsEmpty() )
				{
					CFStringRef CFName = FPlatformString::TCHARToCFString( *Definition->Title );
					[WindowHandle setTitle: ( NSString *)CFName];
					CFRelease( CFName );
					
					[NSApp addWindowsItem: WindowHandle title: [WindowHandle title] filename: NO];
				}
				
				[WindowHandle setCollectionBehavior:NSWindowCollectionBehaviorFullScreenAuxiliary|NSWindowCollectionBehaviorDefault|NSWindowCollectionBehaviorManaged|NSWindowCollectionBehaviorParticipatesInCycle];
			}
			else
			{
				[WindowHandle setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces|NSWindowCollectionBehaviorTransient|NSWindowCollectionBehaviorIgnoresCycle];
			}

			if( Definition->SupportsTransparency )
			{
				SetOpacity( Definition->Opacity );
			}
			else
			{
				SetOpacity( 1.0f );
			}
		}
		else
		{
			// @todo Error message should be localized!
			NSRunInformationalAlertPanel( @"Error", @"Window creation failed!", @"Yes", NULL, NULL );
			check(0);
		}
	}, UE4ShowEventMode, true);
	
	if( WindowHandle == nullptr )
	{
		return;
	}
}

FMacWindow::FMacWindow()
	: WindowHandle(NULL)
	, bIsVisible(false)
	, bIsClosed(false)
{
	PreFullscreenWindowRect.origin.x = PreFullscreenWindowRect.origin.y = PreFullscreenWindowRect.size.width = PreFullscreenWindowRect.size.height = 0.0f;
}

FCocoaWindow* FMacWindow::GetWindowHandle() const
{
	return WindowHandle;
}

void FMacWindow::ReshapeWindow( int32 X, int32 Y, int32 Width, int32 Height )
{
	if (WindowHandle)
	{
		SCOPED_AUTORELEASE_POOL;
		
		const TSharedRef<FGenericApplicationMessageHandler> MessageHandler = OwningApplication->MessageHandler;
		MessageHandler->BeginReshapingWindow( SharedThis( this ) );
		
		if(GetWindowMode() == EWindowMode::Windowed || GetWindowMode() == EWindowMode::WindowedFullscreen)
		{
			const int32 InvertedY = FPlatformMisc::ConvertSlateYPositionToCocoa(Y) - Height + 1;
			NSRect Rect = NSMakeRect(X, InvertedY, FMath::Max(Width, 1), FMath::Max(Height, 1));
			if (Definition->HasOSWindowBorder)
			{
				Rect = [WindowHandle frameRectForContentRect: Rect];
			}
			
			if ( !NSEqualRects([WindowHandle frame], Rect) )
			{
				MainThreadCall(^{
					SCOPED_AUTORELEASE_POOL;
					BOOL DisplayIfNeeded = (GetWindowMode() == EWindowMode::Windowed);
					
					[WindowHandle setFrame: Rect display:DisplayIfNeeded];
					
					// Force resize back to screen size in fullscreen - not ideally pretty but means we don't
					// have to subvert the OS X or UE fullscreen handling events elsewhere.
					if(GetWindowMode() != EWindowMode::Windowed)
					{
						[WindowHandle setFrame: [WindowHandle screen].frame display:YES];
					}
					
					WindowHandle->bZoomed = [WindowHandle isZoomed];
				}, UE4ResizeEventMode, true);
			}
		}
		else
		{
			NSRect NewRect = NSMakeRect(WindowHandle.PreFullScreenRect.origin.x, WindowHandle.PreFullScreenRect.origin.y, (CGFloat)Width, (CGFloat)Height);
			if ( !NSEqualRects(WindowHandle.PreFullScreenRect, NewRect) )
			{
				WindowHandle.PreFullScreenRect = NewRect;
				FMacEvent::SendToGameRunLoop([NSNotification notificationWithName:NSWindowDidResizeNotification object:WindowHandle], EMacEventSendMethod::Sync, @[ NSDefaultRunLoopMode, UE4ResizeEventMode, UE4ShowEventMode, UE4FullscreenEventMode ]);
			}
		}
		
		MessageHandler->FinishedReshapingWindow( SharedThis( this ) );
	}
}

bool FMacWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	SCOPED_AUTORELEASE_POOL;
	bool const bIsFullscreen = (GetWindowMode() == EWindowMode::Fullscreen);
	const NSRect Frame = (!bIsFullscreen) ? [WindowHandle screen].frame : PreFullscreenWindowRect;
	X = Frame.origin.x;
	Y = Frame.origin.y;
	Width = Frame.size.width;
	Height = Frame.size.height;
	return true;
}

void FMacWindow::MoveWindowTo( int32 X, int32 Y )
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		const int32 InvertedY = FPlatformMisc::ConvertSlateYPositionToCocoa(Y) - [WindowHandle openGLFrame].size.height + 1;
		[WindowHandle setFrameOrigin: NSMakePoint(X, InvertedY)];
	}, UE4ResizeEventMode, true);
}

void FMacWindow::BringToFront( bool bForce )
{
	if (!bIsClosed && bIsVisible)
	{
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[WindowHandle orderFrontAndMakeMain:IsRegularWindow() andKey:IsRegularWindow()];
		}, UE4ShowEventMode, true);
	}
}

void FMacWindow::Destroy()
{
	if( WindowHandle )
	{
		SCOPED_AUTORELEASE_POOL;
		FCocoaWindow* Window = WindowHandle;

		bIsClosed = true;

		if( MacApplication->OnWindowDestroyed( Window ) )
		{
			// This FMacWindow may have been destructed by now & so the WindowHandle will probably be invalid memory.
			bool const bIsKey = [Window isKeyWindow];
			
			// Then change the focus to something useful, either the previous in the stack
			TSharedPtr<FMacWindow> KeyWindow = MacApplication->GetKeyWindow();
			if( KeyWindow.IsValid() && bIsKey && KeyWindow->GetOSWindowHandle() && ![(FCocoaWindow*)KeyWindow->GetOSWindowHandle() isMiniaturized] )
			{
				// Activate specified previous window if still present, provided it isn't minimized
				KeyWindow->SetWindowFocus();
			}
			
			// Close the window
			MainThreadCall(^{
				SCOPED_AUTORELEASE_POOL;
				[Window destroy];
			}, UE4CloseEventMode, true);
		}
	}
}

void FMacWindow::Minimize()
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		[WindowHandle miniaturize:nil];
	}, UE4ResizeEventMode, true);
}

void FMacWindow::Maximize()
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		if( ![WindowHandle isZoomed] )
		{
			WindowHandle->bZoomed = true;
			[WindowHandle zoom:nil];
		}
	}, UE4ResizeEventMode, true);
}

void FMacWindow::Restore()
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		if( [WindowHandle isZoomed] )
		{
			WindowHandle->bZoomed = !WindowHandle->bZoomed;
			[WindowHandle zoom:nil];
		}
		else
		{
			WindowHandle->bZoomed = false;
			[WindowHandle deminiaturize:nil];
		}
	}, UE4ResizeEventMode, true);
}

void FMacWindow::Show()
{
	SCOPED_AUTORELEASE_POOL;
	if (!bIsClosed && !bIsVisible)
	{
		const bool bMakeMainAndKey = ([WindowHandle canBecomeKeyWindow] && Definition->ActivateWhenFirstShown);
		
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[WindowHandle orderFrontAndMakeMain:bMakeMainAndKey andKey:bMakeMainAndKey];
		}, UE4ShowEventMode, true);
		
		bIsVisible = true;
	}
}

void FMacWindow::Hide()
{
	if (bIsVisible)
	{
		SCOPED_AUTORELEASE_POOL;
		bIsVisible = false;
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[WindowHandle orderOut:nil];
		}, UE4CloseEventMode, false);
	}
}

void FMacWindow::SetWindowMode( EWindowMode::Type NewWindowMode )
{
	SCOPED_AUTORELEASE_POOL;

	// In OS X fullscreen and windowed fullscreen are the same
	bool bMakeFullscreen = NewWindowMode != EWindowMode::Windowed;
	bool bIsFullscreen = GetWindowMode() != EWindowMode::Windowed;

	if( bIsFullscreen != bMakeFullscreen )
	{
		bool WindowIsFullScreen = !bMakeFullscreen;
		
		NSWindowCollectionBehavior Behaviour = [WindowHandle collectionBehavior];
		if(bMakeFullscreen)
		{
			Behaviour &= ~(NSWindowCollectionBehaviorFullScreenAuxiliary);
			Behaviour |= NSWindowCollectionBehaviorFullScreenPrimary;
		}
		
		if(!bIsFullscreen)
		{
			PreFullscreenWindowRect = [WindowHandle openGLFrame];
			WindowHandle.PreFullScreenRect = PreFullscreenWindowRect;
		}
		
		WindowHandle.TargetWindowMode = NewWindowMode;
		
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[WindowHandle setCollectionBehavior: Behaviour];
			[WindowHandle toggleFullScreen:nil];
		}, UE4FullscreenEventMode, true);
		
		// Ensure that the window has transitioned BEFORE leaving this function
		// this prevents problems with failure to correctly update mouse locks
		// and OpenGL contexts due to bad event ordering.
		do
		{
			FPlatformMisc::PumpMessages(true);
			WindowIsFullScreen = [WindowHandle windowMode] != EWindowMode::Windowed;
		} while(WindowIsFullScreen != bMakeFullscreen);
		
		if(!bMakeFullscreen && !Definition->HasOSWindowBorder)
		{
			Behaviour &= ~(NSWindowCollectionBehaviorFullScreenPrimary);
			Behaviour |= NSWindowCollectionBehaviorFullScreenAuxiliary;
		}
	}
}

EWindowMode::Type FMacWindow::GetWindowMode() const
{
	return [WindowHandle windowMode];
}

bool FMacWindow::IsMaximized() const
{
	return WindowHandle->bZoomed;
}

bool FMacWindow::IsMinimized() const
{
	SCOPED_AUTORELEASE_POOL;
	return [WindowHandle isMiniaturized];
}

bool FMacWindow::IsVisible() const
{
	SCOPED_AUTORELEASE_POOL;
	return bIsVisible && [NSApp isHidden] == false;
}

bool FMacWindow::GetRestoredDimensions(int32& X, int32& Y, int32& Width, int32& Height)
{
	SCOPED_AUTORELEASE_POOL;

	NSRect Frame = [WindowHandle frame];

	X = Frame.origin.x;
	Y = FPlatformMisc::ConvertSlateYPositionToCocoa(Frame.origin.y) - Frame.size.height + 1;

	Width = Frame.size.width;
	Height = Frame.size.height;

	return true;
}

void FMacWindow::SetWindowFocus()
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		[WindowHandle orderFrontAndMakeMain:true andKey:true];
	}, UE4ShowEventMode, true);
}

void FMacWindow::SetOpacity( const float InOpacity )
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		[WindowHandle setAlphaValue:InOpacity];
	}, UE4NilEventMode, true);
}

void FMacWindow::Enable( bool bEnable )
{
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		[WindowHandle setIgnoresMouseEvents: !bEnable];
	}, UE4NilEventMode, true);
}

bool FMacWindow::IsPointInWindow( int32 X, int32 Y ) const
{
	SCOPED_AUTORELEASE_POOL;
	bool PointInWindow = false;
	if(![WindowHandle isMiniaturized])
	{
		NSRect WindowFrame = [WindowHandle frame];
		NSRect VisibleFrame = WindowFrame;
		VisibleFrame.origin = NSMakePoint(0, 0);
		// Only the editor needs to handle the space-per-display logic introduced in Mavericks.
	#if WITH_EDITOR
		// Only fetch the spans-displays once - it requires a log-out to change.
		// Note that we have no way to tell if the current setting is actually in effect,
		// so this won't work if the user schedules a change but doesn't logout to confirm it.
		static bool bScreensHaveSeparateSpaces = false;
		static bool bSettingFetched = false;
		if(!bSettingFetched)
		{
			bSettingFetched = true;
			bScreensHaveSeparateSpaces = [NSScreen screensHaveSeparateSpaces];
		}
		if(bScreensHaveSeparateSpaces)
		{
			NSRect ScreenFrame = [[WindowHandle screen] frame];
			NSRect Intersection = NSIntersectionRect(ScreenFrame, WindowFrame);
			VisibleFrame.size = Intersection.size;
			VisibleFrame.origin.x = WindowFrame.origin.x < ScreenFrame.origin.x ? (ScreenFrame.origin.x - WindowFrame.origin.x) : 0;
			VisibleFrame.origin.y = (WindowFrame.origin.y + WindowFrame.size.height) > (ScreenFrame.origin.y + ScreenFrame.size.height) ? (WindowFrame.size.height - Intersection.size.height) : 0;
		}
	#endif
		
		if([WindowHandle isOnActiveSpace])
		{
			FMacCursor* MacCursor = (FMacCursor*)MacApplication->Cursor.Get();
			FVector2D MouseScale = MacCursor ? MacCursor->GetMouseScaling() : FVector2D(1.0f, 1.0f);
			PointInWindow = (NSPointInRect(NSMakePoint(X / MouseScale.X, Y / MouseScale.Y), VisibleFrame) == YES);
		}
	}
	return PointInWindow;
}

int32 FMacWindow::GetWindowBorderSize() const
{
	return 0;
}

bool FMacWindow::IsForegroundWindow() const
{
	SCOPED_AUTORELEASE_POOL;
	return [WindowHandle isMainWindow];
}

void FMacWindow::SetText(const TCHAR* const Text)
{
	CFStringRef CFName = FPlatformString::TCHARToCFString( Text );
	MainThreadCall(^{
		SCOPED_AUTORELEASE_POOL;
		[WindowHandle setTitle: (NSString*)CFName];
		if(IsRegularWindow())
		{
			[NSApp changeWindowsItem: WindowHandle title: (NSString*)CFName filename: NO];
		}
		CFRelease( CFName );
	}, UE4NilEventMode, true);
}

bool FMacWindow::IsRegularWindow() const
{
	return Definition->IsRegularWindow;
}

void FMacWindow::AdjustCachedSize( FVector2D& Size ) const
{
	// No adjustmnet needed
}

void FMacWindow::OnDisplayReconfiguration(CGDirectDisplayID Display, CGDisplayChangeSummaryFlags Flags)
{
	if(WindowHandle)
	{
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			if(Flags & kCGDisplayBeginConfigurationFlag)
			{
				[WindowHandle setMovable: YES];
				[WindowHandle setMovableByWindowBackground: NO];
				
				[WindowHandle setDisplayReconfiguring: true];
			}
			else if(Flags & kCGDisplayDesktopShapeChangedFlag)
			{
				[WindowHandle setDisplayReconfiguring: false];
			}
		});
	}
}

bool FMacWindow::OnIMKKeyDown(NSEvent* Event)
{
	if(WindowHandle && [WindowHandle openGLView])
	{
		return MainThreadReturn(^{
			SCOPED_AUTORELEASE_POOL;
			FCocoaTextView* View = (FCocoaTextView*)[WindowHandle openGLView];
			return View && [View imkKeyDown:Event];
		}, UE4IMEEventMode);
	}
	else
	{
		return false;
	}
}
