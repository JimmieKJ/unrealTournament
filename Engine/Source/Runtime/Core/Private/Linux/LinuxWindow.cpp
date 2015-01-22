// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "LinuxWindow.h"
#include "LinuxApplication.h"

DEFINE_LOG_CATEGORY( LogLinuxWindow );

FLinuxWindow::~FLinuxWindow()
{
	// NOTE: The HWnd is invalid here!
	//       Doing stuff with HWnds will fail silently.
	//       Use Destroy() instead.
}

const TCHAR FLinuxWindow::AppWindowClass[] = TEXT("UnrealWindow");


TSharedRef< FLinuxWindow > FLinuxWindow::Make()
{
	// First, allocate the new native window object.  This doesn't actually create a native window or anything,
	// we're simply instantiating the object so that we can keep shared references to it.
	return MakeShareable( new FLinuxWindow() );
}

FLinuxWindow::FLinuxWindow()
	: HWnd(NULL)
	, WindowMode( EWindowMode::Windowed )
	, OLEReferenceCount(0)
	, bIsVisible( false )
	, bWasFullscreen( false )
{
	PreFullscreenWindowRect.left = PreFullscreenWindowRect.top = PreFullscreenWindowRect.right = PreFullscreenWindowRect.bottom = 0;
}

SDL_HWindow FLinuxWindow::GetHWnd() const
{
	return HWnd;
}

//	HINSTANCE InHInstance,
void FLinuxWindow::Initialize( FLinuxApplication* const Application, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FLinuxWindow >& InParent, const bool bShowImmediately )
{
	Definition = InDefinition;
	OwningApplication = Application;

	if (!FPlatformMisc::PlatformInitMultimedia()) //	will not initialize more than once
	{
		UE_LOG(LogInit, Fatal, TEXT("FLinuxWindow::Initialize() : PlatformInitMultimedia() failed, cannot initialize window."));
		// unreachable
		return;
	}

#if DO_CHECK
	uint32 InitializedSubsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	check(InitializedSubsystems & SDL_INIT_VIDEO);
#endif // DO_CHECK

	// Finally, let's initialize the new native window object.  Calling this function will often cause OS
	// window messages to be sent! (such as activation messages)
	uint32 WindowExStyle = 0;
	uint32 WindowStyle = 0;

	RegionWidth = RegionHeight = INDEX_NONE;

	const float XInitialRect = Definition->XDesiredPositionOnScreen;
	const float YInitialRect = Definition->YDesiredPositionOnScreen;

	const float WidthInitial = Definition->WidthDesiredOnScreen;
	const float HeightInitial = Definition->HeightDesiredOnScreen;

	int32 X = FMath::TruncToInt( XInitialRect + 0.5f );
	int32 Y = FMath::TruncToInt( YInitialRect + 0.5f );
	int32 ClientWidth = FMath::TruncToInt( WidthInitial + 0.5f );
	int32 ClientHeight = FMath::TruncToInt( HeightInitial + 0.5f );
	int32 WindowWidth = ClientWidth;
	int32 WindowHeight = ClientHeight;

	WindowStyle |= SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;

	if ( !Definition->HasOSWindowBorder )
	{
		WindowStyle |= SDL_WINDOW_BORDERLESS;

		if (Definition->IsTopmostWindow)
		{
			WindowStyle |= SDL_WINDOW_ALWAYS_ON_TOP;
		}

		if (!Definition->AppearsInTaskbar)
		{
			WindowStyle |= SDL_WINDOW_SKIP_TASKBAR;
		}
	}

	if (Definition->AcceptsInput)
	{
		WindowStyle |= SDL_WINDOW_ACCEPTS_INPUT;
	} 

	if ( Definition->HasSizingFrame )
	{
		WindowStyle |= SDL_WINDOW_RESIZABLE;
	}

	//	The SDL window doesn't need to be reshaped.
	//	the size of the window you input is the sizeof the client.
	HWnd = SDL_CreateWindow( TCHAR_TO_ANSI( *Definition->Title ), X, Y, ClientWidth, ClientHeight, WindowStyle  );
	SDL_SetWindowHitTest( HWnd, FLinuxWindow::HitTest, this );

	if (InParent.IsValid())
	{
		SDL_SetWindowModalFor(HWnd, InParent->GetHWnd());
	}

	VirtualWidth  = ClientWidth;
	VirtualHeight = ClientHeight;

	// We call reshape window here because we didn't take into account the non-client area
	// in the initial creation of the window. Slate should only pass client area dimensions.
	// Reshape window may resize the window if the non-client area is encroaching on our
	// desired client area space.
	ReshapeWindow( X, Y, ClientWidth, ClientHeight );

	if( HWnd == NULL )
	{
		// @todo Error message should be localized!
		checkf(0, TEXT("Window Creation Failed (%d)"), SDL_GetError() );
		return;
	}

	if ( Definition->SupportsTransparency )
	{
		SetOpacity( Definition->Opacity );
	}
}

SDL_HitTestResult FLinuxWindow::HitTest( SDL_Window *SDLwin, const SDL_Point *point, void *data )
{
	FLinuxWindow *pself = static_cast<FLinuxWindow *>( data );
	TSharedPtr< FLinuxWindow > self = pself->OwningApplication->FindWindowBySDLWindow( SDLwin );
	if ( !self.IsValid() ) 
	{
		UE_LOG(LogLinuxWindow, Warning, TEXT("BAD EVENT: SDL window = %p\n"), SDLwin);
		return SDL_HITTEST_NORMAL;
	}

	EWindowZone::Type eventZone = pself->OwningApplication->WindowHitTest( self, point->x, point->y );

	static const SDL_HitTestResult Results[] = 
	{
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_RESIZE_TOPLEFT,
		SDL_HITTEST_RESIZE_TOP,
		SDL_HITTEST_RESIZE_TOPRIGHT,
		SDL_HITTEST_RESIZE_LEFT,
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_RESIZE_RIGHT,
		SDL_HITTEST_RESIZE_BOTTOMLEFT,
		SDL_HITTEST_RESIZE_BOTTOM,
		SDL_HITTEST_RESIZE_BOTTOMRIGHT,
		SDL_HITTEST_DRAGGABLE,
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_NORMAL,
		SDL_HITTEST_NORMAL
	};

	return Results[eventZone];
}

/** Native windows should implement MoveWindowTo by relocating the platform-specific window to (X,Y). */
void FLinuxWindow::MoveWindowTo( int32 X, int32 Y )
{
	// we are passed coordinates of a client area, so account for decorations
	SDL_Rect Borders;
	if (SDL_GetWindowBordersSize(HWnd, &Borders) == 0)
	{
		X -= Borders.x;
		Y -= Borders.y;
	}
    SDL_SetWindowPosition( HWnd, X, Y );
}

/** Native windows should implement BringToFront by making this window the top-most window (i.e. focused).
 *
 * @param bForce	Forces the window to the top of the Z order, even if that means stealing focus from other windows
 *					In general do not pass true for this.  It is really only useful for some windows, like game windows where not forcing it to the front
 *					could cause mouse capture and mouse lock to happen but without the window visible
 */
void FLinuxWindow::BringToFront( bool bForce )
{
	if (bForce)
	{
		SDL_RaiseWindow(HWnd);
	}
	else
	{
		// FIXME: we don't bring it to front here now
		SDL_ShowWindow(HWnd);
	}
}

/** Native windows should implement this function by asking the OS to destroy OS-specific resource associated with the window (e.g. Win32 window handle) */
void FLinuxWindow::Destroy()
{
	OwningApplication->RemoveEventWindow( HWnd );
	SDL_DestroyWindow( HWnd );
}

/** Native window should implement this function by performing the equivalent of the Win32 minimize-to-taskbar operation */
void FLinuxWindow::Minimize()
{
	SDL_MinimizeWindow( HWnd );
}

/** Native window should implement this function by performing the equivalent of the Win32 maximize operation */
void FLinuxWindow::Maximize()
{
	SDL_MaximizeWindow( HWnd );
}

/** Native window should implement this function by performing the equivalent of the Win32 maximize operation */
void FLinuxWindow::Restore()
{
	SDL_RestoreWindow( HWnd );
}

/** Native window should make itself visible */
void FLinuxWindow::Show()
{
	if ( !bIsVisible )
	{
		bIsVisible = true;
		SDL_ShowWindow( HWnd );
	}
}

/** Native window should hide itself */
void FLinuxWindow::Hide()
{
	if ( bIsVisible )
	{
		bIsVisible = false;
		SDL_HideWindow( HWnd );
	}
}


static void _GetBestFullscreenResolution( SDL_HWindow hWnd, int32 *pWidth, int32 *pHeight )
{
	uint32 InitializedMode = false;
	uint32 BestWidth = 0;
	uint32 BestHeight = 0;
	uint32 ModeIndex = 0;

	int32 dsp_idx = SDL_GetWindowDisplayIndex( hWnd );
	if ( dsp_idx < 0 )
	{	dsp_idx = 0;	}

	SDL_DisplayMode dsp_mode;
	FMemory::Memzero( &dsp_mode, sizeof(SDL_DisplayMode) );

	while ( !SDL_GetDisplayMode( dsp_idx, ModeIndex++, &dsp_mode ) )
	{
		bool IsEqualOrBetterWidth  = FMath::Abs((int32)dsp_mode.w - (int32)(*pWidth))  <= FMath::Abs((int32)BestWidth  - (int32)(*pWidth ));
		bool IsEqualOrBetterHeight = FMath::Abs((int32)dsp_mode.h - (int32)(*pHeight)) <= FMath::Abs((int32)BestHeight - (int32)(*pHeight));
		if	(!InitializedMode || (IsEqualOrBetterWidth && IsEqualOrBetterHeight))
		{
			BestWidth = dsp_mode.w;
			BestHeight = dsp_mode.h;
			InitializedMode = true;
		}
	}

	check(InitializedMode);

	*pWidth  = BestWidth;
	*pHeight = BestHeight;
}


static void _SetBestFullscreenDisplayMode( SDL_HWindow hWnd, int32 *pWidth, int32 *pHeight )
{
	SDL_DisplayMode dsp_mode;

	dsp_mode.w = *pWidth;
	dsp_mode.h = *pHeight;
	dsp_mode.format = SDL_PIXELFORMAT_ARGB8888;
	dsp_mode.refresh_rate = 60;
	dsp_mode.driverdata = NULL;

	_GetBestFullscreenResolution( hWnd, &dsp_mode.w, &dsp_mode.h );
	SDL_SetWindowDisplayMode( hWnd, &dsp_mode );

	*pWidth  = dsp_mode.w;
	*pHeight = dsp_mode.h;
}

void FLinuxWindow::ReshapeWindow( int32 NewX, int32 NewY, int32 NewWidth, int32 NewHeight )
{
	int32 closest_w = NewWidth;
	int32 closest_h = NewHeight;

	switch( WindowMode )
	{
		case EWindowMode::Fullscreen:
		{
			SDL_SetWindowFullscreen( HWnd, 0 );
			_GetBestFullscreenResolution( HWnd, &closest_w, &closest_h );
			SDL_SetWindowSize( HWnd, closest_w, closest_h );

			_SetBestFullscreenDisplayMode( HWnd, &closest_w, &closest_h );
			SDL_SetWindowFullscreen( HWnd, SDL_WINDOW_FULLSCREEN );

			bWasFullscreen = true;

		}	break;

		case EWindowMode::WindowedFullscreen:
		{
			SDL_SetWindowFullscreen( HWnd, 0 );
			SDL_SetWindowPosition( HWnd, 0, 0 );
			_SetBestFullscreenDisplayMode( HWnd, &closest_w, &closest_h );
			SDL_SetWindowFullscreen( HWnd, SDL_WINDOW_FULLSCREEN_DESKTOP );

			bWasFullscreen = true;

		}	break;

		case EWindowMode::Windowed:
		{
			if (Definition->HasOSWindowBorder)
			{
				// we are passed coordinates of a client area, so account for decorations
				SDL_Rect Borders;
				if (SDL_GetWindowBordersSize(HWnd, &Borders) == 0)
				{
					NewX -= Borders.x;
					NewY -= Borders.y;
				}
			}
			SDL_SetWindowPosition( HWnd, NewX, NewY );
			SDL_SetWindowSize( HWnd, NewWidth, NewHeight );

			bWasFullscreen = false;

		}	break;
	}

	RegionWidth   = NewWidth;
	RegionHeight  = NewHeight;
	VirtualWidth  = NewWidth;
	VirtualHeight = NewHeight;
}

/** Toggle native window between fullscreen and normal mode */
void FLinuxWindow::SetWindowMode( EWindowMode::Type NewWindowMode )
{
	if( NewWindowMode != WindowMode )
	{
		SDL_DisplayMode dsp_mode;
		int32 closest_w;
		int32 closest_h;

		closest_w = VirtualWidth;
		closest_h = VirtualHeight;

		switch( NewWindowMode )
		{
			case EWindowMode::Fullscreen:
			{
				if ( bWasFullscreen != true )
				{
					SDL_SetWindowPosition( HWnd, 0, 0 );
					_SetBestFullscreenDisplayMode( HWnd, &closest_w, &closest_h );

					SDL_SetWindowFullscreen( HWnd, SDL_WINDOW_FULLSCREEN );
					SDL_SetWindowGrab( HWnd, SDL_TRUE );

					bWasFullscreen = true;
				}

			}	break;

			case EWindowMode::WindowedFullscreen:
			{
				if ( bWasFullscreen != true )
				{
					SDL_SetWindowPosition( HWnd, 0, 0 );

					_SetBestFullscreenDisplayMode( HWnd, &closest_w, &closest_h );
					SDL_SetWindowFullscreen( HWnd, SDL_WINDOW_FULLSCREEN_DESKTOP );

					bWasFullscreen = true;
				}

			}	break;

			case EWindowMode::Windowed:
			{
				SDL_SetWindowFullscreen( HWnd, 0 );
				SDL_SetWindowBordered( HWnd, SDL_TRUE );
				SDL_SetWindowSize( HWnd, VirtualWidth, VirtualHeight );

				SDL_SetWindowGrab( HWnd, SDL_FALSE );

				bWasFullscreen = false;
			}	break;
		}


		WindowMode = NewWindowMode;
	}
}


/** @return	Gives the native window a chance to adjust our stored window size before we cache it off */

void FLinuxWindow::AdjustCachedSize( FVector2D& Size ) const
{
	if	( Definition.IsValid() && Definition->SizeWillChangeOften )
	{
		Size = FVector2D( VirtualWidth, VirtualHeight );
	}
	else
	if	( HWnd )
	{
		int SizeW, SizeH;

		if ( WindowMode == EWindowMode::Windowed )
		{
			SDL_GetWindowSize( HWnd, &SizeW, &SizeH );
		}
		else // windowed fullscreen or fullscreen
		{
			SizeW = VirtualWidth ;
			SizeH = VirtualHeight;

			_GetBestFullscreenResolution( HWnd, &SizeW, &SizeH );
		}

		Size = FVector2D( SizeW, SizeH );
	}
}

bool FLinuxWindow::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	SDL_Rect DisplayRect;

	int DisplayIdx = SDL_GetWindowDisplayIndex(HWnd);
	if (DisplayIdx >= 0 && SDL_GetDisplayBounds(DisplayIdx, &DisplayRect) == 0)
	{
		X = DisplayRect.x;
		Y = DisplayRect.y;
		Width = DisplayRect.w;
		Height = DisplayRect.h;

		return true;
	}

	return false;
}

/** @return true if the native window is maximized, false otherwise */
bool FLinuxWindow::IsMaximized() const
{
	uint32 flag = SDL_GetWindowFlags( HWnd );

	if ( flag & SDL_WINDOW_MAXIMIZED )
	{
		return true;
	}
	else
	{
		return false;
	}

}

/** @return true if the native window is visible, false otherwise */
bool FLinuxWindow::IsVisible() const
{
	return bIsVisible;
}

/** Returns the size and location of the window when it is restored */
bool FLinuxWindow::GetRestoredDimensions(int32& X, int32& Y, int32& Width, int32& Height)
{
	SDL_GetWindowPosition(HWnd, &X, &Y);
	SDL_GetWindowSize(HWnd, &Width, &Height);

	return true;
}

/** Sets focus on the native window */
void FLinuxWindow::SetWindowFocus()
{
	SDL_SetWindowActive( HWnd );
}

/**
 * Sets the opacity of this window
 *
 * @param	InOpacity	The new window opacity represented as a floating point scalar
 */
void FLinuxWindow::SetOpacity( const float InOpacity )
{
	SDL_SetWindowOpacity(HWnd, InOpacity);
}

/**
 * Enables or disables this window.  When disabled, a window will receive no input.       
 *
 * @param bEnable	true to enable the window, false to disable it.
 */
void FLinuxWindow::Enable( bool bEnable )
{
	//	SDL2 doesn't offer such functionality...
	//  Could be added to ds_extenstion
}

/** @return true if native window exists underneath the coordinates */
bool FLinuxWindow::IsPointInWindow( int32 X, int32 Y ) const
{
	int32 width = 0, height = 0;

	SDL_GetWindowSize( HWnd, &width, &height );
	
	return X > 0 && Y > 0 && X < width && Y < height;
}

int32 FLinuxWindow::GetWindowBorderSize() const
{
	// need to lie and return 0 border size even if we have a border.
	// reporting anything else causes problems in Slate with menu
	// positioning.
	return 0;
}


bool FLinuxWindow::IsForegroundWindow() const
{
	return (HWnd == SDL_GetMouseFocus() || HWnd == SDL_GetKeyboardFocus());
}


void FLinuxWindow::SetText( const TCHAR* const Text )
{
	SDL_SetWindowTitle( HWnd, TCHAR_TO_ANSI(Text));
}


bool FLinuxWindow::IsRegularWindow() const
{
	return Definition->IsRegularWindow;
}

