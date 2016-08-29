// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "MacWindow.h"
#include "MacApplication.h"
#include "MacCursor.h"
#include "CocoaTextView.h"
#include "CocoaThread.h"

FMacWindow::FMacWindow()
:	WindowHandle(NULL)
,	bIsVisible(false)
,	bIsClosed(false)
{
	PreFullscreenWindowRect.origin.x = PreFullscreenWindowRect.origin.y = PreFullscreenWindowRect.size.width = PreFullscreenWindowRect.size.height = 0.0f;
}

FMacWindow::~FMacWindow()
{
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

	TSharedRef<FMacScreen> TargetScreen = Application->FindScreenByPoint( X, Definition->YDesiredPositionOnScreen );

	int32 Y = FMath::TruncToInt( Definition->YDesiredPositionOnScreen );

	// Make sure it's not under the menu bar on whatever display being targeted
	const int32 MaxVisibleY = FMacApplication::ConvertCocoaYPositionToSlate(TargetScreen->VisibleFrame.origin.y + TargetScreen->VisibleFrame.size.height);
	Y = (Y - MaxVisibleY) >= 0 ? Y : MaxVisibleY;

	const int32 SizeX = FMath::Max(FMath::TruncToInt( Definition->WidthDesiredOnScreen ), 1);
	const int32 SizeY = FMath::Max(FMath::TruncToInt( Definition->HeightDesiredOnScreen ), 1);

	PositionX = X;
	PositionY = Y;

	const int32 InvertedY = FMacApplication::ConvertSlateYPositionToCocoa(Y) - SizeY + 1;
	const NSRect ViewRect = NSMakeRect(X, InvertedY, SizeX, SizeY);

	uint32 WindowStyle = 0;
	if( Definition->IsRegularWindow )
	{
		if( Definition->HasCloseButton )
		{
			WindowStyle = NSClosableWindowMask;
		}
		
		// In order to support rounded, shadowed windows set the window to be
		// titled - we'll set the OpenGL view to cover the whole window
		WindowStyle |= NSTitledWindowMask | (FPlatformMisc::IsRunningOnMavericks() ? NSTexturedBackgroundWindowMask : NSFullSizeContentViewWindowMask);
		
		if( Definition->SupportsMinimize )
		{
			WindowStyle |= NSMiniaturizableWindowMask;
		}
		if( Definition->SupportsMaximize || Definition->HasSizingFrame )
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
		WindowStyle &= FPlatformMisc::IsRunningOnMavericks() ? ~NSTexturedBackgroundWindowMask : ~NSFullSizeContentViewWindowMask;
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

			int32 WindowLevel = NSNormalWindowLevel;

			if (Definition->IsModalWindow)
			{
				WindowLevel = NSFloatingWindowLevel;
			}
			else
			{
				switch (Definition->Type)
				{
					case EWindowType::Normal:
						WindowLevel = NSNormalWindowLevel;
						break;

					case EWindowType::Menu:
						WindowLevel = NSStatusWindowLevel;
						break;

					case EWindowType::ToolTip:
						WindowLevel = NSPopUpMenuWindowLevel;
						break;

					case EWindowType::Notification:
						WindowLevel = NSModalPanelWindowLevel;
						break;

					case EWindowType::CursorDecorator:
						WindowLevel = NSMainMenuWindowLevel;
						break;
				}
			}

			[WindowHandle setLevel:WindowLevel];

			if( !Definition->HasOSWindowBorder )
			{
				[WindowHandle setBackgroundColor: [NSColor clearColor]];
				[WindowHandle setHasShadow: YES];
			}

			[WindowHandle setOpaque: NO];

			[WindowHandle setMinSize:NSMakeSize(Definition->SizeLimits.GetMinWidth().Get(10.0f), Definition->SizeLimits.GetMinHeight().Get(10.0f))];
			[WindowHandle setMaxSize:NSMakeSize(Definition->SizeLimits.GetMaxWidth().Get(10000.0f), Definition->SizeLimits.GetMaxHeight().Get(10000.0f))];

			ReshapeWindow( X, Y, SizeX, SizeY );

			if (Definition->ShouldPreserveAspectRatio)
			{
				[WindowHandle setContentAspectRatio:NSMakeSize((float)SizeX / (float)SizeY, 1.0f)];
			}

			if (Definition->IsRegularWindow)
			{
				[NSApp addWindowsItem:WindowHandle title:Definition->Title.GetNSString() filename:NO];

				// Tell Cocoa that we are opting into drag and drop.
				// Only makes sense for regular windows (windows that last a while.)
				[WindowHandle registerForDraggedTypes:@[NSFilenamesPboardType, NSPasteboardTypeString]];

				if( Definition->HasOSWindowBorder )
				{
					[WindowHandle setCollectionBehavior: NSWindowCollectionBehaviorDefault|NSWindowCollectionBehaviorManaged|NSWindowCollectionBehaviorParticipatesInCycle];
				}
				else
				{
					[WindowHandle setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary|NSWindowCollectionBehaviorDefault|NSWindowCollectionBehaviorManaged|NSWindowCollectionBehaviorParticipatesInCycle];

					if (!FPlatformMisc::IsRunningOnMavericks())
					{
						WindowHandle.titlebarAppearsTransparent = YES;
						WindowHandle.titleVisibility = NSWindowTitleHidden;
					}
				}

				SetText(*Definition->Title);
			}
			else if(Definition->AppearsInTaskbar)
			{
				if (!Definition->Title.IsEmpty())
				{
					[NSApp addWindowsItem:WindowHandle title:Definition->Title.GetNSString() filename:NO];
				}

				[WindowHandle setCollectionBehavior:NSWindowCollectionBehaviorFullScreenAuxiliary|NSWindowCollectionBehaviorDefault|NSWindowCollectionBehaviorManaged|NSWindowCollectionBehaviorParticipatesInCycle];
			}
			else
			{
				[WindowHandle setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces|NSWindowCollectionBehaviorTransient|NSWindowCollectionBehaviorIgnoresCycle];
			}

			if (Definition->TransparencySupport == EWindowTransparency::PerWindow)
			{
				SetOpacity(Definition->Opacity);
			}
			else
			{
				SetOpacity(1.0f);
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

FCocoaWindow* FMacWindow::GetWindowHandle() const
{
	return WindowHandle;
}

void FMacWindow::ReshapeWindow( int32 X, int32 Y, int32 Width, int32 Height )
{
	if (WindowHandle)
	{
		SCOPED_AUTORELEASE_POOL;
		
		if(GetWindowMode() == EWindowMode::Windowed || GetWindowMode() == EWindowMode::WindowedFullscreen)
		{
			const int32 InvertedY = FMacApplication::ConvertSlateYPositionToCocoa(Y) - Height + 1;
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
					else if (Definition->ShouldPreserveAspectRatio)
					{
						[WindowHandle setContentAspectRatio:NSMakeSize((float)Width / (float)Height, 1.0f)];
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
				MainThreadCall(^{
					FMacCursor* MacCursor = (FMacCursor*)MacApplication->Cursor.Get();
					if ( MacCursor )
					{
						NSSize WindowSize = [WindowHandle frame].size;
						NSSize ViewSize = [WindowHandle openGLFrame].size;
						float WidthScale = ViewSize.width / WindowSize.width;
						float HeightScale = ViewSize.height / WindowSize.height;
						MacCursor->SetMouseScaling(FVector2D(WidthScale, HeightScale), WindowHandle);
					}
				}, UE4ResizeEventMode, true);
				MacApplication->DeferEvent([NSNotification notificationWithName:NSWindowDidResizeNotification object:WindowHandle]);
			}
		}
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
		const int32 InvertedY = FMacApplication::ConvertSlateYPositionToCocoa(Y) - [WindowHandle openGLFrame].size.height + 1;
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
	if (WindowHandle)
	{
		SCOPED_AUTORELEASE_POOL;
		bIsClosed = true;
		[WindowHandle setAlphaValue:0.0f];
		[WindowHandle setBackgroundColor:[NSColor clearColor]];
		MacApplication->OnWindowDestroyed(SharedThis(this));
		WindowHandle = nullptr;
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

	if(bIsFullscreen == bMakeFullscreen && NewWindowMode != GetWindowMode())
	{
		SetWindowMode(EWindowMode::Windowed);
	}
	
	if( bIsFullscreen != bMakeFullscreen || NewWindowMode != GetWindowMode() )
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
			PreFullscreenWindowRect.origin = [WindowHandle frame].origin;
			PreFullscreenWindowRect.size = [WindowHandle openGLFrame].size;
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
	}
	else // Already in/out fullscreen but a different mode - we should just update the mode rather than forcing the window to change again
	{
		WindowHandle.TargetWindowMode = NewWindowMode;
		[WindowHandle setWindowMode:NewWindowMode];
	}
}

EWindowMode::Type FMacWindow::GetWindowMode() const
{
	return [WindowHandle windowMode];
}

bool FMacWindow::IsMaximized() const
{
	return WindowHandle && WindowHandle->bZoomed;
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
	if (WindowHandle)
	{
		SCOPED_AUTORELEASE_POOL;
		
		NSRect Frame = [WindowHandle frame];
		
		X = Frame.origin.x;
		Y = FMacApplication::ConvertSlateYPositionToCocoa(Frame.origin.y) - Frame.size.height + 1;
		
		Width = Frame.size.width;
		Height = Frame.size.height;
		
		return true;
	}
	else
	{
		return false;
	}
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

bool FMacWindow::IsPointInWindow( int32 X, int32 Y ) const
{
	SCOPED_AUTORELEASE_POOL;
	bool PointInWindow = false;
	if(![WindowHandle isMiniaturized])
	{
		NSRect WindowFrame = [WindowHandle frame];
		WindowFrame.size = [WindowHandle openGLFrame].size;
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
			VisibleFrame.origin.x = Intersection.origin.x - WindowFrame.origin.x;
			VisibleFrame.origin.y = Intersection.origin.y - WindowFrame.origin.y;
		}
	#endif
		
		if(WindowHandle->bIsOnActiveSpace)
		{
			FMacCursor* MacCursor = (FMacCursor*)MacApplication->Cursor.Get();
			NSPoint CursorPoint = NSMakePoint(X, WindowFrame.size.height - (Y + 1));
			PointInWindow = (NSPointInRect(CursorPoint, VisibleFrame) == YES);
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
	return [WindowHandle isKeyWindow];
}

void FMacWindow::SetText(const TCHAR* const Text)
{
	if( FString([WindowHandle title]) != FString(Text) )
	{
		CFStringRef CFName = FPlatformString::TCHARToCFString( Text );
		MainThreadCall(^{
			SCOPED_AUTORELEASE_POOL;
			[WindowHandle setTitle: (NSString*)CFName];
			if (IsRegularWindow())
			{
				[NSApp changeWindowsItem: WindowHandle title: (NSString*)CFName filename: NO];
			}
			CFRelease( CFName );
		}, UE4NilEventMode, true);
	}
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
