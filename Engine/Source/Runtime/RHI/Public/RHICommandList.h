// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandList.h: RHI Command List definitions for queueing up & executing later.
=============================================================================*/

#pragma once
#include "LockFreeFixedSizeAllocator.h"
#include "TaskGraphInterfaces.h"

DECLARE_STATS_GROUP(TEXT("RHICmdList"), STATGROUP_RHICMDLIST, STATCAT_Advanced);

// set this one to get a stat for each RHI command 
#define RHI_STATS 0

#if RHI_STATS
DECLARE_STATS_GROUP(TEXT("RHICommands"),STATGROUP_RHI_COMMANDS, STATCAT_Advanced);
#define RHISTAT(Method)	DECLARE_SCOPE_CYCLE_COUNTER(TEXT(#Method), STAT_RHI##Method, STATGROUP_RHI_COMMANDS)
#else
#define RHISTAT(Method)
#endif
/** Thread used for RHI */
extern RHI_API bool GEnableAsyncCompute;
extern RHI_API TAutoConsoleVariable<int32> CVarRHICmdWidth;
extern RHI_API TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasks;
class FRHICommandListBase;

enum class ECmdList
{
	EGfx,
	ECompute,	
};

class IRHICommandContextContainer
{
public:
	virtual ~IRHICommandContextContainer()
	{
	}

	virtual IRHICommandContext* GetContext()
	{
		return nullptr;
	}
	virtual void FinishContext()
	{
		check(0);
	}
	virtual void SubmitAndFreeContextContainer(int32 Index, int32 Num)
	{
		check(0);
	}
};

struct FRHICommandBase
{
	FRHICommandBase* Next;
	void(*ExecuteAndDestructPtr)(FRHICommandListBase& CmdList, FRHICommandBase *Cmd);
	FORCEINLINE FRHICommandBase(void(*InExecuteAndDestructPtr)(FRHICommandListBase& CmdList, FRHICommandBase *Cmd))
		: Next(nullptr)
		, ExecuteAndDestructPtr(InExecuteAndDestructPtr)
	{
	}
	FORCEINLINE void CallExecuteAndDestruct(FRHICommandListBase& CmdList)
	{
		ExecuteAndDestructPtr(CmdList, this);
	}
};

// Thread-safe allocator for GPU fences used in deferred command list execution
// Fences are stored in a ringbuffer
class RHI_API FRHICommandListFenceAllocator
{
public:
	static const int MAX_FENCE_INDICES		= 4096;
	FRHICommandListFenceAllocator()
	{
		CurrentFenceIndex = 0;
		for ( int i=0; i<MAX_FENCE_INDICES; i++)
		{
			FenceIDs[i] = 0xffffffffffffffffull;
			FenceFrameNumber[i] = 0xffffffff;
		}
	}

	uint32 AllocFenceIndex()
	{
		check(IsInRenderingThread());
		uint32 FenceIndex = ( FPlatformAtomics::InterlockedIncrement(&CurrentFenceIndex)-1 ) % MAX_FENCE_INDICES;
		check(FenceFrameNumber[FenceIndex] != GFrameNumberRenderThread);
		FenceFrameNumber[FenceIndex] = GFrameNumberRenderThread;

		return FenceIndex;
	}

	volatile uint64& GetFenceID( int32 FenceIndex )
	{
		check( FenceIndex < MAX_FENCE_INDICES );
		return FenceIDs[ FenceIndex ];
	}

private:
	volatile int32 CurrentFenceIndex;
	uint64 FenceIDs[MAX_FENCE_INDICES];
	uint32 FenceFrameNumber[MAX_FENCE_INDICES];
};

extern RHI_API FRHICommandListFenceAllocator GRHIFenceAllocator;

class RHI_API FRHICommandListBase : public FNoncopyable
{
public:

	FRHICommandListBase();
	~FRHICommandListBase();

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

	inline void Flush();
	inline bool IsImmediate();
	inline bool IsImmediateAsyncCompute();

	const int32 GetUsedMemory() const;
	void QueueAsyncCommandListSubmit(FGraphEventRef& AnyThreadCompletionEvent, class FRHICommandList* CmdList);
	void QueueParallelAsyncCommandListSubmit(FGraphEventRef* AnyThreadCompletionEvents, bool bIsPrepass, class FRHICommandList** CmdLists, int32* NumDrawsIfKnown, int32 Num, int32 MinDrawsPerTranslate, bool bSpewMerge);
	void QueueRenderThreadCommandListSubmit(FGraphEventRef& RenderThreadCompletionEvent, class FRHICommandList* CmdList);
	void QueueCommandListSubmit(class FRHICommandList* CmdList);
	void WaitForTasks(bool bKnownToBeComplete = false);
	void WaitForDispatch();
	void WaitForRHIThreadTasks();
	void HandleRTThreadTaskCompletion(const FGraphEventRef& MyCompletionGraphEvent);

	FORCEINLINE_DEBUGGABLE void* Alloc(int32 AllocSize, int32 Alignment)
	{
		return MemManager.Alloc(AllocSize, Alignment);
	}

	template <typename T>
	FORCEINLINE_DEBUGGABLE void* Alloc()
	{
		return Alloc(sizeof(T), ALIGNOF(T));
	}

	template <typename TCmd>
	FORCEINLINE_DEBUGGABLE void* AllocCommand()
	{
		checkSlow(!IsExecuting());
		TCmd* Result = (TCmd*)Alloc<TCmd>();
		++NumCommands;
		*CommandLink = Result;
		CommandLink = &Result->Next;
		return Result;
	}
	FORCEINLINE uint32 GetUID()
	{
		return UID;
	}
	FORCEINLINE bool HasCommands() const
	{
		return (NumCommands > 0);
	}
	FORCEINLINE bool IsExecuting() const
	{
		return bExecuting;
	}

	inline bool Bypass();

	FORCEINLINE void ExchangeCmdList(FRHICommandListBase& Other)
	{
		check(!RTTasks.Num() && !Other.RTTasks.Num());
		FMemory::Memswap(this, &Other, sizeof(FRHICommandListBase));
		if (CommandLink == &Other.Root)
		{
			CommandLink = &Root;
		}
		if (Other.CommandLink == &Root)
		{
			Other.CommandLink = &Other.Root;
		}
	}

	void SetContext(IRHICommandContext* InContext)
	{
		check(InContext);
		Context = InContext;
	}

	FORCEINLINE IRHICommandContext& GetContext()
	{
		checkSlow(Context);
		return *Context;
	}

	void SetComputeContext(IRHIComputeContext* InContext)
	{
		check(InContext);
		ComputeContext = InContext;
	}

	IRHIComputeContext& GetComputeContext()
	{
		checkSlow(ComputeContext);
		return *ComputeContext;
	}

	void CopyContext(FRHICommandListBase& ParentCommandList)
	{
		Context = ParentCommandList.Context;
		ComputeContext = ParentCommandList.ComputeContext;
	}

	struct FDrawUpData
	{

		uint32 PrimitiveType;
		uint32 NumPrimitives;
		uint32 NumVertices;
		uint32 VertexDataStride;
		void* OutVertexData;
		uint32 MinVertexIndex;
		uint32 NumIndices;
		uint32 IndexDataStride;
		void* OutIndexData;

		FDrawUpData()
			: PrimitiveType(PT_Num)
			, OutVertexData(nullptr)
			, OutIndexData(nullptr)
		{
		}
	};

private:
	FRHICommandBase* Root;
	FRHICommandBase** CommandLink;
	bool bExecuting;
	uint32 NumCommands;
	uint32 UID;
	IRHICommandContext* Context;
	IRHIComputeContext* ComputeContext;

	FMemStackBase MemManager; 
	FGraphEventArray RTTasks;

	void Reset();

	friend class FRHICommandListExecutor;
	friend class FRHICommandListIterator;

public:
	TStatId	ExecuteStat;
	enum class ERenderThreadContext
	{
		SceneRenderTargets,
		Num
	};
	void *RenderThreadContexts[(int32)ERenderThreadContext::Num];
	static int32 StateCacheEnabled;
protected:
	struct FRHICommandSetRasterizerState* CachedRasterizerState;
	struct FRHICommandSetDepthStencilState* CachedDepthStencilState;
public:
	void FORCEINLINE FlushStateCache()
	{
		CachedRasterizerState = nullptr;
		CachedDepthStencilState = nullptr;
	}


	void CopyRenderThreadContexts(const FRHICommandListBase& ParentCommandList)
	{
		for (int32 Index = 0; ERenderThreadContext(Index) < ERenderThreadContext::Num; Index++)
		{
			RenderThreadContexts[Index] = ParentCommandList.RenderThreadContexts[Index];
		}
	}
	void SetRenderThreadContext(void* InContext, ERenderThreadContext Slot)
	{
		RenderThreadContexts[int32(Slot)] = InContext;
	}
	FORCEINLINE void* GetRenderThreadContext(ERenderThreadContext Slot)
	{
		return RenderThreadContexts[int32(Slot)];
	}


	FDrawUpData DrawUPData; 
};


template<typename TCmd>
struct FRHICommand : public FRHICommandBase
{
	FORCEINLINE FRHICommand()
		: FRHICommandBase(&ExecuteAndDestruct)
	{

	}
	static FORCEINLINE void ExecuteAndDestruct(FRHICommandListBase& CmdList, FRHICommandBase *Cmd)
	{
		TCmd *ThisCmd = (TCmd*)Cmd;
		ThisCmd->Execute(CmdList);
		ThisCmd->~TCmd();
	}
};

struct FRHICommandSetRasterizerState : public FRHICommand<FRHICommandSetRasterizerState>
{
	FRasterizerStateRHIParamRef State;
	FORCEINLINE_DEBUGGABLE FRHICommandSetRasterizerState(FRasterizerStateRHIParamRef InState)
		: State(InState)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetDepthStencilState : public FRHICommand<FRHICommandSetDepthStencilState>
{
	FDepthStencilStateRHIParamRef State;
	uint32 StencilRef;
	FORCEINLINE_DEBUGGABLE FRHICommandSetDepthStencilState(FDepthStencilStateRHIParamRef InState, uint32 InStencilRef)
		: State(InState)
		, StencilRef(InStencilRef)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderParameter : public FRHICommand<FRHICommandSetShaderParameter<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	const void* NewValue;
	uint32 BufferIndex;
	uint32 BaseIndex;
	uint32 NumBytes;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderParameter(TShaderRHIParamRef InShader, uint32 InBufferIndex, uint32 InBaseIndex, uint32 InNumBytes, const void* InNewValue)
		: Shader(InShader)
		, NewValue(InNewValue)
		, BufferIndex(InBufferIndex)
		, BaseIndex(InBaseIndex)
		, NumBytes(InNumBytes)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderUniformBuffer : public FRHICommand<FRHICommandSetShaderUniformBuffer<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 BaseIndex;
	FUniformBufferRHIParamRef UniformBuffer;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderUniformBuffer(TShaderRHIParamRef InShader, uint32 InBaseIndex, FUniformBufferRHIParamRef InUniformBuffer)
		: Shader(InShader)
		, BaseIndex(InBaseIndex)
		, UniformBuffer(InUniformBuffer)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderTexture : public FRHICommand<FRHICommandSetShaderTexture<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 TextureIndex;
	FTextureRHIParamRef Texture;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderTexture(TShaderRHIParamRef InShader, uint32 InTextureIndex, FTextureRHIParamRef InTexture)
		: Shader(InShader)
		, TextureIndex(InTextureIndex)
		, Texture(InTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderResourceViewParameter : public FRHICommand<FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 SamplerIndex;
	FShaderResourceViewRHIParamRef SRV;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderResourceViewParameter(TShaderRHIParamRef InShader, uint32 InSamplerIndex, FShaderResourceViewRHIParamRef InSRV)
		: Shader(InShader)
		, SamplerIndex(InSamplerIndex)
		, SRV(InSRV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetUAVParameter : public FRHICommand<FRHICommandSetUAVParameter<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 UAVIndex;
	FUnorderedAccessViewRHIParamRef UAV;
	FORCEINLINE_DEBUGGABLE FRHICommandSetUAVParameter(TShaderRHIParamRef InShader, uint32 InUAVIndex, FUnorderedAccessViewRHIParamRef InUAV)
		: Shader(InShader)
		, UAVIndex(InUAVIndex)
		, UAV(InUAV)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetUAVParameter_IntialCount : public FRHICommand<FRHICommandSetUAVParameter_IntialCount<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 UAVIndex;
	FUnorderedAccessViewRHIParamRef UAV;
	uint32 InitialCount;
	FORCEINLINE_DEBUGGABLE FRHICommandSetUAVParameter_IntialCount(TShaderRHIParamRef InShader, uint32 InUAVIndex, FUnorderedAccessViewRHIParamRef InUAV, uint32 InInitialCount)
		: Shader(InShader)
		, UAVIndex(InUAVIndex)
		, UAV(InUAV)
		, InitialCount(InInitialCount)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef, ECmdList CmdListType>
struct FRHICommandSetShaderSampler : public FRHICommand<FRHICommandSetShaderSampler<TShaderRHIParamRef, CmdListType> >
{
	TShaderRHIParamRef Shader;
	uint32 SamplerIndex;
	FSamplerStateRHIParamRef Sampler;
	FORCEINLINE_DEBUGGABLE FRHICommandSetShaderSampler(TShaderRHIParamRef InShader, uint32 InSamplerIndex, FSamplerStateRHIParamRef InSampler)
		: Shader(InShader)
		, SamplerIndex(InSamplerIndex)
		, Sampler(InSampler)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDrawPrimitive : public FRHICommand<FRHICommandDrawPrimitive>
{
	uint32 PrimitiveType;
	uint32 BaseVertexIndex;
	uint32 NumPrimitives;
	uint32 NumInstances;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawPrimitive(uint32 InPrimitiveType, uint32 InBaseVertexIndex, uint32 InNumPrimitives, uint32 InNumInstances)
		: PrimitiveType(InPrimitiveType)
		, BaseVertexIndex(InBaseVertexIndex)
		, NumPrimitives(InNumPrimitives)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDrawIndexedPrimitive : public FRHICommand<FRHICommandDrawIndexedPrimitive>
{
	FIndexBufferRHIParamRef IndexBuffer;
	uint32 PrimitiveType;
	int32 BaseVertexIndex;
	uint32 FirstInstance;
	uint32 NumVertices;
	uint32 StartIndex;
	uint32 NumPrimitives;
	uint32 NumInstances;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawIndexedPrimitive(FIndexBufferRHIParamRef InIndexBuffer, uint32 InPrimitiveType, int32 InBaseVertexIndex, uint32 InFirstInstance, uint32 InNumVertices, uint32 InStartIndex, uint32 InNumPrimitives, uint32 InNumInstances)
		: IndexBuffer(InIndexBuffer)
		, PrimitiveType(InPrimitiveType)
		, BaseVertexIndex(InBaseVertexIndex)
		, FirstInstance(InFirstInstance)
		, NumVertices(InNumVertices)
		, StartIndex(InStartIndex)
		, NumPrimitives(InNumPrimitives)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetBoundShaderState : public FRHICommand<FRHICommandSetBoundShaderState>
{
	FBoundShaderStateRHIParamRef BoundShaderState;
	FORCEINLINE_DEBUGGABLE FRHICommandSetBoundShaderState(FBoundShaderStateRHIParamRef InBoundShaderState)
		: BoundShaderState(InBoundShaderState)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetBlendState : public FRHICommand<FRHICommandSetBlendState>
{
	FBlendStateRHIParamRef State;
	FLinearColor BlendFactor;
	FORCEINLINE_DEBUGGABLE FRHICommandSetBlendState(FBlendStateRHIParamRef InState, const FLinearColor& InBlendFactor)
		: State(InState)
		, BlendFactor(InBlendFactor)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetStreamSource : public FRHICommand<FRHICommandSetStreamSource>
{
	uint32 StreamIndex;
	FVertexBufferRHIParamRef VertexBuffer;
	uint32 Stride;
	uint32 Offset;
	FORCEINLINE_DEBUGGABLE FRHICommandSetStreamSource(uint32 InStreamIndex, FVertexBufferRHIParamRef InVertexBuffer, uint32 InStride, uint32 InOffset)
		: StreamIndex(InStreamIndex)
		, VertexBuffer(InVertexBuffer)
		, Stride(InStride)
		, Offset(InOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetViewport : public FRHICommand<FRHICommandSetViewport>
{
	uint32 MinX;
	uint32 MinY;
	float MinZ;
	uint32 MaxX;
	uint32 MaxY;
	float MaxZ;
	FORCEINLINE_DEBUGGABLE FRHICommandSetViewport(uint32 InMinX, uint32 InMinY, float InMinZ, uint32 InMaxX, uint32 InMaxY, float InMaxZ)
		: MinX(InMinX)
		, MinY(InMinY)
		, MinZ(InMinZ)
		, MaxX(InMaxX)
		, MaxY(InMaxY)
		, MaxZ(InMaxZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetStereoViewport : public FRHICommand<FRHICommandSetStereoViewport>
{
	uint32 LeftMinX;
	uint32 RightMinX;
	uint32 MinY;
	float MinZ;
	uint32 LeftMaxX;
	uint32 RightMaxX;
	uint32 MaxY;
	float MaxZ;
	FORCEINLINE_DEBUGGABLE FRHICommandSetStereoViewport(uint32 InLeftMinX, uint32 InRightMinX, uint32 InMinY, float InMinZ, uint32 InLeftMaxX, uint32 InRightMaxX, uint32 InMaxY, float InMaxZ)
		: LeftMinX(InLeftMinX)
		, RightMinX(InRightMinX)
		, MinY(InMinY)
		, MinZ(InMinZ)
		, LeftMaxX(InLeftMaxX)
		, RightMaxX(InRightMaxX)
		, MaxY(InMaxY)
		, MaxZ(InMaxZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetScissorRect : public FRHICommand<FRHICommandSetScissorRect>
{
	bool bEnable;
	uint32 MinX;
	uint32 MinY;
	uint32 MaxX;
	uint32 MaxY;
	FORCEINLINE_DEBUGGABLE FRHICommandSetScissorRect(bool InbEnable, uint32 InMinX, uint32 InMinY, uint32 InMaxX, uint32 InMaxY)
		: bEnable(InbEnable)
		, MinX(InMinX)
		, MinY(InMinY)
		, MaxX(InMaxX)
		, MaxY(InMaxY)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetRenderTargets : public FRHICommand<FRHICommandSetRenderTargets>
{
	uint32 NewNumSimultaneousRenderTargets;
	FRHIRenderTargetView NewRenderTargetsRHI[MaxSimultaneousRenderTargets];
	FRHIDepthRenderTargetView NewDepthStencilTarget;
	uint32 NewNumUAVs;
	FUnorderedAccessViewRHIParamRef UAVs[MaxSimultaneousUAVs];

	FORCEINLINE_DEBUGGABLE FRHICommandSetRenderTargets(
		uint32 InNewNumSimultaneousRenderTargets,
		const FRHIRenderTargetView* InNewRenderTargetsRHI,
		const FRHIDepthRenderTargetView* InNewDepthStencilTargetRHI,
		uint32 InNewNumUAVs,
		const FUnorderedAccessViewRHIParamRef* InUAVs
		)
		: NewNumSimultaneousRenderTargets(InNewNumSimultaneousRenderTargets)
		, NewNumUAVs(InNewNumUAVs)

	{
		check(InNewNumSimultaneousRenderTargets <= MaxSimultaneousRenderTargets && InNewNumUAVs <= MaxSimultaneousUAVs);
		for (uint32 Index = 0; Index < NewNumSimultaneousRenderTargets; Index++)
		{
			NewRenderTargetsRHI[Index] = InNewRenderTargetsRHI[Index];
		}
		for (uint32 Index = 0; Index < NewNumUAVs; Index++)
		{
			UAVs[Index] = InUAVs[Index];
		}
		if (InNewDepthStencilTargetRHI)
		{
			NewDepthStencilTarget = *InNewDepthStencilTargetRHI;
		}		
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetRenderTargetsAndClear : public FRHICommand<FRHICommandSetRenderTargetsAndClear>
{
	FRHISetRenderTargetsInfo RenderTargetsInfo;

	FORCEINLINE_DEBUGGABLE FRHICommandSetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& InRenderTargetsInfo) :
		RenderTargetsInfo(InRenderTargetsInfo)
	{
	}

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBindClearMRTValues : public FRHICommand<FRHICommandBindClearMRTValues>
{
	bool bClearColor;
	bool bClearDepth;
	bool bClearStencil;

	FORCEINLINE_DEBUGGABLE FRHICommandBindClearMRTValues(
		bool InbClearColor,
		bool InbClearDepth,
		bool InbClearStencil
		) 
		: bClearColor(InbClearColor)
		, bClearDepth(InbClearDepth)
		, bClearStencil(InbClearStencil)
	{
	}	

	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndDrawPrimitiveUP : public FRHICommand<FRHICommandEndDrawPrimitiveUP>
{
	uint32 PrimitiveType;
	uint32 NumPrimitives;
	uint32 NumVertices;
	uint32 VertexDataStride;
	void* OutVertexData;

	FORCEINLINE_DEBUGGABLE FRHICommandEndDrawPrimitiveUP(uint32 InPrimitiveType, uint32 InNumPrimitives, uint32 InNumVertices, uint32 InVertexDataStride, void* InOutVertexData)
		: PrimitiveType(InPrimitiveType)
		, NumPrimitives(InNumPrimitives)
		, NumVertices(InNumVertices)
		, VertexDataStride(InVertexDataStride)
		, OutVertexData(InOutVertexData)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};


struct FRHICommandEndDrawIndexedPrimitiveUP : public FRHICommand<FRHICommandEndDrawIndexedPrimitiveUP>
{
	uint32 PrimitiveType;
	uint32 NumPrimitives;
	uint32 NumVertices;
	uint32 VertexDataStride;
	void* OutVertexData;
	uint32 MinVertexIndex;
	uint32 NumIndices;
	uint32 IndexDataStride;
	void* OutIndexData;

	FORCEINLINE_DEBUGGABLE FRHICommandEndDrawIndexedPrimitiveUP(uint32 InPrimitiveType, uint32 InNumPrimitives, uint32 InNumVertices, uint32 InVertexDataStride, void* InOutVertexData, uint32 InMinVertexIndex, uint32 InNumIndices, uint32 InIndexDataStride, void* InOutIndexData)
		: PrimitiveType(InPrimitiveType)
		, NumPrimitives(InNumPrimitives)
		, NumVertices(InNumVertices)
		, VertexDataStride(InVertexDataStride)
		, OutVertexData(InOutVertexData)
		, MinVertexIndex(InMinVertexIndex)
		, NumIndices(InNumIndices)
		, IndexDataStride(InIndexDataStride)
		, OutIndexData(InOutIndexData)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandSetComputeShader : public FRHICommand<FRHICommandSetComputeShader<CmdListType>>
{
	FComputeShaderRHIParamRef ComputeShader;
	FORCEINLINE_DEBUGGABLE FRHICommandSetComputeShader(FComputeShaderRHIParamRef InComputeShader)
		: ComputeShader(InComputeShader)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandDispatchComputeShader : public FRHICommand<FRHICommandDispatchComputeShader<CmdListType>>
{
	uint32 ThreadGroupCountX;
	uint32 ThreadGroupCountY;
	uint32 ThreadGroupCountZ;
	FORCEINLINE_DEBUGGABLE FRHICommandDispatchComputeShader(uint32 InThreadGroupCountX, uint32 InThreadGroupCountY, uint32 InThreadGroupCountZ)
		: ThreadGroupCountX(InThreadGroupCountX)
		, ThreadGroupCountY(InThreadGroupCountY)
		, ThreadGroupCountZ(InThreadGroupCountZ)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandDispatchIndirectComputeShader : public FRHICommand<FRHICommandDispatchIndirectComputeShader<CmdListType>>
{
	FVertexBufferRHIParamRef ArgumentBuffer;
	uint32 ArgumentOffset;
	FORCEINLINE_DEBUGGABLE FRHICommandDispatchIndirectComputeShader(FVertexBufferRHIParamRef InArgumentBuffer, uint32 InArgumentOffset)
		: ArgumentBuffer(InArgumentBuffer)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandAutomaticCacheFlushAfterComputeShader : public FRHICommand<FRHICommandAutomaticCacheFlushAfterComputeShader>
{
	bool bEnable;
	FORCEINLINE_DEBUGGABLE FRHICommandAutomaticCacheFlushAfterComputeShader(bool InbEnable)
		: bEnable(InbEnable)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandFlushComputeShaderCache : public FRHICommand<FRHICommandFlushComputeShaderCache>
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDrawPrimitiveIndirect : public FRHICommand<FRHICommandDrawPrimitiveIndirect>
{
	FVertexBufferRHIParamRef ArgumentBuffer;
	uint32 PrimitiveType;
	uint32 ArgumentOffset;
	FORCEINLINE_DEBUGGABLE FRHICommandDrawPrimitiveIndirect(uint32 InPrimitiveType, FVertexBufferRHIParamRef InArgumentBuffer, uint32 InArgumentOffset)
		: ArgumentBuffer(InArgumentBuffer)
		, PrimitiveType(InPrimitiveType)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDrawIndexedIndirect : public FRHICommand<FRHICommandDrawIndexedIndirect>
{
	FIndexBufferRHIParamRef IndexBufferRHI;
	uint32 PrimitiveType;
	FStructuredBufferRHIParamRef ArgumentsBufferRHI;
	uint32 DrawArgumentsIndex;
	uint32 NumInstances;

	FORCEINLINE_DEBUGGABLE FRHICommandDrawIndexedIndirect(FIndexBufferRHIParamRef InIndexBufferRHI, uint32 InPrimitiveType, FStructuredBufferRHIParamRef InArgumentsBufferRHI, uint32 InDrawArgumentsIndex, uint32 InNumInstances)
		: IndexBufferRHI(InIndexBufferRHI)
		, PrimitiveType(InPrimitiveType)
		, ArgumentsBufferRHI(InArgumentsBufferRHI)
		, DrawArgumentsIndex(InDrawArgumentsIndex)
		, NumInstances(InNumInstances)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDrawIndexedPrimitiveIndirect : public FRHICommand<FRHICommandDrawIndexedPrimitiveIndirect>
{
	FIndexBufferRHIParamRef IndexBuffer;
	FVertexBufferRHIParamRef ArgumentsBuffer;
	uint32 PrimitiveType;
	uint32 ArgumentOffset;

	FORCEINLINE_DEBUGGABLE FRHICommandDrawIndexedPrimitiveIndirect(uint32 InPrimitiveType, FIndexBufferRHIParamRef InIndexBuffer, FVertexBufferRHIParamRef InArgumentsBuffer, uint32 InArgumentOffset)
		: IndexBuffer(InIndexBuffer)
		, ArgumentsBuffer(InArgumentsBuffer)
		, PrimitiveType(InPrimitiveType)
		, ArgumentOffset(InArgumentOffset)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEnableDepthBoundsTest : public FRHICommand<FRHICommandEnableDepthBoundsTest>
{
	bool bEnable;
	float MinDepth;
	float MaxDepth;

	FORCEINLINE_DEBUGGABLE FRHICommandEnableDepthBoundsTest(bool InbEnable, float InMinDepth, float InMaxDepth)
		: bEnable(InbEnable)
		, MinDepth(InMinDepth)
		, MaxDepth(InMaxDepth)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandClearUAV : public FRHICommand<FRHICommandClearUAV>
{
	FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI;
	uint32 Values[4];

	FORCEINLINE_DEBUGGABLE FRHICommandClearUAV(FUnorderedAccessViewRHIParamRef InUnorderedAccessViewRHI, const uint32* InValues)
		: UnorderedAccessViewRHI(InUnorderedAccessViewRHI)
	{
		Values[0] = InValues[0];
		Values[1] = InValues[1];
		Values[2] = InValues[2];
		Values[3] = InValues[3];
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandCopyToResolveTarget : public FRHICommand<FRHICommandCopyToResolveTarget>
{
	FResolveParams ResolveParams;
	FTextureRHIParamRef SourceTexture;
	FTextureRHIParamRef DestTexture;
	bool bKeepOriginalSurface;

	FORCEINLINE_DEBUGGABLE FRHICommandCopyToResolveTarget(FTextureRHIParamRef InSourceTexture, FTextureRHIParamRef InDestTexture, bool InbKeepOriginalSurface, const FResolveParams& InResolveParams)
		: ResolveParams(InResolveParams)
		, SourceTexture(InSourceTexture)
		, DestTexture(InDestTexture)
		, bKeepOriginalSurface(InbKeepOriginalSurface)
	{
		ensure(SourceTexture);
		ensure(DestTexture);
		ensure(SourceTexture->GetTexture2D() || SourceTexture->GetTexture3D() || SourceTexture->GetTextureCube());
		ensure(DestTexture->GetTexture2D() || DestTexture->GetTexture3D() || DestTexture->GetTextureCube());
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandTransitionTextures : public FRHICommand<FRHICommandTransitionTextures>
{
	static const int32 MaxTexturesToTransition = 16;
	int32 NumTextures;
	FTextureRHIParamRef Textures[MaxTexturesToTransition];
	EResourceTransitionAccess TransitionType;
	FORCEINLINE_DEBUGGABLE FRHICommandTransitionTextures(EResourceTransitionAccess InTransitionType, FTextureRHIParamRef* InTextures, int32 InNumTextures)
		: NumTextures(InNumTextures)
		, TransitionType(InTransitionType)
	{
		for (int32 i = 0; i < NumTextures; ++i)
		{
			Textures[i] = InTextures[i];
		}
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandTransitionTexturesArray : public FRHICommand<FRHICommandTransitionTexturesArray>
{	
	TArray<FTextureRHIParamRef>& Textures;
	EResourceTransitionAccess TransitionType;
	FORCEINLINE_DEBUGGABLE FRHICommandTransitionTexturesArray(EResourceTransitionAccess InTransitionType, TArray<FTextureRHIParamRef>& InTextures)
		: Textures(InTextures)
		, TransitionType(InTransitionType)
	{		
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandTransitionUAVs : public FRHICommand<FRHICommandTransitionUAVs<CmdListType>>
{
	static const int32 MaxUAVsToTransition = 16;
	int32 NumUAVs;
	FUnorderedAccessViewRHIParamRef UAVs[MaxUAVsToTransition];
	EResourceTransitionAccess TransitionType;
	EResourceTransitionPipeline TransitionPipeline;
	FComputeFenceRHIParamRef WriteFence;

		FORCEINLINE_DEBUGGABLE FRHICommandTransitionUAVs(EResourceTransitionAccess InTransitionType, EResourceTransitionPipeline InTransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 InNumUAVs, FComputeFenceRHIParamRef InWriteFence)
		: NumUAVs(InNumUAVs)
		, TransitionType(InTransitionType)
		, TransitionPipeline(InTransitionPipeline)
		, WriteFence(InWriteFence)
	{
		for (int32 i = 0; i < NumUAVs; ++i)
		{
			UAVs[i] = InUAVs[i];
		}
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandSetAsyncComputeBudget : public FRHICommand<FRHICommandSetAsyncComputeBudget<CmdListType>>
{
	EAsyncComputeBudget Budget;

	FORCEINLINE_DEBUGGABLE FRHICommandSetAsyncComputeBudget(EAsyncComputeBudget InBudget)
		: Budget(InBudget)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandWaitComputeFence : public FRHICommand<FRHICommandWaitComputeFence<CmdListType>>
{
	FComputeFenceRHIParamRef WaitFence;

	FORCEINLINE_DEBUGGABLE FRHICommandWaitComputeFence(FComputeFenceRHIParamRef InWaitFence)
		: WaitFence(InWaitFence)
	{		
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandClear : public FRHICommand<FRHICommandClear>
{
	FLinearColor Color;
	FIntRect ExcludeRect;
	float Depth;
	uint32 Stencil;
	bool bClearColor;
	bool bClearDepth;
	bool bClearStencil;

	FORCEINLINE_DEBUGGABLE FRHICommandClear(
		bool InbClearColor,
		const FLinearColor& InColor,
		bool InbClearDepth,
		float InDepth,
		bool InbClearStencil,
		uint32 InStencil,
		FIntRect InExcludeRect
		)
		: Color(InColor)
		, ExcludeRect(InExcludeRect)
		, Depth(InDepth)
		, Stencil(InStencil)
		, bClearColor(InbClearColor)
		, bClearDepth(InbClearDepth)
		, bClearStencil(InbClearStencil)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandClearMRT : public FRHICommand<FRHICommandClearMRT>
{
	FLinearColor ColorArray[MaxSimultaneousRenderTargets];
	FIntRect ExcludeRect;
	float Depth;
	uint32 Stencil;
	int32 NumClearColors;
	bool bClearColor;
	bool bClearDepth;
	bool bClearStencil;

	FORCEINLINE_DEBUGGABLE FRHICommandClearMRT(
		bool InbClearColor,
		int32 InNumClearColors,
		const FLinearColor* InColorArray,
		bool InbClearDepth,
		float InDepth,
		bool InbClearStencil,
		uint32 InStencil,
		FIntRect InExcludeRect
		)
		: ExcludeRect(InExcludeRect)
		, Depth(InDepth)
		, Stencil(InStencil)
		, NumClearColors(InNumClearColors)
		, bClearColor(InbClearColor)
		, bClearDepth(InbClearDepth)
		, bClearStencil(InbClearStencil)
	{
		check(InNumClearColors <= MaxSimultaneousRenderTargets);
		for (int32 Index = 0; Index < InNumClearColors; Index++)
		{
			ColorArray[Index] = InColorArray[Index];
		}
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};


struct FBoundShaderStateInput
{
	FVertexDeclarationRHIParamRef VertexDeclarationRHI;
	FVertexShaderRHIParamRef VertexShaderRHI;
	FHullShaderRHIParamRef HullShaderRHI;
	FDomainShaderRHIParamRef DomainShaderRHI;
	FPixelShaderRHIParamRef PixelShaderRHI;
	FGeometryShaderRHIParamRef GeometryShaderRHI;

	FORCEINLINE FBoundShaderStateInput()
	{
	}

	FORCEINLINE FBoundShaderStateInput(FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
		FVertexShaderRHIParamRef InVertexShaderRHI,
		FHullShaderRHIParamRef InHullShaderRHI,
		FDomainShaderRHIParamRef InDomainShaderRHI,
		FPixelShaderRHIParamRef InPixelShaderRHI,
		FGeometryShaderRHIParamRef InGeometryShaderRHI
		)
		: VertexDeclarationRHI(InVertexDeclarationRHI)
		, VertexShaderRHI(InVertexShaderRHI)
		, HullShaderRHI(InHullShaderRHI)
		, DomainShaderRHI(InDomainShaderRHI)
		, PixelShaderRHI(InPixelShaderRHI)
		, GeometryShaderRHI(InGeometryShaderRHI)
	{
	}
};

struct FComputedBSS
{
	FBoundShaderStateRHIRef BSS;
	int32 UseCount;
	FComputedBSS()
		: UseCount(0)
	{
	}
};

struct FLocalBoundShaderStateWorkArea
{
	FBoundShaderStateInput Args;
	FComputedBSS* ComputedBSS;
#if DO_CHECK // the below variables are used in check(), which can be enabled in Shipping builds (see Build.h)
	FRHICommandListBase* CheckCmdList;
	int32 UID;
#endif

	FORCEINLINE_DEBUGGABLE FLocalBoundShaderStateWorkArea(
		FRHICommandListBase* InCheckCmdList,
		FVertexDeclarationRHIParamRef VertexDeclarationRHI,
		FVertexShaderRHIParamRef VertexShaderRHI,
		FHullShaderRHIParamRef HullShaderRHI,
		FDomainShaderRHIParamRef DomainShaderRHI,
		FPixelShaderRHIParamRef PixelShaderRHI,
		FGeometryShaderRHIParamRef GeometryShaderRHI
		)
		: Args(VertexDeclarationRHI, VertexShaderRHI, HullShaderRHI, DomainShaderRHI, PixelShaderRHI, GeometryShaderRHI)
#if DO_CHECK
		, CheckCmdList(InCheckCmdList)
		, UID(InCheckCmdList->GetUID())
#endif
	{
		ComputedBSS = new (InCheckCmdList->Alloc<FComputedBSS>()) FComputedBSS;
	}
};

struct FLocalBoundShaderState
{
	FLocalBoundShaderStateWorkArea* WorkArea;
	FBoundShaderStateRHIRef BypassBSS; // this is only used in the case of Bypass, should eventually be deleted
	FLocalBoundShaderState()
		: WorkArea(nullptr)
	{
	}
	FLocalBoundShaderState(const FLocalBoundShaderState& Other)
		: WorkArea(Other.WorkArea)
		, BypassBSS(Other.BypassBSS)
	{
	}
};

struct FRHICommandBuildLocalBoundShaderState : public FRHICommand<FRHICommandBuildLocalBoundShaderState>
{
	FLocalBoundShaderStateWorkArea WorkArea;

	FORCEINLINE_DEBUGGABLE FRHICommandBuildLocalBoundShaderState(
		FRHICommandListBase* CheckCmdList,
		FVertexDeclarationRHIParamRef VertexDeclarationRHI,
		FVertexShaderRHIParamRef VertexShaderRHI,
		FHullShaderRHIParamRef HullShaderRHI,
		FDomainShaderRHIParamRef DomainShaderRHI,
		FPixelShaderRHIParamRef PixelShaderRHI,
		FGeometryShaderRHIParamRef GeometryShaderRHI
		)
		: WorkArea(CheckCmdList, VertexDeclarationRHI, VertexShaderRHI, HullShaderRHI, DomainShaderRHI, PixelShaderRHI, GeometryShaderRHI)

	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandSetLocalBoundShaderState : public FRHICommand<FRHICommandSetLocalBoundShaderState>
{
	FLocalBoundShaderState LocalBoundShaderState;
	FORCEINLINE_DEBUGGABLE FRHICommandSetLocalBoundShaderState(FRHICommandListBase* CheckCmdList, FLocalBoundShaderState& InLocalBoundShaderState)
		: LocalBoundShaderState(InLocalBoundShaderState)
	{
		check(CheckCmdList == LocalBoundShaderState.WorkArea->CheckCmdList && CheckCmdList->GetUID() == LocalBoundShaderState.WorkArea->UID); // this BSS was not built for this particular commandlist
		LocalBoundShaderState.WorkArea->ComputedBSS->UseCount++;
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};


struct FComputedUniformBuffer
{
	FUniformBufferRHIRef UniformBuffer;
	mutable int32 UseCount;
	FComputedUniformBuffer()
		: UseCount(0)
	{
	}
};


struct FLocalUniformBufferWorkArea
{
	void* Contents;
	const FRHIUniformBufferLayout* Layout;
	FComputedUniformBuffer* ComputedUniformBuffer;
#if DO_CHECK // the below variables are used in check(), which can be enabled in Shipping builds (see Build.h)
	FRHICommandListBase* CheckCmdList;
	int32 UID;
#endif

	FLocalUniformBufferWorkArea(FRHICommandListBase* InCheckCmdList, const void* InContents, uint32 ContentsSize, const FRHIUniformBufferLayout* InLayout)
		: Layout(InLayout)
#if DO_CHECK
		, CheckCmdList(InCheckCmdList)
		, UID(InCheckCmdList->GetUID())
#endif
	{
		check(ContentsSize);
		Contents = InCheckCmdList->Alloc(ContentsSize, UNIFORM_BUFFER_STRUCT_ALIGNMENT);
		FMemory::Memcpy(Contents, InContents, ContentsSize);
		ComputedUniformBuffer = new (InCheckCmdList->Alloc<FComputedUniformBuffer>()) FComputedUniformBuffer;
	}
};

struct FLocalUniformBuffer
{
	FLocalUniformBufferWorkArea* WorkArea;
	FUniformBufferRHIRef BypassUniform; // this is only used in the case of Bypass, should eventually be deleted
	FLocalUniformBuffer()
		: WorkArea(nullptr)
	{
	}
	FLocalUniformBuffer(const FLocalUniformBuffer& Other)
		: WorkArea(Other.WorkArea)
		, BypassUniform(Other.BypassUniform)
	{
	}
	FORCEINLINE_DEBUGGABLE bool IsValid() const
	{
		return WorkArea || IsValidRef(BypassUniform);
	}
};

struct FRHICommandBuildLocalUniformBuffer : public FRHICommand<FRHICommandBuildLocalUniformBuffer>
{
	FLocalUniformBufferWorkArea WorkArea;
	FORCEINLINE_DEBUGGABLE FRHICommandBuildLocalUniformBuffer(
		FRHICommandListBase* CheckCmdList,
		const void* Contents,
		uint32 ContentsSize,
		const FRHIUniformBufferLayout& Layout
		)
		: WorkArea(CheckCmdList, Contents, ContentsSize, &Layout)

	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template <typename TShaderRHIParamRef>
struct FRHICommandSetLocalUniformBuffer : public FRHICommand<FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef> >
{
	TShaderRHIParamRef Shader;
	uint32 BaseIndex;
	FLocalUniformBuffer LocalUniformBuffer;
	FORCEINLINE_DEBUGGABLE FRHICommandSetLocalUniformBuffer(FRHICommandListBase* CheckCmdList, TShaderRHIParamRef InShader, uint32 InBaseIndex, const FLocalUniformBuffer& InLocalUniformBuffer)
		: Shader(InShader)
		, BaseIndex(InBaseIndex)
		, LocalUniformBuffer(InLocalUniformBuffer)

	{
		check(CheckCmdList == LocalUniformBuffer.WorkArea->CheckCmdList && CheckCmdList->GetUID() == LocalUniformBuffer.WorkArea->UID); // this uniform buffer was not built for this particular commandlist
		LocalUniformBuffer.WorkArea->ComputedUniformBuffer->UseCount++;
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginRenderQuery : public FRHICommand<FRHICommandBeginRenderQuery>
{
	FRenderQueryRHIParamRef RenderQuery;

	FORCEINLINE_DEBUGGABLE FRHICommandBeginRenderQuery(FRenderQueryRHIParamRef InRenderQuery)
		: RenderQuery(InRenderQuery)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndRenderQuery : public FRHICommand<FRHICommandEndRenderQuery>
{
	FRenderQueryRHIParamRef RenderQuery;

	FORCEINLINE_DEBUGGABLE FRHICommandEndRenderQuery(FRenderQueryRHIParamRef InRenderQuery)
		: RenderQuery(InRenderQuery)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginOcclusionQueryBatch : public FRHICommand<FRHICommandBeginOcclusionQueryBatch>
{
	FORCEINLINE_DEBUGGABLE FRHICommandBeginOcclusionQueryBatch()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndOcclusionQueryBatch : public FRHICommand<FRHICommandEndOcclusionQueryBatch>
{
	FORCEINLINE_DEBUGGABLE FRHICommandEndOcclusionQueryBatch()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandSubmitCommandsHint : public FRHICommand<FRHICommandSubmitCommandsHint<CmdListType>>
{
	FORCEINLINE_DEBUGGABLE FRHICommandSubmitCommandsHint()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginScene : public FRHICommand<FRHICommandBeginScene>
{
	FORCEINLINE_DEBUGGABLE FRHICommandBeginScene()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndScene : public FRHICommand<FRHICommandEndScene>
{
	FORCEINLINE_DEBUGGABLE FRHICommandEndScene()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginFrame : public FRHICommand<FRHICommandBeginFrame>
{
	FORCEINLINE_DEBUGGABLE FRHICommandBeginFrame()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndFrame : public FRHICommand<FRHICommandEndFrame>
{
	FORCEINLINE_DEBUGGABLE FRHICommandEndFrame()
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandBeginDrawingViewport : public FRHICommand<FRHICommandBeginDrawingViewport>
{
	FViewportRHIParamRef Viewport;
	FTextureRHIParamRef RenderTargetRHI;

	FORCEINLINE_DEBUGGABLE FRHICommandBeginDrawingViewport(FViewportRHIParamRef InViewport, FTextureRHIParamRef InRenderTargetRHI)
		: Viewport(InViewport)
		, RenderTargetRHI(InRenderTargetRHI)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandEndDrawingViewport : public FRHICommand<FRHICommandEndDrawingViewport>
{
	FViewportRHIParamRef Viewport;
	bool bPresent;
	bool bLockToVsync;

	FORCEINLINE_DEBUGGABLE FRHICommandEndDrawingViewport(FViewportRHIParamRef InViewport, bool InbPresent, bool InbLockToVsync)
		: Viewport(InViewport)
		, bPresent(InbPresent)
		, bLockToVsync(InbLockToVsync)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandPushEvent : public FRHICommand<FRHICommandPushEvent<CmdListType>>
{
	const TCHAR *Name;
	FColor Color;

	FORCEINLINE_DEBUGGABLE FRHICommandPushEvent(const TCHAR *InName, FColor InColor)
		: Name(InName)
		, Color(InColor)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

template<ECmdList CmdListType>
struct FRHICommandPopEvent : public FRHICommand<FRHICommandPopEvent<CmdListType>>
{
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDebugBreak : public FRHICommand<FRHICommandDebugBreak>
{
	void Execute(FRHICommandListBase& CmdList)
	{
		if (FPlatformMisc::IsDebuggerPresent())
		{
			FPlatformMisc::DebugBreak();
		}
	}
};

struct FRHICommandUpdateTextureReference : public FRHICommand<FRHICommandUpdateTextureReference>
{
	FTextureReferenceRHIParamRef TextureRef;
	FTextureRHIParamRef NewTexture;
	FORCEINLINE_DEBUGGABLE FRHICommandUpdateTextureReference(FTextureReferenceRHIParamRef InTextureRef, FTextureRHIParamRef InNewTexture)
		: TextureRef(InTextureRef)
		, NewTexture(InNewTexture)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};


#define CMD_CONTEXT(Method) GetContext().Method
#define COMPUTE_CONTEXT(Method) GetComputeContext().Method

template<> void FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);
template<> void FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);
template<> void FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);
template<> void FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);
template<> void FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::ECompute>::Execute(FRHICommandListBase& CmdList);

class RHI_API FRHICommandList : public FRHICommandListBase
{
public:

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

	FORCEINLINE_DEBUGGABLE FLocalBoundShaderState BuildLocalBoundShaderState(const FBoundShaderStateInput& BoundShaderStateInput)
	{
		return BuildLocalBoundShaderState(
			BoundShaderStateInput.VertexDeclarationRHI,
			BoundShaderStateInput.VertexShaderRHI,
			BoundShaderStateInput.HullShaderRHI,
			BoundShaderStateInput.DomainShaderRHI,
			BoundShaderStateInput.PixelShaderRHI,
			BoundShaderStateInput.GeometryShaderRHI
			);
	}

	FORCEINLINE_DEBUGGABLE void BuildAndSetLocalBoundShaderState(const FBoundShaderStateInput& BoundShaderStateInput)
	{
		SetLocalBoundShaderState(BuildLocalBoundShaderState(
			BoundShaderStateInput.VertexDeclarationRHI,
			BoundShaderStateInput.VertexShaderRHI,
			BoundShaderStateInput.HullShaderRHI,
			BoundShaderStateInput.DomainShaderRHI,
			BoundShaderStateInput.PixelShaderRHI,
			BoundShaderStateInput.GeometryShaderRHI
			));
	}

	FORCEINLINE_DEBUGGABLE FLocalBoundShaderState BuildLocalBoundShaderState(
		FVertexDeclarationRHIParamRef VertexDeclarationRHI,
		FVertexShaderRHIParamRef VertexShaderRHI,
		FHullShaderRHIParamRef HullShaderRHI,
		FDomainShaderRHIParamRef DomainShaderRHI,
		FPixelShaderRHIParamRef PixelShaderRHI,
		FGeometryShaderRHIParamRef GeometryShaderRHI
		)
	{
		FLocalBoundShaderState Result;
		if (Bypass())
		{
			Result.BypassBSS = RHICreateBoundShaderState(VertexDeclarationRHI, VertexShaderRHI, HullShaderRHI, DomainShaderRHI, PixelShaderRHI, GeometryShaderRHI);
		}
		else
		{
			auto* Cmd = new (AllocCommand<FRHICommandBuildLocalBoundShaderState>()) FRHICommandBuildLocalBoundShaderState(this, VertexDeclarationRHI, VertexShaderRHI, HullShaderRHI, DomainShaderRHI, PixelShaderRHI, GeometryShaderRHI);
			Result.WorkArea = &Cmd->WorkArea;
		}
		return Result;
	}

	FORCEINLINE_DEBUGGABLE void SetLocalBoundShaderState(FLocalBoundShaderState LocalBoundShaderState)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetBoundShaderState)(LocalBoundShaderState.BypassBSS);
			return;
		}
		new (AllocCommand<FRHICommandSetLocalBoundShaderState>()) FRHICommandSetLocalBoundShaderState(this, LocalBoundShaderState);
	}

	FORCEINLINE_DEBUGGABLE FLocalUniformBuffer BuildLocalUniformBuffer(const void* Contents, uint32 ContentsSize, const FRHIUniformBufferLayout& Layout)
	{
		FLocalUniformBuffer Result;
		if (Bypass())
		{
			Result.BypassUniform = RHICreateUniformBuffer(Contents, Layout, UniformBuffer_SingleFrame);
		}
		else
		{
			check(Contents && ContentsSize && (&Layout != nullptr));
			auto* Cmd = new (AllocCommand<FRHICommandBuildLocalUniformBuffer>()) FRHICommandBuildLocalUniformBuffer(this, Contents, ContentsSize, Layout);
			Result.WorkArea = &Cmd->WorkArea;
		}
		return Result;
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetLocalShaderUniformBuffer(TShaderRHIParamRef Shader, uint32 BaseIndex, const FLocalUniformBuffer& UniformBuffer)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer.BypassUniform);
			return;
		}
		new (AllocCommand<FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef> >()) FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef>(this, Shader, BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetLocalShaderUniformBuffer(TRefCountPtr<TShaderRHI>& Shader, uint32 BaseIndex, const FLocalUniformBuffer& UniformBuffer)
	{
		SetLocalShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(TShaderRHI* Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderUniformBuffer<TShaderRHI*, ECmdList::EGfx> >()) FRHICommandSetShaderUniformBuffer<TShaderRHI*, ECmdList::EGfx>(Shader, BaseIndex, UniformBuffer);
	}
	template <typename TShaderRHI>
	FORCEINLINE void SetShaderUniformBuffer(TRefCountPtr<TShaderRHI>& Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		SetShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderParameter(TShaderRHI* Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
			return;
		}
		void* UseValue = Alloc(NumBytes, 16);
		FMemory::Memcpy(UseValue, NewValue, NumBytes);
		new (AllocCommand<FRHICommandSetShaderParameter<TShaderRHI*, ECmdList::EGfx> >()) FRHICommandSetShaderParameter<TShaderRHI*, ECmdList::EGfx>(Shader, BufferIndex, BaseIndex, NumBytes, UseValue);
	}
	template <typename TShaderRHI>
	FORCEINLINE void SetShaderParameter(TRefCountPtr<TShaderRHI>& Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		SetShaderParameter(Shader.GetReference(), BufferIndex, BaseIndex, NumBytes, NewValue);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderTexture(TShaderRHIParamRef Shader, uint32 TextureIndex, FTextureRHIParamRef Texture)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderTexture)(Shader, TextureIndex, Texture);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderTexture<TShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetShaderTexture<TShaderRHIParamRef, ECmdList::EGfx>(Shader, TextureIndex, Texture);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderTexture(TRefCountPtr<TShaderRHI>& Shader, uint32 TextureIndex, FTextureRHIParamRef Texture)
	{
		SetShaderTexture(Shader.GetReference(), TextureIndex, Texture);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(TShaderRHIParamRef Shader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef, ECmdList::EGfx>(Shader, SamplerIndex, SRV);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(TRefCountPtr<TShaderRHI>& Shader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV)
	{
		SetShaderResourceViewParameter(Shader.GetReference(), SamplerIndex, SRV);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(TShaderRHIParamRef Shader, uint32 SamplerIndex, FSamplerStateRHIParamRef State)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetShaderSampler)(Shader, SamplerIndex, State);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderSampler<TShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetShaderSampler<TShaderRHIParamRef, ECmdList::EGfx>(Shader, SamplerIndex, State);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(TRefCountPtr<TShaderRHI>& Shader, uint32 SamplerIndex, FSamplerStateRHIParamRef State)
	{
		SetShaderSampler(Shader.GetReference(), SamplerIndex, State);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::EGfx>(Shader, UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(TRefCountPtr<FRHIComputeShader>& Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV)
	{
		SetUAVParameter(Shader.GetReference(), UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::EGfx> >()) FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::EGfx>(Shader, UAVIndex, UAV, InitialCount);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(TRefCountPtr<FRHIComputeShader>& Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount)
	{
		SetUAVParameter(Shader.GetReference(), UAVIndex, UAV, InitialCount);
	}

	FORCEINLINE_DEBUGGABLE void SetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetBoundShaderState)(BoundShaderState);
			return;
		}
		new (AllocCommand<FRHICommandSetBoundShaderState>()) FRHICommandSetBoundShaderState(BoundShaderState);
	}

	FORCEINLINE_DEBUGGABLE void SetRasterizerState(FRasterizerStateRHIParamRef State)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetRasterizerState)(State);
			return;
		}
		if (StateCacheEnabled && CachedRasterizerState && CachedRasterizerState->State == State)
		{
			return;
		}
		CachedRasterizerState = new(AllocCommand<FRHICommandSetRasterizerState>()) FRHICommandSetRasterizerState(State);
	}

	FORCEINLINE_DEBUGGABLE void SetBlendState(FBlendStateRHIParamRef State, const FLinearColor& BlendFactor = FLinearColor::White)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetBlendState)(State, BlendFactor);
			return;
		}
		new (AllocCommand<FRHICommandSetBlendState>()) FRHICommandSetBlendState(State, BlendFactor);
	}

	FORCEINLINE_DEBUGGABLE void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawPrimitive)(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawPrimitive>()) FRHICommandDrawPrimitive(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawIndexedPrimitive)(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedPrimitive>()) FRHICommandDrawIndexedPrimitive(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void SetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetStreamSource)(StreamIndex, VertexBuffer, Stride, Offset);
			return;
		}
		new (AllocCommand<FRHICommandSetStreamSource>()) FRHICommandSetStreamSource(StreamIndex, VertexBuffer, Stride, Offset);
	}

	FORCEINLINE_DEBUGGABLE void SetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef = 0)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetDepthStencilState)(NewStateRHI, StencilRef);
			return;
		}
		if (StateCacheEnabled && CachedDepthStencilState && CachedDepthStencilState->State == NewStateRHI && CachedDepthStencilState->StencilRef == StencilRef)
		{
			return;
		}
		CachedDepthStencilState = new(AllocCommand<FRHICommandSetDepthStencilState>()) FRHICommandSetDepthStencilState(NewStateRHI, StencilRef);
	}

	FORCEINLINE_DEBUGGABLE void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetViewport)(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
			return;
		}
		new (AllocCommand<FRHICommandSetViewport>()) FRHICommandSetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
	}

	FORCEINLINE_DEBUGGABLE void SetStereoViewport(uint32 LeftMinX, uint32 RightMinX, uint32 MinY, float MinZ, uint32 LeftMaxX, uint32 RightMaxX, uint32 MaxY, float MaxZ)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetStereoViewport)(LeftMinX, RightMinX, MinY, MinZ, LeftMaxX, RightMaxX, MaxY, MaxZ);
			return;
		}
		new (AllocCommand<FRHICommandSetStereoViewport>()) FRHICommandSetStereoViewport(LeftMinX, RightMinX, MinY, MinZ, LeftMaxX, RightMaxX, MaxY, MaxZ);
	}

	FORCEINLINE_DEBUGGABLE void SetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetScissorRect)(bEnable, MinX, MinY, MaxX, MaxY);
			return;
		}
		new (AllocCommand<FRHICommandSetScissorRect>()) FRHICommandSetScissorRect(bEnable, MinX, MinY, MaxX, MaxY);
	}

	FORCEINLINE_DEBUGGABLE void SetRenderTargets(
		uint32 NewNumSimultaneousRenderTargets,
		const FRHIRenderTargetView* NewRenderTargetsRHI,
		const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI,
		uint32 NewNumUAVs,
		const FUnorderedAccessViewRHIParamRef* UAVs
		)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetRenderTargets)(
				NewNumSimultaneousRenderTargets,
				NewRenderTargetsRHI,
				NewDepthStencilTargetRHI,
				NewNumUAVs,
				UAVs);
			return;
		}
		new (AllocCommand<FRHICommandSetRenderTargets>()) FRHICommandSetRenderTargets(
			NewNumSimultaneousRenderTargets,
			NewRenderTargetsRHI,
			NewDepthStencilTargetRHI,
			NewNumUAVs,
			UAVs);
	}

	FORCEINLINE_DEBUGGABLE void SetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetRenderTargetsAndClear)(RenderTargetsInfo);
			return;
		}
		new (AllocCommand<FRHICommandSetRenderTargetsAndClear>()) FRHICommandSetRenderTargetsAndClear(RenderTargetsInfo);
	}	

	FORCEINLINE_DEBUGGABLE void BindClearMRTValues(bool bClearColor, bool bClearDepth, bool bClearStencil)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBindClearMRTValues)(bClearColor, bClearDepth, bClearStencil);
			return;
		}
		new (AllocCommand<FRHICommandBindClearMRTValues>()) FRHICommandBindClearMRTValues(bClearColor, bClearDepth, bClearStencil);
	}	

	FORCEINLINE_DEBUGGABLE void BeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginDrawPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData);
			return;
		}
		check(!DrawUPData.OutVertexData && NumVertices * VertexDataStride > 0);

		OutVertexData = Alloc(NumVertices * VertexDataStride, 16);

		DrawUPData.PrimitiveType = PrimitiveType;
		DrawUPData.NumPrimitives = NumPrimitives;
		DrawUPData.NumVertices = NumVertices;
		DrawUPData.VertexDataStride = VertexDataStride;
		DrawUPData.OutVertexData = OutVertexData;
	}

	FORCEINLINE_DEBUGGABLE void EndDrawPrimitiveUP()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndDrawPrimitiveUP)();
			return;
		}
		check(DrawUPData.OutVertexData && DrawUPData.NumVertices);
		new (AllocCommand<FRHICommandEndDrawPrimitiveUP>()) FRHICommandEndDrawPrimitiveUP(DrawUPData.PrimitiveType, DrawUPData.NumPrimitives, DrawUPData.NumVertices, DrawUPData.VertexDataStride, DrawUPData.OutVertexData);

		DrawUPData.OutVertexData = nullptr;
		DrawUPData.NumVertices = 0;
	}

	FORCEINLINE_DEBUGGABLE void BeginDrawIndexedPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginDrawIndexedPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData, MinVertexIndex, NumIndices, IndexDataStride, OutIndexData);
			return;
		}
		check(!DrawUPData.OutVertexData && !DrawUPData.OutIndexData && NumVertices * VertexDataStride > 0 && NumIndices * IndexDataStride > 0);

		OutVertexData = Alloc(NumVertices * VertexDataStride, 16);
		OutIndexData = Alloc(NumIndices * IndexDataStride, 16);

		DrawUPData.PrimitiveType = PrimitiveType;
		DrawUPData.NumPrimitives = NumPrimitives;
		DrawUPData.NumVertices = NumVertices;
		DrawUPData.VertexDataStride = VertexDataStride;
		DrawUPData.OutVertexData = OutVertexData;
		DrawUPData.MinVertexIndex = MinVertexIndex;
		DrawUPData.NumIndices = NumIndices;
		DrawUPData.IndexDataStride = IndexDataStride;
		DrawUPData.OutIndexData = OutIndexData;

	}

	FORCEINLINE_DEBUGGABLE void EndDrawIndexedPrimitiveUP()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndDrawIndexedPrimitiveUP)();
			return;
		}
		check(DrawUPData.OutVertexData && DrawUPData.OutIndexData && DrawUPData.NumIndices && DrawUPData.NumVertices);

		new (AllocCommand<FRHICommandEndDrawIndexedPrimitiveUP>()) FRHICommandEndDrawIndexedPrimitiveUP(DrawUPData.PrimitiveType, DrawUPData.NumPrimitives, DrawUPData.NumVertices, DrawUPData.VertexDataStride, DrawUPData.OutVertexData, DrawUPData.MinVertexIndex, DrawUPData.NumIndices, DrawUPData.IndexDataStride, DrawUPData.OutIndexData);

		DrawUPData.OutVertexData = nullptr;
		DrawUPData.OutIndexData = nullptr;
		DrawUPData.NumIndices = 0;
		DrawUPData.NumVertices = 0;
	}

	FORCEINLINE_DEBUGGABLE void SetComputeShader(FComputeShaderRHIParamRef ComputeShader)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISetComputeShader)(ComputeShader);
			return;
		}
		new (AllocCommand<FRHICommandSetComputeShader<ECmdList::EGfx>>()) FRHICommandSetComputeShader<ECmdList::EGfx>(ComputeShader);
	}

	FORCEINLINE_DEBUGGABLE void DispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
			return;
		}
		new (AllocCommand<FRHICommandDispatchComputeShader<ECmdList::EGfx>>()) FRHICommandDispatchComputeShader<ECmdList::EGfx>(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	FORCEINLINE_DEBUGGABLE void DispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDispatchIndirectComputeShader<ECmdList::EGfx>>()) FRHICommandDispatchIndirectComputeShader<ECmdList::EGfx>(ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void AutomaticCacheFlushAfterComputeShader(bool bEnable)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIAutomaticCacheFlushAfterComputeShader)(bEnable);
			return;
		}
		new (AllocCommand<FRHICommandAutomaticCacheFlushAfterComputeShader>()) FRHICommandAutomaticCacheFlushAfterComputeShader(bEnable);
	}

	FORCEINLINE_DEBUGGABLE void FlushComputeShaderCache()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIFlushComputeShaderCache)();
			return;
		}
		new (AllocCommand<FRHICommandFlushComputeShaderCache>()) FRHICommandFlushComputeShaderCache();
	}

	FORCEINLINE_DEBUGGABLE void DrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawPrimitiveIndirect)(PrimitiveType, ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDrawPrimitiveIndirect>()) FRHICommandDrawPrimitiveIndirect(PrimitiveType, ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, uint32 DrawArgumentsIndex, uint32 NumInstances)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawIndexedIndirect)(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedIndirect>()) FRHICommandDrawIndexedIndirect(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentsBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIDrawIndexedPrimitiveIndirect)(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedPrimitiveIndirect>()) FRHICommandDrawIndexedPrimitiveIndirect(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void EnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEnableDepthBoundsTest)(bEnable, MinDepth, MaxDepth);
			return;
		}
		new (AllocCommand<FRHICommandEnableDepthBoundsTest>()) FRHICommandEnableDepthBoundsTest(bEnable, MinDepth, MaxDepth);
	}

	FORCEINLINE_DEBUGGABLE void ClearUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32(&Values)[4])
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIClearUAV)(UnorderedAccessViewRHI, Values);
			return;
		}
		new (AllocCommand<FRHICommandClearUAV>()) FRHICommandClearUAV(UnorderedAccessViewRHI, Values);
	}

	FORCEINLINE_DEBUGGABLE void CopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHICopyToResolveTarget)(SourceTextureRHI, DestTextureRHI, bKeepOriginalSurface, ResolveParams);
			return;
		}
		new (AllocCommand<FRHICommandCopyToResolveTarget>()) FRHICommandCopyToResolveTarget(SourceTextureRHI, DestTextureRHI, bKeepOriginalSurface, ResolveParams);
	}

	FORCEINLINE_DEBUGGABLE void Clear(bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIClear)(bClearColor, Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
			return;
		}
		new (AllocCommand<FRHICommandClear>()) FRHICommandClear(bClearColor, Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
	}

	FORCEINLINE_DEBUGGABLE void ClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIClearMRT)(bClearColor, NumClearColors, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
			return;
		}
		new (AllocCommand<FRHICommandClearMRT>()) FRHICommandClearMRT(bClearColor, NumClearColors, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
	}

	FORCEINLINE_DEBUGGABLE void BeginRenderQuery(FRenderQueryRHIParamRef RenderQuery)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginRenderQuery)(RenderQuery);
			return;
		}
		new (AllocCommand<FRHICommandBeginRenderQuery>()) FRHICommandBeginRenderQuery(RenderQuery);
	}
	FORCEINLINE_DEBUGGABLE void EndRenderQuery(FRenderQueryRHIParamRef RenderQuery)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndRenderQuery)(RenderQuery);
			return;
		}
		new (AllocCommand<FRHICommandEndRenderQuery>()) FRHICommandEndRenderQuery(RenderQuery);
	}

	FORCEINLINE_DEBUGGABLE void BeginOcclusionQueryBatch()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIBeginOcclusionQueryBatch)();
			return;
		}
		new (AllocCommand<FRHICommandBeginOcclusionQueryBatch>()) FRHICommandBeginOcclusionQueryBatch();
	}

	FORCEINLINE_DEBUGGABLE void EndOcclusionQueryBatch()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIEndOcclusionQueryBatch)();
			return;
		}
		new (AllocCommand<FRHICommandEndOcclusionQueryBatch>()) FRHICommandEndOcclusionQueryBatch();
	}

	FORCEINLINE_DEBUGGABLE void SubmitCommandsHint()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHISubmitCommandsHint)();
			return;
		}
		new (AllocCommand<FRHICommandSubmitCommandsHint<ECmdList::EGfx>>()) FRHICommandSubmitCommandsHint<ECmdList::EGfx>();
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, FTextureRHIParamRef InTexture)
	{
		FTextureRHIParamRef Texture = InTexture;
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, &Texture, 1);
			return;
		}
		new (AllocCommand<FRHICommandTransitionTextures>()) FRHICommandTransitionTextures(TransitionType, &Texture, 1);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, FTextureRHIParamRef* InTextures, int32 NumTextures)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, InTextures, NumTextures);
			return;
		}
		new (AllocCommand<FRHICommandTransitionTextures>()) FRHICommandTransitionTextures(TransitionType, InTextures, NumTextures);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResourceArrayNoCopy(EResourceTransitionAccess TransitionType, TArray<FTextureRHIParamRef>& InTextures)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, &InTextures[0], InTextures.Num());
			return;
		}
		new (AllocCommand<FRHICommandTransitionTexturesArray>()) FRHICommandTransitionTexturesArray(TransitionType, InTextures);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef InUAV, FComputeFenceRHIParamRef WriteFence)
	{
		FUnorderedAccessViewRHIParamRef UAV = InUAV;
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, &UAV, 1, WriteFence);
			return;
		}
		new (AllocCommand<FRHICommandTransitionUAVs<ECmdList::EGfx>>()) FRHICommandTransitionUAVs<ECmdList::EGfx>(TransitionType, TransitionPipeline, &UAV, 1, WriteFence);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef InUAV)
	{
		TransitionResource(TransitionType, TransitionPipeline, InUAV, nullptr);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteFence)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, InUAVs, NumUAVs, WriteFence);
			return;
		}
		new (AllocCommand<FRHICommandTransitionUAVs<ECmdList::EGfx>>()) FRHICommandTransitionUAVs<ECmdList::EGfx>(TransitionType, TransitionPipeline, InUAVs, NumUAVs, WriteFence);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs)
	{
		TransitionResources(TransitionType, TransitionPipeline, InUAVs, NumUAVs, nullptr);
	}

	FORCEINLINE_DEBUGGABLE void WaitComputeFence(FComputeFenceRHIParamRef WaitFence)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIWaitComputeFence)(WaitFence);
			return;
		}
		new (AllocCommand<FRHICommandWaitComputeFence<ECmdList::EGfx>>()) FRHICommandWaitComputeFence<ECmdList::EGfx>(WaitFence);
	}

	

// These 6 are special in that they must be called on the immediate command list and they force a flush only when we are not doing RHI thread
	void BeginScene();
	void EndScene();
	void BeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI);
	void EndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync);
	void BeginFrame();
	void EndFrame();

	FORCEINLINE_DEBUGGABLE void PushEvent(const TCHAR* Name, FColor Color)
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIPushEvent)(Name, Color);
			return;
		}
		int32 Len = FCString::Strlen(Name) + 1;
		TCHAR* NameCopy  = (TCHAR*)Alloc(Len * (int32)sizeof(TCHAR), (int32)sizeof(TCHAR));
		FCString::Strcpy(NameCopy, Len, Name);
		new (AllocCommand<FRHICommandPushEvent<ECmdList::EGfx>>()) FRHICommandPushEvent<ECmdList::EGfx>(NameCopy, Color);
	}

	FORCEINLINE_DEBUGGABLE void PopEvent()
	{
		if (Bypass())
		{
			CMD_CONTEXT(RHIPopEvent)();
			return;
		}
		new (AllocCommand<FRHICommandPopEvent<ECmdList::EGfx>>()) FRHICommandPopEvent<ECmdList::EGfx>();
	}

	FORCEINLINE_DEBUGGABLE void BreakPoint()
	{
#if !UE_BUILD_SHIPPING
		if (Bypass())
		{
			if (FPlatformMisc::IsDebuggerPresent())
			{
				FPlatformMisc::DebugBreak();
			}
			return;
		}
		new (AllocCommand<FRHICommandDebugBreak>()) FRHICommandDebugBreak();
#endif
	}
};

class RHI_API FRHIAsyncComputeCommandList : public FRHICommandListBase
{
public:

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

	FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(FComputeShaderRHIParamRef Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderUniformBuffer<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, BaseIndex, UniformBuffer);		
	}
	
	FORCEINLINE void SetShaderUniformBuffer(FComputeShaderRHIRef& Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		SetShaderUniformBuffer(Shader.GetReference(), BaseIndex, UniformBuffer);
	}

	FORCEINLINE_DEBUGGABLE void SetShaderParameter(FComputeShaderRHIParamRef Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
			return;
		}
		void* UseValue = Alloc(NumBytes, 16);
		FMemory::Memcpy(UseValue, NewValue, NumBytes);
		new (AllocCommand<FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, BufferIndex, BaseIndex, NumBytes, UseValue);
	}
	
	FORCEINLINE void SetShaderParameter(FComputeShaderRHIRef& Shader, uint32 BufferIndex, uint32 BaseIndex, uint32 NumBytes, const void* NewValue)
	{
		SetShaderParameter(Shader.GetReference(), BufferIndex, BaseIndex, NumBytes, NewValue);
	}
	
	FORCEINLINE_DEBUGGABLE void SetShaderTexture(FComputeShaderRHIParamRef Shader, uint32 TextureIndex, FTextureRHIParamRef Texture)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderTexture)(Shader, TextureIndex, Texture);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderTexture<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, TextureIndex, Texture);
	}
	
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(FComputeShaderRHIParamRef Shader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderResourceViewParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, SamplerIndex, SRV);
	}
	
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(FComputeShaderRHIParamRef Shader, uint32 SamplerIndex, FSamplerStateRHIParamRef State)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetShaderSampler)(Shader, SamplerIndex, State);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetShaderSampler<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, SamplerIndex, State);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetUAVParameter<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::ECompute> >()) FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef, ECmdList::ECompute>(Shader, UAVIndex, UAV, InitialCount);
	}
	
	FORCEINLINE_DEBUGGABLE void SetComputeShader(FComputeShaderRHIParamRef ComputeShader)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetComputeShader)(ComputeShader);
			return;
		}
		new (AllocCommand<FRHICommandSetComputeShader<ECmdList::ECompute> >()) FRHICommandSetComputeShader<ECmdList::ECompute>(ComputeShader);
	}

	FORCEINLINE_DEBUGGABLE void SetAsyncComputeBudget(EAsyncComputeBudget Budget)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISetAsyncComputeBudget)(Budget);
			return;
		}
		new (AllocCommand<FRHICommandSetAsyncComputeBudget<ECmdList::ECompute>>()) FRHICommandSetAsyncComputeBudget<ECmdList::ECompute>(Budget);
	}

	FORCEINLINE_DEBUGGABLE void DispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIDispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
			return;
		}
		new (AllocCommand<FRHICommandDispatchComputeShader<ECmdList::ECompute> >()) FRHICommandDispatchComputeShader<ECmdList::ECompute>(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	FORCEINLINE_DEBUGGABLE void DispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIDispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDispatchIndirectComputeShader<ECmdList::ECompute> >()) FRHICommandDispatchIndirectComputeShader<ECmdList::ECompute>(ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef InUAV, FComputeFenceRHIParamRef WriteFence)
	{
		FUnorderedAccessViewRHIParamRef UAV = InUAV;
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, &UAV, 1, WriteFence);
			return;
		}
		new (AllocCommand<FRHICommandTransitionUAVs<ECmdList::ECompute> >()) FRHICommandTransitionUAVs<ECmdList::ECompute>(TransitionType, TransitionPipeline, &UAV, 1, WriteFence);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResource(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef InUAV)
	{
		TransitionResource(TransitionType, TransitionPipeline, InUAV, nullptr);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs, FComputeFenceRHIParamRef WriteFence)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHITransitionResources)(TransitionType, TransitionPipeline, InUAVs, NumUAVs, WriteFence);
			return;
		}
		new (AllocCommand<FRHICommandTransitionUAVs<ECmdList::ECompute> >()) FRHICommandTransitionUAVs<ECmdList::ECompute>(TransitionType, TransitionPipeline, InUAVs, NumUAVs, WriteFence);
	}

	FORCEINLINE_DEBUGGABLE void TransitionResources(EResourceTransitionAccess TransitionType, EResourceTransitionPipeline TransitionPipeline, FUnorderedAccessViewRHIParamRef* InUAVs, int32 NumUAVs)
	{
		TransitionResources(TransitionType, TransitionPipeline, InUAVs, NumUAVs, nullptr);
	}
	
	FORCEINLINE_DEBUGGABLE void PushEvent(const TCHAR* Name, FColor Color)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIPushEvent)(Name, Color);
			return;
		}
		int32 Len = FCString::Strlen(Name) + 1;
		TCHAR* NameCopy = (TCHAR*)Alloc(Len * (int32)sizeof(TCHAR), (int32)sizeof(TCHAR));
		FCString::Strcpy(NameCopy, Len, Name);
		new (AllocCommand<FRHICommandPushEvent<ECmdList::ECompute> >()) FRHICommandPushEvent<ECmdList::ECompute>(NameCopy, Color);
	}

	FORCEINLINE_DEBUGGABLE void PopEvent()
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIPopEvent)();
			return;
		}
		new (AllocCommand<FRHICommandPopEvent<ECmdList::ECompute> >()) FRHICommandPopEvent<ECmdList::ECompute>();
	}

	FORCEINLINE_DEBUGGABLE void BreakPoint()
	{
#if !UE_BUILD_SHIPPING
		if (Bypass())
		{
			if (FPlatformMisc::IsDebuggerPresent())
			{
				FPlatformMisc::DebugBreak();
			}
			return;
		}
		new (AllocCommand<FRHICommandDebugBreak>()) FRHICommandDebugBreak();
#endif
	}

	FORCEINLINE_DEBUGGABLE void SubmitCommandsHint()
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHISubmitCommandsHint)();
			return;
		}
		new (AllocCommand<FRHICommandSubmitCommandsHint<ECmdList::ECompute>>()) FRHICommandSubmitCommandsHint<ECmdList::ECompute>();
	}

	FORCEINLINE_DEBUGGABLE void WaitComputeFence(FComputeFenceRHIParamRef WaitFence)
	{
		if (Bypass())
		{
			COMPUTE_CONTEXT(RHIWaitComputeFence)(WaitFence);
			return;
		}
		new (AllocCommand<FRHICommandWaitComputeFence<ECmdList::ECompute>>()) FRHICommandWaitComputeFence<ECmdList::ECompute>(WaitFence);
	}
};

namespace EImmediateFlushType
{
	enum Type
	{ 
		WaitForOutstandingTasksOnly = 0, 
		DispatchToRHIThread, 
		WaitForDispatchToRHIThread,
		FlushRHIThread,
		FlushRHIThreadFlushResources
	};
};

class FScopedRHIThreadStaller
{
	class FRHICommandListImmediate* Immed; // non-null if we need to unstall
public:
	inline FScopedRHIThreadStaller(class FRHICommandListImmediate& InImmed);
	inline ~FScopedRHIThreadStaller();
};

class RHI_API FRHICommandListImmediate : public FRHICommandList
{
	friend class FRHICommandListExecutor;
	FRHICommandListImmediate()
	{
	}
	~FRHICommandListImmediate()
	{
		check(!HasCommands());
	}
public:

	inline void ImmediateFlush(EImmediateFlushType::Type FlushType);
	bool StallRHIThread();
	void UnStallRHIThread();

	void SetCurrentStat(TStatId Stat);

	static FGraphEventRef RenderThreadTaskFence();
	static void WaitOnRenderThreadTaskFence(FGraphEventRef& Fence);
	static bool AnyRenderThreadTasksOutstanding();
	FGraphEventRef RHIThreadFence(bool bSetLockFence = false);

	//Queue the given async compute commandlists in order with the current immediate commandlist
	void QueueAsyncCompute(FRHIAsyncComputeCommandList& RHIComputeCmdList);
	
	FORCEINLINE FSamplerStateRHIRef CreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
	{
		return RHICreateSamplerState(Initializer);
	}
	
	FORCEINLINE FRasterizerStateRHIRef CreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
	{
		return RHICreateRasterizerState(Initializer);
	}
	
	FORCEINLINE FDepthStencilStateRHIRef CreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
	{
		return RHICreateDepthStencilState(Initializer);
	}
	
	FORCEINLINE FBlendStateRHIRef CreateBlendState(const FBlendStateInitializerRHI& Initializer)
	{
		return RHICreateBlendState(Initializer);
	}
	
	FORCEINLINE FVertexDeclarationRHIRef CreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
	{
		return GDynamicRHI->CreateVertexDeclaration_RenderThread(*this, Elements);
	}
	
	FORCEINLINE FPixelShaderRHIRef CreatePixelShader(const TArray<uint8>& Code)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreatePixelShader(Code);
	}
	
	FORCEINLINE FVertexShaderRHIRef CreateVertexShader(const TArray<uint8>& Code)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateVertexShader(Code);
	}
	
	FORCEINLINE FHullShaderRHIRef CreateHullShader(const TArray<uint8>& Code)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateHullShader(Code);
	}
	
	FORCEINLINE FDomainShaderRHIRef CreateDomainShader(const TArray<uint8>& Code)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateDomainShader(Code);
	}
	
	FORCEINLINE FGeometryShaderRHIRef CreateGeometryShader(const TArray<uint8>& Code)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateGeometryShader(Code);
	}
	
	FORCEINLINE FGeometryShaderRHIRef CreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateGeometryShaderWithStreamOutput(Code, ElementList, NumStrides, Strides, RasterizedStream);
	}
	
	FORCEINLINE FComputeShaderRHIRef CreateComputeShader(const TArray<uint8>& Code)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateComputeShader(Code);
	}
	
	FORCEINLINE FComputeFenceRHIRef CreateComputeFence(const FName& Name)
	{		
		return GDynamicRHI->RHICreateComputeFence(Name);
	}	
	
	FORCEINLINE FBoundShaderStateRHIRef CreateBoundShaderState(FVertexDeclarationRHIParamRef VertexDeclaration, FVertexShaderRHIParamRef VertexShader, FHullShaderRHIParamRef HullShader, FDomainShaderRHIParamRef DomainShader, FPixelShaderRHIParamRef PixelShader, FGeometryShaderRHIParamRef GeometryShader)
	{
		return RHICreateBoundShaderState(VertexDeclaration, VertexShader, HullShader, DomainShader, PixelShader, GeometryShader);
	}
	
	FORCEINLINE FUniformBufferRHIRef CreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
	{
		return RHICreateUniformBuffer(Contents, Layout, Usage);
	}
	
	FORCEINLINE FIndexBufferRHIRef CreateAndLockIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
	{
		return GDynamicRHI->CreateAndLockIndexBuffer_RenderThread(*this, Stride, Size, InUsage, CreateInfo, OutDataBuffer);
	}
	
	FORCEINLINE FIndexBufferRHIRef CreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	{
		return GDynamicRHI->CreateIndexBuffer_RenderThread(*this, Stride, Size, InUsage, CreateInfo);
	}
	
	FORCEINLINE void* LockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		return GDynamicRHI->LockIndexBuffer_RenderThread(*this, IndexBuffer, Offset, SizeRHI, LockMode);
	}
	
	FORCEINLINE void UnlockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer)
	{
		GDynamicRHI->UnlockIndexBuffer_RenderThread(*this, IndexBuffer);
	}
	
	FORCEINLINE FVertexBufferRHIRef CreateAndLockVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
	{
		return GDynamicRHI->CreateAndLockVertexBuffer_RenderThread(*this, Size, InUsage, CreateInfo, OutDataBuffer);
	}

	FORCEINLINE FVertexBufferRHIRef CreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	{
		return GDynamicRHI->CreateVertexBuffer_RenderThread(*this, Size, InUsage, CreateInfo);
	}
	
	FORCEINLINE void* LockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		return GDynamicRHI->LockVertexBuffer_RenderThread(*this, VertexBuffer, Offset, SizeRHI, LockMode);
	}
	
	FORCEINLINE void UnlockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer)
	{
		GDynamicRHI->UnlockVertexBuffer_RenderThread(*this, VertexBuffer);
	}
	
	FORCEINLINE void CopyVertexBuffer(FVertexBufferRHIParamRef SourceBuffer,FVertexBufferRHIParamRef DestBuffer)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_CopyVertexBuffer_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHICopyVertexBuffer(SourceBuffer,DestBuffer);
	}
	
	FORCEINLINE FStructuredBufferRHIRef CreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateStructuredBuffer(Stride, Size, InUsage, CreateInfo);
	}
	
	FORCEINLINE void* LockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockStructuredBuffer_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHILockStructuredBuffer(StructuredBuffer, Offset, SizeRHI, LockMode);
	}
	
	FORCEINLINE void UnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockStructuredBuffer_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		  
		GDynamicRHI->RHIUnlockStructuredBuffer(StructuredBuffer);
	}
	
	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer)
	{
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, StructuredBuffer, bUseUAVCounter, bAppendBuffer);
	}
	
	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FTextureRHIParamRef Texture, uint32 MipLevel)
	{
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, Texture, MipLevel);
	}
	
	FORCEINLINE FUnorderedAccessViewRHIRef CreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBuffer, uint8 Format)
	{
		return GDynamicRHI->RHICreateUnorderedAccessView_RenderThread(*this, VertexBuffer, Format);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBuffer)
	{
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, StructuredBuffer);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format)
	{
		return GDynamicRHI->CreateShaderResourceView_RenderThread(*this, VertexBuffer, Stride, Format);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FIndexBufferRHIParamRef Buffer)
	{
		return GDynamicRHI->CreateShaderResourceView_RenderThread(*this, Buffer);
	}
	
	FORCEINLINE uint64 CalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
	{
		return RHICalcTexture2DPlatformSize(SizeX, SizeY, Format, NumMips, NumSamples, Flags, OutAlign);
	}
	
	FORCEINLINE uint64 CalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
	{
		return RHICalcTexture3DPlatformSize(SizeX, SizeY, SizeZ, Format, NumMips, Flags, OutAlign);
	}
	
	FORCEINLINE uint64 CalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
	{
		return RHICalcTextureCubePlatformSize(Size, Format, NumMips, Flags, OutAlign);
	}
	
	FORCEINLINE void GetTextureMemoryStats(FTextureMemoryStats& OutStats)
	{
		RHIGetTextureMemoryStats(OutStats);
	}
	
	FORCEINLINE bool GetTextureMemoryVisualizeData(FColor* TextureData,int32 SizeX,int32 SizeY,int32 Pitch,int32 PixelSize)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetTextureMemoryVisualizeData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		return GDynamicRHI->RHIGetTextureMemoryVisualizeData(TextureData,SizeX,SizeY,Pitch,PixelSize);
	}
	
	FORCEINLINE FTextureReferenceRHIRef CreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateTextureReference(LastRenderTime);
	}
	
	FORCEINLINE FTexture2DRHIRef CreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		return GDynamicRHI->RHICreateTexture2D_RenderThread(*this, SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
	}

	FORCEINLINE FStructuredBufferRHIRef CreateRTWriteMaskBuffer(FTexture2DRHIRef RenderTarget)
	{
		return GDynamicRHI->RHICreateRTWriteMaskBuffer(RenderTarget);
	}

	FORCEINLINE FTexture2DRHIRef AsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips)
	{
		return GDynamicRHI->RHIAsyncCreateTexture2D(SizeX, SizeY, Format, NumMips, Flags, InitialMipData, NumInitialMips);
	}
	
	FORCEINLINE void CopySharedMips(FTexture2DRHIParamRef DestTexture2D, FTexture2DRHIParamRef SrcTexture2D)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_CopySharedMips_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHICopySharedMips(DestTexture2D, SrcTexture2D);
	}
	
	FORCEINLINE FTexture2DArrayRHIRef CreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		return GDynamicRHI->RHICreateTexture2DArray_RenderThread(*this, SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
	}
	
	FORCEINLINE FTexture3DRHIRef CreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		return GDynamicRHI->RHICreateTexture3D_RenderThread(*this, SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
	}
	
	FORCEINLINE void GetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
	{
		return RHIGetResourceInfo(Ref, OutInfo);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
	{
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture2DRHI, MipLevel);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
	{
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture2DRHI, MipLevel, NumMipLevels, Format);
	}
	
	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
	{
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture3DRHI, MipLevel);
	}

	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
	{
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, Texture2DArrayRHI, MipLevel);
	}

	FORCEINLINE FShaderResourceViewRHIRef CreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
	{
		return GDynamicRHI->RHICreateShaderResourceView_RenderThread(*this, TextureCubeRHI, MipLevel);
	}

	FORCEINLINE void GenerateMips(FTextureRHIParamRef Texture)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GenerateMips_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); return GDynamicRHI->RHIGenerateMips(Texture);
	}
	
	FORCEINLINE uint32 ComputeMemorySize(FTextureRHIParamRef TextureRHI)
	{
		return RHIComputeMemorySize(TextureRHI);
	}
	
	FORCEINLINE FTexture2DRHIRef AsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
	{
		return GDynamicRHI->AsyncReallocateTexture2D_RenderThread(*this, Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}
	
	FORCEINLINE ETextureReallocationStatus FinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
	{
		return GDynamicRHI->FinalizeAsyncReallocateTexture2D_RenderThread(*this, Texture2D, bBlockUntilCompleted);
	}
	
	FORCEINLINE ETextureReallocationStatus CancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
	{
		return GDynamicRHI->CancelAsyncReallocateTexture2D_RenderThread(*this, Texture2D, bBlockUntilCompleted);
	}
	
	FORCEINLINE void* LockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bFlushRHIThread = true)
	{
		return GDynamicRHI->LockTexture2D_RenderThread(*this, Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, bFlushRHIThread);
	}
	
	FORCEINLINE void UnlockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bFlushRHIThread = true)
	{		
		GDynamicRHI->UnlockTexture2D_RenderThread(*this, Texture, MipIndex, bLockWithinMiptail, bFlushRHIThread);
	}
	
	FORCEINLINE void* LockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2DArray_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		return GDynamicRHI->RHILockTexture2DArray(Texture, TextureIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	
	FORCEINLINE void UnlockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTexture2DArray_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);   
		GDynamicRHI->RHIUnlockTexture2DArray(Texture, TextureIndex, MipIndex, bLockWithinMiptail);
	}
	
	FORCEINLINE void UpdateTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
	{		
		GDynamicRHI->UpdateTexture2D_RenderThread(*this, Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
	}
	
	FORCEINLINE void UpdateTexture3D(FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UpdateTexture3D_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);   
		GDynamicRHI->RHIUpdateTexture3D(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
	
	FORCEINLINE FTextureCubeRHIRef CreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		return GDynamicRHI->RHICreateTextureCube_RenderThread(*this, Size, Format, NumMips, Flags, CreateInfo);
	}
	
	FORCEINLINE FTextureCubeRHIRef CreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
	{
		return GDynamicRHI->RHICreateTextureCubeArray_RenderThread(*this, Size, ArraySize, Format, NumMips, Flags, CreateInfo);
	}
	
	FORCEINLINE void* LockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTextureCubeFace_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		return GDynamicRHI->RHILockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	
	FORCEINLINE void UnlockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnlockTextureCubeFace_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);   
		GDynamicRHI->RHIUnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
	}
	
	FORCEINLINE void BindDebugLabelName(FTextureRHIParamRef Texture, const TCHAR* Name)
	{
		RHIBindDebugLabelName(Texture, Name);
	}

	FORCEINLINE void BindDebugLabelName(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const TCHAR* Name)
	{
		RHIBindDebugLabelName(UnorderedAccessViewRHI, Name);
	}

	FORCEINLINE void ReadSurfaceData(FTextureRHIParamRef Texture,FIntRect Rect,TArray<FColor>& OutData,FReadSurfaceDataFlags InFlags)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIReadSurfaceData(Texture,Rect,OutData,InFlags);
	}

	FORCEINLINE void ReadSurfaceData(FTextureRHIParamRef Texture, FIntRect Rect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		GDynamicRHI->RHIReadSurfaceData(Texture, Rect, OutData, InFlags);
	}
	
	FORCEINLINE void MapStagingSurface(FTextureRHIParamRef Texture,void*& OutData,int32& OutWidth,int32& OutHeight)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_MapStagingSurface_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIMapStagingSurface(Texture,OutData,OutWidth,OutHeight);
	}
	
	FORCEINLINE void UnmapStagingSurface(FTextureRHIParamRef Texture)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_UnmapStagingSurface_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIUnmapStagingSurface(Texture);
	}
	
	FORCEINLINE void ReadSurfaceFloatData(FTextureRHIParamRef Texture,FIntRect Rect,TArray<FFloat16Color>& OutData,ECubeFace CubeFace,int32 ArrayIndex,int32 MipIndex)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReadSurfaceFloatData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIReadSurfaceFloatData(Texture,Rect,OutData,CubeFace,ArrayIndex,MipIndex);
	}

	FORCEINLINE void Read3DSurfaceFloatData(FTextureRHIParamRef Texture,FIntRect Rect,FIntPoint ZMinMax,TArray<FFloat16Color>& OutData)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_Read3DSurfaceFloatData_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIRead3DSurfaceFloatData(Texture,Rect,ZMinMax,OutData);
	}
	
	FORCEINLINE FRenderQueryRHIRef CreateRenderQuery(ERenderQueryType QueryType)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		return GDynamicRHI->RHICreateRenderQuery(QueryType);
	}
	
	FORCEINLINE bool GetRenderQueryResult(FRenderQueryRHIParamRef RenderQuery, uint64& OutResult, bool bWait)
	{
		return RHIGetRenderQueryResult(RenderQuery, OutResult, bWait);
	}
	
	FORCEINLINE FTexture2DRHIRef GetViewportBackBuffer(FViewportRHIParamRef Viewport)
	{
		return RHIGetViewportBackBuffer(Viewport);
	}
	
	FORCEINLINE void AdvanceFrameForGetViewportBackBuffer()
	{
		return RHIAdvanceFrameForGetViewportBackBuffer();
	}
	
	FORCEINLINE void AcquireThreadOwnership()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_AcquireThreadOwnership_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIAcquireThreadOwnership();
	}
	
	FORCEINLINE void ReleaseThreadOwnership()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_ReleaseThreadOwnership_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIReleaseThreadOwnership();
	}
	
	FORCEINLINE void FlushResources()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_FlushResources_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIFlushResources();
	}
	
	FORCEINLINE uint32 GetGPUFrameCycles()
	{
		return RHIGetGPUFrameCycles();
	}
	
	FORCEINLINE FViewportRHIRef CreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
	{
		return RHICreateViewport(WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
	}
	
	FORCEINLINE void ResizeViewport(FViewportRHIParamRef Viewport, uint32 SizeX, uint32 SizeY, bool bIsFullscreen)
	{
		RHIResizeViewport(Viewport, SizeX, SizeY, bIsFullscreen);
	}
	
	FORCEINLINE void Tick(float DeltaTime)
	{
		RHITick(DeltaTime);
	}
	
	FORCEINLINE void SetStreamOutTargets(uint32 NumTargets,const FVertexBufferRHIParamRef* VertexBuffers,const uint32* Offsets)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_SetStreamOutTargets_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHISetStreamOutTargets(NumTargets,VertexBuffers,Offsets);
	}
	
	FORCEINLINE void DiscardRenderTargets(bool Depth,bool Stencil,uint32 ColorBitMask)
	{
		GDynamicRHI->RHIDiscardRenderTargets(Depth,Stencil,ColorBitMask);
	}
	
	FORCEINLINE void BlockUntilGPUIdle()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_BlockUntilGPUIdle_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread);  
		GDynamicRHI->RHIBlockUntilGPUIdle();
	}
	
	FORCEINLINE void SuspendRendering()
	{
		RHISuspendRendering();
	}
	
	FORCEINLINE void ResumeRendering()
	{
		RHIResumeRendering();
	}
	
	FORCEINLINE bool IsRenderingSuspended()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_IsRenderingSuspended_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		return GDynamicRHI->RHIIsRenderingSuspended();
	}

	FORCEINLINE bool EnqueueDecompress(uint8_t* SrcBuffer, uint8_t* DestBuffer, int CompressedSize, void* ErrorCodeBuffer)
	{
		return GDynamicRHI->RHIEnqueueDecompress(SrcBuffer, DestBuffer, CompressedSize, ErrorCodeBuffer);
	}

	FORCEINLINE bool EnqueueCompress(uint8_t* SrcBuffer, uint8_t* DestBuffer, int UnCompressedSize, void* ErrorCodeBuffer)
	{
		return GDynamicRHI->RHIEnqueueCompress(SrcBuffer, DestBuffer, UnCompressedSize, ErrorCodeBuffer);
	}
	
	FORCEINLINE bool GetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
	{
		return RHIGetAvailableResolutions(Resolutions, bIgnoreRefreshRate);
	}
	
	FORCEINLINE void GetSupportedResolution(uint32& Width, uint32& Height)
	{
		RHIGetSupportedResolution(Width, Height);
	}
	
	FORCEINLINE void VirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef Texture, uint32 FirstMip)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		GDynamicRHI->RHIVirtualTextureSetFirstMipInMemory(Texture, FirstMip);
	}
	
	FORCEINLINE void VirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef Texture, uint32 FirstMip)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		GDynamicRHI->RHIVirtualTextureSetFirstMipVisible(Texture, FirstMip);
	}

	FORCEINLINE void CopySubTextureRegion(FTexture2DRHIParamRef SourceTexture, FTexture2DRHIParamRef DestinationTexture, FBox2D SourceBox, FBox2D DestinationBox)
	{
		GDynamicRHI->RHICopySubTextureRegion(SourceTexture, DestinationTexture, SourceBox, DestinationBox);
	}
	
	FORCEINLINE void ExecuteCommandList(FRHICommandList* CmdList)
	{
		FScopedRHIThreadStaller StallRHIThread(*this);
		GDynamicRHI->RHIExecuteCommandList(CmdList);
	}
	
	FORCEINLINE void* GetNativeDevice()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_GetNativeDevice_Flush);
		ImmediateFlush(EImmediateFlushType::FlushRHIThread); 
		 
		return GDynamicRHI->RHIGetNativeDevice();
	}
	
	FORCEINLINE class IRHICommandContext* GetDefaultContext()
	{
		return RHIGetDefaultContext();
	}
	
	FORCEINLINE class IRHICommandContextContainer* GetCommandContextContainer()
	{
		return RHIGetCommandContextContainer();
	}
	void UpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture);
	

};

// Single commandlist for async compute generation.  In the future we may expand this to allow async compute command generation
// on multiple threads at once.
class RHI_API FRHIAsyncComputeCommandListImmediate : public FRHIAsyncComputeCommandList
{
public:

	//If RHIThread is enabled this will dispatch all current commands to the RHI Thread.  If RHI thread is disabled
	//this will immediately execute the current commands.
	//This also queues a GPU Submission command as the final command in the dispatch.
	static void ImmediateDispatch(FRHIAsyncComputeCommandListImmediate& RHIComputeCmdList);

private:
};

// typedef to mark the recursive use of commandlists in the RHI implementations

class RHI_API FRHICommandList_RecursiveHazardous : public FRHICommandList
{
	FRHICommandList_RecursiveHazardous()
	{

	}
public:
	FRHICommandList_RecursiveHazardous(IRHICommandContext *Context)
	{
		SetContext(Context);
	}
};


// This controls if the cmd list bypass can be toggled at runtime. It is quite expensive to have these branches in there.
#define CAN_TOGGLE_COMMAND_LIST_BYPASS (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)

class RHI_API FRHICommandListExecutor
{
public:
	enum
	{
		DefaultBypass = PLATFORM_RHITHREAD_DEFAULT_BYPASS
	};
	FRHICommandListExecutor()
		: bLatchedBypass(!!DefaultBypass)
		, bLatchedUseParallelAlgorithms(false)
	{
	}
	static inline FRHICommandListImmediate& GetImmediateCommandList();
	static inline FRHIAsyncComputeCommandListImmediate& GetImmediateAsyncComputeCommandList();

	void ExecuteList(FRHICommandListBase& CmdList);
	void ExecuteList(FRHICommandListImmediate& CmdList);
	void LatchBypass();

	static void WaitOnRHIThreadFence(FGraphEventRef& Fence);

	FORCEINLINE_DEBUGGABLE bool Bypass()
	{
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
		return bLatchedBypass;
#else
		return !!DefaultBypass;
#endif
	}
	FORCEINLINE_DEBUGGABLE bool UseParallelAlgorithms()
	{
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
		return bLatchedUseParallelAlgorithms;
#else
		return  FApp::ShouldUseThreadingForPerformance() && !Bypass();
#endif
	}
	static void CheckNoOutstandingCmdLists();
	static bool IsRHIThreadActive();
	static bool IsRHIThreadCompletelyFlushed();

private:

	void ExecuteInner(FRHICommandListBase& CmdList);
	friend class FExecuteRHIThreadTask;
	static void ExecuteInner_DoExecute(FRHICommandListBase& CmdList);

	bool bLatchedBypass;
	bool bLatchedUseParallelAlgorithms;
	friend class FRHICommandListBase;
	FThreadSafeCounter UIDCounter;
	FThreadSafeCounter OutstandingCmdListCount;
	FRHICommandListImmediate CommandListImmediate;
	FRHIAsyncComputeCommandListImmediate AsyncComputeCmdListImmediate;
};

extern RHI_API FRHICommandListExecutor GRHICommandList;

extern RHI_API FAutoConsoleTaskPriority CPrio_SceneRenderingTask;

class FRenderTask
{
public:
	FORCEINLINE static ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_SceneRenderingTask.Get();
	}
};


FORCEINLINE_DEBUGGABLE FRHICommandListImmediate& FRHICommandListExecutor::GetImmediateCommandList()
{
	return GRHICommandList.CommandListImmediate;
}

FORCEINLINE_DEBUGGABLE FRHIAsyncComputeCommandListImmediate& FRHICommandListExecutor::GetImmediateAsyncComputeCommandList()
{
	return GRHICommandList.AsyncComputeCmdListImmediate;
}

struct FScopedCommandListWaitForTasks
{
	FRHICommandListImmediate& RHICmdList;
	bool bWaitForTasks;

	FScopedCommandListWaitForTasks(bool InbWaitForTasks, FRHICommandListImmediate& InRHICmdList = FRHICommandListExecutor::GetImmediateCommandList())
		: RHICmdList(InRHICmdList)
		, bWaitForTasks(InbWaitForTasks)
	{
	}
	RHI_API ~FScopedCommandListWaitForTasks();
};


FORCEINLINE FVertexDeclarationRHIRef RHICreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateVertexDeclaration(Elements);
}

FORCEINLINE FPixelShaderRHIRef RHICreatePixelShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreatePixelShader(Code);
}

FORCEINLINE FVertexShaderRHIRef RHICreateVertexShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateVertexShader(Code);
}

FORCEINLINE FHullShaderRHIRef RHICreateHullShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateHullShader(Code);
}

FORCEINLINE FDomainShaderRHIRef RHICreateDomainShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateDomainShader(Code);
}

FORCEINLINE FGeometryShaderRHIRef RHICreateGeometryShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateGeometryShader(Code);
}

FORCEINLINE FGeometryShaderRHIRef RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateGeometryShaderWithStreamOutput(Code, ElementList, NumStrides, Strides, RasterizedStream);
}

FORCEINLINE FComputeShaderRHIRef RHICreateComputeShader(const TArray<uint8>& Code)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateComputeShader(Code);
}

FORCEINLINE FComputeFenceRHIRef RHICreateComputeFence(const FName& Name)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateComputeFence(Name);
}

FORCEINLINE FIndexBufferRHIRef RHICreateAndLockIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateAndLockIndexBuffer(Stride, Size, InUsage, CreateInfo, OutDataBuffer);
}

FORCEINLINE FIndexBufferRHIRef RHICreateIndexBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateIndexBuffer(Stride, Size, InUsage, CreateInfo);
}

FORCEINLINE void* RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockIndexBuffer(IndexBuffer, Offset, Size, LockMode);
}

FORCEINLINE void RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBuffer)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockIndexBuffer(IndexBuffer);
}

FORCEINLINE FVertexBufferRHIRef RHICreateAndLockVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo, void*& OutDataBuffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateAndLockVertexBuffer(Size, InUsage, CreateInfo, OutDataBuffer);
}

FORCEINLINE FVertexBufferRHIRef RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateVertexBuffer(Size, InUsage, CreateInfo);
}

FORCEINLINE void* RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockVertexBuffer(VertexBuffer, Offset, SizeRHI, LockMode);
}

FORCEINLINE void RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBuffer)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockVertexBuffer(VertexBuffer);
}

FORCEINLINE FStructuredBufferRHIRef RHICreateStructuredBuffer(uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateStructuredBuffer(Stride, Size, InUsage, CreateInfo);
}

FORCEINLINE void* RHILockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockStructuredBuffer(StructuredBuffer, Offset, SizeRHI, LockMode);
}

FORCEINLINE void RHIUnlockStructuredBuffer(FStructuredBufferRHIParamRef StructuredBuffer)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockStructuredBuffer(StructuredBuffer);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FStructuredBufferRHIParamRef StructuredBuffer, bool bUseUAVCounter, bool bAppendBuffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(StructuredBuffer, bUseUAVCounter, bAppendBuffer);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FTextureRHIParamRef Texture, uint32 MipLevel = 0)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(Texture, MipLevel);
}

FORCEINLINE FUnorderedAccessViewRHIRef RHICreateUnorderedAccessView(FVertexBufferRHIParamRef VertexBuffer, uint8 Format)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateUnorderedAccessView(VertexBuffer, Format);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FStructuredBufferRHIParamRef StructuredBuffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(StructuredBuffer);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint8 Format)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(VertexBuffer, Stride, Format);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FIndexBufferRHIParamRef Buffer)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Buffer);
}

FORCEINLINE FTextureReferenceRHIRef RHICreateTextureReference(FLastRenderTimeContainer* LastRenderTime)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTextureReference(LastRenderTime);
}

FORCEINLINE void RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRef, FTextureRHIParamRef NewTexture)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UpdateTextureReference(TextureRef, NewTexture);
}

FORCEINLINE FTexture2DRHIRef RHICreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTexture2D(SizeX, SizeY, Format, NumMips, NumSamples, Flags, CreateInfo);
}

FORCEINLINE FStructuredBufferRHIRef RHICreateRTWriteMaskBuffer(FTexture2DRHIRef RenderTarget)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateRTWriteMaskBuffer(RenderTarget);
}

FORCEINLINE FTexture2DRHIRef RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips)
{
	return FRHICommandListExecutor::GetImmediateCommandList().AsyncCreateTexture2D(SizeX, SizeY, Format, NumMips, Flags, InitialMipData, NumInitialMips);
}

FORCEINLINE void RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D, FTexture2DRHIParamRef SrcTexture2D)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CopySharedMips(DestTexture2D, SrcTexture2D);
}

FORCEINLINE FTexture2DArrayRHIRef RHICreateTexture2DArray(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTexture2DArray(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
}

FORCEINLINE FTexture3DRHIRef RHICreateTexture3D(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTexture3D(SizeX, SizeY, SizeZ, Format, NumMips, Flags, CreateInfo);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture2DRHI, MipLevel);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture2DRHI, MipLevel, NumMipLevels, Format);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture3DRHI, MipLevel);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(Texture2DArrayRHI, MipLevel);
}

FORCEINLINE FShaderResourceViewRHIRef RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateShaderResourceView(TextureCubeRHI, MipLevel);
}

FORCEINLINE FTexture2DRHIRef RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	return FRHICommandListExecutor::GetImmediateCommandList().AsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
}

FORCEINLINE ETextureReallocationStatus RHIFinalizeAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return FRHICommandListExecutor::GetImmediateCommandList().FinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

FORCEINLINE ETextureReallocationStatus RHICancelAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

FORCEINLINE void* RHILockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bFlushRHIThread = true)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, bFlushRHIThread);
}

FORCEINLINE void RHIUnlockTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bFlushRHIThread = true)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockTexture2D(Texture, MipIndex, bLockWithinMiptail, bFlushRHIThread);
}

FORCEINLINE void* RHILockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockTexture2DArray(Texture, TextureIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

FORCEINLINE void RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef Texture, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockTexture2DArray(Texture, TextureIndex, MipIndex, bLockWithinMiptail);
}

FORCEINLINE void RHIUpdateTexture2D(FTexture2DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UpdateTexture2D(Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
}

FORCEINLINE void RHIUpdateTexture3D(FTexture3DRHIParamRef Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UpdateTexture3D(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
}

FORCEINLINE FTextureCubeRHIRef RHICreateTextureCube(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTextureCube(Size, Format, NumMips, Flags, CreateInfo);
}

FORCEINLINE FTextureCubeRHIRef RHICreateTextureCubeArray(uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateTextureCubeArray(Size, ArraySize, Format, NumMips, Flags, CreateInfo);
}

FORCEINLINE void* RHILockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	return FRHICommandListExecutor::GetImmediateCommandList().LockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
}

FORCEINLINE void RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	 FRHICommandListExecutor::GetImmediateCommandList().UnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
}

FORCEINLINE FRenderQueryRHIRef RHICreateRenderQuery(ERenderQueryType QueryType)
{
	return FRHICommandListExecutor::GetImmediateCommandList().CreateRenderQuery(QueryType);
}

FORCEINLINE void RHIAcquireThreadOwnership()
{
	return FRHICommandListExecutor::GetImmediateCommandList().AcquireThreadOwnership();
}

FORCEINLINE void RHIReleaseThreadOwnership()
{
	return FRHICommandListExecutor::GetImmediateCommandList().ReleaseThreadOwnership();
}

FORCEINLINE void RHIFlushResources()
{
	return FRHICommandListExecutor::GetImmediateCommandList().FlushResources();
}

FORCEINLINE void RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef Texture, uint32 FirstMip)
{
	 FRHICommandListExecutor::GetImmediateCommandList().VirtualTextureSetFirstMipInMemory(Texture, FirstMip);
}

FORCEINLINE void RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef Texture, uint32 FirstMip)
{
	 FRHICommandListExecutor::GetImmediateCommandList().VirtualTextureSetFirstMipVisible(Texture, FirstMip);
}

FORCEINLINE void RHIExecuteCommandList(FRHICommandList* CmdList)
{
	 FRHICommandListExecutor::GetImmediateCommandList().ExecuteCommandList(CmdList);
}

FORCEINLINE void* RHIGetNativeDevice()
{
	return FRHICommandListExecutor::GetImmediateCommandList().GetNativeDevice();
}

FORCEINLINE void RHIRecreateRecursiveBoundShaderStates()
{
	FRHICommandListExecutor::GetImmediateCommandList(). ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	GDynamicRHI->RHIRecreateRecursiveBoundShaderStates();
}



#include "RHICommandList.inl"
