// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandList.h: RHI Command List definitions for queueing up & executing later.
=============================================================================*/

#pragma once
#include "LockFreeFixedSizeAllocator.h"
#include "TaskGraphInterfaces.h"

DECLARE_STATS_GROUP(TEXT("RHICmdList"), STATGROUP_RHICMDLIST, STATCAT_Advanced);

#define USE_RHICOMMAND_STATE_REDUCTION (0)

extern RHI_API TAutoConsoleVariable<int32> CVarRHICmdWidth;
class FRHICommandListBase;


class IRHICommandContext
{
public:
	virtual ~IRHICommandContext() {}

#define DEFINE_RHIMETHOD(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation)
#define DEFINE_RHIMETHOD_CMDLIST(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation) virtual Type RHI##Name ParameterTypesAndNames = 0
#include "RHIMethods.h"
#undef DEFINE_RHIMETHOD
#undef DEFINE_RHIMETHOD_CMDLIST
};

class IRHICommandContextContainer
{
public:
	virtual ~IRHICommandContextContainer() {}

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

class RHI_API FRHICommandListBase : public FNoncopyable
{
public:

	FRHICommandListBase();
	~FRHICommandListBase();

	inline void Flush();
	inline bool IsImmediate();
	const int32 GetUsedMemory() const;
	void QueueAsyncCommandListSubmit(FGraphEventRef& AnyThreadCompletionEvent, class FRHICommandList* CmdList);
	void QueueParallelAsyncCommandListSubmit(FGraphEventRef* AnyThreadCompletionEvents, class FRHICommandList** CmdLists, int32 Num);
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
		return MemManager.Alloc(sizeof(T), ALIGNOF(T));
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

	void CopyContext(FRHICommandListBase& ParentCommandList)
	{
		Context = ParentCommandList.Context;
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

	FMemStackBase MemManager; 
#if USE_RHICOMMAND_STATE_REDUCTION
protected:
	struct FRHICommandListStateCache* StateCache;
private:
#endif
	FGraphEventArray RTTasks;

	void Reset();

	friend class FRHICommandListExecutor;
	friend class FRHICommandListIterator;

public:
	FDrawUpData DrawUPData; 
};

#if USE_RHICOMMAND_STATE_REDUCTION
struct FRHICommandListStateCache
{
	// warning, no destructor is ever called for this struct

	FRHICommandListStateCache()
	{
		FlushShaderState();
		FlushSamplerState();
	}
	enum 
	{
		MAX_SAMPLERS_PER_SHADER_STAGE = 32, // it is okish if this is too small, those buffers simply won't get state reduction
		MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE = 14, // it is okish if this is too small, those buffers simply won't get state reduction
	};
	FSamplerStateRHIParamRef Samplers[SF_NumFrequencies][MAX_SAMPLERS_PER_SHADER_STAGE];
	FUniformBufferRHIParamRef BoundUniformBuffers[SF_NumFrequencies][MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE];

	FORCEINLINE void FlushSamplerState()
	{
		for (int32 Index = 0; Index < SF_NumFrequencies; Index++)
		{
			for (int32 IndexInner = 0; IndexInner < MAX_SAMPLERS_PER_SHADER_STAGE; IndexInner++)
			{
				Samplers[Index][IndexInner] = nullptr;
			}
		}
	}

	FORCEINLINE void FlushShaderState()
	{
		for (int32 Index = 0; Index < SF_NumFrequencies; Index++)
		{
			for (int32 IndexInner = 0; IndexInner < MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE; IndexInner++)
			{
				BoundUniformBuffers[Index][IndexInner] = nullptr;
			}
		}
	}
};
#endif


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

template <typename TShaderRHIParamRef>
struct FRHICommandSetShaderParameter : public FRHICommand<FRHICommandSetShaderParameter<TShaderRHIParamRef> >
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

template <typename TShaderRHIParamRef>
struct FRHICommandSetShaderUniformBuffer : public FRHICommand<FRHICommandSetShaderUniformBuffer<TShaderRHIParamRef> >
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

template <typename TShaderRHIParamRef>
struct FRHICommandSetShaderTexture : public FRHICommand<FRHICommandSetShaderTexture<TShaderRHIParamRef> >
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

template <typename TShaderRHIParamRef>
struct FRHICommandSetShaderResourceViewParameter : public FRHICommand<FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef> >
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

template <typename TShaderRHIParamRef>
struct FRHICommandSetUAVParameter : public FRHICommand<FRHICommandSetUAVParameter<TShaderRHIParamRef> >
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

template <typename TShaderRHIParamRef>
struct FRHICommandSetUAVParameter_IntialCount : public FRHICommand<FRHICommandSetUAVParameter_IntialCount<TShaderRHIParamRef> >
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

template <typename TShaderRHIParamRef>
struct FRHICommandSetShaderSampler : public FRHICommand<FRHICommandSetShaderSampler<TShaderRHIParamRef> >
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
	FLinearColor ColorArray[MaxSimultaneousRenderTargets];	
	float Depth;
	uint32 Stencil;
	int32 NumClearColors;
	bool bClearColor;
	bool bClearDepth;
	bool bClearStencil;

	FORCEINLINE_DEBUGGABLE FRHICommandBindClearMRTValues(
		bool InbClearColor,
		int32 InNumClearColors,
		const FLinearColor* InColorArray,
		bool InbClearDepth,
		float InDepth,
		bool InbClearStencil,
		uint32 InStencil		
		) 
		: Depth(InDepth)
		, Stencil(InStencil)
		, NumClearColors(InNumClearColors)
		, bClearColor(InbClearColor)
		, bClearDepth(InbClearDepth)
		, bClearStencil(InbClearStencil)
	{
		check(InNumClearColors < MaxSimultaneousRenderTargets);
		for (int32 Index = 0; Index < InNumClearColors; Index++)
		{
			ColorArray[Index] = InColorArray[Index];
		}
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

struct FRHICommandSetComputeShader : public FRHICommand<FRHICommandSetComputeShader>
{
	FComputeShaderRHIParamRef ComputeShader;
	FORCEINLINE_DEBUGGABLE FRHICommandSetComputeShader(FComputeShaderRHIParamRef InComputeShader)
		: ComputeShader(InComputeShader)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandDispatchComputeShader : public FRHICommand<FRHICommandDispatchComputeShader>
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

struct FRHICommandDispatchIndirectComputeShader : public FRHICommand<FRHICommandDispatchIndirectComputeShader>
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

struct FRHICommandSubmitCommandsHint : public FRHICommand<FRHICommandSubmitCommandsHint>
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

struct FRHICommandUpdateVertexBuffer : public FRHICommand<FRHICommandUpdateVertexBuffer>
{
	FVertexBufferRHIParamRef VertexBuffer;
	void const* Buffer;
	int32 BufferSize;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateVertexBuffer(FVertexBufferRHIParamRef InVertexBuffer, void const* InBuffer, int32 InBufferSize)
		: VertexBuffer(InVertexBuffer)
		, Buffer(InBuffer)
		, BufferSize(InBufferSize)
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

struct FRHICommandPushEvent : public FRHICommand<FRHICommandPushEvent>
{
	const TCHAR *Name;

	FORCEINLINE_DEBUGGABLE FRHICommandPushEvent(const TCHAR *InName)
		: Name(InName)
	{
	}
	RHI_API void Execute(FRHICommandListBase& CmdList);
};

struct FRHICommandPopEvent : public FRHICommand<FRHICommandPopEvent>
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

#define CMD_CONTEXT(Method) GetContext().RHI##Method


class RHI_API FRHICommandList : public FRHICommandListBase
{
public:

	/** Custom new/delete with recycling */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

#if USE_RHICOMMAND_STATE_REDUCTION
	FORCEINLINE_DEBUGGABLE bool AllocStateCache()
	{
		if (!StateCache)
		{
			StateCache = new (Alloc<FRHICommandListStateCache>()) FRHICommandListStateCache();
			return false;
		}
		return true;
	}
#endif
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
			CMD_CONTEXT(SetBoundShaderState)(LocalBoundShaderState.BypassBSS);
			return;
		}
#if USE_RHICOMMAND_STATE_REDUCTION
		if (StateCache)
		{
			StateCache->FlushShaderState();
		}
#endif
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
			CMD_CONTEXT(SetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer.BypassUniform);
			return;
		}
#if USE_RHICOMMAND_STATE_REDUCTION
		if (StateCache)
		{
			//local uniform buffers are rare, so we will just flush
			StateCache->BoundUniformBuffers[TRHIShaderToEnum<TShaderRHIParamRef>::ShaderFrequency][BaseIndex] = nullptr;
		}
#endif
		new (AllocCommand<FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef> >()) FRHICommandSetLocalUniformBuffer<TShaderRHIParamRef>(this, Shader, BaseIndex, UniformBuffer);
	}

	template <typename TShaderRHI>
	FORCEINLINE_DEBUGGABLE void SetShaderUniformBuffer(TShaderRHI* Shader, uint32 BaseIndex, FUniformBufferRHIParamRef UniformBuffer)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetShaderUniformBuffer)(Shader, BaseIndex, UniformBuffer);
			return;
		}
#if USE_RHICOMMAND_STATE_REDUCTION
		if (BaseIndex < FRHICommandListStateCache::MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE)
		{
			if (AllocStateCache() && StateCache->BoundUniformBuffers[TRHIShaderToEnum<TShaderRHI*>::ShaderFrequency][BaseIndex] == UniformBuffer)
			{
				return;
			}
			StateCache->BoundUniformBuffers[TRHIShaderToEnum<TShaderRHI*>::ShaderFrequency][BaseIndex] = UniformBuffer;
		}
#endif
		new (AllocCommand<FRHICommandSetShaderUniformBuffer<TShaderRHI*> >()) FRHICommandSetShaderUniformBuffer<TShaderRHI*>(Shader, BaseIndex, UniformBuffer);
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
			CMD_CONTEXT(SetShaderParameter)(Shader, BufferIndex, BaseIndex, NumBytes, NewValue);
			return;
		}
		void* UseValue = Alloc(NumBytes, 16);
		FMemory::Memcpy(UseValue, NewValue, NumBytes);
		new (AllocCommand<FRHICommandSetShaderParameter<TShaderRHI*> >()) FRHICommandSetShaderParameter<TShaderRHI*>(Shader, BufferIndex, BaseIndex, NumBytes, UseValue);
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
			CMD_CONTEXT(SetShaderTexture)(Shader, TextureIndex, Texture);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderTexture<TShaderRHIParamRef> >()) FRHICommandSetShaderTexture<TShaderRHIParamRef>(Shader, TextureIndex, Texture);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderResourceViewParameter(TShaderRHIParamRef Shader, uint32 SamplerIndex, FShaderResourceViewRHIParamRef SRV)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetShaderResourceViewParameter)(Shader, SamplerIndex, SRV);
			return;
		}
		new (AllocCommand<FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef> >()) FRHICommandSetShaderResourceViewParameter<TShaderRHIParamRef>(Shader, SamplerIndex, SRV);
	}

	template <typename TShaderRHIParamRef>
	FORCEINLINE_DEBUGGABLE void SetShaderSampler(TShaderRHIParamRef Shader, uint32 SamplerIndex, FSamplerStateRHIParamRef State)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetShaderSampler)(Shader, SamplerIndex, State);
			return;
		}
#if USE_RHICOMMAND_STATE_REDUCTION
		if (SamplerIndex < FRHICommandListStateCache::MAX_SAMPLERS_PER_SHADER_STAGE)
		{
			if (AllocStateCache() && StateCache->Samplers[TRHIShaderToEnum<TShaderRHIParamRef>::ShaderFrequency][SamplerIndex] == State)
			{
				return;
			}
			StateCache->Samplers[TRHIShaderToEnum<TShaderRHIParamRef>::ShaderFrequency][SamplerIndex] = State;
		}
#endif
		new (AllocCommand<FRHICommandSetShaderSampler<TShaderRHIParamRef> >()) FRHICommandSetShaderSampler<TShaderRHIParamRef>(Shader, SamplerIndex, State);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetUAVParameter)(Shader, UAVIndex, UAV);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter<FComputeShaderRHIParamRef> >()) FRHICommandSetUAVParameter<FComputeShaderRHIParamRef>(Shader, UAVIndex, UAV);
	}

	FORCEINLINE_DEBUGGABLE void SetUAVParameter(FComputeShaderRHIParamRef Shader, uint32 UAVIndex, FUnorderedAccessViewRHIParamRef UAV, uint32 InitialCount)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetUAVParameter)(Shader, UAVIndex, UAV, InitialCount);
			return;
		}
		new (AllocCommand<FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef> >()) FRHICommandSetUAVParameter_IntialCount<FComputeShaderRHIParamRef>(Shader, UAVIndex, UAV, InitialCount);
	}

	FORCEINLINE_DEBUGGABLE void SetBoundShaderState(FBoundShaderStateRHIParamRef BoundShaderState)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetBoundShaderState)(BoundShaderState);
			return;
		}
#if USE_RHICOMMAND_STATE_REDUCTION
		if (StateCache)
		{
			StateCache->FlushShaderState();
		}
#endif
		new (AllocCommand<FRHICommandSetBoundShaderState>()) FRHICommandSetBoundShaderState(BoundShaderState);
	}

	FORCEINLINE_DEBUGGABLE void SetRasterizerState(FRasterizerStateRHIParamRef State)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetRasterizerState)(State);
			return;
		}
		new (AllocCommand<FRHICommandSetRasterizerState>()) FRHICommandSetRasterizerState(State);
	}

	FORCEINLINE_DEBUGGABLE void SetBlendState(FBlendStateRHIParamRef State, const FLinearColor& BlendFactor = FLinearColor::White)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetBlendState)(State, BlendFactor);
			return;
		}
		new (AllocCommand<FRHICommandSetBlendState>()) FRHICommandSetBlendState(State, BlendFactor);
	}

	FORCEINLINE_DEBUGGABLE void DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		if (Bypass())
		{
			CMD_CONTEXT(DrawPrimitive)(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawPrimitive>()) FRHICommandDrawPrimitive(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBuffer, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
	{
		if (Bypass())
		{
			CMD_CONTEXT(DrawIndexedPrimitive)(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedPrimitive>()) FRHICommandDrawIndexedPrimitive(IndexBuffer, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void SetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBuffer, uint32 Stride, uint32 Offset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetStreamSource)(StreamIndex, VertexBuffer, Stride, Offset);
			return;
		}
		new (AllocCommand<FRHICommandSetStreamSource>()) FRHICommandSetStreamSource(StreamIndex, VertexBuffer, Stride, Offset);
	}

	FORCEINLINE_DEBUGGABLE void SetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI, uint32 StencilRef = 0)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetDepthStencilState)(NewStateRHI, StencilRef);
			return;
		}
		new (AllocCommand<FRHICommandSetDepthStencilState>()) FRHICommandSetDepthStencilState(NewStateRHI, StencilRef);
	}

	FORCEINLINE_DEBUGGABLE void SetViewport(uint32 MinX, uint32 MinY, float MinZ, uint32 MaxX, uint32 MaxY, float MaxZ)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetViewport)(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
			return;
		}
		new (AllocCommand<FRHICommandSetViewport>()) FRHICommandSetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
	}

	FORCEINLINE_DEBUGGABLE void SetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
	{
		if (Bypass())
		{
			CMD_CONTEXT(SetScissorRect)(bEnable, MinX, MinY, MaxX, MaxY);
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
			CMD_CONTEXT(SetRenderTargets)(
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
			CMD_CONTEXT(SetRenderTargetsAndClear)(RenderTargetsInfo);
			return;
		}
		new (AllocCommand<FRHICommandSetRenderTargetsAndClear>()) FRHICommandSetRenderTargetsAndClear(RenderTargetsInfo);
	}	

	FORCEINLINE_DEBUGGABLE void BindClearMRTValues(bool bClearColor, int32 NumClearColors, const FLinearColor* ColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil)
	{
		if (Bypass())
		{
			CMD_CONTEXT(BindClearMRTValues)(bClearColor, NumClearColors, ColorArray, bClearDepth, Depth, bClearStencil, Stencil);
			return;
		}
		new (AllocCommand<FRHICommandBindClearMRTValues>()) FRHICommandBindClearMRTValues(bClearColor, NumClearColors, ColorArray, bClearDepth, Depth, bClearStencil, Stencil);
	}	

	FORCEINLINE_DEBUGGABLE void BeginDrawPrimitiveUP(uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
	{
		if (Bypass())
		{
			CMD_CONTEXT(BeginDrawPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData);
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
			CMD_CONTEXT(EndDrawPrimitiveUP)();
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
			CMD_CONTEXT(BeginDrawIndexedPrimitiveUP)(PrimitiveType, NumPrimitives, NumVertices, VertexDataStride, OutVertexData, MinVertexIndex, NumIndices, IndexDataStride, OutIndexData);
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
			CMD_CONTEXT(EndDrawIndexedPrimitiveUP)();
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
			CMD_CONTEXT(SetComputeShader)(ComputeShader);
			return;
		}
#if USE_RHICOMMAND_STATE_REDUCTION
		if (StateCache)
		{
			StateCache->FlushShaderState();
		}
#endif
		new (AllocCommand<FRHICommandSetComputeShader>()) FRHICommandSetComputeShader(ComputeShader);
	}

	FORCEINLINE_DEBUGGABLE void DispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
	{
		if (Bypass())
		{
			CMD_CONTEXT(DispatchComputeShader)(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
			return;
		}
		new (AllocCommand<FRHICommandDispatchComputeShader>()) FRHICommandDispatchComputeShader(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
	}

	FORCEINLINE_DEBUGGABLE void DispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(DispatchIndirectComputeShader)(ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDispatchIndirectComputeShader>()) FRHICommandDispatchIndirectComputeShader(ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void AutomaticCacheFlushAfterComputeShader(bool bEnable)
	{
		if (Bypass())
		{
			CMD_CONTEXT(AutomaticCacheFlushAfterComputeShader)(bEnable);
			return;
		}
		new (AllocCommand<FRHICommandAutomaticCacheFlushAfterComputeShader>()) FRHICommandAutomaticCacheFlushAfterComputeShader(bEnable);
	}

	FORCEINLINE_DEBUGGABLE void FlushComputeShaderCache()
	{
		if (Bypass())
		{
			CMD_CONTEXT(FlushComputeShaderCache)();
			return;
		}
		new (AllocCommand<FRHICommandFlushComputeShaderCache>()) FRHICommandFlushComputeShaderCache();
	}

	FORCEINLINE_DEBUGGABLE void DrawPrimitiveIndirect(uint32 PrimitiveType, FVertexBufferRHIParamRef ArgumentBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(DrawPrimitiveIndirect)(PrimitiveType, ArgumentBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDrawPrimitiveIndirect>()) FRHICommandDrawPrimitiveIndirect(PrimitiveType, ArgumentBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, uint32 DrawArgumentsIndex, uint32 NumInstances)
	{
		if (Bypass())
		{
			CMD_CONTEXT(DrawIndexedIndirect)(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedIndirect>()) FRHICommandDrawIndexedIndirect(IndexBufferRHI, PrimitiveType, ArgumentsBufferRHI, DrawArgumentsIndex, NumInstances);
	}

	FORCEINLINE_DEBUGGABLE void DrawIndexedPrimitiveIndirect(uint32 PrimitiveType, FIndexBufferRHIParamRef IndexBuffer, FVertexBufferRHIParamRef ArgumentsBuffer, uint32 ArgumentOffset)
	{
		if (Bypass())
		{
			CMD_CONTEXT(DrawIndexedPrimitiveIndirect)(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
			return;
		}
		new (AllocCommand<FRHICommandDrawIndexedPrimitiveIndirect>()) FRHICommandDrawIndexedPrimitiveIndirect(PrimitiveType, IndexBuffer, ArgumentsBuffer, ArgumentOffset);
	}

	FORCEINLINE_DEBUGGABLE void EnableDepthBoundsTest(bool bEnable, float MinDepth, float MaxDepth)
	{
		if (Bypass())
		{
			CMD_CONTEXT(EnableDepthBoundsTest)(bEnable, MinDepth, MaxDepth);
			return;
		}
		new (AllocCommand<FRHICommandEnableDepthBoundsTest>()) FRHICommandEnableDepthBoundsTest(bEnable, MinDepth, MaxDepth);
	}

	FORCEINLINE_DEBUGGABLE void ClearUAV(FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI, const uint32(&Values)[4])
	{
		if (Bypass())
		{
			CMD_CONTEXT(ClearUAV)(UnorderedAccessViewRHI, Values);
			return;
		}
		new (AllocCommand<FRHICommandClearUAV>()) FRHICommandClearUAV(UnorderedAccessViewRHI, Values);
	}

	FORCEINLINE_DEBUGGABLE void CopyToResolveTarget(FTextureRHIParamRef SourceTextureRHI, FTextureRHIParamRef DestTextureRHI, bool bKeepOriginalSurface, const FResolveParams& ResolveParams)
	{
		if (Bypass())
		{
			CMD_CONTEXT(CopyToResolveTarget)(SourceTextureRHI, DestTextureRHI, bKeepOriginalSurface, ResolveParams);
			return;
		}
		new (AllocCommand<FRHICommandCopyToResolveTarget>()) FRHICommandCopyToResolveTarget(SourceTextureRHI, DestTextureRHI, bKeepOriginalSurface, ResolveParams);
	}

	FORCEINLINE_DEBUGGABLE void Clear(bool bClearColor, const FLinearColor& Color, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
	{
		if (Bypass())
		{
			CMD_CONTEXT(Clear)(bClearColor, Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
			return;
		}
		new (AllocCommand<FRHICommandClear>()) FRHICommandClear(bClearColor, Color, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
	}

	FORCEINLINE_DEBUGGABLE void ClearMRT(bool bClearColor, int32 NumClearColors, const FLinearColor* ClearColorArray, bool bClearDepth, float Depth, bool bClearStencil, uint32 Stencil, FIntRect ExcludeRect)
	{
		if (Bypass())
		{
			CMD_CONTEXT(ClearMRT)(bClearColor, NumClearColors, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
			return;
		}
		new (AllocCommand<FRHICommandClearMRT>()) FRHICommandClearMRT(bClearColor, NumClearColors, ClearColorArray, bClearDepth, Depth, bClearStencil, Stencil, ExcludeRect);
	}

	FORCEINLINE_DEBUGGABLE void UpdateVertexBuffer(FVertexBufferRHIParamRef VertexBuffer, void const* Buffer, int32 BufferSize)
	{
		if (Bypass())
		{
			void* Data = LockVertexBuffer_Internal(VertexBuffer, 0, BufferSize, RLM_WriteOnly);
			FMemory::Memcpy(Data, Buffer, BufferSize);
			UnlockVertexBuffer_Internal(VertexBuffer);
			return;
		}
		void* Data = Alloc(BufferSize, 16);
		FMemory::Memcpy(Data, Buffer, BufferSize);
		new (AllocCommand<FRHICommandUpdateVertexBuffer>()) FRHICommandUpdateVertexBuffer(VertexBuffer, Data, BufferSize);
	}
	FORCEINLINE_DEBUGGABLE void BeginRenderQuery(FRenderQueryRHIParamRef RenderQuery)
	{
		if (Bypass())
		{
			CMD_CONTEXT(BeginRenderQuery)(RenderQuery);
			return;
		}
		new (AllocCommand<FRHICommandBeginRenderQuery>()) FRHICommandBeginRenderQuery(RenderQuery);
	}
	FORCEINLINE_DEBUGGABLE void EndRenderQuery(FRenderQueryRHIParamRef RenderQuery)
	{
		if (Bypass())
		{
			CMD_CONTEXT(EndRenderQuery)(RenderQuery);
			return;
		}
		new (AllocCommand<FRHICommandEndRenderQuery>()) FRHICommandEndRenderQuery(RenderQuery);
	}

	FORCEINLINE_DEBUGGABLE void BeginOcclusionQueryBatch()
	{
		if (Bypass())
		{
			CMD_CONTEXT(BeginOcclusionQueryBatch)();
			return;
		}
		new (AllocCommand<FRHICommandBeginOcclusionQueryBatch>()) FRHICommandBeginOcclusionQueryBatch();
	}

	FORCEINLINE_DEBUGGABLE void EndOcclusionQueryBatch()
	{
		if (Bypass())
		{
			CMD_CONTEXT(EndOcclusionQueryBatch)();
			return;
		}
		new (AllocCommand<FRHICommandEndOcclusionQueryBatch>()) FRHICommandEndOcclusionQueryBatch();
	}

	FORCEINLINE_DEBUGGABLE void SubmitCommandsHint()
	{
		if (Bypass())
		{
			CMD_CONTEXT(SubmitCommandsHint)();
			return;
		}
		new (AllocCommand<FRHICommandSubmitCommandsHint>()) FRHICommandSubmitCommandsHint();
	}	

// These 6 are special in that they must be called on the immediate command list and they force a flush only when we are not doing RHI thread
	void BeginScene();
	void EndScene();
	void BeginDrawingViewport(FViewportRHIParamRef Viewport, FTextureRHIParamRef RenderTargetRHI);
	void EndDrawingViewport(FViewportRHIParamRef Viewport, bool bPresent, bool bLockToVsync);
	void BeginFrame();
	void EndFrame();

	FORCEINLINE_DEBUGGABLE void PushEvent(const TCHAR* Name)
	{
		if (Bypass())
		{
			CMD_CONTEXT(PushEvent)(Name);
			return;
		}
		int32 Len = FCString::Strlen(Name) + 1;
		TCHAR* NameCopy  = (TCHAR*)Alloc(Len * (int32)sizeof(TCHAR), (int32)sizeof(TCHAR));
		FCString::Strcpy(NameCopy, Len, Name);
		new (AllocCommand<FRHICommandPushEvent>()) FRHICommandPushEvent(NameCopy);
	}

	FORCEINLINE_DEBUGGABLE void PopEvent()
	{
		if (Bypass())
		{
			CMD_CONTEXT(PopEvent)();
			return;
		}
		new (AllocCommand<FRHICommandPopEvent>()) FRHICommandPopEvent();
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

namespace EImmediateFlushType
{
	enum Type
	{ 
		WaitForOutstandingTasksOnly = 0, 
		DispatchToRHIThread, 
		WaitForRHIThread, 
		WaitForDispatchToRHIThread,
		FlushRHIThread,
		FlushRHIThreadFlushResources
	};
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

	static bool bFlushedGlobal;	
	
	inline void ImmediateFlush(EImmediateFlushType::Type FlushType);

	void SetCurrentStat(TStatId Stat);

	static bool IsFullyFlushed()
	{
		return bFlushedGlobal;
	};

	#define DEFINE_RHIMETHOD_CMDLIST(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD(Type, Name, ParameterTypesAndNames, ParameterNames, ReturnStatement, NullImplementation) \
		FORCEINLINE_DEBUGGABLE Type Name ParameterTypesAndNames \
		{ \
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_##Name##_Flush); \
			ImmediateFlush(EImmediateFlushType::FlushRHIThread); \
			ReturnStatement Name##_Internal ParameterNames; \
		}
	#define DEFINE_RHIMETHOD_GLOBAL(Type, Name, ParameterTypesAndNames, ParameterNames, ReturnStatement, NullImplementation) \
		FORCEINLINE_DEBUGGABLE Type Name ParameterTypesAndNames \
		{ \
			if (GRHIThread) \
			{ \
				QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_##Name##_WaitRHI); \
				ImmediateFlush(EImmediateFlushType::WaitForRHIThread); \
			} \
			ReturnStatement Name##_Internal ParameterNames; \
		}
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE(Type, Name, ParameterTypesAndNames, ParameterNames, ReturnStatement, NullImplementation) \
		FORCEINLINE_DEBUGGABLE Type Name ParameterTypesAndNames \
		{ \
			ReturnStatement RHI##Name ParameterNames; \
		}
	#define DEFINE_RHIMETHOD_GLOBALFLUSH(Type, Name, ParameterTypesAndNames, ParameterNames, ReturnStatement, NullImplementation) \
		FORCEINLINE_DEBUGGABLE Type Name ParameterTypesAndNames \
		{ \
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_##Name##_Flush); \
			ImmediateFlush(EImmediateFlushType::FlushRHIThread); \
			TGuardValue<bool> GuardIsFlushedGlobal( bFlushedGlobal, true ); \
			ReturnStatement Name##_Internal ParameterNames; \
		}
	#include "RHIMethods.h"
	#undef DEFINE_RHIMETHOD
	#undef DEFINE_RHIMETHOD_CMDLIST
	#undef DEFINE_RHIMETHOD_GLOBAL
	#undef DEFINE_RHIMETHOD_GLOBALFLUSH
	#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE

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


class RHI_API FRHICommandListExecutor
{
public:
	enum
	{
		DefaultBypass = 1
	};
	FRHICommandListExecutor()
		: bLatchedBypass(!!DefaultBypass)
		, bLatchedUseParallelAlgorithms(false)
	{
	}
	static inline FRHICommandListImmediate& GetImmediateCommandList();

	void ExecuteList(FRHICommandListBase& CmdList);
	void ExecuteList(FRHICommandListImmediate& CmdList);
	void LatchBypass();

	static FGraphEventRef RHIThreadFence();
	static void WaitOnRHIThreadFence(FGraphEventRef& Fence);

	FORCEINLINE_DEBUGGABLE bool Bypass()
	{
#if !UE_BUILD_SHIPPING
		return bLatchedBypass;
#else
		return !!DefaultBypass;
#endif
	}
	FORCEINLINE_DEBUGGABLE bool UseParallelAlgorithms()
	{
#if !UE_BUILD_SHIPPING
		return bLatchedUseParallelAlgorithms;
#else
		return  FApp::ShouldUseThreadingForPerformance() && !Bypass();
#endif
	}
	static void CheckNoOutstandingCmdLists();
	static bool IsRHIThreadActive();

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
};

extern RHI_API FRHICommandListExecutor GRHICommandList;

FORCEINLINE_DEBUGGABLE FRHICommandListImmediate& FRHICommandListExecutor::GetImmediateCommandList()
{
	return GRHICommandList.CommandListImmediate;
}

struct FScopedCommandListWaitForTasks
{
	FRHICommandListImmediate& RHICmdList;

	FScopedCommandListWaitForTasks(FRHICommandListImmediate& InRHICmdList = FRHICommandListExecutor::GetImmediateCommandList())
		: RHICmdList(InRHICmdList)
	{
	}
	~FScopedCommandListWaitForTasks()
	{
		check(IsInRenderingThread());
		if (GRHIThread)
		{
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FScopedCommandListWaitForTasks_Dispatch);
				RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
			}
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FScopedCommandListWaitForTasks_WaitAsync);
				RHICmdList.ImmediateFlush(EImmediateFlushType::WaitForOutstandingTasksOnly);
			}
		}
		else
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FScopedCommandListWaitForTasks_Flush);
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}
	}
};

#define DEFINE_RHIMETHOD_CMDLIST(Type,Name,ParameterTypesAndNames,ParameterNames,ReturnStatement,NullImplementation)
#define DEFINE_RHIMETHOD(Type, Name, ParameterTypesAndNames, ParameterNames, ReturnStatement, NullImplementation)
#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE(Type, Name, ParameterTypesAndNames, ParameterNames, ReturnStatement, NullImplementation)

#define DEFINE_RHIMETHOD_GLOBAL(Type, Name, ParameterTypesAndNames, ParameterNames, ReturnStatement, NullImplementation) \
	FORCEINLINE_DEBUGGABLE Type RHI##Name ParameterTypesAndNames \
	{ \
		ReturnStatement FRHICommandListExecutor::GetImmediateCommandList().Name ParameterNames; \
	}

#define DEFINE_RHIMETHOD_GLOBALFLUSH(Type, Name, ParameterTypesAndNames, ParameterNames, ReturnStatement, NullImplementation) \
	FORCEINLINE_DEBUGGABLE Type RHI##Name ParameterTypesAndNames \
	{ \
		ReturnStatement FRHICommandListExecutor::GetImmediateCommandList().Name ParameterNames; \
	}
#include "RHIMethods.h"
#undef DEFINE_RHIMETHOD
#undef DEFINE_RHIMETHOD_CMDLIST
#undef DEFINE_RHIMETHOD_GLOBAL
#undef DEFINE_RHIMETHOD_GLOBALFLUSH
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE


#include "RHICommandList.inl"
