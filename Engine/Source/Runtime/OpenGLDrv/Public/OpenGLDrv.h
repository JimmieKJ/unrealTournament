// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLDrv.h: Public OpenGL RHI definitions.
=============================================================================*/

#pragma once

// Dependencies.
#include "Core.h"
#include "RHI.h"
#include "GPUProfiler.h"
#include "ShaderCore.h"
#include "Engine.h"

//TODO: Move these to OpenGLDrvPrivate.h
#if PLATFORM_WINDOWS
#include "Windows/OpenGLWindows.h"
#elif PLATFORM_MAC
#include "Mac/OpenGLMac.h"
#elif PLATFORM_LINUX
#include "Linux/OpenGLLinux.h"
#elif PLATFORM_IOS
#include "IOS/IOSOpenGL.h"
#elif PLATFORM_ANDROIDES31
#include "Android/AndroidES31OpenGL.h"
#elif PLATFORM_ANDROIDGL4
#include "Android/AndroidGL4OpenGL.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidOpenGL.h"
#elif PLATFORM_HTML5
#include "HTML5/HTML5OpenGL.h"
#elif PLATFORM_LINUX
#include "Linux/OpenGLLinux.h"
#endif

#define OPENGL_USE_BINDABLE_UNIFORMS 0
#define OPENGL_USE_BLIT_FOR_BACK_BUFFER 1

// OpenGL RHI public headers.
#include "OpenGLUtil.h"
#include "OpenGLState.h"
#include "OpenGLResources.h"

#define FOpenGLCachedUniformBuffer_Invalid 0xFFFFFFFF

// This class has multiple inheritance but really FGPUTiming is a static class
class FOpenGLBufferedGPUTiming : public FRenderResource, public FGPUTiming
{
public:

	/**
	 * Constructor.
	 *
	 * @param InOpenGLRHI			RHI interface
	 * @param InBufferSize		Number of buffered measurements
	 */
	FOpenGLBufferedGPUTiming(class FOpenGLDynamicRHI* InOpenGLRHI, int32 BufferSize);

	/**
	 * Start a GPU timing measurement.
	 */
	void	StartTiming();

	/**
	 * End a GPU timing measurement.
	 * The timing for this particular measurement will be resolved at a later time by the GPU.
	 */
	void	EndTiming();

	/**
	 * Retrieves the most recently resolved timing measurement.
	 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
	 *
	 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
	 */
	uint64	GetTiming(bool bGetCurrentResultsAndBlock = false);

	/**
	 * Initializes all OpenGL resources.
	 */
	virtual void InitDynamicRHI() override;

	/**
	 * Releases all OpenGL resources.
	 */
	virtual void ReleaseDynamicRHI() override;


private:

	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData);

	/** RHI interface */
	FOpenGLDynamicRHI*					OpenGLRHI;
	/** Number of timestamps created in 'StartTimestamps' and 'EndTimestamps'. */
	int32								BufferSize;
	/** Current timing being measured on the CPU. */
	int32								CurrentTimestamp;
	/** Number of measurements in the buffers (0 - BufferSize). */
	int32								NumIssuedTimestamps;
	/** Timestamps for all StartTimings. */
	TArray<FOpenGLRenderQuery *>		StartTimestamps;
	/** Timestamps for all EndTimings. */
	TArray<FOpenGLRenderQuery *>		EndTimestamps;
	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool								bIsTiming;
};

/**
  * Used to track whether a period was disjoint on the GPU, which means GPU timings are invalid.
  * OpenGL lacks this concept at present, so the class is just a placeholder
  * Timings are all assumed to be non-disjoint
  */
class FOpenGLDisjointTimeStampQuery : public FRenderResource
{
public:
	FOpenGLDisjointTimeStampQuery(class FOpenGLDynamicRHI* InOpenGLRHI=NULL);

	void Init(class FOpenGLDynamicRHI* InOpenGLRHI)
	{
		OpenGLRHI = InOpenGLRHI;
		InitResource();
	}

	void StartTracking();
	void EndTracking();
	bool IsResultValid();
	bool GetResult(uint64* OutResult=NULL);
	static uint64 GetTimingFrequency()
	{
		return 1000000000ull;
	}
	static bool IsSupported()
	{
		return FOpenGL::SupportsDisjointTimeQueries();
	}

	/**
	 * Initializes all resources.
	 */
	virtual void InitDynamicRHI();

	/**
	 * Releases all resources.
	 */
	virtual void ReleaseDynamicRHI();


private:
	bool	bIsResultValid;
	GLuint	DisjointQuery;
	uint64	Context;

	FOpenGLDynamicRHI* OpenGLRHI;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FOpenGLEventNode : public FGPUProfilerEventNode
{
public:

	FOpenGLEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent, class FOpenGLDynamicRHI* InRHI)
	:	FGPUProfilerEventNode(InName, InParent)
	,	Timing(InRHI, 1)
	{
		// Initialize Buffered timestamp queries 
		Timing.InitResource();
	}

	virtual ~FOpenGLEventNode()
	{
		Timing.ReleaseResource();
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	float GetTiming() override;

	virtual void StartTiming() override
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override
	{
		Timing.EndTiming();
	}

	FOpenGLBufferedGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FOpenGLEventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:
	FOpenGLEventNodeFrame(class FOpenGLDynamicRHI* InRHI) :
		FGPUProfilerEventNodeFrame(),
		RootEventTiming(InRHI, 1),
		DisjointQuery(InRHI)
	{
	  RootEventTiming.InitResource();
	  DisjointQuery.InitResource();
	}

	~FOpenGLEventNodeFrame()
	{

		RootEventTiming.ReleaseResource();
		DisjointQuery.ReleaseResource();
	}

	/** Start this frame of per tracking */
	void StartFrame() override;

	/** End this frame of per tracking, but do not block yet */
	void EndFrame() override;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override;

	virtual void LogDisjointQuery() override;

	/** Timer tracking inclusive time spent in the root nodes. */
	FOpenGLBufferedGPUTiming RootEventTiming;

	/** Disjoint query tracking whether the times reported by DumpEventTree are reliable. */
	FOpenGLDisjointTimeStampQuery DisjointQuery;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FOpenGLGPUProfiler : public FGPUProfiler
{
	/** Used to measure GPU time per frame. */
	FOpenGLBufferedGPUTiming FrameTiming;

	/** Measuring GPU frame time with a disjoint query. */
	static const int MAX_GPUFRAMEQUERIES = 4;
	FOpenGLDisjointTimeStampQuery DisjointGPUFrameTimeQuery[MAX_GPUFRAMEQUERIES];
	int CurrentGPUFrameQueryIndex;

	class FOpenGLDynamicRHI* OpenGLRHI;

	/** GPU hitch profile histories */
	TIndirectArray<FOpenGLEventNodeFrame> GPUHitchEventNodeFrames;

	FOpenGLGPUProfiler(class FOpenGLDynamicRHI* InOpenGLRHI)
	:	FGPUProfiler()
	,	FrameTiming(InOpenGLRHI, 4)
	,	CurrentGPUFrameQueryIndex(0)
	,	OpenGLRHI(InOpenGLRHI)
	{
		FrameTiming.InitResource();
		for ( int32 Index=0; Index < MAX_GPUFRAMEQUERIES; ++Index )
		{
			DisjointGPUFrameTimeQuery[Index].Init(InOpenGLRHI);
		}
	}

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override
	{
		FOpenGLEventNode* EventNode = new FOpenGLEventNode(InName, InParent, OpenGLRHI);
		return EventNode;
	}

	void Cleanup();

	virtual void PushEvent(const TCHAR* Name) override;
	virtual void PopEvent() override;

	void BeginFrame(class FOpenGLDynamicRHI* InRHI);
	void EndFrame();
};


/** The interface which is implemented by the dynamically bound RHI. */
class FOpenGLDynamicRHI : public FDynamicRHI, public IRHICommandContext
{
public:

	friend class FOpenGLViewport;
#if PLATFORM_MAC || PLATFORM_ANDROIDES31 // Flithy hack to workaround radr://16011763
	friend class FOpenGLTextureBase;
#endif

	/** Initialization constructor. */
	FOpenGLDynamicRHI();

	/** Destructor */
	~FOpenGLDynamicRHI() {}

	// FDynamicRHI interface.
	virtual void Init();
	virtual void Shutdown();

	template<typename TRHIType>
	static FORCEINLINE typename TOpenGLResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
	{
		return static_cast<typename TOpenGLResourceTraits<TRHIType>::TConcreteType*>(Resource);
	}

	#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type RHI##Name ParameterTypesAndNames
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD

	void Cleanup();

	void PurgeFramebufferFromCaches(GLuint Framebuffer);
	void OnVertexBufferDeletion(GLuint VertexBufferResource);
	void OnIndexBufferDeletion(GLuint IndexBufferResource);
	void OnPixelBufferDeletion(GLuint PixelBufferResource);
	void OnUniformBufferDeletion(GLuint UniformBufferResource,uint32 AllocatedSize,bool bStreamDraw);
	void OnProgramDeletion(GLint ProgramResource);
	void InvalidateTextureResourceInCache(GLuint Resource);
	void InvalidateUAVResourceInCache(GLuint Resource);
	/** Set a resource on texture target of a specific real OpenGL stage. Goes through cache to eliminate redundant calls. */
	void CachedSetupTextureStage(FOpenGLContextState& ContextState, GLint TextureIndex, GLenum Target, GLuint Resource, GLint BaseMip, GLint NumMips);
	void CachedSetupUAVStage(FOpenGLContextState& ContextState, GLint UAVIndex, GLenum Format, GLuint Resource);
	FOpenGLContextState& GetContextStateForCurrentContext();

	void CachedBindArrayBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		if( ContextState.ArrayBufferBound != Buffer )
		{
			glBindBuffer( GL_ARRAY_BUFFER, Buffer );
			ContextState.ArrayBufferBound = Buffer;
		}
	}

	void CachedBindElementArrayBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		if( ContextState.ElementArrayBufferBound != Buffer )
		{
			glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, Buffer );
			ContextState.ElementArrayBufferBound = Buffer;
		}
	}

	void CachedBindPixelUnpackBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();

		if( ContextState.PixelUnpackBufferBound != Buffer )
		{
			glBindBuffer( GL_PIXEL_UNPACK_BUFFER, Buffer );
			ContextState.PixelUnpackBufferBound = Buffer;
		}
	}

	void CachedBindUniformBuffer( FOpenGLContextState& ContextState, GLuint Buffer )
	{
		VERIFY_GL_SCOPE();
		check(IsInRenderingThread());
		if( ContextState.UniformBufferBound != Buffer )
		{
			glBindBuffer( GL_UNIFORM_BUFFER, Buffer );
			ContextState.UniformBufferBound = Buffer;
		}
	}

	bool IsUniformBufferBound( FOpenGLContextState& ContextState, GLuint Buffer ) const
	{
		return ( ContextState.UniformBufferBound == Buffer );
	}

	/** Add query to Queries list upon its creation. */
	void RegisterQuery( FOpenGLRenderQuery* Query );

	/** Remove query from Queries list upon its deletion. */
	void UnregisterQuery( FOpenGLRenderQuery* Query );

	/** Inform all queries about the need to recreate themselves after OpenGL context they're in gets deleted. */
	void InvalidateQueries();

	FOpenGLSamplerState* GetPointSamplerState() const { return (FOpenGLSamplerState*)PointSamplerState.GetReference(); }

private:

	/** Counter incremented each time RHIBeginScene is called. */
	uint32 SceneFrameCounter;

	/** Value used to detect when resource tables need to be recached. INDEX_NONE means always recache. */
	uint32 ResourceTableFrameCounter;

	/** RHI device state, independent of underlying OpenGL context used */
	FOpenGLRHIState						PendingState;
	FOpenGLStreamedVertexBufferArray	DynamicVertexBuffers;
	FOpenGLStreamedIndexBufferArray		DynamicIndexBuffers;
	FSamplerStateRHIRef					PointSamplerState;

	/** A list of all viewport RHIs that have been created. */
	TArray<FOpenGLViewport*> Viewports;
	TRefCountPtr<FOpenGLViewport>		DrawingViewport;
	bool								bRevertToSharedContextAfterDrawingViewport;

	bool								bIsRenderingContextAcquired;

	/** A history of the most recently used bound shader states, used to keep transient bound shader states from being recreated for each use. */
	TGlobalResource< TBoundShaderStateHistory<10000> > BoundShaderStateHistory;

	/** Per-context state caching */
	FOpenGLContextState	SharedContextState;
	FOpenGLContextState	RenderingContextState;

	/** Underlying platform-specific data */
	struct FPlatformOpenGLDevice* PlatformDevice;

	/** Query list. This is used to inform queries they're no longer valid when OpenGL context they're in gets released from another thread. */
	TArray<FOpenGLRenderQuery*> Queries;

	/** A critical section to protect modifications and iteration over Queries list */
	FCriticalSection QueriesListCriticalSection;

	/** Query list. This is used to inform queries they're no longer valid when OpenGL context they're in gets released from another thread. */
	TArray<FOpenGLRenderQuery*> TimerQueries;

	/** A critical section to protect modifications and iteration over Queries list */
	FCriticalSection TimerQueriesListCriticalSection;

	FOpenGLGPUProfiler GPUProfilingData;
	friend FOpenGLGPUProfiler;
//	FOpenGLEventQuery FrameSyncEvent;

	FRHITexture* CreateOpenGLTexture(uint32 SizeX,uint32 SizeY,bool CubeTexture, bool ArrayTexture, uint8 Format,uint32 NumMips,uint32 NumSamples, uint32 ArraySize, uint32 Flags, FResourceBulkDataInterface* BulkData = NULL);
	GLuint GetOpenGLFramebuffer(uint32 NumSimultaneousRenderTargets, FOpenGLTextureBase** RenderTargets, uint32* ArrayIndices, uint32* MipmapLevels, FOpenGLTextureBase* DepthStencilTarget);

	void InitializeStateResources();

	/** needs to be called before each dispatch call */
	

	void EnableVertexElementCached(FOpenGLContextState& ContextCache, const FOpenGLVertexElement &VertexElement, GLsizei Stride, void *Pointer, GLuint Buffer);
	void EnableVertexElementCachedZeroStride(FOpenGLContextState& ContextCache, const FOpenGLVertexElement &VertexElement, uint32 NumVertices, FOpenGLVertexBuffer* VertexBuffer);
	void SetupVertexArrays(FOpenGLContextState& ContextCache, uint32 BaseVertexIndex, FOpenGLStream* Streams, uint32 NumStreams, uint32 MaxVertices);
	void SetupVertexArraysVAB(FOpenGLContextState& ContextCache, uint32 BaseVertexIndex, FOpenGLStream* Streams, uint32 NumStreams, uint32 MaxVertices);
	void SetupVertexArraysUP(FOpenGLContextState& ContextState, void* Buffer, uint32 Stride);

	void SetupBindlessTextures( FOpenGLContextState& ContextState, const TArray<FOpenGLBindlessSamplerInfo> &Samplers );

	/** needs to be called before each draw call */
	void BindPendingFramebuffer( FOpenGLContextState& ContextState );
	void BindPendingShaderState( FOpenGLContextState& ContextState );
	void BindPendingComputeShaderState( FOpenGLContextState& ContextState, FComputeShaderRHIParamRef ComputeShaderRHI);
	void UpdateRasterizerStateInOpenGLContext( FOpenGLContextState& ContextState );
	void UpdateDepthStencilStateInOpenGLContext( FOpenGLContextState& ContextState );
	void UpdateScissorRectInOpenGLContext( FOpenGLContextState& ContextState );
	void UpdateViewportInOpenGLContext( FOpenGLContextState& ContextState );
	
	template <class ShaderType> void SetResourcesFromTables(const ShaderType* RESTRICT);
	void CommitGraphicsResourceTables();
	void CommitComputeResourceTables(class FOpenGLComputeShader* ComputeShader);
	void CommitNonComputeShaderConstants();
	void CommitComputeShaderConstants(FComputeShaderRHIParamRef ComputeShaderRHI);
	void SetPendingBlendStateForActiveRenderTargets( FOpenGLContextState& ContextState );
	
	void SetupTexturesForDraw( FOpenGLContextState& ContextState);
	template <typename StateType>
	void SetupTexturesForDraw( FOpenGLContextState& ContextState, const StateType ShaderState, int32 MaxTexturesNeeded);

	void SetupUAVsForDraw(FOpenGLContextState& ContextState, const TRefCountPtr<FOpenGLComputeShader> &ComputeShader, int32 MaxUAVsNeeded);

public:
	/** Remember what RHI user wants set on a specific OpenGL texture stage, translating from Stage and TextureIndex for stage pair. */
	void InternalSetShaderTexture(FOpenGLTextureBase* Texture, FOpenGLShaderResourceView* SRV, GLint TextureIndex, GLenum Target, GLuint Resource, int NumMips, int LimitMip);
	void InternalSetShaderUAV(GLint UAVIndex, GLenum Format, GLuint Resource);
	void InternalSetSamplerStates(GLint TextureIndex, FOpenGLSamplerState* SamplerState);
#if PLATFORM_MAC
	/** On OS X force a rebind of the texture buffer to the texture name to workaround radr://18379338 */
	void InternalUpdateTextureBuffer( FOpenGLContextState& ContextState, FOpenGLShaderResourceView* SRV, GLint TextureIndex );
#endif

private:
	void ApplyTextureStage(FOpenGLContextState& ContextState, GLint TextureIndex, const FTextureStage& TextureStage, FOpenGLSamplerState* SamplerState);

	void ReadSurfaceDataRaw(FOpenGLContextState& ContextState, FTextureRHIParamRef TextureRHI,FIntRect Rect,TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags);

	void BindUniformBufferBase(FOpenGLContextState& ContextState, int32 NumUniformBuffers, FUniformBufferRHIRef* BoundUniformBuffers, uint32 FirstUniformBuffer, bool ForceUpdate);

	void ClearCurrentFramebufferWithCurrentScissor(FOpenGLContextState& ContextState, int8 ClearType, int32 NumClearColors, const FLinearColor* ClearColorArray, float Depth, uint32 Stencil);

	void FreeZeroStrideBuffers();

	/** Consumes about 100ms of GPU time (depending on resolution and GPU), useful for making sure we're not CPU bound when GPU profiling. */
	void IssueLongGPUTask();

	/** Remaps vertex attributes on devices where GL_MAX_VERTEX_ATTRIBS < 16 */
	uint32 RemapVertexAttrib(uint32 VertexAttributeIndex)
	{
		if (FOpenGL::NeedsVertexAttribRemapTable())
		{
			check(VertexAttributeIndex < ARRAY_COUNT(PendingState.BoundShaderState->VertexShader->Bindings.VertexAttributeRemap));
			VertexAttributeIndex = PendingState.BoundShaderState->VertexShader->Bindings.VertexAttributeRemap[VertexAttributeIndex];
			check(VertexAttributeIndex < NUM_OPENGL_VERTEX_STREAMS); // check that this attribute has remaped correctly.
		}
		return VertexAttributeIndex;
	}
};

/** Implements the OpenGLDrv module as a dynamic RHI providing module. */
class FOpenGLDynamicRHIModule : public IDynamicRHIModule
{
public:
	
	// IModuleInterface
	virtual bool SupportsDynamicReloading() override { return false; }

	// IDynamicRHIModule
	virtual bool IsSupported() override;

	virtual FDynamicRHI* CreateRHI() override
	{
		return new FOpenGLDynamicRHI();
	}
};
