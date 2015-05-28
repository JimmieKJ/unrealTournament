// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "HTML5Window.h"

#if !PLATFORM_HTML5_WIN32 
#include <emscripten.h>
#endif
#include <SDL.h>

DEFINE_LOG_CATEGORY_STATIC(LogHTML5Window, Log, All);

FHTML5Window::~FHTML5Window()
{
	//    Use NativeWindow_Destroy() instead.
}

TSharedRef<FHTML5Window> FHTML5Window::Make()
{
	return MakeShareable( new FHTML5Window() );
}

FHTML5Window::FHTML5Window()
{
}

bool FHTML5Window::GetFullScreenInfo( int32& X, int32& Y, int32& Width, int32& Height ) const
{
	FPlatformRect ScreenRect = GetScreenRect();
	X = ScreenRect.Left;
	Y = ScreenRect.Top;
	Width = ScreenRect.Right - ScreenRect.Left;
	Height = ScreenRect.Bottom - ScreenRect.Top;
	return true;
}


void FHTML5Window::SetOSWindowHandle(void* InWindow)
{
}


FPlatformRect FHTML5Window::GetScreenRect()
{
	FPlatformRect ScreenRect;
	ScreenRect.Left = 0;
	ScreenRect.Top = 0;

	int Width, Height;
#if !PLATFORM_HTML5_WIN32
	int fs;
	emscripten_get_canvas_size(&Width, &Height, &fs);
	UE_LOG(LogHTML5Window, Verbose, TEXT("emscripten_get_canvas_size: Width:%d, Height:%d, Fullscreen:%d"), Width, Height, fs);
#else
	SDL_Window* WindowHandle= SDL_GL_GetCurrentWindow();
	SDL_GetWindowSize(WindowHandle, &Width, &Height);
	UE_LOG(LogHTML5Window, Verbose, TEXT("SDL_GetWindowSize: Width:%d, Height:%d"), Width, Height);
#endif 
	CalculateSurfaceSize(NULL,Width,Height);
	ScreenRect.Right = Width;
	ScreenRect.Bottom = Height;
	return ScreenRect;
}

void FHTML5Window::CalculateSurfaceSize(void* InWindow, int32_t& SurfaceWidth, int32_t& SurfaceHeight)
{
	// ensure the size is divisible by a specified amount
	const int DividableBy = 8;
	SurfaceWidth  = ((SurfaceWidth  + DividableBy - 1) / DividableBy) * DividableBy;
	SurfaceHeight = ((SurfaceHeight + DividableBy - 1) / DividableBy) * DividableBy;

}

EWindowMode::Type FHTML5Window::GetWindowMode() const 
{
#if !PLATFORM_HTML5_WIN32
	int Width,Height,FullScreen; 
	emscripten_get_canvas_size(&Width,&Height,&FullScreen);
	return FullScreen ? EWindowMode::Fullscreen : EWindowMode::Windowed;
#else
	return EWindowMode::Windowed; 
#endif 

}

void FHTML5Window::ReshapeWindow(int32 X, int32 Y, int32 Width, int32 Height)
{
}
