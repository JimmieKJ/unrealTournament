// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StandaloneRendererPrivate.h"
#include "OpenGL/SlateOpenGLRenderer.h"

static SDL_Window* CreateDummyGLWindow()
{
	FPlatformMisc::PlatformInitMultimedia(); //	will not initialize more than once

#if DO_CHECK
	uint32 InitializedSubsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	check(InitializedSubsystems & SDL_INIT_VIDEO);
#endif // DO_CHECK

	// Create a dummy window.
	SDL_Window *h_wnd = SDL_CreateWindow(	NULL,
		0, 0, 1, 1,
		SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN );

	return h_wnd;
}

FSlateOpenGLContext::FSlateOpenGLContext()
	: WindowHandle(NULL)
	, Context(NULL)
	, bReleaseWindowOnDestroy(false)
#if LINUX_USE_OPENGL_3_2
	, VertexArrayObject(0)
#endif // LINUX_USE_OPENGL_3_2
{
}

FSlateOpenGLContext::~FSlateOpenGLContext()
{
	Destroy();
}

void FSlateOpenGLContext::Initialize( void* InWindow, const FSlateOpenGLContext* SharedContext )
{
	WindowHandle = (SDL_Window*)InWindow;

	if	( WindowHandle == NULL )
	{
		WindowHandle = CreateDummyGLWindow();
		bReleaseWindowOnDestroy = true;
	}

	if (SDL_GL_LoadLibrary(nullptr))
	{
		FString SdlError(ANSI_TO_TCHAR(SDL_GetError()));
		UE_LOG(LogInit, Fatal, TEXT("FSlateOpenGLContext::Initialize - Unable to dynamically load libGL: %s\n."), *SdlError);
	}

	int OpenGLMajorVersionToUse = 2;
	int OpenGLMinorVersionToUse = 1;

#if LINUX_USE_OPENGL_3_2
	OpenGLMajorVersionToUse = 3;
	OpenGLMinorVersionToUse = 2;
#endif // LINUX_USE_OPENGL_3_2

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, OpenGLMajorVersionToUse );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, OpenGLMinorVersionToUse );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG );

	if( SharedContext )
	{
		SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1 );
		SDL_GL_MakeCurrent( SharedContext->WindowHandle, SharedContext->Context );
	}
	else
	{
		SDL_GL_SetAttribute( SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0 );
	}

	UE_LOG(LogInit, Log, TEXT("FSlateOpenGLContext::Initialize - creating OpenGL %d.%d context"), OpenGLMajorVersionToUse, OpenGLMinorVersionToUse);
	Context = SDL_GL_CreateContext( WindowHandle );
	if (Context == nullptr)
	{
		FString SdlError(ANSI_TO_TCHAR(SDL_GetError()));
		
		// ignore errors getting version, it will be clear from the logs
		int OpenGLMajorVersion = -1;
		int OpenGLMinorVersion = -1;
		SDL_GL_GetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, &OpenGLMajorVersion );
		SDL_GL_GetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, &OpenGLMinorVersion );

		UE_LOG(LogInit, Fatal, TEXT("FSlateOpenGLContext::Initialize - Could not create OpenGL %d.%d context, SDL error: '%s'"), 
				OpenGLMajorVersion, OpenGLMinorVersion,
				*SdlError
			);
		// unreachable
		return;
	}
	SDL_GL_MakeCurrent( WindowHandle, Context );

	LoadOpenGLExtensions();

#if LINUX_USE_OPENGL_3_2
	// one Vertex Array Object is reportedly needed for OpenGL 3.2+ core profiles
	glGenVertexArrays(1, &VertexArrayObject);
	glBindVertexArray(VertexArrayObject);
#endif // LINUX_USE_OPENGL_3_2
}

void FSlateOpenGLContext::Destroy()
{
	if	( WindowHandle != NULL )
	{
		SDL_GL_MakeCurrent( NULL, NULL );
#if LINUX_USE_OPENGL_3_2
		glDeleteVertexArrays(1, &VertexArrayObject);
#endif // LINUX_USE_OPENGL_3_2
		SDL_GL_DeleteContext( Context );

		if	( bReleaseWindowOnDestroy )
		{
			SDL_DestroyWindow( WindowHandle );
			// we will tear down SDL in PlatformTearDown()
		}
		WindowHandle = NULL;

	}
}

void FSlateOpenGLContext::MakeCurrent()
{
	if	( WindowHandle )
	{
		if(SDL_GL_MakeCurrent( WindowHandle, Context ) == 0)
		{
#if LINUX_USE_OPENGL_3_2
			glBindVertexArray(VertexArrayObject);
#endif // LINUX_USE_OPENGL_3_2
		}
	}
}
