// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "StandaloneRendererPrivate.h"

#include "OpenGL/SlateOpenGLRenderer.h"

#include "MacWindow.h"

/** Used to temporarily disable Cocoa screen updates to make window updates happen only on the render thread. */
bool GMacEnableCocoaScreenUpdates = true;

FSlateOpenGLViewport::FSlateOpenGLViewport()
{
}

void FSlateOpenGLViewport::Initialize( TSharedRef<SWindow> InWindow, const FSlateOpenGLContext& SharedContext )
{
	TSharedRef<FGenericWindow> NativeWindow = InWindow->GetNativeWindow().ToSharedRef();
	RenderingContext.Initialize( NativeWindow->GetOSWindowHandle(), &SharedContext );

	const int32 Width = FMath::TruncToInt(InWindow->GetSizeInScreen().X);
	const int32 Height = FMath::TruncToInt(InWindow->GetSizeInScreen().Y);

	ProjectionMatrix = CreateProjectionMatrix( Width, Height );

	ViewportRect.Right = Width;
	ViewportRect.Bottom = Height;
	ViewportRect.Top = 0;
	ViewportRect.Left = 0;
}

void FSlateOpenGLViewport::Destroy()
{
	RenderingContext.Destroy();
}

void FSlateOpenGLViewport::MakeCurrent()
{
	RenderingContext.MakeCurrent();
}

void FSlateOpenGLViewport::SwapBuffers()
{
	// Clear the Alpha channel
	glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
	glClearColor(0.f,0.f,0.f,1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
	glClearColor(0.f,0.f,0.f,0.f);
	
	if(RenderingContext.CompositeProgram && RenderingContext.CompositeTexture)
	{
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_BLEND);
		
		glBindVertexArrayAPPLE(RenderingContext.CompositeVAO);
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		
		glActiveTexture(GL_TEXTURE0);
		glBindTexture (GL_TEXTURE_2D, RenderingContext.CompositeTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
		static const float Position[12] = {
		-1.0,-1.0,
		 1.0,-1.0,
		 1.0, 1.0,
		-1.0,-1.0,
		 1.0, 1.0,
		-1.0, 1.0
		};
		
		glVertexAttribPointerARB(0, 2, GL_FLOAT, GL_FALSE, 0, Position);
		glEnableVertexAttribArrayARB(0);
		
		static const float TexCoords[12] = {
		0.0, 1.0,
		1.0, 1.0,
		1.0, 0.0,
		0.0, 1.0,
		1.0, 0.0,
		0.0, 0.0
		};
		
		glVertexAttribPointerARB(1, 2, GL_FLOAT, GL_FALSE, 0, TexCoords);
		glEnableVertexAttribArrayARB(1);
		
		glUseProgram(RenderingContext.CompositeProgram);
		
		glUniform1i(RenderingContext.WindowTextureUniform, 0);
		
		glUniform1i(RenderingContext.TextureDirectionUniform, 0);
		
		glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
		
		glViewport(0, ViewportRect.Bottom-32, 32, 32);
		
		glDrawArraysInstancedARB(GL_TRIANGLES, 0, 6, 1);
		
		glUniform1i(RenderingContext.TextureDirectionUniform, 1);
		
		glViewport(ViewportRect.Right-32, ViewportRect.Bottom-32, 32, 32);
		
		glDrawArraysInstancedARB(GL_TRIANGLES, 0, 6, 1);
		
		glViewport(0, 0, ViewportRect.Right, ViewportRect.Bottom);
		
		glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
		
		glBindTexture (GL_TEXTURE_2D, 0);
		
		glBindVertexArrayAPPLE(0);
		
		glEnable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
	}
	
	[RenderingContext.Context flushBuffer];
	
	if(!GMacEnableCocoaScreenUpdates)
	{
		GMacEnableCocoaScreenUpdates = true;
		NSEnableScreenUpdates();
	}
	
	NSWindow* Window = [[RenderingContext.Context view] window];
	FCocoaWindow* SlateCocoaWindow = [Window isKindOfClass:[FCocoaWindow class]] ? (FCocoaWindow*)Window : nil;
	if(SlateCocoaWindow)
	{
		[SlateCocoaWindow performDeferredOrderFront];
	}
}

void FSlateOpenGLViewport::Resize( int32 Width, int32 Height, bool bInFullscreen )
{
	ViewportRect.Right = Width;
	ViewportRect.Bottom = Height;

	// Need to create a new projection matrix each time the window is resized
	ProjectionMatrix = CreateProjectionMatrix( Width, Height );

	[RenderingContext.Context update];
}
