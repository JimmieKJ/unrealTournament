// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHI.h: Render Hardware Interface definitions.
=============================================================================*/

#ifndef __RHI_h__
#define __RHI_h__

#include "Core.h"
#include "RHIDefinitions.h"
#include "StaticArray.h"

/** Uniform buffer structs must be aligned to 16-byte boundaries. */
#define UNIFORM_BUFFER_STRUCT_ALIGNMENT 16

// Forward declarations.
class FSceneView;
struct FMeshBatch;

/** RHI Logging. */
RHI_API DECLARE_LOG_CATEGORY_EXTERN(LogRHI,Log,VeryVerbose);

/**
 * RHI configuration settings.
 */

namespace RHIConfig
{
	RHI_API bool ShouldSaveScreenshotAfterProfilingGPU();
	RHI_API bool ShouldShowProfilerAfterProfilingGPU();
	RHI_API float GetGPUHitchThreshold();
}

/**
 * RHI globals.
 */

/** True if the render hardware has been initialized. */
extern RHI_API bool GIsRHIInitialized;

class RHI_API FRHICommandList;

/**
 * RHI capabilities.
 */


/** The maximum number of mip-maps that a texture can contain. 	*/
extern	RHI_API int32		GMaxTextureMipCount;

/** true if this platform has quad buffer stereo support. */
extern RHI_API bool GSupportsQuadBufferStereo;

/** true if the RHI supports textures that may be bound as both a render target and a shader resource. */
extern RHI_API bool GSupportsRenderDepthTargetableShaderResources;

/** true if the RHI supports binding depth as a texture when testing against depth */
extern RHI_API bool GSupportsDepthFetchDuringDepthTest;

/** 
 * only set if RHI has the information (after init of the RHI and only if RHI has that information, never changes after that)
 * e.g. "NVIDIA GeForce GTX 670"
 */
extern RHI_API FString GRHIAdapterName;

// 0 means not defined yet, use functions like IsRHIDeviceAMD() to access
extern RHI_API uint32 GRHIVendorId;

// to trigger GPU specific optimizations and fallbacks
RHI_API bool IsRHIDeviceAMD();

// to trigger GPU specific optimizations and fallbacks
RHI_API bool IsRHIDeviceIntel();

// to trigger GPU specific optimizations and fallbacks
RHI_API bool IsRHIDeviceNVIDIA();

/** true if PF_G8 render targets are supported */
extern RHI_API bool GSupportsRenderTargetFormat_PF_G8;

/** true if PF_FloatRGBA render targets are supported */
extern RHI_API bool GSupportsRenderTargetFormat_PF_FloatRGBA;

/** true if mobile framebuffer fetch is supported */
extern RHI_API bool GSupportsShaderFramebufferFetch;

/** true if the GPU supports hidden surface removal in hardware. */
extern RHI_API bool GHardwareHiddenSurfaceRemoval;

/** true if the RHI supports asynchronous creation of texture resources */
extern RHI_API bool GRHISupportsAsyncTextureCreation;

/** Can we handle quad primitives? */
extern RHI_API bool GSupportsQuads;

/** True if and only if the GPU support rendering to volume textures (2D Array, 3D). Some OpenGL 3.3 cards support SM4, but can't render to volume textures. */
extern RHI_API bool GSupportsVolumeTextureRendering;

/** True if the RHI supports separate blend states per render target. */
extern RHI_API bool GSupportsSeparateRenderTargetBlendState;

/** True if the RHI can render to a depth-only render target with no additional color render target. */
extern RHI_API bool GSupportsDepthRenderTargetWithoutColorRenderTarget;

/** True if the RHI supports depth bounds testing */
extern RHI_API bool GSupportsDepthBoundsTest;

/** The minimum Z value in clip space for the RHI. */
extern RHI_API float GMinClipZ;

/** The sign to apply to the Y axis of projection matrices. */
extern RHI_API float GProjectionSignY;

/** The maximum size to allow for the shadow depth buffer in the X dimension.  This must be larger or equal to GMaxShadowDepthBufferSizeY. */
extern RHI_API int32 GMaxShadowDepthBufferSizeX;
/** The maximum size to allow for the shadow depth buffer in the Y dimension. */
extern RHI_API int32 GMaxShadowDepthBufferSizeY;

/** The maximum size allowed for 2D textures in both dimensions. */
extern RHI_API int32 GMaxTextureDimensions;
FORCEINLINE uint32 GetMax2DTextureDimension()
{
	return GMaxTextureDimensions;
}

/** The maximum size allowed for cube textures. */
extern RHI_API int32 GMaxCubeTextureDimensions;
FORCEINLINE uint32 GetMaxCubeTextureDimension()
{
	return GMaxCubeTextureDimensions;
}

/** The Maximum number of layers in a 1D or 2D texture array. */
extern RHI_API int32 GMaxTextureArrayLayers;
FORCEINLINE uint32 GetMaxTextureArrayLayers()
{
	return GMaxTextureArrayLayers;
}

/** true if we are running with the NULL RHI */
extern RHI_API bool GUsingNullRHI;

/**
 *	The size to check against for Draw*UP call vertex counts.
 *	If greater than this value, the draw call will not occur.
 */
extern RHI_API int32 GDrawUPVertexCheckCount;
/**
 *	The size to check against for Draw*UP call index counts.
 *	If greater than this value, the draw call will not occur.
 */
extern RHI_API int32 GDrawUPIndexCheckCount;

/** true for each VET that is supported. One-to-one mapping with EVertexElementType */
extern RHI_API class FVertexElementTypeSupportInfo GVertexElementTypeSupport;

/** When greater than one, indicates that SLI rendering is enabled */
#if PLATFORM_DESKTOP
#define WITH_SLI (1)
extern RHI_API int32 GNumActiveGPUsForRendering;
#else
#define WITH_SLI (0)
#define GNumActiveGPUsForRendering (1)
#endif

/** Whether the next frame should profile the GPU. */
extern RHI_API bool GTriggerGPUProfile;

/** Whether we are profiling GPU hitches. */
extern RHI_API bool GTriggerGPUHitchProfile;

/** True if the RHI supports texture streaming */
extern RHI_API bool GRHISupportsTextureStreaming;
/** Amount of memory allocated by textures. In kilobytes. */
extern RHI_API volatile int32 GCurrentTextureMemorySize;
/** Amount of memory allocated by rendertargets. In kilobytes. */
extern RHI_API volatile int32 GCurrentRendertargetMemorySize;
/** Current texture streaming pool size, in bytes. 0 means unlimited. */
extern RHI_API int64 GTexturePoolSize;
/** In percent. If non-zero, the texture pool size is a percentage of GTotalGraphicsMemory. */
extern RHI_API int32 GPoolSizeVRAMPercentage;

/** Some simple runtime stats, reset on every call to RHIBeginFrame. */
extern RHI_API int32 GNumDrawCallsRHI;
extern RHI_API int32 GNumPrimitivesDrawnRHI;

/** Whether or not the RHI can handle a non-zero BaseVertexIndex - extra SetStreamSource calls will be needed if this is false */
extern RHI_API bool GRHISupportsBaseVertexIndex;

/** Whether or not the RHI can handle a non-zero FirstInstance - extra SetStreamSource calls will be needed if this is false */
extern RHI_API bool GRHISupportsFirstInstance;

/** Whether or not the engine should set the BackBuffer as a render target early in the frame. */
extern RHI_API bool GRHIRequiresEarlyBackBufferRenderTarget;

/** Whether or not the RHI supports an RHI thread.
Requirements for RHI thread
* Microresources (those in RHIStaticStates.h) need to be able to be created by any thread at any time and be able to work with a radically simplified rhi resource lifecycle. CreateSamplerState, CreateRasterizerState, CreateDepthStencilState, CreateBlendState
* CreateUniformBuffer needs to be threadsafe
* GetRenderQueryResult should be threadsafe, but this isn't required. If it isn't threadsafe, then you need to flush yourself in the RHI
* GetViewportBackBuffer and AdvanceFrameForGetViewportBackBuffer need to be threadsafe and need to support the fact that the render thread has a different concept of "current backbuffer" than the RHI thread. Without an RHIThread this is moot due to the next two items.
* AdvanceFrameForGetViewportBackBuffer needs be added as an RHI method and this needs to work with GetViewportBackBuffer to give the render thread the right back buffer even though many commands relating to the beginning and end of the frame are queued.
* BeginDrawingViewport, and 5 or so other frame advance methods are queued with an RHIThread. Without an RHIThread, these just flush internally.
***/
extern RHI_API bool GRHISupportsRHIThread;

/** Whether or not the RHI supports parallel RHIThread executes / translates
Requirements:
* RHICreateBoundShaderState is threadsafe and GetCachedBoundShaderState must not be used. GetCachedBoundShaderState_Threadsafe has a slightly different protocol.
***/
extern RHI_API bool GRHISupportsParallelRHIExecute;

/** Called once per frame only from within an RHI. */
extern RHI_API void RHIPrivateBeginFrame();

RHI_API FName LegacyShaderPlatformToShaderFormat(EShaderPlatform Platform);
RHI_API EShaderPlatform ShaderFormatToLegacyShaderPlatform(FName ShaderFormat);

/**
 * Adjusts a projection matrix to output in the correct clip space for the
 * current RHI. Unreal projection matrices follow certain conventions and
 * need to be patched for some RHIs. All projection matrices should be adjusted
 * before being used for rendering!
 */
inline FMatrix AdjustProjectionMatrixForRHI(const FMatrix& InProjectionMatrix)
{
	FScaleMatrix ClipSpaceFixScale(FVector(1.0f, GProjectionSignY, 1.0f - GMinClipZ));
	FTranslationMatrix ClipSpaceFixTranslate(FVector(0.0f, 0.0f, GMinClipZ));	
	return InProjectionMatrix * ClipSpaceFixScale * ClipSpaceFixTranslate;
}

/** Current shader platform. */


/** Finds a corresponding ERHIFeatureLevel::Type given an FName, or returns false if one could not be found. */
extern RHI_API bool GetFeatureLevelFromName(FName Name, ERHIFeatureLevel::Type& OutFeatureLevel);

/** Creates a string for the given feature level. */
extern RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FString& OutName);

/** Creates an FName for the given feature level. */
extern RHI_API void GetFeatureLevelName(ERHIFeatureLevel::Type InFeatureLevel, FName& OutName);

// The maximum feature level and shader platform available on this system
// GRHIFeatureLevel and GRHIShaderPlatform have been deprecated. There is no longer a current featurelevel/shaderplatform that
// should be used for all rendering, rather a specific set for each view.
extern RHI_API ERHIFeatureLevel::Type GMaxRHIFeatureLevel;
extern RHI_API EShaderPlatform GMaxRHIShaderPlatform;

/** Table for finding out which shader platform corresponds to a given feature level for this RHI. */
extern RHI_API EShaderPlatform GShaderPlatformForFeatureLevel[ERHIFeatureLevel::Num];

/** Get the shader platform associated with the supplied feature level on this machine */
inline EShaderPlatform GetFeatureLevelShaderPlatform(ERHIFeatureLevel::Type InFeatureLevel)
{
	return GShaderPlatformForFeatureLevel[InFeatureLevel];
}

inline FArchive& operator <<(FArchive& Ar, EResourceLockMode& LockMode)
{
	uint32 Temp = LockMode;
	Ar << Temp;
	LockMode = (EResourceLockMode)Temp;
	return Ar;
}

/** to customize the RHIReadSurfaceData() output */
class FReadSurfaceDataFlags
{
public:
	// @param InCompressionMode defines the value input range that is mapped to output range
	// @param InCubeFace defined which cubemap side is used, only required for cubemap content, then it needs to be a valid side
	FReadSurfaceDataFlags(ERangeCompressionMode InCompressionMode = RCM_UNorm, ECubeFace InCubeFace = CubeFace_MAX) 
		:CubeFace(InCubeFace), CompressionMode(InCompressionMode), bLinearToGamma(true), MaxDepthRange(16000.0f), bOutputStencil(false), MipLevel(0)
	{
	}

	ECubeFace GetCubeFace() const
	{
		checkSlow(CubeFace <= CubeFace_NegZ);
		return CubeFace;
	}

	ERangeCompressionMode GetCompressionMode() const
	{
		return CompressionMode;
	}

	void SetLinearToGamma(bool Value)
	{
		bLinearToGamma = Value;
	}

	bool GetLinearToGamma() const
	{
		return bLinearToGamma;
	}

	void SetOutputStencil(bool Value)
	{
		bOutputStencil = Value;
	}

	bool GetOutputStencil() const
	{
		return bOutputStencil;
	}

	void SetMip(uint8 InMipLevel)
	{
		MipLevel = InMipLevel;
	}

	uint8 GetMip() const
	{
		return MipLevel;
	}	

	void SetMaxDepthRange(float Value)
	{
		MaxDepthRange = Value;
	}

	float ComputeNormalizedDepth(float DeviceZ) const
	{
		return FMath::Abs(ConvertFromDeviceZ(DeviceZ) / MaxDepthRange);
	}

private:

	// @return SceneDepth
	float ConvertFromDeviceZ(float DeviceZ) const
	{
		DeviceZ = FMath::Min(DeviceZ, 1 - Z_PRECISION);

		// for depth to linear conversion
		const FVector2D InvDeviceZToWorldZ(0.1f, 0.1f);

		return 1.0f / (DeviceZ * InvDeviceZToWorldZ.X - InvDeviceZToWorldZ.Y);
	}

	ECubeFace CubeFace;
	ERangeCompressionMode CompressionMode;
	bool bLinearToGamma;	
	float MaxDepthRange;
	bool bOutputStencil;
	uint8 MipLevel;
};

/** Info for supporting the vertex element types */
class FVertexElementTypeSupportInfo
{
public:
	FVertexElementTypeSupportInfo() { for(int32 i=0; i<VET_MAX; i++) ElementCaps[i]=true; }
	FORCEINLINE bool IsSupported(EVertexElementType ElementType) { return ElementCaps[ElementType]; }
	FORCEINLINE void SetSupported(EVertexElementType ElementType,bool bIsSupported) { ElementCaps[ElementType]=bIsSupported; }
private:
	/** cap bit set for each VET. One-to-one mapping based on EVertexElementType */
	bool ElementCaps[VET_MAX];
};

struct FVertexElement
{
	uint8 StreamIndex;
	uint8 Offset;
	TEnumAsByte<EVertexElementType> Type;
	uint8 AttributeIndex;
	uint16 Stride;
	uint16 bUseInstanceIndex;

	FVertexElement() {}
	FVertexElement(uint8 InStreamIndex,uint8 InOffset,EVertexElementType InType,uint8 InAttributeIndex,uint16 InStride,bool bInUseInstanceIndex = false):
		StreamIndex(InStreamIndex),
		Offset(InOffset),
		Type(InType),
		AttributeIndex(InAttributeIndex),
		Stride(InStride),
		bUseInstanceIndex(bInUseInstanceIndex)
	{}
	/**
	* Suppress the compiler generated assignment operator so that padding won't be copied.
	* This is necessary to get expected results for code that zeros, assigns and then CRC's the whole struct.
	*/
	void operator=(const FVertexElement& Other)
	{
		StreamIndex = Other.StreamIndex;
		Offset = Other.Offset;
		Type = Other.Type;
		AttributeIndex = Other.AttributeIndex;
		bUseInstanceIndex = Other.bUseInstanceIndex;
	}

	friend FArchive& operator<<(FArchive& Ar,FVertexElement& Element)
	{
		Ar << Element.StreamIndex;
		Ar << Element.Offset;
		Ar << Element.Type;
		Ar << Element.AttributeIndex;
		Ar << Element.Stride;
		Ar << Element.bUseInstanceIndex;
		return Ar;
	}
};

typedef TArray<FVertexElement,TFixedAllocator<MaxVertexElementCount> > FVertexDeclarationElementList;

/** RHI representation of a single stream out element. */
struct FStreamOutElement
{
	/** Index of the output stream from the geometry shader. */
	uint32 Stream;

	/** Semantic name of the output element as defined in the geometry shader.  This should not contain the semantic number. */
	const ANSICHAR* SemanticName;

	/** Semantic index of the output element as defined in the geometry shader.  For example "TEXCOORD5" in the shader would give a SemanticIndex of 5. */
	uint32 SemanticIndex;

	/** Start component index of the shader output element to stream out. */
	uint8 StartComponent;

	/** Number of components of the shader output element to stream out. */
	uint8 ComponentCount;

	/** Stream output target slot, corresponding to the streams set by RHISetStreamOutTargets. */
	uint8 OutputSlot;

	FStreamOutElement() {}
	FStreamOutElement(uint32 InStream, const ANSICHAR* InSemanticName, uint32 InSemanticIndex, uint8 InComponentCount, uint8 InOutputSlot) :
		Stream(InStream),
		SemanticName(InSemanticName),
		SemanticIndex(InSemanticIndex),
		StartComponent(0),
		ComponentCount(InComponentCount),
		OutputSlot(InOutputSlot)
	{}
};

typedef TArray<FStreamOutElement,TFixedAllocator<MaxVertexElementCount> > FStreamOutElementList;

struct FSamplerStateInitializerRHI
{
	FSamplerStateInitializerRHI() {}
	FSamplerStateInitializerRHI(
		ESamplerFilter InFilter,
		ESamplerAddressMode InAddressU = AM_Wrap,
		ESamplerAddressMode InAddressV = AM_Wrap,
		ESamplerAddressMode InAddressW = AM_Wrap,
		int32 InMipBias = 0,
		int32 InMaxAnisotropy = 0,
		float InMinMipLevel = 0,
		float InMaxMipLevel = FLT_MAX,
		uint32 InBorderColor = 0,
		/** Only supported in D3D11 */
		ESamplerCompareFunction InSamplerComparisonFunction = SCF_Never
		)
	:	Filter(InFilter)
	,	AddressU(InAddressU)
	,	AddressV(InAddressV)
	,	AddressW(InAddressW)
	,	MipBias(InMipBias)
	,	MinMipLevel(InMinMipLevel)
	,	MaxMipLevel(InMaxMipLevel)
	,	MaxAnisotropy(InMaxAnisotropy)
	,	BorderColor(InBorderColor)
	,	SamplerComparisonFunction(InSamplerComparisonFunction)
	{
	}
	TEnumAsByte<ESamplerFilter> Filter;
	TEnumAsByte<ESamplerAddressMode> AddressU;
	TEnumAsByte<ESamplerAddressMode> AddressV;
	TEnumAsByte<ESamplerAddressMode> AddressW;
	int32 MipBias;
	/** Smallest mip map level that will be used, where 0 is the highest resolution mip level. */
	float MinMipLevel;
	/** Largest mip map level that will be used, where 0 is the highest resolution mip level. */
	float MaxMipLevel;
	int32 MaxAnisotropy;
	uint32 BorderColor;
	TEnumAsByte<ESamplerCompareFunction> SamplerComparisonFunction;

	friend FArchive& operator<<(FArchive& Ar,FSamplerStateInitializerRHI& SamplerStateInitializer)
	{
		Ar << SamplerStateInitializer.Filter;
		Ar << SamplerStateInitializer.AddressU;
		Ar << SamplerStateInitializer.AddressV;
		Ar << SamplerStateInitializer.AddressW;
		Ar << SamplerStateInitializer.MipBias;
		Ar << SamplerStateInitializer.MinMipLevel;
		Ar << SamplerStateInitializer.MaxMipLevel;
		Ar << SamplerStateInitializer.MaxAnisotropy;
		Ar << SamplerStateInitializer.BorderColor;
		Ar << SamplerStateInitializer.SamplerComparisonFunction;
		return Ar;
	}
};
struct FRasterizerStateInitializerRHI
{
	TEnumAsByte<ERasterizerFillMode> FillMode;
	TEnumAsByte<ERasterizerCullMode> CullMode;
	float DepthBias;
	float SlopeScaleDepthBias;
	bool bAllowMSAA;
	bool bEnableLineAA;
	
	friend FArchive& operator<<(FArchive& Ar,FRasterizerStateInitializerRHI& RasterizerStateInitializer)
	{
		Ar << RasterizerStateInitializer.FillMode;
		Ar << RasterizerStateInitializer.CullMode;
		Ar << RasterizerStateInitializer.DepthBias;
		Ar << RasterizerStateInitializer.SlopeScaleDepthBias;
		Ar << RasterizerStateInitializer.bAllowMSAA;
		Ar << RasterizerStateInitializer.bEnableLineAA;
		return Ar;
	}
};
struct FDepthStencilStateInitializerRHI
{
	bool bEnableDepthWrite;
	TEnumAsByte<ECompareFunction> DepthTest;

	bool bEnableFrontFaceStencil;
	TEnumAsByte<ECompareFunction> FrontFaceStencilTest;
	TEnumAsByte<EStencilOp> FrontFaceStencilFailStencilOp;
	TEnumAsByte<EStencilOp> FrontFaceDepthFailStencilOp;
	TEnumAsByte<EStencilOp> FrontFacePassStencilOp;
	bool bEnableBackFaceStencil;
	TEnumAsByte<ECompareFunction> BackFaceStencilTest;
	TEnumAsByte<EStencilOp> BackFaceStencilFailStencilOp;
	TEnumAsByte<EStencilOp> BackFaceDepthFailStencilOp;
	TEnumAsByte<EStencilOp> BackFacePassStencilOp;
	uint8 StencilReadMask;
	uint8 StencilWriteMask;

	FDepthStencilStateInitializerRHI(
		bool bInEnableDepthWrite = true,
		ECompareFunction InDepthTest = CF_LessEqual,
		bool bInEnableFrontFaceStencil = false,
		ECompareFunction InFrontFaceStencilTest = CF_Always,
		EStencilOp InFrontFaceStencilFailStencilOp = SO_Keep,
		EStencilOp InFrontFaceDepthFailStencilOp = SO_Keep,
		EStencilOp InFrontFacePassStencilOp = SO_Keep,
		bool bInEnableBackFaceStencil = false,
		ECompareFunction InBackFaceStencilTest = CF_Always,
		EStencilOp InBackFaceStencilFailStencilOp = SO_Keep,
		EStencilOp InBackFaceDepthFailStencilOp = SO_Keep,
		EStencilOp InBackFacePassStencilOp = SO_Keep,
		uint8 InStencilReadMask = 0xFF,
		uint8 InStencilWriteMask = 0xFF
		)
	: bEnableDepthWrite(bInEnableDepthWrite)
	, DepthTest(InDepthTest)
	, bEnableFrontFaceStencil(bInEnableFrontFaceStencil)
	, FrontFaceStencilTest(InFrontFaceStencilTest)
	, FrontFaceStencilFailStencilOp(InFrontFaceStencilFailStencilOp)
	, FrontFaceDepthFailStencilOp(InFrontFaceDepthFailStencilOp)
	, FrontFacePassStencilOp(InFrontFacePassStencilOp)
	, bEnableBackFaceStencil(bInEnableBackFaceStencil)
	, BackFaceStencilTest(InBackFaceStencilTest)
	, BackFaceStencilFailStencilOp(InBackFaceStencilFailStencilOp)
	, BackFaceDepthFailStencilOp(InBackFaceDepthFailStencilOp)
	, BackFacePassStencilOp(InBackFacePassStencilOp)
	, StencilReadMask(InStencilReadMask)
	, StencilWriteMask(InStencilWriteMask)
	{}
	
	friend FArchive& operator<<(FArchive& Ar,FDepthStencilStateInitializerRHI& DepthStencilStateInitializer)
	{
		Ar << DepthStencilStateInitializer.bEnableDepthWrite;
		Ar << DepthStencilStateInitializer.DepthTest;
		Ar << DepthStencilStateInitializer.bEnableFrontFaceStencil;
		Ar << DepthStencilStateInitializer.FrontFaceStencilTest;
		Ar << DepthStencilStateInitializer.FrontFaceStencilFailStencilOp;
		Ar << DepthStencilStateInitializer.FrontFaceDepthFailStencilOp;
		Ar << DepthStencilStateInitializer.FrontFacePassStencilOp;
		Ar << DepthStencilStateInitializer.bEnableBackFaceStencil;
		Ar << DepthStencilStateInitializer.BackFaceStencilTest;
		Ar << DepthStencilStateInitializer.BackFaceStencilFailStencilOp;
		Ar << DepthStencilStateInitializer.BackFaceDepthFailStencilOp;
		Ar << DepthStencilStateInitializer.BackFacePassStencilOp;
		Ar << DepthStencilStateInitializer.StencilReadMask;
		Ar << DepthStencilStateInitializer.StencilWriteMask;
		return Ar;
	}
};
class FBlendStateInitializerRHI
{
public:

	struct FRenderTarget
	{
		TEnumAsByte<EBlendOperation> ColorBlendOp;
		TEnumAsByte<EBlendFactor> ColorSrcBlend;
		TEnumAsByte<EBlendFactor> ColorDestBlend;
		TEnumAsByte<EBlendOperation> AlphaBlendOp;
		TEnumAsByte<EBlendFactor> AlphaSrcBlend;
		TEnumAsByte<EBlendFactor> AlphaDestBlend;
		TEnumAsByte<EColorWriteMask> ColorWriteMask;
		
		FRenderTarget(
			EBlendOperation InColorBlendOp = BO_Add,
			EBlendFactor InColorSrcBlend = BF_One,
			EBlendFactor InColorDestBlend = BF_Zero,
			EBlendOperation InAlphaBlendOp = BO_Add,
			EBlendFactor InAlphaSrcBlend = BF_One,
			EBlendFactor InAlphaDestBlend = BF_Zero,
			EColorWriteMask InColorWriteMask = CW_RGBA
			)
		: ColorBlendOp(InColorBlendOp)
		, ColorSrcBlend(InColorSrcBlend)
		, ColorDestBlend(InColorDestBlend)
		, AlphaBlendOp(InAlphaBlendOp)
		, AlphaSrcBlend(InAlphaSrcBlend)
		, AlphaDestBlend(InAlphaDestBlend)
		, ColorWriteMask(InColorWriteMask)
		{}
		
		friend FArchive& operator<<(FArchive& Ar,FRenderTarget& RenderTarget)
		{
			Ar << RenderTarget.ColorBlendOp;
			Ar << RenderTarget.ColorSrcBlend;
			Ar << RenderTarget.ColorDestBlend;
			Ar << RenderTarget.AlphaBlendOp;
			Ar << RenderTarget.AlphaSrcBlend;
			Ar << RenderTarget.AlphaDestBlend;
			Ar << RenderTarget.ColorWriteMask;
			return Ar;
		}
	};

	FBlendStateInitializerRHI() {}

	FBlendStateInitializerRHI(const FRenderTarget& InRenderTargetBlendState)
	:	bUseIndependentRenderTargetBlendStates(false)
	{
		RenderTargets[0] = InRenderTargetBlendState;
	}

	template<uint32 NumRenderTargets>
	FBlendStateInitializerRHI(const TStaticArray<FRenderTarget,NumRenderTargets>& InRenderTargetBlendStates)
	:	bUseIndependentRenderTargetBlendStates(NumRenderTargets > 1)
	{
		static_assert(NumRenderTargets <= MaxSimultaneousRenderTargets, "Too many render target blend states.");

		for(uint32 RenderTargetIndex = 0;RenderTargetIndex < NumRenderTargets;++RenderTargetIndex)
		{
			RenderTargets[RenderTargetIndex] = InRenderTargetBlendStates[RenderTargetIndex];
		}
	}

	TStaticArray<FRenderTarget,MaxSimultaneousRenderTargets> RenderTargets;
	bool bUseIndependentRenderTargetBlendStates;
	
	friend FArchive& operator<<(FArchive& Ar,FBlendStateInitializerRHI& BlendStateInitializer)
	{
		Ar << BlendStateInitializer.RenderTargets;
		Ar << BlendStateInitializer.bUseIndependentRenderTargetBlendStates;
		return Ar;
	}
};

/**
 *	Screen Resolution
 */
struct FScreenResolutionRHI
{
	uint32	Width;
	uint32	Height;
	uint32	RefreshRate;
};

/**
 *	Viewport bounds structure to set multiple view ports for the geometry shader
 *  (needs to be 1:1 to the D3D11 structure)
 */
struct FViewportBounds
{
	float	TopLeftX;
	float	TopLeftY;
	float	Width;
	float	Height;
	float	MinDepth;
	float	MaxDepth;

	FViewportBounds() {}

	FViewportBounds(float InTopLeftX, float InTopLeftY, float InWidth, float InHeight, float InMinDepth = 0.0f, float InMaxDepth = 1.0f)
		:TopLeftX(InTopLeftX), TopLeftY(InTopLeftY), Width(InWidth), Height(InHeight), MinDepth(InMinDepth), MaxDepth(InMaxDepth)
	{
	}

	friend FArchive& operator<<(FArchive& Ar,FViewportBounds& ViewportBounds)
	{
		Ar << ViewportBounds.TopLeftX;
		Ar << ViewportBounds.TopLeftY;
		Ar << ViewportBounds.Width;
		Ar << ViewportBounds.Height;
		Ar << ViewportBounds.MinDepth;
		Ar << ViewportBounds.MaxDepth;
		return Ar;
	}
};


typedef TArray<FScreenResolutionRHI>	FScreenResolutionArray;

struct FVRamAllocation
{
	FVRamAllocation(uint32 InAllocationStart = 0, uint32 InAllocationSize = 0)
		: AllocationStart(InAllocationStart)
		, AllocationSize(InAllocationSize)
	{
	}

	bool IsValid() { return AllocationSize > 0; }

	// in bytes
	uint32 AllocationStart;
	// in bytes
	uint32 AllocationSize;
};

struct FRHIResourceInfo
{
	FVRamAllocation VRamAllocation;
};

struct FRHIResourceCreateInfo
{
	FRHIResourceCreateInfo()
		: BulkData(0)
		, ResourceArray(0)
	{}

	// for CreateTexture calls
	FRHIResourceCreateInfo(FResourceBulkDataInterface* InBulkData)
		: BulkData(InBulkData)
	{}

	// for CreateVertexBuffer/CreateStructuredBuffer calls
	FRHIResourceCreateInfo(FResourceArrayInterface* InResourceArray)
		: ResourceArray(InResourceArray)
	{}

	// for CreateTexture calls
	FResourceBulkDataInterface* BulkData;
	// for CreateVertexBuffer/CreateStructuredBuffer calls
	FResourceArrayInterface* ResourceArray;
};

// Forward-declaration.
struct FResolveParams;

struct FResolveRect
{
	int32 X1;
	int32 Y1;
	int32 X2;
	int32 Y2;
	// e.g. for a a full 256 x 256 area starting at (0, 0) it would be 
	// the values would be 0, 0, 256, 256
	FORCEINLINE FResolveRect(int32 InX1=-1, int32 InY1=-1, int32 InX2=-1, int32 InY2=-1)
	:	X1(InX1)
	,	Y1(InY1)
	,	X2(InX2)
	,	Y2(InY2)
	{}

	FORCEINLINE FResolveRect(const FResolveRect& Other)
		: X1(Other.X1)
		, Y1(Other.Y1)
		, X2(Other.X2)
		, Y2(Other.Y2)
	{}

	bool IsValid() const
	{
		return X1 >= 0 && Y1 >= 0 && X2 - X1 > 0 && Y2 - Y1 > 0;
	}

	friend FArchive& operator<<(FArchive& Ar,FResolveRect& ResolveRect)
	{
		Ar << ResolveRect.X1;
		Ar << ResolveRect.Y1;
		Ar << ResolveRect.X2;
		Ar << ResolveRect.Y2;
		return Ar;
	}
};

struct FResolveParams
{
	/** used to specify face when resolving to a cube map texture */
	ECubeFace CubeFace;
	/** resolve RECT bounded by [X1,Y1]..[X2,Y2]. Or -1 for fullscreen */
	FResolveRect Rect;
	/** The mip index to resolve in both source and dest. */
	int32 MipIndex;
	/** Array index to resolve in the source. */
	int32 SourceArrayIndex;
	/** Array index to resolve in the dest. */
	int32 DestArrayIndex;

	/** constructor */
	FResolveParams(
		const FResolveRect& InRect = FResolveRect(), 
		ECubeFace InCubeFace = CubeFace_PosX,
		int32 InMipIndex = 0,
		int32 InSourceArrayIndex = 0,
		int32 InDestArrayIndex = 0)
		:	CubeFace(InCubeFace)
		,	Rect(InRect)
		,	MipIndex(InMipIndex)
		,	SourceArrayIndex(InSourceArrayIndex)
		,	DestArrayIndex(InDestArrayIndex)
	{}

	FORCEINLINE FResolveParams(const FResolveParams& Other)
		: CubeFace(Other.CubeFace)
		, Rect(Other.Rect)
		, MipIndex(Other.MipIndex)
		, SourceArrayIndex(Other.SourceArrayIndex)
		, DestArrayIndex(Other.DestArrayIndex)
	{}
};

/** specifies an update region for a texture */
struct FUpdateTextureRegion2D
{
	/** offset in texture */
	int32 DestX;
	int32 DestY;
	
	/** offset in source image data */
	int32 SrcX;
	int32 SrcY;
	
	/** size of region to copy */
	int32 Width;
	int32 Height;

	FUpdateTextureRegion2D()
	{}

	FUpdateTextureRegion2D(int32 InDestX, int32 InDestY, int32 InSrcX, int32 InSrcY, int32 InWidth, int32 InHeight)
	:	DestX(InDestX)
	,	DestY(InDestY)
	,	SrcX(InSrcX)
	,	SrcY(InSrcY)
	,	Width(InWidth)
	,	Height(InHeight)
	{}

	friend FArchive& operator<<(FArchive& Ar,FUpdateTextureRegion2D& Region)
	{
		Ar << Region.DestX;
		Ar << Region.DestY;
		Ar << Region.SrcX;
		Ar << Region.SrcY;
		Ar << Region.Width;
		Ar << Region.Height;
		return Ar;
	}
};

/** specifies an update region for a texture */
struct FUpdateTextureRegion3D
{
	/** offset in texture */
	int32 DestX;
	int32 DestY;
	int32 DestZ;

	/** offset in source image data */
	int32 SrcX;
	int32 SrcY;
	int32 SrcZ;

	/** size of region to copy */
	int32 Width;
	int32 Height;
	int32 Depth;

	FUpdateTextureRegion3D()
	{}

	FUpdateTextureRegion3D(int32 InDestX, int32 InDestY, int32 InDestZ, int32 InSrcX, int32 InSrcY, int32 InSrcZ, int32 InWidth, int32 InHeight, int32 InDepth)
	:	DestX(InDestX)
	,	DestY(InDestY)
	,	DestZ(InDestZ)
	,	SrcX(InSrcX)
	,	SrcY(InSrcY)
	,	SrcZ(InSrcZ)
	,	Width(InWidth)
	,	Height(InHeight)
	,	Depth(InDepth)
	{}

	friend FArchive& operator<<(FArchive& Ar,FUpdateTextureRegion3D& Region)
	{
		Ar << Region.DestX;
		Ar << Region.DestY;
		Ar << Region.DestZ;
		Ar << Region.SrcX;
		Ar << Region.SrcY;
		Ar << Region.SrcZ;
		Ar << Region.Width;
		Ar << Region.Height;
		Ar << Region.Depth;
		return Ar;
	}
};

struct FRHIDispatchIndirectParameters
{
	uint32 ThreadGroupCountX;
	uint32 ThreadGroupCountY;
	uint32 ThreadGroupCountZ;
};

struct FRHIDrawIndirectParameters
{
	uint32 VertexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartVertexLocation;
	uint32 StartInstanceLocation;
};

struct FRHIDrawIndexedIndirectParameters
{
	uint32 IndexCountPerInstance;
	uint32 InstanceCount;
	uint32 StartIndexLocation;
	int32 BaseVertexLocation;
	uint32 StartInstanceLocation;
};


struct FTextureMemoryStats
{
	// Hardware state (never change after device creation):

	// -1 if unknown, in bytes
	int64 DedicatedVideoMemory;
	// -1 if unknown, in bytes
	int64 DedicatedSystemMemory;
	// -1 if unknown, in bytes
	int64 SharedSystemMemory;
	// Total amount of "graphics memory" that we think we can use for all our graphics resources, in bytes. -1 if unknown.
	int64 TotalGraphicsMemory;

	// Size of allocated memory, in bytes
	int64 AllocatedMemorySize;
	// 0 if streaming pool size limitation is disabled, in bytes
	int64 TexturePoolSize;
	// Upcoming adjustments to allocated memory, in bytes (async reallocations)
	int32 PendingMemoryAdjustment;

	// defaults
	FTextureMemoryStats()
		: DedicatedVideoMemory(-1)
		, DedicatedSystemMemory(-1)
		, SharedSystemMemory(-1)
		, TotalGraphicsMemory(-1)
		, AllocatedMemorySize(0)
		, TexturePoolSize(0)
		, PendingMemoryAdjustment(0)
	{
	}

	bool AreHardwareStatsValid() const
	{
#if !PLATFORM_HTML5
		return DedicatedVideoMemory >= 0 && DedicatedSystemMemory >= 0 && SharedSystemMemory >= 0;
#else 
		return false; 
#endif 
	}

	bool IsUsingLimitedPoolSize() const
	{
		return TexturePoolSize > 0;
	}

	int64 ComputeAvailableMemorySize() const
	{
		return FMath::Max(TexturePoolSize - AllocatedMemorySize, (int64)0);
	}
};

// RHI counter stats.
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("DrawPrimitive calls"),STAT_RHIDrawPrimitiveCalls,STATGROUP_RHI,RHI_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Triangles drawn"),STAT_RHITriangles,STATGROUP_RHI,RHI_API);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Lines drawn"),STAT_RHILines,STATGROUP_RHI,RHI_API);

#if STATS
	#define RHI_DRAW_CALL_INC() \
		INC_DWORD_STAT(STAT_RHIDrawPrimitiveCalls); \
		GNumDrawCallsRHI++;

	#define RHI_DRAW_CALL_STATS(PrimitiveType,NumPrimitives) \
		RHI_DRAW_CALL_INC(); \
		INC_DWORD_STAT_BY(STAT_RHITriangles,(uint32)(PrimitiveType != PT_LineList ? (NumPrimitives) : 0)); \
		INC_DWORD_STAT_BY(STAT_RHILines,(uint32)(PrimitiveType == PT_LineList ? (NumPrimitives) : 0)); \
		GNumPrimitivesDrawnRHI += NumPrimitives;
#else
	#define RHI_DRAW_CALL_INC()
	#define RHI_DRAW_CALL_STATS(PrimitiveType,NumPrimitives)
#endif

// RHI memory stats.
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render target memory 2D"),STAT_RenderTargetMemory2D,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render target memory 3D"),STAT_RenderTargetMemory3D,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Render target memory Cube"),STAT_RenderTargetMemoryCube,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture memory 2D"),STAT_TextureMemory2D,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture memory 3D"),STAT_TextureMemory3D,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Texture memory Cube"),STAT_TextureMemoryCube,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Uniform buffer memory"),STAT_UniformBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Index buffer memory"),STAT_IndexBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Vertex buffer memory"),STAT_VertexBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Structured buffer memory"),STAT_StructuredBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Pixel buffer memory"),STAT_PixelBufferMemory,STATGROUP_RHI,FPlatformMemory::MCR_GPU,RHI_API);

// RHI base resource types.
#include "RHIResources.h"

//
// Platform-specific RHI types.
//
#if PLATFORM_USES_DYNAMIC_RHI
	// Use dynamically bound RHIs on PCs and when using the null RHI.
	#define USE_DYNAMIC_RHI 1
	#include "DynamicRHI.h"
#else
	// Fall back to the null RHI
	#define USE_DYNAMIC_RHI 1
	#include "DynamicRHI.h"
#endif

#if !USE_DYNAMIC_RHI
	// Define the statically bound RHI methods with the RHI name prefix.
#define DEFINE_RHIMETHOD_CMDLIST(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	extern Type Name##_Internal ParameterTypesAndNames
#define DEFINE_RHIMETHOD_GLOBAL(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	extern Type Name##_Internal ParameterTypesAndNames
#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	extern Type RHI##Name ParameterTypesAndNames
#define DEFINE_RHIMETHOD_GLOBALFLUSH(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	extern Type Name##_Internal ParameterTypesAndNames
#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) \
	extern Type Name##_Internal ParameterTypesAndNames

#include "RHIMethods.h"
#undef DEFINE_RHIMETHOD
#undef DEFINE_RHIMETHOD_CMDLIST
#undef DEFINE_RHIMETHOD_GLOBAL
#undef DEFINE_RHIMETHOD_GLOBALFLUSH
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE


#endif

/** Initializes the RHI. */
extern RHI_API void RHIInit(bool bHasEditorToken);

/** Shuts down the RHI. */
extern RHI_API void RHIExit();


// the following helper macros allow to safely convert shader types without much code clutter
#define GETSAFERHISHADER_PIXEL(Shader) (Shader ? Shader->GetPixelShader() : (FPixelShaderRHIParamRef)FPixelShaderRHIRef())
#define GETSAFERHISHADER_VERTEX(Shader) (Shader ? Shader->GetVertexShader() : (FVertexShaderRHIParamRef)FVertexShaderRHIRef())
#define GETSAFERHISHADER_HULL(Shader) (Shader ? Shader->GetHullShader() : FHullShaderRHIRef())
#define GETSAFERHISHADER_DOMAIN(Shader) (Shader ? Shader->GetDomainShader() : FDomainShaderRHIRef())
#define GETSAFERHISHADER_GEOMETRY(Shader) (Shader ? Shader->GetGeometryShader() : FGeometryShaderRHIRef())
#define GETSAFERHISHADER_COMPUTE(Shader) (Shader ? Shader->GetComputeShader() : FComputeShaderRHIRef())

// RHI utility functions that depend on the RHI definitions.
#include "RHIUtilities.h"

#endif // __RHI_h__
