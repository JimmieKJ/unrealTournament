// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"

enum EMetalPipelineHashBits
{
	NumBits_BlendState = 5, //(x6=30),
#if PLATFORM_MAC
	NumBits_RenderTargetFormat = 5, //(x6=30),
#else
	NumBits_RenderTargetFormat = 4, //(x6=24),
#endif
	NumBits_DepthFormat = 3, //(x1=3),
	NumBits_StencilFormat = 3, //(x1=3),
	NumBits_SampleCount = 3, //(x1=3),
#if PLATFORM_MAC
	NumBits_PrimitiveTopology = 2, //(x1=2)
#endif
};

enum EMetalPipelineHashOffsets
{
	Offset_BlendState0 = 0,
	Offset_BlendState1 = Offset_BlendState0 + NumBits_BlendState,
	Offset_BlendState2 = Offset_BlendState1 + NumBits_BlendState,
	Offset_BlendState3 = Offset_BlendState2 + NumBits_BlendState,
	Offset_BlendState4 = Offset_BlendState3 + NumBits_BlendState,
	Offset_BlendState5 = Offset_BlendState4 + NumBits_BlendState,
	Offset_RenderTargetFormat0 = Offset_BlendState5 + NumBits_BlendState,
	Offset_RenderTargetFormat1 = Offset_RenderTargetFormat0 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat2 = Offset_RenderTargetFormat1 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat3 = Offset_RenderTargetFormat2 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat4 = Offset_RenderTargetFormat3 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat5 = Offset_RenderTargetFormat4 + NumBits_RenderTargetFormat,
	Offset_DepthFormat = Offset_RenderTargetFormat5 + NumBits_RenderTargetFormat,
	Offset_StencilFormat = Offset_DepthFormat + NumBits_DepthFormat,
	Offset_SampleCount = Offset_StencilFormat + NumBits_StencilFormat,
#if PLATFORM_MAC
	Offset_PrimitiveTopology = Offset_SampleCount + NumBits_SampleCount,
	Offset_End = Offset_PrimitiveTopology + NumBits_PrimitiveTopology
#else
	Offset_End = Offset_SampleCount + NumBits_SampleCount
#endif
};

struct FMetalRenderPipelineDesc
{
	FMetalRenderPipelineDesc();
	~FMetalRenderPipelineDesc();

	template<typename Type>
	inline void SetHashValue(uint32 Offset, uint32 NumBits, Type Value)
	{
		FMetalRenderPipelineHash BitMask = ((((FMetalRenderPipelineHash)1ULL) << NumBits) - 1) << Offset;
		Hash = (Hash & ~BitMask) | (((FMetalRenderPipelineHash)Value << Offset) & BitMask);
	}

	/**
	 * @return the hash of the current pipeline state
	 */
	inline FMetalRenderPipelineHash GetHash() const { return Hash; }
	
	/**
	 * @return an pipeline state object that matches the current state and the BSS
	 */
	id<MTLRenderPipelineState> CreatePipelineStateForBoundShaderState(FMetalBoundShaderState* BSS, FMetalHashedVertexDescriptor const& VertexDesc, MTLRenderPipelineReflection** Reflection = nil) const;

#if METAL_DEBUG_OPTIONS
	/**
	 * @return a reflection object that matches the current state and the BSS
	 */
	MTLRenderPipelineReflection* GetReflectionData(FMetalBoundShaderState* BSS, FMetalHashedVertexDescriptor const& VertexDesc) const;
#endif

	MTLRenderPipelineDescriptor* PipelineDescriptor;
	uint32 SampleCount;

	// running hash of the pipeline state
	FMetalRenderPipelineHash Hash;

	struct FMetalRenderPipelineKey
	{
		FMetalRenderPipelineHash RenderPipelineHash;
		FMetalHashedVertexDescriptor VertexDescriptorHash;
		id<MTLFunction> VertexFunction;
		id<MTLFunction> PixelFunction;
		
		bool operator==(FMetalRenderPipelineKey const& Other) const;
		
		friend uint32 GetTypeHash(FMetalRenderPipelineKey const& Key)
		{
			return GetTypeHash(Key.RenderPipelineHash) ^ GetTypeHash(Key.VertexDescriptorHash) ^ GetTypeHash(Key.VertexFunction) ^ GetTypeHash(Key.PixelFunction);
		}
	};
	
	struct FMetalRWMutex
	{
		FMetalRWMutex()
		{
			int Err = pthread_rwlock_init(&Mutex, nullptr);
			checkf(Err == 0, TEXT("pthread_rwlock_init failed with error: %d"), errno);
		}
		
		~FMetalRWMutex()
		{
			int Err = pthread_rwlock_destroy(&Mutex);
			checkf(Err == 0, TEXT("pthread_rwlock_destroy failed with error: %d"), errno);
		}
		
		pthread_rwlock_t Mutex;
	};
	
	static FMetalRWMutex MetalPipelineMutex;
	static TMap<FMetalRenderPipelineKey, id<MTLRenderPipelineState>> MetalPipelineCache;
	static uint32 BlendBitOffsets[6];
	static uint32 RTBitOffsets[6];
#if METAL_DEBUG_OPTIONS
	static TMap<FMetalRenderPipelineKey, MTLRenderPipelineReflection*> MetalReflectionCache;
#endif
};
