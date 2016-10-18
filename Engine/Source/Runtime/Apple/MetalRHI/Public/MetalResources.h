// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalResources.h: Metal resource RHI definitions.
=============================================================================*/

#pragma once

#include "BoundShaderStateCache.h"
#include "MetalShaderResources.h"
#include "ShaderCache.h"

/** Parallel execution is available on Mac but not iOS for the moment - it needs to be tested because it isn't cost-free */
#define METAL_SUPPORTS_PARALLEL_RHI_EXECUTE PLATFORM_MAC

class FMetalContext;

/** The MTLVertexDescriptor and a pre-calculated hash value used to simplify comparisons (as vendor MTLVertexDescriptor implementations aren't all comparable) */
struct FMetalHashedVertexDescriptor
{
	NSUInteger VertexDescHash;
	MTLVertexDescriptor* VertexDesc;
	
	FMetalHashedVertexDescriptor();
	FMetalHashedVertexDescriptor(MTLVertexDescriptor* Desc);
	FMetalHashedVertexDescriptor(FMetalHashedVertexDescriptor const& Other);
	~FMetalHashedVertexDescriptor();
	
	FMetalHashedVertexDescriptor& operator=(FMetalHashedVertexDescriptor const& Other);
	bool operator==(FMetalHashedVertexDescriptor const& Other) const;
	
	friend uint32 GetTypeHash(FMetalHashedVertexDescriptor const& Hash)
	{
		return Hash.VertexDescHash;
	}
};

/** This represents a vertex declaration that hasn't been combined with a specific shader to create a bound shader. */
class FMetalVertexDeclaration : public FRHIVertexDeclaration
{
public:

	/** Initialization constructor. */
	FMetalVertexDeclaration(const FVertexDeclarationElementList& InElements);
	~FMetalVertexDeclaration();
	
	/** Cached element info array (offset, stream index, etc) */
	FVertexDeclarationElementList Elements;

	/** This is the layout for the vertex elements */
	FMetalHashedVertexDescriptor Layout;

protected:
	void GenerateLayout(const FVertexDeclarationElementList& Elements);

};



/** This represents a vertex shader that hasn't been combined with a specific declaration to create a bound shader. */
template<typename BaseResourceType, int32 ShaderType>
class TMetalBaseShader : public BaseResourceType, public IRefCountedObject
{
public:
	enum { StaticFrequency = ShaderType };

	/** Initialization constructor. */
	TMetalBaseShader()
	{
	}
	TMetalBaseShader(const TArray<uint8>& InCode);

	/** Destructor */
	virtual ~TMetalBaseShader();


	// IRefCountedObject interface.
	virtual uint32 AddRef() const
	{
		return FRHIResource::AddRef();
	}
	virtual uint32 Release() const
	{
		return FRHIResource::Release();
	}
	virtual uint32 GetRefCount() const
	{
		return FRHIResource::GetRefCount();
	}
	
	// this is the compiler shader
	id<MTLFunction> Function;

	/** External bindings for this shader. */
	FMetalShaderBindings Bindings;

	// List of memory copies from RHIUniformBuffer to packed uniforms
	TArray<CrossCompiler::FUniformBufferCopyInfo> UniformBuffersCopyInfo;
	
	/** The binding for the buffer side-table if present */
	int32 SideTableBinding;
	
	/** The debuggable text source */
	NSString* GlslCodeNSString;
};

typedef TMetalBaseShader<FRHIVertexShader, SF_Vertex> FMetalVertexShader;
typedef TMetalBaseShader<FRHIPixelShader, SF_Pixel> FMetalPixelShader;
typedef TMetalBaseShader<FRHIHullShader, SF_Hull> FMetalHullShader;
typedef TMetalBaseShader<FRHIDomainShader, SF_Domain> FMetalDomainShader;
typedef TMetalBaseShader<FRHIGeometryShader, SF_Geometry> FMetalGeometryShader;

class FMetalComputeShader : public TMetalBaseShader<FRHIComputeShader, SF_Compute>
{
public:
	FMetalComputeShader(const TArray<uint8>& InCode);
	virtual ~FMetalComputeShader();
	
	// the state object for a compute shader
	id <MTLComputePipelineState> Kernel;
	
	// thread group counts
	int32 NumThreadsX;
	int32 NumThreadsY;
	int32 NumThreadsZ;
};

#if PLATFORM_MAC
typedef __uint128_t FMetalRenderPipelineHash;
#else
typedef uint64 FMetalRenderPipelineHash;
#endif

/**
 * Combined shader state and vertex definition for rendering geometry.
 * Each unique instance consists of a vertex decl, vertex shader, and pixel shader.
 */
class FMetalBoundShaderState : public FRHIBoundShaderState
{
public:
	
#if METAL_SUPPORTS_PARALLEL_RHI_EXECUTE
	FCachedBoundShaderStateLink_Threadsafe CacheLink;
#else
	FCachedBoundShaderStateLink CacheLink;
#endif

	/** Cached vertex structure */
	TRefCountPtr<FMetalVertexDeclaration> VertexDeclaration;

	/** Cached shaders */
	TRefCountPtr<FMetalVertexShader> VertexShader;
	TRefCountPtr<FMetalPixelShader> PixelShader;
	TRefCountPtr<FMetalHullShader> HullShader;
	TRefCountPtr<FMetalDomainShader> DomainShader;
	TRefCountPtr<FMetalGeometryShader> GeometryShader;

	/** Initialization constructor. */
	FMetalBoundShaderState(
		FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI);

	/**
	 *Destructor
	 */
	~FMetalBoundShaderState();
	
	/**
	 * Prepare a pipeline state object for the current state right before drawing
	 */
	void PrepareToDraw(FMetalContext* Context, FMetalHashedVertexDescriptor const& VertexDesc, const struct FMetalRenderPipelineDesc& RenderPipelineDesc, MTLRenderPipelineReflection** Reflection = nil);

protected:
	FCriticalSection PipelineMutex;
	TMap<FMetalRenderPipelineHash, TMap<FMetalHashedVertexDescriptor, id<MTLRenderPipelineState>>> PipelineStates;
};


/** Texture/RT wrapper. */
class FMetalSurface
{
public:

	/** 
	 * Constructor that will create Texture and Color/DepthBuffers as needed
	 */
	FMetalSurface(ERHIResourceType ResourceType, EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumSamples, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData);

	FMetalSurface(FMetalSurface& Source, NSRange MipRange);
	
	FMetalSurface(FMetalSurface& Source, NSRange MipRange, EPixelFormat Format);
	
	/**
	 * Destructor
	 */
	~FMetalSurface();

	/** Prepare for texture-view support - need only call this once on the source texture which is to be viewed. */
	void PrepareTextureView();
	
	/**
	 * Locks one of the texture's mip-maps.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 * @return A pointer to the specified texture data.
	 */
	void* Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride);

	/** Unlocks a previously locked mip-map.
	 * @param ArrayIndex Index of the texture array/face in the form Index*6+Face
	 */
	void Unlock(uint32 MipIndex, uint32 ArrayIndex);

	/**
	 * Returns how much memory a single mip uses, and optionally returns the stride
	 */
	uint32 GetMipSize(uint32 MipIndex, uint32* Stride, bool bSingleLayer, bool bBlitAligned);

	/**
	 * Returns how much memory is used by the surface
	 */
	uint32 GetMemorySize();

	/** Returns the number of faces for the texture */
	uint32 GetNumFaces();
	
	/** Gets the drawable texture if this is a back-buffer surface. */
	id<MTLTexture> GetDrawableTexture();
	
	/** Updates an SRV surface's internal data if required.
	 *  @param SourceTex Source textures that the UAV/SRV was created from.
	 */
	void UpdateSRV(FTextureRHIRef SourceTex);

	ERHIResourceType Type;
	EPixelFormat PixelFormat;
	uint8 FormatKey;
	id<MTLTexture> Texture;
	id<MTLTexture> MSAATexture;
	id<MTLTexture> StencilTexture;
	uint32 SizeX, SizeY, SizeZ;
	bool bIsCubemap;
	bool bWritten;
	
	uint32 Flags;
	// one per mip
	id<MTLBuffer> LockedMemory[16];
    uint32 WriteLock;

	// how much memory is allocated for this texture
	uint64 TotalTextureSize;
	
	// For back-buffers, the owning viewport.
	class FMetalViewport* Viewport;

private:
	// The movie playback IOSurface/CVTexture wrapper to avoid page-off
	CFTypeRef CoreVideoImageRef;
	
	// next format for the pixel format mapping
	static uint8 NextKey;
	
	// Count of outstanding async. texture uploads
	static int32 ActiveUploads;
};

class FMetalTexture2D : public FRHITexture2D
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTexture2D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture2D(SizeX, SizeY, NumMips, NumSamples, Format, Flags, InClearValue)
		, Surface(RRT_Texture2D, Format, SizeX, SizeY, 1, NumSamples, /*bArray=*/ false, 1, NumMips, Flags, BulkData)
	{
		FShaderCache* ShaderCache = FShaderCache::GetShaderCache();
		if ( ShaderCache )
		{
			FShaderTextureKey Tex;
			Tex.Format = (EPixelFormat)Format;
			Tex.Flags = Flags;
			Tex.MipLevels = NumMips;
			Tex.Samples = NumSamples;
			Tex.X = SizeX;
			Tex.Y = SizeY;
			Tex.Type = SCTT_Texture2D;
			
			FShaderCache::LogTexture(Tex, this);
		}
	}
	
	virtual ~FMetalTexture2D()
	{
		FShaderCache::RemoveTexture(this);
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
};

class FMetalTexture2DArray : public FRHITexture2DArray
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTexture2DArray(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture2DArray(SizeX, SizeY, ArraySize, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_Texture2DArray, Format, SizeX, SizeY, 1, /*NumSamples=*/1, /*bArray=*/ true, ArraySize, NumMips, Flags, BulkData)
	{
		FShaderCache* ShaderCache = FShaderCache::GetShaderCache();
		if ( ShaderCache )
		{
			FShaderTextureKey Tex;
			Tex.Format = (EPixelFormat)Format;
			Tex.Flags = Flags;
			Tex.MipLevels = NumMips;
			Tex.Samples = 1;
			Tex.X = SizeX;
			Tex.Y = SizeY;
			Tex.Z = ArraySize;
			Tex.Type = SCTT_Texture2DArray;
			
			FShaderCache::LogTexture(Tex, this);
		}
	}
	
	virtual ~FMetalTexture2DArray()
	{
		FShaderCache::RemoveTexture(this);
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
};

class FMetalTexture3D : public FRHITexture3D
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTexture3D(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITexture3D(SizeX, SizeY, SizeZ, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_Texture3D, Format, SizeX, SizeY, SizeZ, /*NumSamples=*/1, /*bArray=*/ false, 1, NumMips, Flags, BulkData)
	{
		FShaderCache* ShaderCache = FShaderCache::GetShaderCache();
		if ( ShaderCache )
		{
			FShaderTextureKey Tex;
			Tex.Format = (EPixelFormat)Format;
			Tex.Flags = Flags;
			Tex.MipLevels = NumMips;
			Tex.Samples = 1;
			Tex.X = SizeX;
			Tex.Y = SizeY;
			Tex.Z = SizeZ;
			Tex.Type = SCTT_Texture3D;
			
			FShaderCache::LogTexture(Tex, this);
		}
	}
	
	virtual ~FMetalTexture3D()
	{
		FShaderCache::RemoveTexture(this);
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
};

class FMetalTextureCube : public FRHITextureCube
{
public:
	/** The surface info */
	FMetalSurface Surface;

	// Constructor, just calls base and Surface constructor
	FMetalTextureCube(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 Flags, FResourceBulkDataInterface* BulkData, const FClearValueBinding& InClearValue)
		: FRHITextureCube(Size, NumMips, Format, Flags, InClearValue)
		, Surface(RRT_TextureCube, Format, Size, Size, 6, /*NumSamples=*/1, bArray, ArraySize, NumMips, Flags, BulkData)
	{
		FShaderCache* ShaderCache = FShaderCache::GetShaderCache();
		if ( ShaderCache )
		{
			FShaderTextureKey Tex;
			Tex.Format = (EPixelFormat)Format;
			Tex.Flags = Flags;
			Tex.MipLevels = NumMips;
			Tex.Samples = 1;
			Tex.X = Size;
			Tex.Y = Size;
			Tex.Z = ArraySize;
			Tex.Type = bArray ? SCTT_TextureCube : SCTT_TextureCubeArray;
			
			FShaderCache::LogTexture(Tex, this);
		}
	}
	
	virtual ~FMetalTextureCube()
	{
		FShaderCache::RemoveTexture(this);
	}
	
	virtual void* GetTextureBaseRHI() override final
	{
		return &Surface;
	}
};

struct FMetalQueryBuffer : public FRHIResource
{
    FMetalQueryBuffer(FMetalContext* InContext, id<MTLBuffer> InBuffer);
    
    virtual ~FMetalQueryBuffer();
    
    bool Wait(uint64 Millis);
    void const* GetResult(uint32 Offset);
	
	TWeakPtr<struct FMetalQueryBufferPool, ESPMode::ThreadSafe> Pool;
    id<MTLBuffer> Buffer;
	uint32 WriteOffset;
	bool bCompleted;
	id<MTLCommandBuffer> CommandBuffer;
};
typedef TRefCountPtr<FMetalQueryBuffer> FMetalQueryBufferRef;

struct FMetalQueryResult
{
    bool Wait(uint64 Millis);
    void const* GetResult();
    
    FMetalQueryBufferRef SourceBuffer;
    uint32 Offset;
};

/** Metal occlusion query */
class FMetalRenderQuery : public FRHIRenderQuery
{
public:

	/** Initialization constructor. */
	FMetalRenderQuery(ERenderQueryType InQueryType);

	~FMetalRenderQuery();

	/**
	 * Kick off an occlusion test 
	 */
	void Begin(FMetalContext* Context);

	/**
	 * Finish up an occlusion test 
	 */
	void End(FMetalContext* Context);
	
	// The type of query
	ERenderQueryType Type;

	// Query buffer allocation details as the buffer is already set on the command-encoder
    FMetalQueryResult Buffer;

    // Query result.
    uint64 Result;
    
    // Result availability - if not set the first call to acquite it will read the buffer & cache
    bool bAvailable;
};

/** Index buffer resource class that stores stride information. */
class FMetalIndexBuffer : public FRHIIndexBuffer
{
public:

	/** Constructor */
	FMetalIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage);
	~FMetalIndexBuffer();

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size=0);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
	
	// balsa buffer memory
	id<MTLBuffer> Buffer;

	// offet into the buffer (for lock usage)
	uint32 LockOffset;
	
	// Lock size
	uint32 LockSize;
	
	// 16- or 32-bit
	MTLIndexType IndexType;
};


/** Vertex buffer resource class that stores usage type. */
class FMetalVertexBuffer : public FRHIVertexBuffer
{
public:

	/** Constructor */
	FMetalVertexBuffer(uint32 InSize, uint32 InUsage);
	~FMetalVertexBuffer();

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size=0);

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
	
	// balsa buffer memory
	id<MTLBuffer> Buffer;

	// offset into the buffer (for lock usage)
	uint32 LockOffset;
	
	// Sizeof outstanding lock.
	uint32 LockSize;

	// if the buffer is a zero stride buffer, we need to duplicate and grow on demand, this is the size of one element to copy
	uint32 ZeroStrideElementSize;
};

class FMetalUniformBuffer : public FRHIUniformBuffer
{
public:

	// Constructor
	FMetalUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage);

	// Destructor 
	~FMetalUniformBuffer();
	

	bool IsConstantBuffer() const
	{
		return Buffer.length > 0;
	}


	/** The buffer containing the contents - either a fresh buffer or the ring buffer */
	id<MTLBuffer> Buffer;
	
	/** This offset is used when passing to setVertexBuffer, etc */
	uint32 Offset;

	uint32 Size; // @todo zebra: HACK! This should be removed and the code that uses it should be changed to use GetSize() instead once we fix the problem with FRHIUniformBufferLayout being released too early

	/** The intended usage of the uniform buffer. */
	EUniformBufferUsage Usage;
	
	/** Resource table containing RHI references. */
	TArray<TRefCountPtr<FRHIResource> > ResourceTable;

};


class FMetalStructuredBuffer : public FRHIStructuredBuffer
{
public:
	// Constructor
	FMetalStructuredBuffer(uint32 Stride, uint32 Size, FResourceArrayInterface* ResourceArray, uint32 InUsage);

	// Destructor
	~FMetalStructuredBuffer();

	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void* Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size=0);
	
	/**
	 * Prepare a CPU accessible buffer for uploading to GPU memory
	 */
	void Unlock();
	
	// offset into the buffer (for lock usage)
	uint32 LockOffset;
	
	// Sizeof outstanding lock.
	uint32 LockSize;
	
	// the actual buffer
	id<MTLBuffer> Buffer;
};



class FMetalUnorderedAccessView : public FRHIUnorderedAccessView
{
public:

	// the potential resources to refer to with the UAV object
	TRefCountPtr<FMetalStructuredBuffer> SourceStructuredBuffer;
	TRefCountPtr<FMetalVertexBuffer> SourceVertexBuffer;
	TRefCountPtr<FRHITexture> SourceTexture;
};


class FMetalShaderResourceView : public FRHIShaderResourceView
{
public:

	// The vertex buffer this SRV comes from (can be null)
	TRefCountPtr<FMetalVertexBuffer> SourceVertexBuffer;
	
	// The index buffer this SRV comes from (can be null)
	TRefCountPtr<FMetalIndexBuffer> SourceIndexBuffer;

	// The texture that this SRV come from
	TRefCountPtr<FRHITexture> SourceTexture;
	
	FMetalSurface* TextureView;
	uint8 MipLevel;
	uint8 NumMips;
	uint8 Format;

	~FMetalShaderResourceView();
};

class FMetalShaderParameterCache
{
public:
	/** Constructor. */
	FMetalShaderParameterCache();

	/** Destructor. */
	~FMetalShaderParameterCache();

	void InitializeResources(int32 UniformArraySize);

	/**
	 * Marks all uniform arrays as dirty.
	 */
	void MarkAllDirty();

	/**
	 * Sets values directly into the packed uniform array
	 */
	void Set(uint32 BufferIndex, uint32 ByteOffset, uint32 NumBytes, const void* NewValues);

	/**
	 * Commit shader parameters to the currently bound program.
	 */
	void CommitPackedGlobals(FMetalContext* Context, int32 Stage, const FMetalShaderBindings& Bindings);
	void CommitPackedUniformBuffers(TRefCountPtr<FMetalBoundShaderState> BoundShaderState, FMetalComputeShader* ComputeShader, int32 Stage, const TArray< TRefCountPtr<FRHIUniformBuffer> >& UniformBuffers, const TArray<CrossCompiler::FUniformBufferCopyInfo>& UniformBuffersCopyInfo);

private:
	/** CPU memory block for storing uniform values. */
	uint8* PackedGlobalUniforms[CrossCompiler::PACKED_TYPEINDEX_MAX];

	struct FRange
	{
		uint32	LowVector;
		uint32	HighVector;
	};
	/** Dirty ranges for each uniform array. */
	FRange	PackedGlobalUniformDirty[CrossCompiler::PACKED_TYPEINDEX_MAX];

	/** Scratch CPU memory block for uploading packed uniforms. */
	uint8* PackedUniformsScratch[CrossCompiler::PACKED_TYPEINDEX_MAX];

	int32 GlobalUniformArraySize;
};

class FMetalComputeFence : public FRHIComputeFence
{
public:
	
	FMetalComputeFence(FName InName)
	: FRHIComputeFence(InName)
	, CommandBuffer(nil)
	{}
	
	virtual void Reset() final override
	{
		FRHIComputeFence::Reset();
		[CommandBuffer release];
		CommandBuffer = nil;
	}
	
	void Write(id<MTLCommandBuffer> Buffer)
	{
		check(CommandBuffer == nil);
		check(Buffer != nil);
		CommandBuffer = [Buffer retain];
		FRHIComputeFence::WriteFence();
	}
	
	void Wait();
	
private:
	id<MTLCommandBuffer> CommandBuffer;
};

template<class T>
struct TMetalResourceTraits
{
};
template<>
struct TMetalResourceTraits<FRHIVertexDeclaration>
{
	typedef FMetalVertexDeclaration TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexShader>
{
	typedef FMetalVertexShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIGeometryShader>
{
	typedef FMetalGeometryShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIHullShader>
{
	typedef FMetalHullShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIDomainShader>
{
	typedef FMetalDomainShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIPixelShader>
{
	typedef FMetalPixelShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputeShader>
{
	typedef FMetalComputeShader TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIBoundShaderState>
{
	typedef FMetalBoundShaderState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITexture3D>
{
	typedef FMetalTexture3D TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITexture2D>
{
	typedef FMetalTexture2D TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITexture2DArray>
{
	typedef FMetalTexture2DArray TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHITextureCube>
{
	typedef FMetalTextureCube TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIRenderQuery>
{
	typedef FMetalRenderQuery TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIUniformBuffer>
{
	typedef FMetalUniformBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIIndexBuffer>
{
	typedef FMetalIndexBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIStructuredBuffer>
{
	typedef FMetalStructuredBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIVertexBuffer>
{
	typedef FMetalVertexBuffer TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIShaderResourceView>
{
	typedef FMetalShaderResourceView TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIUnorderedAccessView>
{
	typedef FMetalUnorderedAccessView TConcreteType;
};

template<>
struct TMetalResourceTraits<FRHISamplerState>
{
	typedef FMetalSamplerState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIRasterizerState>
{
	typedef FMetalRasterizerState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIDepthStencilState>
{
	typedef FMetalDepthStencilState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIBlendState>
{
	typedef FMetalBlendState TConcreteType;
};
template<>
struct TMetalResourceTraits<FRHIComputeFence>
{
	typedef FMetalComputeFence TConcreteType;
};
