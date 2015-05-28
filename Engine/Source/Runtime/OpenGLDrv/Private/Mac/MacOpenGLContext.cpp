// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OpenGLDrvPrivate.h"

#include "MacOpenGLContext.h"
#include "MacOpenGLQuery.h"

#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <OpenGL/gl3.h>

/*------------------------------------------------------------------------------
 OpenGL defines.
 ------------------------------------------------------------------------------*/

#if WITH_EDITOR
#define MAC_OPENGL_SETTINGS TEXT("/Script/MacGraphicsSwitching.MacGraphicsSwitchingSettings")
#define MAC_OPENGL_INI GEditorSettingsIni
#else
#define MAC_OPENGL_SETTINGS TEXT("OpenGL")
#define MAC_OPENGL_INI GEngineIni
#endif

/*------------------------------------------------------------------------------
 OpenGL static variables.
 ------------------------------------------------------------------------------*/

/** Whether we allow use of multiple GPUs. */
static bool GMacUseMultipleGPUs = false;

/** Whether we use automatic graphics switching or rely on the system to handle it for us. */
static bool GMacUseAutomaticGraphicsSwitching = false;

/** The current global output renderer ID. */
static GLint GMacCurrentRendererID = 0;

/** The selected explicit renderer ID. */
static int32 GMacExplicitRendererID = 0;
static FAutoConsoleVariableRef CVarMacExplicitRendererID(
	TEXT("r.Mac.ExplicitRendererID"),
	GMacExplicitRendererID,
	TEXT("Explicitly sets all contexts to the provided renderer ID when using automatic graphics switching. (Default: 0, off)"),
	ECVF_RenderThreadSafe
	);

/*------------------------------------------------------------------------------
 OpenGL internal static variables.
 ------------------------------------------------------------------------------*/

/** Default context attributes */
const NSOpenGLPixelFormatAttribute ContextCreationAttributes[] =
{
	kCGLPFAOpenGLProfile,
	kCGLOGLPVersion_3_2_Core,
	kCGLPFAAccelerated,
	kCGLPFANoRecovery,
	kCGLPFADoubleBuffer,
	kCGLPFAColorSize,
	32,
	0
};

/** The context pixel format is cached to reduce the expense of creating new contexts. */
static NSOpenGLPixelFormat* PixelFormat = nil;

/*------------------------------------------------------------------------------
	OpenGL context helper functions.
------------------------------------------------------------------------------*/

static NSOpenGLContext* CreateContext( NSOpenGLContext* SharedContext )
{
	SCOPED_AUTORELEASE_POOL;

	if (!PixelFormat)
	{
		TArray<NSOpenGLPixelFormatAttribute> Attributes;
		Attributes.Append(ContextCreationAttributes, ARRAY_COUNT(ContextCreationAttributes) - 1);
		
		int32 DisplayMask = 0;
		if (GConfig->GetInt(MAC_OPENGL_SETTINGS, TEXT("DisplayMask"), DisplayMask, MAC_OPENGL_INI) && DisplayMask)
		{
			Attributes.Add(NSOpenGLPFAScreenMask);
			Attributes.Add(DisplayMask);
		}
		
		// Only initialise explicit renderer ID values once, require a restart to change, as otherwise the contexts cannot be in the same share group
		static bool bExplicitRendererSetup = false;
		if(!bExplicitRendererSetup)
		{
			int32 ExplicitRendererId = 0;
			if(GConfig->GetInt(MAC_OPENGL_SETTINGS, TEXT("RendererID"), ExplicitRendererId, MAC_OPENGL_INI) && ExplicitRendererId)
			{
				// Be sure to test that the renderer actually exists, otherwise device creation will fail and that'd be bad
				bool bRendererFound = false;
				GLint NumberOfRenderers = 0;
				CGLRendererInfoObj RendererInfo;
				// If the display mask is overridden then respect that and disallow renderers not bound to that display
				int32 OpenGLDisplayMask = DisplayMask ? DisplayMask : 0xffffffff;
				CGLQueryRendererInfo(OpenGLDisplayMask, &RendererInfo, &NumberOfRenderers);
				if(RendererInfo)
				{
					for(uint32 i = 0; i < NumberOfRenderers; i++)
					{
						GLint DeviceRendererID = 0;
						CGLDescribeRenderer(RendererInfo, i, kCGLRPRendererID, &DeviceRendererID);
						if(ExplicitRendererId == DeviceRendererID)
						{
							bRendererFound = true;
							break;
						}
					}
					
					CGLDestroyRendererInfo(RendererInfo);
				}
				
				GMacExplicitRendererID = bRendererFound ? ExplicitRendererId : 0;
				GMacCurrentRendererID = GMacExplicitRendererID;
			}
			bExplicitRendererSetup = true;
		}
		
		if(DisplayMask || GMacExplicitRendererID || GNumActiveGPUsForRendering > 1 || GMacUseAutomaticGraphicsSwitching)
		{
			Attributes.Add(kCGLPFASupportsAutomaticGraphicsSwitching);
			Attributes.Add(NSOpenGLPFAAllowOfflineRenderers);
		}
		
		// Specify a single explicit renderer ID
		if (GMacExplicitRendererID && (GNumActiveGPUsForRendering == 1) && !GMacUseAutomaticGraphicsSwitching)
		{
			Attributes.Add(NSOpenGLPFARendererID);
			Attributes.Add(GMacExplicitRendererID);
		}
		
		Attributes.Add(0);
		
		PixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes: Attributes.GetData()];
		check(PixelFormat);
	}

	NSOpenGLContext* Context = [[NSOpenGLContext alloc] initWithFormat: PixelFormat shareContext: SharedContext];
	check(Context);
	
	if((GNumActiveGPUsForRendering > 1 || GMacUseAutomaticGraphicsSwitching) && GMacExplicitRendererID)
	{
		for(uint32 i = 0; i < [PixelFormat numberOfVirtualScreens]; i++)
		{
			GLint ScreenRendererID = 0;
			[PixelFormat getValues:&ScreenRendererID forAttribute:NSOpenGLPFARendererID forVirtualScreen:i];
			
			if(ScreenRendererID == GMacExplicitRendererID)
			{
				[Context setCurrentVirtualScreen:i];
				break;
			}
		}
	}

	int32 SyncInterval = 0;
	[Context setValues: &SyncInterval forParameter: NSOpenGLCPSwapInterval];

	extern int32 GMacUseMTGL;
	if (GMacUseMTGL)
	{
		CGLEnable((CGLContextObj)[Context CGLContextObj], kCGLCEMPEngine);
		
		// @todo: We should disable OpenGL.UseMapBuffer for improved performance under MTGL, but radr://17662549 prevents it when using GL_TEXTURE_BUFFER's for skinning.
	}

	return Context;
}

void MacOpenGLContextReconfigurationCallBack(CGDirectDisplayID Display, CGDisplayChangeSummaryFlags Flags, void* UserInfo)
{
    if(GMacUseAutomaticGraphicsSwitching && (Flags & kCGDisplaySetModeFlag))
    {
        // Display has been reconfigured so find the online GPU so we may adapt to the new capabilities.
		GLint NumberOfRenderers = 0;
		CGLRendererInfoObj RendererInfo;
		CGLQueryRendererInfo(0xffffffff, &RendererInfo, &NumberOfRenderers);
		if(RendererInfo)
		{
			for(uint32 i = 0; i < NumberOfRenderers; i++)
			{
				GLint DeviceOnline = 0;
				CGLDescribeRenderer(RendererInfo, i, kCGLRPOnline, &DeviceOnline);
				if(DeviceOnline)
				{
					CGLDescribeRenderer(RendererInfo, i, kCGLRPRendererID, &GMacCurrentRendererID);
					break;
				}
			}
			
			CGLDestroyRendererInfo(RendererInfo);
		}
    }
}
	
/*------------------------------------------------------------------------------
 OpenGL context.
 ------------------------------------------------------------------------------*/

FPlatformOpenGLContext::FPlatformOpenGLContext()
: OpenGLContext(nil)
, OpenGLPixelFormat(nil)
, OpenGLView(nil)
, WindowHandle(nil)
, EmulatedQueries(nullptr)
, VertexArrayObject(0)
, ViewportFramebuffer(0)
, ViewportRenderbuffer(0)
, SyncInterval(0)
, VirtualScreen(0)
, VendorID(0)
, RendererID(0)
, SupportsDepthFetchDuringDepthTest(true)
{
	FMemory::Memzero(ViewportSize);
}

FPlatformOpenGLContext::~FPlatformOpenGLContext()
{
	SCOPED_AUTORELEASE_POOL;
	
	NSOpenGLContext* CurrentContext = [NSOpenGLContext currentContext];
	bool bMadeCurrent = false;
	
	if ((ViewportFramebuffer && !CurrentContext) || VertexArrayObject || GIsEmulatingTimestamp)
	{
		MakeCurrent();
		bMadeCurrent = true;
	}
	
	if (ViewportRenderbuffer)
	{
		// this can be done from any context, as long as it's not nil.
		glDeleteRenderbuffers(1, &ViewportRenderbuffer);
		ViewportRenderbuffer = 0;
	}

	if (ViewportFramebuffer)
	{
		// this can be done from any context, as long as it's not nil.
		glDeleteFramebuffers(1, &ViewportFramebuffer);
	}
	
	if(VertexArrayObject)
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &VertexArrayObject);
	}
	
	delete EmulatedQueries;
	
	if ( OpenGLContext )
	{
		// Unbind the platform context from the CGL context
		intptr_t Val = 0;
		CGLSetParameter((CGLContextObj)[OpenGLContext CGLContextObj], kCGLCPClientStorage, (GLint*)&Val);
		
		if (CurrentContext == OpenGLContext)
		{
			CurrentContext = nil;
		}
		
		[OpenGLContext release];
	}
	
	if ( bMadeCurrent || CurrentContext == nil )
	{
		if ( CurrentContext && CurrentContext != OpenGLContext )
		{
			[CurrentContext makeCurrentContext];
		}
		else
		{
			[NSOpenGLContext clearCurrentContext];
		}
	}
}
	
void FPlatformOpenGLContext::Initialise( NSOpenGLContext* const SharedContext )
{
	SCOPED_AUTORELEASE_POOL;
	
	if ( !OpenGLContext )
	{
		OpenGLContext = CreateContext(SharedContext);
		OpenGLPixelFormat = PixelFormat;
	}
	check(OpenGLContext);
	
	MakeCurrent();
	
	// Initialise default stuff.
	extern void InitDebugContext();
	InitDebugContext();
	
	InitDefaultGLContextState();
	
	if ( !VertexArrayObject )
	{
		glGenVertexArrays(1,&VertexArrayObject);
		glBindVertexArray(VertexArrayObject);
	}
	
	// Bind our context pointer into the CGL user pointer field
	intptr_t ContextPtr = (intptr_t)this;
	CGLSetParameter((CGLContextObj)[OpenGLContext CGLContextObj], kCGLCPClientStorage, (GLint*)&ContextPtr);
	
	CGLGetVirtualScreen((CGLContextObj)[OpenGLContext CGLContextObj], &VirtualScreen);
	
	EmulatedQueries = new FMacOpenGLQueryEmu(this);
}

void FPlatformOpenGLContext::Reset(void)
{
	SCOPED_AUTORELEASE_POOL;
	
	NSOpenGLContext* CurrentContext = [NSOpenGLContext currentContext];
	bool bMadeCurrent = false;
	
	if ((ViewportFramebuffer && !CurrentContext) || GIsEmulatingTimestamp)
	{
		MakeCurrent();
		bMadeCurrent = true;
	}
	
	FMemory::Memzero(ViewportSize);
	
	if (ViewportRenderbuffer)
	{
		// this can be done from any context, as long as it's not nil.
		glDeleteRenderbuffers(1, &ViewportRenderbuffer);
		ViewportRenderbuffer = 0;
	}

	if (ViewportFramebuffer)
	{
		// this can be done from any context, as long as it's not nil.
		glDeleteFramebuffers(1, &ViewportFramebuffer);
		ViewportFramebuffer = 0;
	}

	[OpenGLContext clearDrawable];
	
	if ( OpenGLView )
	{
		[OpenGLView release];
		OpenGLView = nil;
	}
	
	WindowHandle = nil;
	
	delete EmulatedQueries;
	EmulatedQueries = nullptr;
	
	if ( bMadeCurrent )
	{
		if ( CurrentContext )
		{
			[CurrentContext makeCurrentContext];
		}
		else
		{
			[NSOpenGLContext clearCurrentContext];
		}
	}
}

void FPlatformOpenGLContext::MakeCurrent(void)
{
	SCOPED_AUTORELEASE_POOL;
	
	check(OpenGLContext);
	if ( [NSOpenGLContext currentContext] )
	{
		glFlushRenderAPPLE();
	}
	
	// Make the context current
	[OpenGLContext makeCurrentContext];
	
	// Ensure that the OpenGL device state hasn't changed.
	VerifyCurrentContext();
}

void FPlatformOpenGLContext::VerifyCurrentContext()
{
	CGLContextObj Current = CGLGetCurrentContext();
	FPlatformOpenGLContext* PlatformContext = nullptr;
	if(Current && (CGLGetParameter(Current, kCGLCPClientStorage, (GLint *)&PlatformContext) == kCGLNoError) && PlatformContext)
	{
		// Get the current renderer ID
		GLint RendererID = 0;
		CGLError Error = CGLGetParameter(Current, kCGLCPCurrentRendererID, &RendererID);
		check(Error == kCGLNoError && RendererID != 0);
		
		// When using automatic graphics switching each context much switch to use the same GPU, so that we idle the other GPU.
		// Or when using an explicit renderer for automatic or multiple GPU setups we want to prevent the OS switching it for us.
		// We have to do this ourselves because we have offscreen contexts which OS X is quite happy to leave running on an offline GPU, but that's bad for laptops.
		int32 const RendererToMatch = GMacExplicitRendererID ? GMacExplicitRendererID : GMacCurrentRendererID;
		bool const bGPUChange = (GMacUseAutomaticGraphicsSwitching || GMacUseMultipleGPUs && GMacExplicitRendererID);
		if(bGPUChange && RendererID != RendererToMatch)
		{
			CGLPixelFormatObj CurrentPixelFormat = CGLGetPixelFormat(Current);
			GLint NumVirtualScreens = 0;
			CGLDescribePixelFormat(CurrentPixelFormat, 0, kCGLPFAVirtualScreenCount, &NumVirtualScreens);
			if(NumVirtualScreens > 1)
			{
				for(GLint i = 0; i < NumVirtualScreens; i++)
				{
					CGLDescribePixelFormat(CurrentPixelFormat, i, kCGLPFARendererID, &RendererID);
					if(RendererID == RendererToMatch)
					{
						CGLSetVirtualScreen(Current, i);
						break;
					}
				}
			}
		}
		
		// Get the current virtual screen
		GLint VirtualScreen = 0;
		CGLGetVirtualScreen(Current, &VirtualScreen);
		
		// Verify that the renderer ID hasn't changed, as if it has our capabilities may also be different
		if(RendererID != PlatformContext->RendererID || VirtualScreen != PlatformContext->VirtualScreen)
		{
			PlatformContext->RendererID = RendererID;
			PlatformContext->VirtualScreen = VirtualScreen;
			
			// Get the Vendor ID by parsing the renderer string
			FString VendorName( ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VENDOR)));
			if (VendorName.Contains(TEXT("Intel")))
			{
				PlatformContext->VendorID = 0x8086;
			}
			else if (VendorName.Contains(TEXT("NVIDIA")))
			{
				PlatformContext->VendorID = 0x10DE;
			}
			else if (VendorName.Contains(TEXT("ATi")) || VendorName.Contains(TEXT("AMD")))
			{
				PlatformContext->VendorID = 0x1002;
			}
			
			if(PlatformContext->VendorID == 0)
			{
				// Get the current renderer ID
				switch(RendererID & 0x000ff000)
				{
					case 0x00021000:
					{
						PlatformContext->VendorID = 0x1002;
						break;
					}
					case 0x00022000:
					{
						PlatformContext->VendorID = 0x10DE;
						break;
					}
					case 0x00024000:
					{
						PlatformContext->VendorID = 0x8086;
						break;
					}
					default:
					{
						// Unknown GPU vendor - assuming Intel!
						PlatformContext->VendorID = 0x8086;
						break;
					}
				}
			}
			check(PlatformContext->VendorID != 0);
			
			// Renderer IDs matchup to driver kexts, so switching based on them will allow us to target workarouds to many GPUs
			// which exhibit the same unfortunate driver bugs without having to parse their individual ID strings.
			switch((PlatformContext->RendererID & kCGLRendererIDMatchingMask))
			{
				case kCGLRendererATIRadeonX2000ID:
				{
					PlatformContext->SupportsDepthFetchDuringDepthTest = false;
					break;
				}
				case kCGLRendererATIRadeonX4000ID:
				{
					// @todo: remove once AMD fix the AMDX4000 driver for GCN cards so that it is possible to sample the depth while also stencil testing to the same DEPTH_STENCIL texture - it works on all other cards we have.
					PlatformContext->SupportsDepthFetchDuringDepthTest = FPlatformMisc::IsRunningOnMavericks() ? false : true;
					break;
				}
				default:
				{
					PlatformContext->SupportsDepthFetchDuringDepthTest = true;
				}
			}
		}
		
		GRHIVendorId = PlatformContext->VendorID;
		GSupportsDepthFetchDuringDepthTest = PlatformContext->SupportsDepthFetchDuringDepthTest;
	}
}

void FPlatformOpenGLContext::RegisterGraphicsSwitchingCallback(void)
{
#if WITH_SLI
	GNumActiveGPUsForRendering = 1;
#endif
	
	GConfig->GetBool(MAC_OPENGL_SETTINGS, TEXT("bAllowAutomaticGraphicsSwitching"), GMacUseAutomaticGraphicsSwitching, MAC_OPENGL_INI);
	GConfig->GetBool(MAC_OPENGL_SETTINGS, TEXT("bUseMultipleRenderers"), GMacUseMultipleGPUs, MAC_OPENGL_INI);
	
	// Graphics switching requires a laptop, while multi-GPU requires a desktop.
	// Check the power sources for info about this Mac to determine which it is.
	CFTypeRef PowerSourcesInfo = IOPSCopyPowerSourcesInfo();
	if (PowerSourcesInfo)
	{
		CFArrayRef PowerSourcesArray = IOPSCopyPowerSourcesList(PowerSourcesInfo);
		for (CFIndex Index = 0; Index < CFArrayGetCount(PowerSourcesArray); Index++)
		{
			CFTypeRef PowerSource = CFArrayGetValueAtIndex(PowerSourcesArray, Index);
			NSDictionary* Description = (NSDictionary*)IOPSGetPowerSourceDescription(PowerSourcesInfo, PowerSource);
			if ([(NSString*)[Description objectForKey: @kIOPSTypeKey] isEqualToString: @kIOPSInternalBatteryType])
			{
				// This is a laptop
				GMacUseAutomaticGraphicsSwitching &= true;
				GMacUseMultipleGPUs = false;
				break;
			}
		}
		CFRelease(PowerSourcesArray);
		CFRelease(PowerSourcesInfo);
	}
	
	if(GMacUseMultipleGPUs || GMacUseAutomaticGraphicsSwitching)
	{
		GLint NumberOfRenderers = 0;
		CGLRendererInfoObj RendererInfo;
		CGLQueryRendererInfo(0xffffffff, &RendererInfo, &NumberOfRenderers);
		if(RendererInfo)
		{
			TArray<uint32> HardwareRenderers;
			for(uint32 i = 0; i < NumberOfRenderers; i++)
			{
				GLint bAccelerated = 0;
				CGLDescribeRenderer(RendererInfo, i, kCGLRPAccelerated, &bAccelerated);
				if(bAccelerated)
				{
					HardwareRenderers.Push(i);
				}
			}
			
			bool bRenderersMatch = true;
			GLint MajorGLVersion = 0;
			CGLDescribeRenderer(RendererInfo, 0, kCGLRPMajorGLVersion, &MajorGLVersion);
			for(uint32 i = 1; i < HardwareRenderers.Num(); i++)
			{
				GLint OtherMajorGLVersion = 0;
				CGLDescribeRenderer(RendererInfo, i, kCGLRPMajorGLVersion, &OtherMajorGLVersion);
				bRenderersMatch &= MajorGLVersion == OtherMajorGLVersion;
			}
			
			CGLDestroyRendererInfo(RendererInfo);
			
			// Can only automatically switch when the renderers use the same major GL version, or errors will occur.
			GMacUseAutomaticGraphicsSwitching = bRenderersMatch;
			
#if WITH_SLI
			if(GMacUseMultipleGPUs)
			{
				GNumActiveGPUsForRendering = !bRenderersMatch ? 1 : HardwareRenderers.Num();
			}
#endif
		}
	}
	
	CGDisplayRegisterReconfigurationCallback(&MacOpenGLContextReconfigurationCallBack, nullptr);
}

void FPlatformOpenGLContext::UnregisterGraphicsSwitchingCallback(void)
{
	CGDisplayRemoveReconfigurationCallback(&MacOpenGLContextReconfigurationCallBack, nullptr);
}

