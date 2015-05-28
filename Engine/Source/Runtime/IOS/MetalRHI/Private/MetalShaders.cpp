// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalShaders.cpp: Metal shader RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalShaderResources.h"
#include "MetalResources.h"

/** Set to 1 to enable shader debugging (makes the driver save the shader source) */
#define DEBUG_METAL_SHADERS (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)


class FMetalCompiledShaderKey
{
public:
	FMetalCompiledShaderKey(
		uint32 InCodeSize,
		uint32 InCodeCRC
		)
		: CodeSize(InCodeSize)
		, CodeCRC(InCodeCRC)
	{}

	friend bool operator ==(const FMetalCompiledShaderKey& A, const FMetalCompiledShaderKey& B)
	{
		return A.CodeSize == B.CodeSize && A.CodeCRC == B.CodeCRC;
	}

	friend uint32 GetTypeHash(const FMetalCompiledShaderKey &Key)
	{
		return GetTypeHash(Key.CodeSize) ^ GetTypeHash(Key.CodeCRC);
	}

private:
	GLenum TypeEnum;
	uint32 CodeSize;
	uint32 CodeCRC;
};

typedef TMap<FMetalCompiledShaderKey, GLuint> FMetalCompiledShaderCache;

static FMetalCompiledShaderCache& GetMetalCompiledShaderCache()
{
	static FMetalCompiledShaderCache CompiledShaderCache;
	return CompiledShaderCache;
}

/** Initialization constructor. */
template<typename BaseResourceType, int32 ShaderType>
TMetalBaseShader<BaseResourceType, ShaderType>::TMetalBaseShader(const TArray<uint8>& InCode)
	: DirtyUniformBuffers(0)
{
	FMemoryReader Ar(InCode, true);

	// was the shader already compiled offline?
	uint8 OfflineCompiledFlag;
	Ar << OfflineCompiledFlag;
	check(OfflineCompiledFlag == 0 || OfflineCompiledFlag == 1);

	// get the header
	FMetalCodeHeader Header = { 0 };
	Ar << Header;

	// remember where the header ended and code (precompiled or source) begins
	int32 CodeOffset = Ar.Tell();
	const ANSICHAR* SourceCode = (ANSICHAR*)InCode.GetData() + CodeOffset;

	uint32 CodeLength, CodeCRC;
	if (OfflineCompiledFlag)
	{
		CodeLength = InCode.Num() - CodeOffset;

		// CRC the compiled code
		CodeCRC = FCrc::MemCrc_DEPRECATED(InCode.GetData() + CodeOffset, InCode.Num() - CodeOffset);
	}
	else
	{
		UE_LOG(LogMetal, Display, TEXT("Loaded a non-offline compiled shader (will be slower to load)"));

		CodeLength = InCode.Num() - CodeOffset - 1;

		// CRC the source
		CodeCRC = FCrc::MemCrc_DEPRECATED(SourceCode, CodeLength);
	}
	FMetalCompiledShaderKey Key(CodeLength, CodeCRC);

	// Find the existing compiled shader in the cache.
	auto Resource = GetMetalCompiledShaderCache().FindRef(Key);
//	if (!Resource)
	{
		id<MTLLibrary> Library;

		if (OfflineCompiledFlag)
		{
			// allow GCD to copy the data into its own buffer
			//		dispatch_data_t GCDBuffer = dispatch_data_create(InCode.GetTypedData() + CodeOffset, InCode.Num() - CodeOffset, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
			uint32 BufferSize = InCode.Num() - CodeOffset;
			void* Buffer = FMemory::Malloc( BufferSize );
			FMemory::Memcpy( Buffer, InCode.GetData() + CodeOffset, BufferSize );
			dispatch_data_t GCDBuffer = dispatch_data_create(Buffer, BufferSize, dispatch_get_main_queue(), ^(void) { FMemory::Free(Buffer); } );

			// load up the already compiled shader
			NSError* Error;
			Library = [FMetalManager::GetDevice() newLibraryWithData:GCDBuffer error:&Error];
			if (Library == nil)
			{
				NSLog(@"Failed to create library: %@", Error);
			}
			
			GlslCodeNSString = @"OFFLINE";
			dispatch_release(GCDBuffer);
		}
		else
		{
			NSError* Error;
			NSString* ShaderSource = [NSString stringWithUTF8String:SourceCode];

	#if DEBUG_METAL_SHADERS
			NSDictionary *PreprocessorMacros = @{ @"MTLSL_ENABLE_DEBUG_INFO" : @(1)};
			MTLCompileOptions *CompileOptions = [[MTLCompileOptions alloc] init];
			[CompileOptions setPreprocessorMacros : PreprocessorMacros];

			Library = [FMetalManager::GetDevice() newLibraryWithSource:ShaderSource options : CompileOptions error : &Error];
			[CompileOptions release];
	#else
			Library = [FMetalManager::GetDevice() newLibraryWithSource:ShaderSource options : nil error : &Error];
	#endif

			if (Library == nil)
			{
				NSLog(@"Failed to create library: %@", Error);
				NSLog(@"*********** Error\n%@", ShaderSource);
			}
			else if (Error != nil)
			{
				// Warning...
				NSLog(@"%@\n", Error);
			}

			GlslCode.Empty(CodeLength + 1);
			GlslCode.AddUninitialized(CodeLength + 1);
			FMemory::Memcpy(GlslCode.GetData(), SourceCode, CodeLength + 1);
			GlslCodeString = (ANSICHAR*)GlslCode.GetData();
			GlslCodeNSString = ShaderSource;

#if !UE_BUILD_SHIPPING
			[GlslCodeNSString retain];
			TRACK_OBJECT(GlslCodeNSString);
#endif
		}

		// assume there's only one function called 'Main', and use that to get the function from the library
		Function = [Library newFunctionWithName:@"Main"];
		[Library release];

//		Resource = Resource;
		Bindings = Header.Bindings;
		BoundUniformBuffers.AddZeroed(Header.Bindings.NumUniformBuffers);
		UniformBuffersCopyInfo = Header.UniformBuffersCopyInfo;

		//@todo: Find better way...
		if (ShaderType == SF_Compute)
		{
			auto* ComputeShader = (FMetalComputeShader*)this;
			ComputeShader->NumThreadsX = Header.NumThreadsX;
			ComputeShader->NumThreadsY = Header.NumThreadsY;
			ComputeShader->NumThreadsZ = Header.NumThreadsZ;
		}
	}
}

/** Destructor */
template<typename BaseResourceType, int32 ShaderType>
TMetalBaseShader<BaseResourceType, ShaderType>::~TMetalBaseShader()
{
	UNTRACK_OBJECT(Function);
	[Function release];
	UNTRACK_OBJECT(GlslCodeNSString);
	[GlslCodeNSString release];
}

FMetalComputeShader::FMetalComputeShader(const TArray<uint8>& InCode)
	: TMetalBaseShader<FRHIComputeShader, SF_Compute>(InCode)
//	, NumThreadsX(0)
//	, NumThreadsY(0)
//	, NumThreadsZ(0)
{
	NSError* Error;
	Kernel = [FMetalManager::GetDevice() newComputePipelineStateWithFunction:Function error:&Error];
	
	if (Kernel == nil)
	{
		NSLog(@"Failed to create compute kernel: %@", Error);
	}
}


FVertexShaderRHIRef FMetalDynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
	FMetalVertexShader* Shader = new FMetalVertexShader(Code);
	return Shader;
}

FPixelShaderRHIRef FMetalDynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	FMetalPixelShader* Shader = new FMetalPixelShader(Code);
	return Shader;
}

FHullShaderRHIRef FMetalDynamicRHI::RHICreateHullShader(const TArray<uint8>& Code) 
{ 
	FMetalHullShader* Shader = new FMetalHullShader(Code);
	return Shader;
}

FDomainShaderRHIRef FMetalDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code) 
{ 
	FMetalDomainShader* Shader = new FMetalDomainShader(Code);
	return Shader;
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code) 
{ 
	FMetalGeometryShader* Shader = new FMetalGeometryShader(Code);
	return Shader;
}

FGeometryShaderRHIRef FMetalDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList,
	uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	checkf(0, TEXT("Not supported yet"));
	return NULL;
}

FComputeShaderRHIRef FMetalDynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code) 
{ 
	FMetalComputeShader* Shader = new FMetalComputeShader(Code);
	return Shader;
}

FMetalBoundShaderState::FMetalBoundShaderState(
			FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
			FVertexShaderRHIParamRef InVertexShaderRHI,
			FPixelShaderRHIParamRef InPixelShaderRHI,
			FHullShaderRHIParamRef InHullShaderRHI,
			FDomainShaderRHIParamRef InDomainShaderRHI,
			FGeometryShaderRHIParamRef InGeometryShaderRHI)
	:	CacheLink(InVertexDeclarationRHI,InVertexShaderRHI,InPixelShaderRHI,InHullShaderRHI,InDomainShaderRHI,InGeometryShaderRHI,this)
{
	FMetalVertexDeclaration* InVertexDeclaration = FMetalDynamicRHI::ResourceCast(InVertexDeclarationRHI);
	FMetalVertexShader* InVertexShader = FMetalDynamicRHI::ResourceCast(InVertexShaderRHI);
	FMetalPixelShader* InPixelShader = FMetalDynamicRHI::ResourceCast(InPixelShaderRHI);
	FMetalHullShader* InHullShader = FMetalDynamicRHI::ResourceCast(InHullShaderRHI);
	FMetalDomainShader* InDomainShader = FMetalDynamicRHI::ResourceCast(InDomainShaderRHI);
	FMetalGeometryShader* InGeometryShader = FMetalDynamicRHI::ResourceCast(InGeometryShaderRHI);

	// cache everything
	VertexDeclaration = InVertexDeclaration;
	VertexShader = InVertexShader;
	PixelShader = InPixelShader;
	HullShader = InHullShader;
	DomainShader = InDomainShader;
	GeometryShader = InGeometryShader;

#if !UE_BUILD_SHIPPING
	if (GFrameCounter > 3)
	{
		NSLog(@"===============================================================");
		NSLog(@"Creating a BSS at runtime frame %lld... this may hitch! [this = %p]", GFrameCounter, this);
		NSLog(@"Vertex declaration:");
		FVertexDeclarationElementList& Elements = VertexDeclaration->Elements;
		for (int32 i = 0; i < Elements.Num(); i++)
		{
			FVertexElement& Elem = Elements[i];
			NSLog(@"   Elem %d: attr: %d, stream: %d, type: %d, stride: %d, offset: %d", i, Elem.AttributeIndex, Elem.StreamIndex, (uint32)Elem.Type, Elem.Stride, Elem.Offset);
		}
		
		NSLog(@"\nVertexShader:");
		NSLog(@"%@", VertexShader ? VertexShader->GlslCodeNSString : @"NONE");
		NSLog(@"\nPixelShader:");
		NSLog(@"%@", PixelShader ? PixelShader->GlslCodeNSString : @"NONE");
		NSLog(@"===============================================================");
	}
#endif
	
	AddRef();
}

FMetalBoundShaderState::~FMetalBoundShaderState()
{
	// free all of the pipeline state objects we have made
	for (auto It = PipelineStates.CreateIterator(); It; ++It)
	{
		TRACK_OBJECT(It.Value());
		[It.Value() release];
	}
}

void FMetalBoundShaderState::PrepareToDraw(const FPipelineShadow& PipelineShadow)
{
	// generate a key for the current state
	uint64 Hash = PipelineShadow.GetHash();
	
	
	// have we made a matching state object yet?
	id<MTLRenderPipelineState> PipelineState = PipelineStates.FindRef(Hash);
	
	// make one if not
	if (PipelineState == nil)
	{
		PipelineState = PipelineShadow.CreatePipelineStateForBoundShaderState(this);
		PipelineStates.Add(Hash, PipelineState);
		
#if !UE_BUILD_SHIPPING
		if (GFrameCounter > 3)
		{
			NSLog(@"Created a hitchy pipeline state for hash %llx (this = %p)", Hash, this);
		}
#endif
	}
	
	// set it now
	[FMetalManager::GetContext() setRenderPipelineState:PipelineState];
}

FBoundShaderStateRHIRef FMetalDynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{
	check(IsInRenderingThread());
	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);

	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalBoundShaderStateTime);
//		FBoundShaderStateKey Key(VertexDeclarationRHI,VertexShaderRHI,PixelShaderRHI,HullShaderRHI,DomainShaderRHI,GeometryShaderRHI);
//		NSLog(@"BSS Key = %x", GetTypeHash(Key));
		return new FMetalBoundShaderState(VertexDeclarationRHI,VertexShaderRHI,PixelShaderRHI,HullShaderRHI,DomainShaderRHI,GeometryShaderRHI);
	}
}








FMetalShaderParameterCache::FMetalShaderParameterCache() :
	GlobalUniformArraySize(-1)
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = 0;
	}
}

void FMetalShaderParameterCache::InitializeResources(int32 UniformArraySize)
{
	check(GlobalUniformArraySize == -1);
	
	PackedGlobalUniforms[0] = (uint8*)FMemory::Malloc(UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	PackedUniformsScratch[0] = (uint8*)FMemory::Malloc(UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	
	FMemory::Memzero(PackedGlobalUniforms[0], UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	FMemory::Memzero(PackedUniformsScratch[0], UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	for (int32 ArrayIndex = 1; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniforms[ArrayIndex] = PackedGlobalUniforms[ArrayIndex - 1] + UniformArraySize;
		PackedUniformsScratch[ArrayIndex] = PackedUniformsScratch[ArrayIndex - 1] + UniformArraySize;
	}
	GlobalUniformArraySize = UniformArraySize;
	
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = 0;
	}
}

/** Destructor. */
FMetalShaderParameterCache::~FMetalShaderParameterCache()
{
	if (GlobalUniformArraySize > 0)
	{
		FMemory::Free(PackedUniformsScratch[0]);
		FMemory::Free(PackedGlobalUniforms[0]);
	}
	
	FMemory::Memzero(PackedUniformsScratch);
	FMemory::Memzero(PackedGlobalUniforms);
	
	GlobalUniformArraySize = -1;
}

/**
 * Marks all uniform arrays as dirty.
 */
void FMetalShaderParameterCache::MarkAllDirty()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].LowVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].HighVector = 0;//UniformArraySize / 16;
	}
}

const int SizeOfFloat4 = 4 * sizeof(float);

/**
 * Set parameter values.
 */
void FMetalShaderParameterCache::Set(uint32 BufferIndexName, uint32 ByteOffset, uint32 NumBytes, const void* NewValues)
{
	uint32 BufferIndex = CrossCompiler::PackedTypeNameToTypeIndex(BufferIndexName);
	check(GlobalUniformArraySize != -1);
	check(BufferIndex < CrossCompiler::PACKED_TYPEINDEX_MAX);
	check(ByteOffset + NumBytes <= (uint32)GlobalUniformArraySize);
	PackedGlobalUniformDirty[BufferIndex].LowVector = FMath::Min(PackedGlobalUniformDirty[BufferIndex].LowVector, ByteOffset / SizeOfFloat4);
	PackedGlobalUniformDirty[BufferIndex].HighVector = FMath::Max(PackedGlobalUniformDirty[BufferIndex].HighVector, (ByteOffset + NumBytes + SizeOfFloat4 - 1) / SizeOfFloat4);
	FMemory::Memcpy(PackedGlobalUniforms[BufferIndex] + ByteOffset, NewValues, NumBytes);
}

void FMetalShaderParameterCache::CommitPackedGlobals(int32 Stage, const FMetalShaderBindings& Bindings)
{
	// copy the current uniform buffer into the ring buffer to submit
	for (int32 Index = 0; Index < Bindings.PackedGlobalArrays.Num(); ++Index)
	{
		int32 UniformBufferIndex = Bindings.PackedGlobalArrays[Index].TypeIndex;
 
		// is there any data that needs to be copied?
		if (PackedGlobalUniformDirty[UniformBufferIndex].HighVector > 0)//PackedGlobalUniformDirty[UniformBufferIndex].LowVector)
		{
			//@todo rco safe to remove this forever?
			//check(UniformBufferIndex == 0 || UniformBufferIndex == 1 || UniformBufferIndex == 3);
			uint32 TotalSize = Bindings.PackedGlobalArrays[Index].Size;
			uint32 SizeToUpload = PackedGlobalUniformDirty[UniformBufferIndex].HighVector * SizeOfFloat4;
//@todo-rco: Temp workaround
			//check(SizeToUpload <= TotalSize);
			SizeToUpload = TotalSize;
 /*
			if (UniformBufferIndex == CrossCompiler::PACKED_TYPEINDEX_MEDIUMP)
			{
				// Float4 -> Half4
				SizeToUpload /= 2;
			}
*/
			// allocate memory in the ring buffer
			id<MTLBuffer> RingBuffer = FMetalManager::Get()->GetRingBuffer();
			uint32 Offset = FMetalManager::Get()->AllocateFromRingBuffer(TotalSize);
/*
			// copy into the middle of the ring buffer
			if (UniformBufferIndex == CrossCompiler::PACKED_TYPEINDEX_MEDIUMP)
			{
				// Convert to half
				const int NumVectors = PackedGlobalUniformDirty[UniformBufferIndex].HighVector;
				const auto* RESTRICT SourceF4 = (FVector4* )PackedGlobalUniforms[UniformBufferIndex];
				auto* RESTRICT Dest = (FFloat16*)((uint8*)[RingBuffer contents]) + Offset;
				for (int Vector = 0; Vector < NumVectors; ++Vector, ++SourceF4, Dest += 4)
				{
					Dest[0].Set(SourceF4->X);
					Dest[1].Set(SourceF4->Y);
					Dest[2].Set(SourceF4->Z);
					Dest[3].Set(SourceF4->W);
				}
			}
			else
 */
			{
				//check(UniformBufferIndex != CrossCompiler::PACKED_TYPEINDEX_LOWP);
//@todo-rco: Temp workaround
				FMemory::Memcpy(((uint8*)[RingBuffer contents]) + Offset, PackedGlobalUniforms[UniformBufferIndex], FMath::Min(TotalSize, SizeToUpload));
			}
			
			switch (Stage)
			{
				case CrossCompiler::SHADER_STAGE_VERTEX:
//					NSLog(@"Uploading %d bytes to vertex", SizeToUpload);
					[FMetalManager::GetContext() setVertexBuffer:RingBuffer offset:Offset atIndex:UniformBufferIndex];
					break;

				case CrossCompiler::SHADER_STAGE_PIXEL:
//					NSLog(@"Uploading %d bytes to pixel", SizeToUpload);
					[FMetalManager::GetContext() setFragmentBuffer:RingBuffer offset : Offset atIndex : UniformBufferIndex];
					break;
					
				
				default:check(0);
			}

			// mark as clean
			PackedGlobalUniformDirty[UniformBufferIndex].HighVector = 0;
		}
	}
}

void FMetalShaderParameterCache::CommitPackedUniformBuffers(TRefCountPtr<FMetalBoundShaderState> BoundShaderState, int32 Stage, const TArray< TRefCountPtr<FRHIUniformBuffer> >& RHIUniformBuffers, const TArray<FMetalUniformBufferCopyInfo>& UniformBuffersCopyInfo)
{
//	SCOPE_CYCLE_COUNTER(STAT_MetalConstantBufferUpdateTime);
	// Uniform Buffers are split into precision/type; the list of RHI UBs is traversed and if a new one was set, its
	// contents are copied per precision/type into corresponding scratch buffers which are then uploaded to the program
	if (Stage == CrossCompiler::SHADER_STAGE_PIXEL && !IsValidRef(BoundShaderState->PixelShader))
	{
		return;
	}

	auto& Bindings = Stage == CrossCompiler::SHADER_STAGE_VERTEX ? BoundShaderState->VertexShader->Bindings : BoundShaderState->PixelShader->Bindings;
	check(Bindings.NumUniformBuffers == RHIUniformBuffers.Num());
	if (!Bindings.bHasRegularUniformBuffers)
	{
		int32 LastInfoIndex = 0;
		for (int32 BufferIndex = 0; BufferIndex < RHIUniformBuffers.Num(); ++BufferIndex)
		{
			const FRHIUniformBuffer* RHIUniformBuffer = RHIUniformBuffers[BufferIndex];
			check(RHIUniformBuffer);
			FMetalUniformBuffer* EmulatedUniformBuffer = (FMetalUniformBuffer*)RHIUniformBuffer;
			const uint32* RESTRICT SourceData = (uint32*)((uint8*)[EmulatedUniformBuffer->Buffer contents] + EmulatedUniformBuffer->Offset);//->Data.GetTypedData();
			for (int32 InfoIndex = LastInfoIndex; InfoIndex < UniformBuffersCopyInfo.Num(); ++InfoIndex)
			{
				const FMetalUniformBufferCopyInfo& Info = UniformBuffersCopyInfo[InfoIndex];
				if (Info.SourceUBIndex == BufferIndex)
				{
					float* RESTRICT ScratchMem = (float*)PackedGlobalUniforms[Info.DestUBTypeIndex];
					ScratchMem += Info.DestOffsetInFloats;
					FMemory::Memcpy(ScratchMem, SourceData + Info.SourceOffsetInFloats, Info.SizeInFloats * sizeof(float));
					PackedGlobalUniformDirty[Info.DestUBTypeIndex].LowVector = FMath::Min(PackedGlobalUniformDirty[Info.DestUBTypeIndex].LowVector, uint32(Info.DestOffsetInFloats / SizeOfFloat4));
					PackedGlobalUniformDirty[Info.DestUBTypeIndex].HighVector = FMath::Max(PackedGlobalUniformDirty[Info.DestUBTypeIndex].HighVector, uint32(((Info.DestOffsetInFloats + Info.SizeInFloats) * sizeof(float) + SizeOfFloat4 - 1) / SizeOfFloat4));
				}
				else
				{
					LastInfoIndex = InfoIndex;
					break;
				}
			}
		}
	}
}
