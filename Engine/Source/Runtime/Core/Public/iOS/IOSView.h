// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>

#if HAS_METAL
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#endif

@interface FIOSView : UIView
{
@public
	// are we initialized?
	bool bIsInitialized;

//@private
	// keeps track of the number of active touches
	// used to bring up the three finger touch debug console after 3 active touches are registered
	int NumActiveTouches;

	// track the touches by pointer (which will stay the same for a given finger down) - note we don't deref the pointers in this array
	UITouch* AllTouches[10];


	//// GL MEMBERS
	// the GL context
	EAGLContext* Context;

	// the internal MSAA FBO used to resolve the color buffer at present-time
	GLuint ResolveFrameBuffer;


	//// METAL MEMBERS
#if HAS_METAL
	// global metal device
	id<MTLDevice> MetalDevice;
#endif

	// are we using the Metal API?
	bool bIsUsingMetal;
}

//// SHARED FUNCTIONALITY
@property (nonatomic) GLuint SwapCount;

-(bool)CreateFramebuffer:(bool)bIsForOnDevice;
-(void)DestroyFramebuffer;
-(void)UpdateRenderWidth:(uint32)Width andHeight:(uint32)Height;

//// GL FUNCTIONALITY
@property (nonatomic) GLuint OnScreenColorRenderBuffer;
@property (nonatomic) GLuint OnScreenColorRenderBufferMSAA;

- (void)MakeCurrent;
- (void)UnmakeCurrent;
- (void)SwapBuffers;

//// METAL FUNCTIONALITY
#if HAS_METAL
// Return a drawable object (ie a back buffer texture) for the RHI to render to
- (id<CAMetalDrawable>)MakeDrawable;
#endif

@end

/**
 * A view controller subclass that handles loading our IOS view as well as autorotation
 */
@interface IOSViewController : UIViewController
{

}
@end
