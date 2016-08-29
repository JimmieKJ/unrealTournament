// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "WindowsWindow.h"
#include "WindowsApplication.h"
#include "HAL/ThreadHeartBeat.h"

#include "AllowWindowsPlatformTypes.h"
#if WINVER > 0x502	// Windows Vista or better required for DWM
	#include "Dwmapi.h"
#endif
#include <ShlObj.h>
#include "HideWindowsPlatformTypes.h"


FWindowsWindow::~FWindowsWindow()
{
	// NOTE: The HWnd is invalid here!
	//       Doing stuff with HWnds will fail silently.
	//       Use Destroy() instead.
}


const TCHAR FWindowsWindow::AppWindowClass[] = TEXT("UnrealWindow");

// This is a hard-coded constant for Windows' WS_THICKFRAME
// There does not seem to be a way to alter this
static int32 WindowsAeroBorderSize = 8;
static int32 WindowsStandardBorderSize = 4;


TSharedRef< FWindowsWindow > FWindowsWindow::Make()
{
	// First, allocate the new native window object.  This doesn't actually create a native window or anything,
	// we're simply instantiating the object so that we can keep shared references to it.
	return MakeShareable( new FWindowsWindow() );
}

void FWindowsWindow::Initialize( FWindowsApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, HINSTANCE InHInstance, const TSharedPtr< FWindowsWindow >& InParent, const bool bShowImmediately )
{
	Definition = InDefinition;
	OwningApplication = Application;

	// Finally, let's initialize the new native window object.  Calling this function will often cause OS
	// window messages to be sent! (such as activation messages)
	uint32 WindowExStyle = 0;
	uint32 WindowStyle = 0;

	RegionWidth = RegionHeight = INDEX_NONE;

	const float XInitialRect = Definition->XDesiredPositionOnScreen;
	const float YInitialRect = Definition->YDesiredPositionOnScreen;

	const float WidthInitial = Definition->WidthDesiredOnScreen;
	const float HeightInitial = Definition->HeightDesiredOnScreen;

	int32 ClientX = FMath::TruncToInt( XInitialRect );
	int32 ClientY = FMath::TruncToInt( YInitialRect );
	int32 ClientWidth = FMath::TruncToInt( WidthInitial );
	int32 ClientHeight = FMath::TruncToInt( HeightInitial );
	int32 WindowX = ClientX;
	int32 WindowY = ClientY;
	int32 WindowWidth = ClientWidth;
	int32 WindowHeight = ClientHeight;
	const bool bApplicationSupportsPerPixelBlending =
#if ALPHA_BLENDED_WINDOWS
		Application->GetWindowTransparencySupport() == EWindowTransparency::PerPixel;
#else
		false;
#endif

	if( !Definition->HasOSWindowBorder )
	{
		WindowExStyle = WS_EX_WINDOWEDGE;

		if( Definition->TransparencySupport == EWindowTransparency::PerWindow )
		{
			WindowExStyle |= WS_EX_LAYERED;
		}
#if ALPHA_BLENDED_WINDOWS
		else if( Definition->TransparencySupport == EWindowTransparency::PerPixel )
		{
			if( bApplicationSupportsPerPixelBlending )
			{
				WindowExStyle |= WS_EX_COMPOSITED;
			}
		}
#endif

		WindowStyle = WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
		if ( Definition->AppearsInTaskbar )
		{
			WindowExStyle |= WS_EX_APPWINDOW;
		}
		else
		{
			WindowExStyle |= WS_EX_TOOLWINDOW;
		}

		if( Definition->IsTopmostWindow )
		{
			// Tool tips are always top most windows
			WindowExStyle |= WS_EX_TOPMOST;
		}

		if ( !Definition->AcceptsInput )
		{
			// Window should never get input
			WindowExStyle |= WS_EX_TRANSPARENT;
		}
	}
	else
	{
		// OS Window border setup
		WindowExStyle = WS_EX_APPWINDOW;
		WindowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;

		if (IsRegularWindow())
		{
			if (Definition->SupportsMaximize)
			{
				WindowStyle |= WS_MAXIMIZEBOX;
			}

			if (Definition->SupportsMinimize)
			{
				WindowStyle |= WS_MINIMIZEBOX;
			}

			if (Definition->HasSizingFrame)
			{
				WindowStyle |= WS_THICKFRAME;
			}
			else
			{
				WindowStyle |= WS_BORDER;
			}
		}
		else
		{
			WindowStyle |= WS_POPUP | WS_BORDER;
		}

		// X,Y, Width, Height defines the top-left pixel of the client area on the screen
		// This adjusts a zero rect to give us the size of the border
		RECT BorderRect = { 0, 0, 0, 0 };
		::AdjustWindowRectEx(&BorderRect, WindowStyle, false, WindowExStyle);

		// Border rect size is negative - see MoveWindowTo
		WindowX += BorderRect.left;
		WindowY += BorderRect.top;

		// Inflate the window size by the OS border
		WindowWidth += BorderRect.right - BorderRect.left;
		WindowHeight += BorderRect.bottom - BorderRect.top;
	}

	// Creating the Window
	HWnd = CreateWindowEx(
		WindowExStyle,
		AppWindowClass,
		*Definition->Title,
		WindowStyle,
		WindowX, WindowY, 
		WindowWidth, WindowHeight,
		( InParent.IsValid() ) ? static_cast<HWND>( InParent->HWnd ) : NULL,
		NULL, InHInstance, NULL);

	VirtualWidth = ClientWidth;
	VirtualHeight = ClientHeight;

	// We call reshape window here because we didn't take into account the non-client area
	// in the initial creation of the window. Slate should only pass client area dimensions.
	// Reshape window may resize the window if the non-client area is encroaching on our
	// desired client area space.
	ReshapeWindow( ClientX, ClientY, ClientWidth, ClientHeight );

	if( HWnd == NULL )
	{
		FSlowHeartBeatScope SuspendHeartBeat;

		// @todo Error message should be localized!
		MessageBox(NULL, TEXT("Window Creation Failed!"), TEXT("Error!"), MB_ICONEXCLAMATION | MB_OK);
		checkf(0, TEXT("Window Creation Failed (%d)"), ::GetLastError() );
		return;
	}

	if ( Definition->TransparencySupport == EWindowTransparency::PerWindow )
	{
		SetOpacity( Definition->Opacity );
	}

#if WINVER > 0x502	// Windows Vista or better required for DWM
	// Disable DWM Rendering and Nonclient Area painting if not showing the os window border
	// This prevents the standard windows frame from ever being drawn
	if( !Definition->HasOSWindowBorder )
	{
		const DWMNCRENDERINGPOLICY RenderingPolicy = DWMNCRP_DISABLED;
		verify(SUCCEEDED(DwmSetWindowAttribute(HWnd, DWMWA_NCRENDERING_POLICY, &RenderingPolicy, sizeof(RenderingPolicy))));

		const BOOL bEnableNCPaint = false;
		verify(SUCCEEDED(DwmSetWindowAttribute(HWnd, DWMWA_ALLOW_NCPAINT, &bEnableNCPaint, sizeof(bEnableNCPaint))));

	#if ALPHA_BLENDED_WINDOWS
		if ( bApplicationSupportsPerPixelBlending && Definition->TransparencySupport == EWindowTransparency::PerPixel )
		{
			MARGINS Margins = {-1};
			verify(SUCCEEDED(::DwmExtendFrameIntoClientArea(HWnd, &Margins)));
		}
	#endif
	}

#endif	// WINVER

	// No region for non regular windows or windows displaying the os window border
	if ( IsRegularWindow() && !Definition->HasOSWindowBorder )
	{
		WindowStyle |= WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
		if ( Definition->SupportsMaximize )
		{
			WindowStyle |= WS_MAXIMIZEBOX;
		}
		if ( Definition->SupportsMinimize )
		{
			WindowStyle |= WS_MINIMIZEBOX;
		}
		if ( Definition->HasSizingFrame )
		{
			WindowStyle |= WS_THICKFRAME;
		}

		verify(SetWindowLong(HWnd, GWL_STYLE, WindowStyle));
		::SetWindowPos(HWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

		AdjustWindowRegion( ClientWidth, ClientHeight );
	}

	if ( IsRegularWindow() )
	{
		// Tell OLE that we are opting into drag and drop.
		// Only makes sense for regular windows (windows that last a while.)
		RegisterDragDrop( HWnd, this );
	}
}

FWindowsWindow::FWindowsWindow()
	: HWnd(NULL)
	, WindowMode( EWindowMode::Windowed )
	, OLEReferenceCount(0)
	, AspectRatio(1.0f)
	, bIsVisible( false )
{
	FMemory::Memzero(PreFullscreenWindowPlacement);
	PreFullscreenWindowPlacement.length = sizeof(WINDOWPLACEMENT);

	FMemory::Memzero(PreParentMinimizedWindowPlacement);
	PreParentMinimizedWindowPlacement.length = sizeof(WINDOWPLACEMENT);
}

HWND FWindowsWindow::GetHWnd() const
{
	return HWnd;
}

void FWindowsWindow::OnTransparencySupportChanged(EWindowTransparency NewTransparency)
{
#if ALPHA_BLENDED_WINDOWS
	if ( Definition->TransparencySupport == EWindowTransparency::PerPixel )
	{
		const auto Style = GetWindowLong( HWnd, GWL_EXSTYLE );

		if( NewTransparency == EWindowTransparency::PerPixel )
		{
			SetWindowLong( HWnd, GWL_EXSTYLE, Style | WS_EX_COMPOSITED );

			MARGINS Margins = {-1};
			verify(SUCCEEDED(::DwmExtendFrameIntoClientArea(HWnd, &Margins)));
		}
		else
		{
			SetWindowLong( HWnd, GWL_EXSTYLE, Style & ~WS_EX_COMPOSITED );
		}

		// Must call SWP_FRAMECHANGED when updating the style attribute of a window in order to update internal caches (according to MSDN)
		SetWindowPos( HWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOSIZE | SWP_NOSENDCHANGING | SWP_NOZORDER );
	}
#endif
}

HRGN FWindowsWindow::MakeWindowRegionObject() const
{
	HRGN Region;
	if ( RegionWidth != INDEX_NONE && RegionHeight != INDEX_NONE )
	{
		if (IsMaximized())
		{
			if (GetDefinition().Type == EWindowType::GameWindow && !GetDefinition().HasOSWindowBorder)
			{
				// Windows caches the cxWindowBorders size at window creation. Even if borders are removed or resized Windows will continue to use this value when evaluating regions
				// and sizing windows. When maximized this means that our window position will be offset from the screen origin by (-cxWindowBorders,-cxWindowBorders). We want to
				// display only the region within the maximized screen area, so offset our upper left and lower right by cxWindowBorders.
				WINDOWINFO WindowInfo;
				FMemory::Memzero(WindowInfo);
				WindowInfo.cbSize = sizeof(WindowInfo);
				::GetWindowInfo(HWnd, &WindowInfo);

				Region = CreateRectRgn(WindowInfo.cxWindowBorders, WindowInfo.cxWindowBorders, RegionWidth + WindowInfo.cxWindowBorders, RegionHeight + WindowInfo.cxWindowBorders);
			}
			else
			{
				int32 WindowBorderSize = GetWindowBorderSize();
				Region = CreateRectRgn(WindowBorderSize, WindowBorderSize, RegionWidth - WindowBorderSize, RegionHeight - WindowBorderSize);
			}
		}
		else
		{
			const bool bUseCornerRadius  = WindowMode == EWindowMode::Windowed &&
#if ALPHA_BLENDED_WINDOWS
				// Corner radii cause DWM window composition blending to fail, so we always set regions to full size rectangles
				Definition->TransparencySupport != EWindowTransparency::PerPixel &&
#endif
				Definition->CornerRadius > 0;

			if( bUseCornerRadius )
			{
				// @todo mac: Corner radius not applied on Mac platform yet

				// CreateRoundRectRgn gives you a duff region that's 1 pixel smaller than you ask for. CreateRectRgn behaves correctly.
				// This can be verified by uncommenting the assert below
				Region = CreateRoundRectRgn( 0, 0, RegionWidth+1, RegionHeight+1, Definition->CornerRadius, Definition->CornerRadius );

				// Test that a point that should be in the region, is in the region
				// check(!!PtInRegion(Region, RegionWidth-1, RegionHeight/2));
			}
			else
			{
				Region = CreateRectRgn( 0, 0, RegionWidth, RegionHeight );
			}
		}
	}
	else
	{
		RECT rcWnd;
		GetWindowRect( HWnd, &rcWnd );
		Region = CreateRectRgn( 0, 0, rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top );
	}

	return Region;
}

void FWindowsWindow::AdjustWindowRegion( int32 Width, int32 Height )
{
	RegionWidth = Width;
	RegionHeight = Height;

	HRGN Region = MakeWindowRegionObject();

	// NOTE: We explicitly don't delete the Region object, because the OS deletes the handle when it no longer needed after
	// a call to SetWindowRgn.
	verify( SetWindowRgn( HWnd, Region, false ) );
}

void FWindowsWindow::ReshapeWindow( int32 NewX, int32 NewY, int32 NewWidth, int32 NewHeight )
{
	WINDOWINFO WindowInfo;
	FMemory::Memzero( WindowInfo );
	WindowInfo.cbSize = sizeof( WindowInfo );
	::GetWindowInfo( HWnd, &WindowInfo );

	AspectRatio = (float)NewWidth / (float)NewHeight;

	// X,Y, Width, Height defines the top-left pixel of the client area on the screen
	if( Definition->HasOSWindowBorder )
	{
		// This adjusts a zero rect to give us the size of the border
		RECT BorderRect = { 0, 0, 0, 0 };
		::AdjustWindowRectEx(&BorderRect, WindowInfo.dwStyle, false, WindowInfo.dwExStyle);

		// Border rect size is negative - see MoveWindowTo
		NewX += BorderRect.left;
		NewY += BorderRect.top;

		// Inflate the window size by the OS border
		NewWidth += BorderRect.right - BorderRect.left;
		NewHeight += BorderRect.bottom - BorderRect.top;
	}

	// the window position is the requested position
	int32 WindowX = NewX;
	int32 WindowY = NewY;
	
	// If the window size changes often, only grow the window, never shrink it
	const bool bVirtualSizeChanged = NewWidth != VirtualWidth || NewHeight != VirtualHeight;
	VirtualWidth = NewWidth;
	VirtualHeight = NewHeight;

	if( Definition->SizeWillChangeOften )
	{
		// When SizeWillChangeOften is set, we set a minimum width and height window size that we'll keep allocated
		// even when the requested actual window size is smaller.  This just avoids constantly resizing the window
		// and associated GPU buffers, which can be very slow on some platforms.

		const RECT OldWindowRect = WindowInfo.rcWindow;
		const int32 OldWidth = OldWindowRect.right - OldWindowRect.left;
		const int32 OldHeight = OldWindowRect.bottom - OldWindowRect.top;

		const int32 MinRetainedWidth = Definition->ExpectedMaxWidth != INDEX_NONE ? Definition->ExpectedMaxWidth : OldWidth;
		const int32 MinRetainedHeight = Definition->ExpectedMaxHeight != INDEX_NONE ? Definition->ExpectedMaxHeight : OldHeight;

		NewWidth = FMath::Max( NewWidth, FMath::Min( OldWidth, MinRetainedWidth ) );
		NewHeight = FMath::Max( NewHeight, FMath::Min( OldHeight, MinRetainedHeight ) );
	}

	if (IsMaximized())
	{
		Restore();
	}

	// We use SWP_NOSENDCHANGING when in fullscreen mode to prevent Windows limiting our window size to the current resolution, as that 
	// prevents us being able to change to a higher resolution while in fullscreen mode
	::SetWindowPos( HWnd, nullptr, WindowX, WindowY, NewWidth, NewHeight, SWP_NOZORDER | SWP_NOACTIVATE | ((WindowMode == EWindowMode::Fullscreen) ? SWP_NOSENDCHANGING : 0) );

	if( Definition->SizeWillChangeOften && bVirtualSizeChanged )
	{
		AdjustWindowRegion( VirtualWidth, VirtualHeight );
	}
}

bool FWindowsWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	bool bTrueFullscreen = ( WindowMode == EWindowMode::Fullscreen );

	// Grab current monitor data for sizing
	HMONITOR Monitor = MonitorFromWindow( HWnd, bTrueFullscreen ? MONITOR_DEFAULTTOPRIMARY : MONITOR_DEFAULTTONEAREST );
	MONITORINFO MonitorInfo;
	MonitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo( Monitor, &MonitorInfo );

	X = MonitorInfo.rcMonitor.left;
	Y = MonitorInfo.rcMonitor.top;
	Width = MonitorInfo.rcMonitor.right - X;
	Height = MonitorInfo.rcMonitor.bottom - Y;

	return true;
}

/** Native windows should implement MoveWindowTo by relocating the client area of the platform-specific window to (X,Y). */
void FWindowsWindow::MoveWindowTo( int32 X, int32 Y )
{
	// Slate gives the window position as relative to the client area of a window, so we may need to compensate for the OS border
	if (Definition->HasOSWindowBorder)
	{
		const LONG WindowStyle = ::GetWindowLong(HWnd, GWL_STYLE);
		const LONG WindowExStyle = ::GetWindowLong(HWnd, GWL_EXSTYLE);

		// This adjusts a zero rect to give us the size of the border
		RECT BorderRect = { 0, 0, 0, 0 };
		::AdjustWindowRectEx(&BorderRect, WindowStyle, false, WindowExStyle);

		// Border rect size is negative
		X += BorderRect.left;
		Y += BorderRect.top;
	}

	::SetWindowPos(HWnd, nullptr, X, Y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
}

/** Native windows should implement BringToFront by making this window the top-most window (i.e. focused).
 *
 * @param bForce	Forces the window to the top of the Z order, even if that means stealing focus from other windows
 *					In general do not pass true for this.  It is really only useful for some windows, like game windows where not forcing it to the front
 *					could cause mouse capture and mouse lock to happen but without the window visible
 */
void FWindowsWindow::BringToFront( bool bForce )
{
	if ( IsRegularWindow() )
	{
		if (::IsIconic(HWnd))
		{
			::ShowWindow(HWnd, SW_RESTORE);
		}
		else
		{
			::SetActiveWindow(HWnd);
		}
	}
	else
	{
		HWND HWndInsertAfter = HWND_TOP;
		// By default we activate the window or it isn't actually brought to the front 
		uint32 Flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER;

		if( !bForce )
		{
			Flags |= SWP_NOACTIVATE;
		}

		if( Definition->IsTopmostWindow )
		{
			HWndInsertAfter = HWND_TOPMOST;
		}

		::SetWindowPos( HWnd, HWndInsertAfter, 0, 0, 0, 0, Flags );
	}
}

void FWindowsWindow::HACK_ForceToFront()
{
	::SetForegroundWindow(HWnd);
}

/** Native windows should implement this function by asking the OS to destroy OS-specific resource associated with the window (e.g. Win32 window handle) */
void FWindowsWindow::Destroy()
{
	if (OLEReferenceCount > 0 && IsWindow(HWnd))
	{
		HRESULT Result = RevokeDragDrop(HWnd);
		// If we decremented OLEReferenceCount check it for being null (shutdown)
		if (Result == S_OK)
		{
			checkf(OLEReferenceCount == 0, TEXT("Not all references to window are released, %i left"), OLEReferenceCount);
		}
	}

	::DestroyWindow( HWnd );
}

/** Native window should implement this function by performing the equivalent of the Win32 minimize-to-taskbar operation */
void FWindowsWindow::Minimize()
{
	::ShowWindow(HWnd, SW_MINIMIZE);
}

/** Native window should implement this function by performing the equivalent of the Win32 maximize operation */
void FWindowsWindow::Maximize()
{
	::ShowWindow(HWnd, SW_MAXIMIZE);
}

/** Native window should implement this function by performing the equivalent of the Win32 maximize operation */
void FWindowsWindow::Restore()
{
	::ShowWindow(HWnd, SW_RESTORE);
}

/** Native window should make itself visible */
void FWindowsWindow::Show()
{
	if (!bIsVisible)
	{
		bIsVisible = true;

		// Do not activate windows that do not take input; e.g. tool-tips and cursor decorators
		// Also dont activate if a window wants to appear but not activate itself
		const bool bShouldActivate = Definition->AcceptsInput && Definition->ActivateWhenFirstShown;
		::ShowWindow(HWnd, bShouldActivate ? SW_SHOW : SW_SHOWNA);
	}
}

/** Native window should hide itself */
void FWindowsWindow::Hide()
{
	if (bIsVisible)
	{
		bIsVisible = false;
		::ShowWindow(HWnd, SW_HIDE);
	}
}

/** Toggle native window between fullscreen and normal mode */
void FWindowsWindow::SetWindowMode( EWindowMode::Type NewWindowMode )
{
	if (NewWindowMode != WindowMode)
	{
		WindowMode = NewWindowMode;

		const bool bTrueFullscreen = NewWindowMode == EWindowMode::Fullscreen;

		// Setup Win32 Flags to be used for Fullscreen mode
		LONG WindowStyle = GetWindowLong(HWnd, GWL_STYLE);
		const LONG FullscreenModeStyle = WS_POPUP;

		LONG WindowedModeStyle = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;
		if (IsRegularWindow())
		{
			if (Definition->SupportsMaximize)
			{
				WindowedModeStyle |= WS_MAXIMIZEBOX;
			}

			if (Definition->SupportsMinimize)
			{
				WindowedModeStyle |= WS_MINIMIZEBOX;
			}

			if (Definition->HasSizingFrame)
			{
				WindowedModeStyle |= WS_THICKFRAME;
			}
			else
			{
				WindowedModeStyle |= WS_BORDER;
			}
		}
		else
		{
			WindowedModeStyle |= WS_POPUP | WS_BORDER;
		}

		// If we're not in fullscreen, make it so
		if (NewWindowMode == EWindowMode::WindowedFullscreen || NewWindowMode == EWindowMode::Fullscreen)
		{
			const bool bIsBorderlessGameWindow = Definition->Type == EWindowType::GameWindow && !Definition->HasOSWindowBorder;

			::GetWindowPlacement(HWnd, &PreFullscreenWindowPlacement);

			// Setup Win32 flags for fullscreen window
			if (bIsBorderlessGameWindow && !bTrueFullscreen)
			{
				WindowStyle &= ~FullscreenModeStyle;
				WindowStyle |= WindowedModeStyle;
			}
			else
			{
				WindowStyle &= ~WindowedModeStyle;
				WindowStyle |= FullscreenModeStyle;
			}

			SetWindowLong(HWnd, GWL_STYLE, WindowStyle);
			::SetWindowPos(HWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

			if (!bTrueFullscreen)
			{
				// Ensure the window is restored if we are going for WindowedFullscreen
				ShowWindow(HWnd, SW_RESTORE);
			}

			// Get the current window position.
			RECT ClientRect;
			GetClientRect(HWnd, &ClientRect);

			// Grab current monitor data for sizing
			HMONITOR Monitor = MonitorFromWindow( HWnd, bTrueFullscreen ? MONITOR_DEFAULTTOPRIMARY : MONITOR_DEFAULTTONEAREST );
			MONITORINFO MonitorInfo;
			MonitorInfo.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo( Monitor, &MonitorInfo );

			// Get the target client width to send to ReshapeWindow.
			// Preserve the current res if going to true fullscreen and the monitor supports it and allow the calling code
			// to resize if required.
			// Else, use the monitor's res for windowed fullscreen.
			LONG MonitorWidth  = MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left;
			LONG TargetClientWidth = bTrueFullscreen ?
				FMath::Min(MonitorWidth, ClientRect.right - ClientRect.left) :
				MonitorWidth;

			LONG MonitorHeight = MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top;
			LONG TargetClientHeight = bTrueFullscreen ?
				FMath::Min(MonitorHeight, ClientRect.bottom - ClientRect.top) :
				MonitorHeight;


			// Resize and position fullscreen window
			ReshapeWindow(
				MonitorInfo.rcMonitor.left,
				MonitorInfo.rcMonitor.top,
				TargetClientWidth,
				TargetClientHeight);
		}
		else
		{
			// Windowed:

			// Setup Win32 flags for restored window
			WindowStyle &= ~FullscreenModeStyle;
			WindowStyle |= WindowedModeStyle;
			SetWindowLong(HWnd, GWL_STYLE, WindowStyle);
			::SetWindowPos(HWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

			::SetWindowPlacement(HWnd, &PreFullscreenWindowPlacement);
		}
	}
}

/** @return true if the native window is maximized, false otherwise */
bool FWindowsWindow::IsMaximized() const
{
	bool bIsMaximized = !!::IsZoomed(HWnd);
	return bIsMaximized;
}

bool FWindowsWindow::IsMinimized() const
{
	return !!::IsIconic(HWnd);
}

/** @return true if the native window is visible, false otherwise */
bool FWindowsWindow::IsVisible() const
{
	return bIsVisible;
}

/** Returns the size and location of the window when it is restored */
bool FWindowsWindow::GetRestoredDimensions(int32& X, int32& Y, int32& Width, int32& Height)
{
	WINDOWPLACEMENT WindowPlacement;
	WindowPlacement.length = sizeof( WINDOWPLACEMENT );

	if( ::GetWindowPlacement( HWnd, &WindowPlacement) != 0 )
	{
		// This window may be minimized.  Get the position when it is restored.
		const RECT Restored = WindowPlacement.rcNormalPosition;

		X = Restored.left;
		Y = Restored.top;
		Width = Restored.right - Restored.left;
		Height = Restored.bottom - Restored.top;
		return true;
	}
	else
	{
		return false;
	}
}


void FWindowsWindow::AdjustCachedSize( FVector2D& Size ) const
{
	if( Definition.IsValid() && Definition->SizeWillChangeOften )
	{
		Size = FVector2D( VirtualWidth, VirtualHeight );
	}
	else if(HWnd)
	{
		RECT ClientRect;
		::GetClientRect(HWnd,&ClientRect);
		Size.X = ClientRect.right - ClientRect.left;
		Size.Y = ClientRect.bottom - ClientRect.top;
	}
}

void FWindowsWindow::OnParentWindowMinimized()
{
	// This function is called from SW_PARENTCLOSING, because there's a bug in Win32 that causes the equivalent SW_PARENTOPENING
	// message to restore in an incorrect state (eg, it will lose the maximized status of the window)
	// To work around this, we cache our window placement here so that we can restore it later (see OnParentWindowRestored)
	::GetWindowPlacement(HWnd, &PreParentMinimizedWindowPlacement);
}

void FWindowsWindow::OnParentWindowRestored()
{
	// This function is called from SW_PARENTOPENING so that we can restore the window placement that was cached in OnParentWindowMinimized
	::SetWindowPlacement(HWnd, &PreParentMinimizedWindowPlacement);
}

/** Sets focus on the native window */
void FWindowsWindow::SetWindowFocus()
{
	if( GetFocus() != HWnd )
	{
		::SetFocus( HWnd );
	}
}

/**
 * Sets the opacity of this window
 *
 * @param	InOpacity	The new window opacity represented as a floating point scalar
 */
void FWindowsWindow::SetOpacity( const float InOpacity )
{
	SetLayeredWindowAttributes( HWnd, 0, FMath::TruncToInt( InOpacity * 255.0f ), LWA_ALPHA );
}

/**
 * Enables or disables this window.  When disabled, a window will receive no input.       
 *
 * @param bEnable	true to enable the window, false to disable it.
 */
void FWindowsWindow::Enable( bool bEnable )
{
	::EnableWindow( HWnd, bEnable );
}

/** @return True if the window is enabled */
bool FWindowsWindow::IsEnabled()
{
	return !!::IsWindowEnabled( HWnd );
}

/** @return true if native window exists underneath the coordinates */
bool FWindowsWindow::IsPointInWindow( int32 X, int32 Y ) const
{
	bool Result = false;

	HRGN Region = MakeWindowRegionObject();
	Result = !!PtInRegion( Region, X, Y );
	DeleteObject( Region );

	return Result;
}

int32 FWindowsWindow::GetWindowBorderSize() const
{
	if (GetDefinition().Type == EWindowType::GameWindow && !GetDefinition().HasOSWindowBorder)
	{
		// Our borderless game windows actually have a thick border to allow sizing, which we draw over to simulate
		// a borderless window. We return zero here so that the game will correctly behave as if this is truly a
		// borderless window.
		return 0;
	}

	WINDOWINFO WindowInfo;
	FMemory::Memzero( WindowInfo );
	WindowInfo.cbSize = sizeof( WindowInfo );
	::GetWindowInfo( HWnd, &WindowInfo );

	return WindowInfo.cxWindowBorders;
}

int32 FWindowsWindow::GetWindowTitleBarSize() const
{
	return GetSystemMetrics(SM_CYCAPTION);
}

bool FWindowsWindow::IsForegroundWindow() const
{
	return ::GetForegroundWindow() == HWnd;
}

void FWindowsWindow::SetText( const TCHAR* const Text )
{
	SetWindowText(HWnd, Text);
}


bool FWindowsWindow::IsRegularWindow() const
{
	return Definition->IsRegularWindow;
}

HRESULT STDCALL FWindowsWindow::QueryInterface( REFIID iid, void ** ppvObject )
{
	if ( IID_IDropTarget == iid || IID_IUnknown == iid )
	{
		AddRef();
		*ppvObject = (IDropTarget*)(this);
		return S_OK;
	}
	else
	{
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
}

ULONG STDCALL FWindowsWindow::AddRef( void )
{
	// We only do this for correctness checking
	FPlatformAtomics::InterlockedIncrement( &OLEReferenceCount );
	return OLEReferenceCount;
}

ULONG STDCALL FWindowsWindow::Release( void )
{
	// We only do this for correctness checking
	FPlatformAtomics::InterlockedDecrement( &OLEReferenceCount );
	return OLEReferenceCount;
}

FDragDropOLEData DecipherOLEData(IDataObject* DataObjectPointer)
{
	FDragDropOLEData OLEData;

	// Utility to ensure resource release
	struct FOLEResourceGuard
	{
		STGMEDIUM& StorageMedium;
		LPVOID DataPointer;

		FOLEResourceGuard(STGMEDIUM& InStorage)
			: StorageMedium(InStorage)
			, DataPointer(GlobalLock(InStorage.hGlobal))
		{
		}

		~FOLEResourceGuard()
		{
			GlobalUnlock(StorageMedium.hGlobal);
			ReleaseStgMedium(&StorageMedium);
		}
	};

	// Attempt to get plain text or unicode text from the data being dragged in

	FORMATETC FormatEtc_Ansii = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	const bool bHaveAnsiText = (DataObjectPointer->QueryGetData(&FormatEtc_Ansii) == S_OK)
		? true
		: false;

	FORMATETC FormatEtc_UNICODE = { CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	const bool bHaveUnicodeText = (DataObjectPointer->QueryGetData(&FormatEtc_UNICODE) == S_OK)
		? true
		: false;

	FORMATETC FormatEtc_File = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	const bool bHaveFiles = (DataObjectPointer->QueryGetData(&FormatEtc_File) == S_OK)
		? true
		: false;

	STGMEDIUM StorageMedium;

	if (bHaveUnicodeText && S_OK == DataObjectPointer->GetData(&FormatEtc_UNICODE, &StorageMedium))
	{
		FOLEResourceGuard ResourceGuard(StorageMedium);
		OLEData.Type |= FDragDropOLEData::Text;
		OLEData.OperationText = static_cast<TCHAR*>(ResourceGuard.DataPointer);
	}
	if (bHaveAnsiText && S_OK == DataObjectPointer->GetData(&FormatEtc_Ansii, &StorageMedium))
	{
		FOLEResourceGuard ResourceGuard(StorageMedium);
		OLEData.Type |= FDragDropOLEData::Text;
		OLEData.OperationText = static_cast<ANSICHAR*>(ResourceGuard.DataPointer);
	}
	if (bHaveFiles && S_OK == DataObjectPointer->GetData(&FormatEtc_File, &StorageMedium))
	{
		OLEData.Type |= FDragDropOLEData::Files;

		FOLEResourceGuard ResourceGuard(StorageMedium);
		const DROPFILES* DropFiles = static_cast<DROPFILES*>(ResourceGuard.DataPointer);

		// pFiles is the offset to the beginning of the file list, in bytes
		LPVOID FileListStart = (BYTE*)ResourceGuard.DataPointer + DropFiles->pFiles;

		if (DropFiles->fWide)
		{
			// Unicode filenames
			// The file list is NULL delimited with an extra NULL character at the end.
			TCHAR* Pos = static_cast<TCHAR*>(FileListStart);
			while (Pos[0] != 0)
			{
				const FString ListElement = FString(Pos);
				OLEData.OperationFilenames.Add(ListElement);
				Pos += ListElement.Len() + 1;
			}
		}
		else
		{
			// Ansi filenames
			// The file list is NULL delimited with an extra NULL character at the end.
			ANSICHAR* Pos = static_cast<ANSICHAR*>(FileListStart);
			while (Pos[0] != 0)
			{
				const FString ListElement = FString(Pos);
				OLEData.OperationFilenames.Add(ListElement);
				Pos += ListElement.Len() + 1;
			}
		}
	}

	return OLEData;
}

HRESULT STDCALL FWindowsWindow::DragEnter( __RPC__in_opt IDataObject *DataObjectPointer, ::DWORD KeyState, POINTL CursorPosition, __RPC__inout ::DWORD *CursorEffect )
{
	// Decipher the OLE data
	const FDragDropOLEData& OLEData = DecipherOLEData(DataObjectPointer);

	if (IsInGameThread())
	{
		return OwningApplication->OnOLEDragEnter(HWnd, OLEData, KeyState, CursorPosition, CursorEffect);
	}
	else
	{
		// The occurred in a non-game thread. We must synchronize to the main thread as Slate is not designed to be multi-threaded.
		// Note that doing this means that we cannot determine the CursorEffect, but this is unfixable at the moment so we will just leave it alone.
		OwningApplication->DeferDragDropOperation( FDeferredWindowsDragDropOperation::MakeDragEnter(HWnd, OLEData, KeyState, CursorPosition) );
		return 0;
	}
}

HRESULT STDCALL FWindowsWindow::DragOver( ::DWORD KeyState, POINTL CursorPosition, __RPC__inout ::DWORD *CursorEffect )
{
	if (IsInGameThread())
	{
		return OwningApplication->OnOLEDragOver(HWnd, KeyState, CursorPosition, CursorEffect);
	}
	else
	{
		// The occurred in a non-game thread. We must synchronize to the main thread as Slate is not designed to be multi-threaded.
		// Note that doing this means that we cannot determine the CursorEffect, but this is unfixable at the moment so we will just leave it alone.
		OwningApplication->DeferDragDropOperation(FDeferredWindowsDragDropOperation::MakeDragOver(HWnd, KeyState, CursorPosition));
		return 0;
	}
}

HRESULT STDCALL FWindowsWindow::DragLeave( void )
{
	if (IsInGameThread())
	{
		return OwningApplication->OnOLEDragOut(HWnd);
	}
	else
	{
		// The occurred in a non-game thread. We must synchronize to the main thread as Slate is not designed to be multi-threaded.
		OwningApplication->DeferDragDropOperation(FDeferredWindowsDragDropOperation::MakeDragLeave(HWnd));
		return 0;
	}
}

HRESULT STDCALL FWindowsWindow::Drop( __RPC__in_opt IDataObject *DataObjectPointer, ::DWORD KeyState, POINTL CursorPosition, __RPC__inout ::DWORD *CursorEffect )
{
	// Decipher the OLE data
	const FDragDropOLEData& OLEData = DecipherOLEData(DataObjectPointer);

	if (IsInGameThread())
	{
		return OwningApplication->OnOLEDrop(HWnd, OLEData, KeyState, CursorPosition, CursorEffect);
	}
	else
	{
		// The occurred in a non-game thread. We must synchronize to the main thread as Slate is not designed to be multi-threaded.
		// Note that doing this means that we cannot determine the CursorEffect, but this is unfixable at the moment so we will just leave it alone.
		OwningApplication->DeferDragDropOperation(FDeferredWindowsDragDropOperation::MakeDrop(HWnd, OLEData, KeyState, CursorPosition));
		return 0;
	}
}
