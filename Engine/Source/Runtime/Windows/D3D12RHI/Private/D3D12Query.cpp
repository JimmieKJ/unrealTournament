// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Query.cpp: D3D query RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

namespace D3D12RHI
{
/**
* RHI console variables used by queries.
*/
namespace RHIConsoleVariables
{
    int32 bStablePowerState = 0;
    static FAutoConsoleVariableRef CVarStablePowerState(
        TEXT("D3D12.StablePowerState"),
        bStablePowerState,
        TEXT("If true, enable stable power state. This increases GPU timing measurement accuracy but may decrease overall GPU clock rate."),
        ECVF_Default
        );
}
}
using namespace D3D12RHI;

FRenderQueryRHIRef FD3D12DynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
    TRefCountPtr<ID3D12QueryHeap> queryHeap;
    TRefCountPtr<ID3D12Resource> queryResultBuffer;

	if(QueryType == RQT_Occlusion)
	{
        // Don't actually allocate an element until BeginQuery
        GetRHIDevice()->GetQueryHeap()->ReserveElement();
	}
	else if(QueryType == RQT_AbsoluteTime)
	{
        D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
        queryHeapDesc.Count = 1;

		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

        VERIFYD3D11RESULT(GetRHIDevice()->GetDevice()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(queryHeap.GetInitReference())));

        D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

        D3D12_RESOURCE_DESC heapDesc = CD3DX12_RESOURCE_DESC::Buffer(8);

        VERIFYD3D11RESULT(
            GetRHIDevice()->GetDevice()->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &heapDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(queryResultBuffer.GetInitReference())
            )
            );
	}
	else
	{
		check(0);
	}

	return new FD3D12OcclusionQuery(queryHeap, queryResultBuffer, QueryType);
}

bool FD3D12DynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI,uint64& OutResult,bool bWait)
{
	check(IsInRenderingThread());
	FD3D12OcclusionQuery* Query = FD3D12DynamicRHI::ResourceCast(QueryRHI);

	bool bSuccess = true;
	if (Query != NULL)
	{
		if (!Query->bResultIsCached)
		{
            Query->bResultIsCached = GetRHIDevice()->GetQueryData(*Query, bWait);
		}

		if (Query->Type == RQT_AbsoluteTime)
		{
			// GetTimingFrequency is the number of ticks per second
			uint64 Div = FGPUTiming::GetTimingFrequency() / (1000 * 1000);

			// convert from GPU specific timestamp to micro sec (1 / 1 000 000 s) which seems a reasonable resolution
			OutResult = Query->Result / Div;
		}
		else
		{
			OutResult = Query->Result;
		}
	}
	return bSuccess;
}

bool FD3D12Device::GetQueryData(FD3D12OcclusionQuery& Query, bool bWait)
{
	FD3D12CommandContext& CmdContext = (Query.Type == RQT_Occlusion) ? OcclusionQueryHeap.GetOwningContext() : *Query.OwningContext;
	FD3D12CLSyncPoint SyncPoint = (Query.Type == RQT_Occlusion) ? OcclusionQueryHeap.GetSyncPoint() : Query.OwningCommandList;
    CommandListState ListState = GetCommandListManager().GetCommandListState(SyncPoint);

	if (ListState != CommandListState::kFinished)
	{
		if (!bWait)
			return false;

		if (ListState == CommandListState::kOpen)
			CmdContext.FlushCommands(true);
		else
			GetCommandListManager().WaitForCompletion(SyncPoint);
	}
	
	// Read the data from the query's buffer.
	if (Query.Type == RQT_Occlusion)
	{
		const uint32 BeginOffset = Query.HeapIndex * sizeof(Query.Result);
		CD3DX12_RANGE range(BeginOffset, BeginOffset + sizeof(Query.Result));
		VERIFYD3D11RESULT(OcclusionQueryHeap.MapResultBufferRange(range));
		FMemory::Memcpy(&Query.Result, OcclusionQueryHeap.GetResultBufferData(range), sizeof(Query.Result));
		OcclusionQueryHeap.UnmapResultBufferRange();
        return true;
	}
	else
	{
        void* pSrcData = nullptr;
        HRESULT Result = Query.ResultBuffer->Map(0, nullptr, &pSrcData);
        if (Result == S_OK)
        {
            // Copy the data from the query result buffer to the destination
            FMemory::Memcpy(&Query.Result, pSrcData, sizeof(Query.Result));

            // Unmap
            Query.ResultBuffer->Unmap(0, nullptr);

            return true;
        }
        else if (Result == DXGI_ERROR_DEVICE_REMOVED || Result == DXGI_ERROR_DEVICE_RESET || Result == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
        {
            bDeviceRemoved = true;
            return false;
        }
        else
        {
            VERIFYD3D11RESULT(Result);
            return false;
        }
	}
}

void FD3D12CommandContext::RHIBeginOcclusionQueryBatch()
{
	GetParentDevice()->GetQueryHeap()->StartQueryBatch(*this);
}

void FD3D12CommandContext::RHIEndOcclusionQueryBatch()
{
	GetParentDevice()->GetQueryHeap()->EndQueryBatchAndResolveQueryData(*this, D3D12_QUERY_TYPE_OCCLUSION);
	
	// MSFT: Seb: Do we still need to do this if we support parallel execute?
	// Break up the command list here so that the wait on the previous frame's results don't block.
	FlushCommands();
}

/*=============================================================================
* class FD3D12QueryHeap
*=============================================================================*/

FD3D12QueryHeap::FD3D12QueryHeap(FD3D12Device* InParent, const D3D12_QUERY_HEAP_TYPE &InQueryHeapType, uint32 InQueryHeapCount)
:HeadActiveElement(0)
, TailActiveElement(0)
, ActiveAllocatedElementCount(0)
, LastAllocatedElement(InQueryHeapCount - 1)
, ReservedElementCount(0)
, ResultSize(8)
, OwningContext(nullptr)
, pResultData(nullptr)
, FD3D12DeviceChild(InParent)
, LastBatch(MAX_ACTIVE_BATCHES - 1)
{
	if (InQueryHeapType == D3D12_QUERY_HEAP_TYPE_OCCLUSION)
		QueryType = D3D12_QUERY_TYPE_OCCLUSION;

	else if (InQueryHeapType == D3D12_QUERY_HEAP_TYPE_TIMESTAMP)
		QueryType = D3D12_QUERY_TYPE_TIMESTAMP;

    // Setup the query heap desc
    QueryHeapDesc = {};
    QueryHeapDesc.Type = InQueryHeapType;
    QueryHeapDesc.Count = InQueryHeapCount;

    CurrentQueryBatch.Clear();

    ActiveQueryBatches.Reserve(MAX_ACTIVE_BATCHES);
    ActiveQueryBatches.AddZeroed(MAX_ACTIVE_BATCHES);

    // Don't Init() until the RHI has created the device
}

FD3D12QueryHeap::~FD3D12QueryHeap()
{
    // Unmap the result buffer
    if (pResultData)
    {
        ResultBuffer->GetResource()->Unmap(0, nullptr);
        pResultData = nullptr;
    }
}

void FD3D12QueryHeap::Init()
{
    check(GetParentDevice());
    check(GetParentDevice()->GetDevice());

    // Create the query heap
    CreateQueryHeap();

    // Create the result buffer
    CreateResultBuffer();
}

uint32 FD3D12QueryHeap::ReserveElement()
{
    // Increment the reserved count
    ReservedElementCount++;

    // Make sure we have enough room in the query heap
    if (ReservedElementCount > GetQueryHeapCount())
    {
        // We need more queries than what currently fit in the query heap. Need to grow/rollover.
        // MSFT: TODO: Do some checks to make sure active heap elements rollover?
        check(0);
    }

    return ReservedElementCount;
}

uint32 FD3D12QueryHeap::GetNextElement(uint32 InElement)
{
    // Increment the provided element
    InElement++;

    // See if we need to wrap around to the begining of the heap
    if (InElement >= GetQueryHeapCount())
    {
        InElement = 0;
    }

    return InElement;
}

uint32 FD3D12QueryHeap::GetPreviousElement(uint32 InElement)
{
    // Decrement the provided element
    InElement--;

    // See if we need to wrap around to the end of the heap
    if (InElement == UINT_MAX)
    {
        InElement = GetQueryHeapCount() - 1;
    }

    return InElement;
}

uint32 FD3D12QueryHeap::GetNextBatchElement(uint32 InBatchElement)
{
    // Increment the provided element
    InBatchElement++;

    // See if we need to wrap around to the begining of the heap
    if (InBatchElement >= MAX_ACTIVE_BATCHES)
    {
        InBatchElement = 0;
    }

    return InBatchElement;
}

uint32 FD3D12QueryHeap::GetPreviousBatchElement(uint32 InBatchElement)
{
    // Decrement the provided element
    InBatchElement--;

    // See if we need to wrap around to the end of the heap
    if (InBatchElement == UINT_MAX)
    {
        InBatchElement = MAX_ACTIVE_BATCHES - 1;
    }

    return InBatchElement;
}

bool FD3D12QueryHeap::IsHeapFull()
{
    // Find the next element after the active tail and compare with the head
    return (GetNextElement(TailActiveElement) == HeadActiveElement);
}

uint32 FD3D12QueryHeap::AllocQuery(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType)
{
    check(CurrentQueryBatch.bOpen);

    // Get the element for this allocation
    const uint32 CurrentElement = GetNextElement(LastAllocatedElement);

    if (CurrentQueryBatch.StartElement > CurrentElement)
    {
        // We're in the middle of a batch, but we're at the end of the heap
        // We need to split the batch in two and resolve the first piece
        EndQueryBatchAndResolveQueryData(CmdContext, InQueryType);

        StartQueryBatch(CmdContext);
    }

    // Increment the count for the current batch
    CurrentQueryBatch.ElementCount++;

    LastAllocatedElement = CurrentElement;
    check(CurrentElement < GetQueryHeapCount());
    return CurrentElement;
}

void FD3D12QueryHeap::StartQueryBatch(FD3D12CommandContext& CmdContext)
{
    check(&CmdContext == &GetParentDevice()->GetDefaultCommandContext());
    check(!CurrentQueryBatch.bOpen);

	OwningContext = &CmdContext;
	SyncPoint = CmdContext.CommandListHandle;

    // Clear the current batch
    CurrentQueryBatch.Clear();

    // Start a new batch
    CurrentQueryBatch.StartElement = GetNextElement(LastAllocatedElement);
    CurrentQueryBatch.bOpen = true;
}

void FD3D12QueryHeap::EndQueryBatchAndResolveQueryData(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType)
{
    check(CurrentQueryBatch.bOpen);
	check(&CmdContext == OwningContext);
	//check(CommandListHandle == CmdContext.CommandListHandle);

    // Close the current batch
    CurrentQueryBatch.bOpen = false;

    // Discard empty batches
    if (CurrentQueryBatch.ElementCount == 0)
    {
        return;
    }

    // Update the end element
    CurrentQueryBatch.EndElement = CurrentQueryBatch.StartElement + CurrentQueryBatch.ElementCount - 1;

    // Update the tail
    TailActiveElement = CurrentQueryBatch.EndElement;
    check(TailActiveElement < GetQueryHeapCount());

    // Increment the active element count
    ActiveAllocatedElementCount += CurrentQueryBatch.ElementCount;
	checkf(ActiveAllocatedElementCount <= GetQueryHeapCount(), TEXT("The query heap is too small. Either increase the heap count (larger resource) or decrease MAX_ACTIVE_BATCHES."));

    // Track the current active batches (application is using the data)
    LastBatch = GetNextBatchElement(LastBatch);
    ActiveQueryBatches[LastBatch] = CurrentQueryBatch;

    // Update the head
    QueryBatch& OldestBatch = ActiveQueryBatches[GetNextBatchElement(LastBatch)];
    HeadActiveElement = OldestBatch.StartElement;
    ActiveAllocatedElementCount -= OldestBatch.ElementCount;

	CmdContext.otherWorkCounter++;
    CmdContext.CommandListHandle->ResolveQueryData(
		QueryHeap, InQueryType, CurrentQueryBatch.StartElement, CurrentQueryBatch.ElementCount,
        ResultBuffer->GetResource(), GetResultBufferOffsetForElement(CurrentQueryBatch.StartElement));
}

uint32 FD3D12QueryHeap::BeginQuery(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType)
{
    const uint32 Element = AllocQuery(CmdContext, InQueryType);
	CmdContext.otherWorkCounter++;
	CmdContext.CommandListHandle->BeginQuery(QueryHeap, InQueryType, Element);
    return Element;
}

void FD3D12QueryHeap::EndQuery(FD3D12CommandContext& CmdContext, D3D12_QUERY_TYPE InQueryType, uint32 InElement)
{
	CmdContext.otherWorkCounter++;
	CmdContext.CommandListHandle->EndQuery(QueryHeap, InQueryType, InElement);
}

void FD3D12QueryHeap::CreateQueryHeap()
{
    // Create the upload heap
    VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(QueryHeap.GetInitReference())));
}

void FD3D12QueryHeap::CreateResultBuffer()
{
    D3D12_HEAP_PROPERTIES ResultBufferHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
    D3D12_RESOURCE_DESC ResultBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ResultSize * QueryHeapDesc.Count); // Each query's result occupies ResultSize bytes.

    // Create the readback heap
    VERIFYD3D11RESULT(GetParentDevice()->GetResourceHelper().CreateCommittedResource(
        ResultBufferDesc, 
        ResultBufferHeapProperties, 
        D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
        ResultBuffer.GetInitReference()));

    // Map the result buffer (and keep it mapped)
    VERIFYD3D11RESULT(ResultBuffer->GetResource()->Map(0, nullptr, &pResultData));
}


/*=============================================================================
 * class FD3D12BufferedGPUTiming
 *=============================================================================*/

/**
 * Constructor.
 *
 * @param InD3DRHI			RHI interface
 * @param InBufferSize		Number of buffered measurements
 */
FD3D12BufferedGPUTiming::FD3D12BufferedGPUTiming( FD3D12Device* InParent, int32 InBufferSize )
: 	BufferSize( InBufferSize )
,	CurrentTimestamp( -1 )
,	NumIssuedTimestamps( 0 )
,   StartTimestampQueryHeap(nullptr)
,   StartTimestampListHandles(nullptr)
,   StartTimestampQueryHeapBuffer(nullptr)
,   StartTimestampQueryHeapBufferData(nullptr)
,   EndTimestampQueryHeap(nullptr)
,   EndTimestampListHandles(nullptr)
,   EndTimestampQueryHeapBuffer(nullptr)
,   EndTimestampQueryHeapBufferData(nullptr)
,	bIsTiming( false )
,   bStablePowerState( false)
,   FD3D12DeviceChild(InParent)
{
}

/**
 * Initializes the static variables, if necessary.
 */
void FD3D12BufferedGPUTiming::PlatformStaticInitialize(void* UserData)
{
	// Are the static variables initialized?
	check( !GAreGlobalsInitialized );

	GTimingFrequency = 0;
    FD3D12Device* ParentDevice = (FD3D12Device*) UserData;
    VERIFYD3D11RESULT(ParentDevice->GetCommandListManager().GetTimestampFrequency(&GTimingFrequency));
}

/**
 * Initializes all D3D resources and if necessary, the static variables.
 */
void FD3D12BufferedGPUTiming::InitDynamicRHI()
{
	StaticInitialize(GetParentDevice(), PlatformStaticInitialize);

	CurrentTimestamp = 0;
	NumIssuedTimestamps = 0;
	bIsTiming = false;

	// Now initialize the queries and backing buffers for this timing object.

	if (GIsSupported)
	{
		D3D12_QUERY_HEAP_DESC QueryHeapDesc = {};
		QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		QueryHeapDesc.Count = BufferSize;

		VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(StartTimestampQueryHeap.GetInitReference())));
		VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(EndTimestampQueryHeap.GetInitReference())));

		D3D12_HEAP_PROPERTIES queryHeapBufferHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

        D3D12_RESOURCE_DESC queryHeapBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(8 * BufferSize); // Each timestamp query occupies 8 bytes.
		
        VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateCommittedResource(&queryHeapBufferHeapProperties, D3D12_HEAP_FLAG_NONE, &queryHeapBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(StartTimestampQueryHeapBuffer.GetInitReference())));
		VERIFYD3D11RESULT(StartTimestampQueryHeapBuffer->Map(0, nullptr, reinterpret_cast<void**>(&StartTimestampQueryHeapBufferData)));
		
        VERIFYD3D11RESULT(GetParentDevice()->GetDevice()->CreateCommittedResource(&queryHeapBufferHeapProperties, D3D12_HEAP_FLAG_NONE, &queryHeapBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(EndTimestampQueryHeapBuffer.GetInitReference())));
        VERIFYD3D11RESULT(EndTimestampQueryHeapBuffer->Map(0, nullptr, reinterpret_cast<void**>(&EndTimestampQueryHeapBufferData)));

		StartTimestampListHandles = new FD3D12CLSyncPoint[BufferSize];
		ZeroMemory(StartTimestampListHandles, sizeof(StartTimestampListHandles));
		EndTimestampListHandles = new FD3D12CLSyncPoint[BufferSize];
		ZeroMemory(EndTimestampListHandles, sizeof(EndTimestampListHandles));
	}
}

/**
 * Releases all D3D resources.
 */
void FD3D12BufferedGPUTiming::ReleaseDynamicRHI()
{
	StartTimestampQueryHeap = nullptr;
    if (StartTimestampListHandles)
    {
        delete[] StartTimestampListHandles;
        StartTimestampListHandles = nullptr;
    }
	StartTimestampQueryHeapBuffer = nullptr;

	EndTimestampQueryHeap = nullptr;
    if (EndTimestampListHandles)
    {
        delete[] EndTimestampListHandles;
        EndTimestampListHandles = nullptr;
    }
	EndTimestampQueryHeapBuffer = nullptr;
}

/**
 * Start a GPU timing measurement.
 */
void FD3D12BufferedGPUTiming::StartTiming()
{
	// Issue a timestamp query for the 'start' time.
	if (GIsSupported && !bIsTiming)
	{
        // Check to see if stable power state cvar has changed
        bool bStablePowerStateCVar = RHIConsoleVariables::bStablePowerState != 0;
        if (bStablePowerState != bStablePowerStateCVar)
        {
            if (SUCCEEDED(GetParentDevice()->GetDevice()->SetStablePowerState(bStablePowerStateCVar)))
            {
                // SetStablePowerState succeeded. Update timing frequency.
                VERIFYD3D11RESULT(GetParentDevice()->GetCommandListManager().GetTimestampFrequency(&GTimingFrequency));
                bStablePowerState = bStablePowerStateCVar;
            }
            else
            {
                // SetStablePowerState failed. This can occur if SDKLayers is not present on the system.
                RHIConsoleVariables::CVarStablePowerState->Set(0, ECVF_SetByConsole);
            }
        }
		int32 NewTimestampIndex = (CurrentTimestamp + 1) % BufferSize;
        FD3D12CommandContext& CmdContext = GetParentDevice()->GetDefaultCommandContext();
		CmdContext.otherWorkCounter += 2;
		CmdContext.CommandListHandle->EndQuery(StartTimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, NewTimestampIndex);
		CmdContext.CommandListHandle->ResolveQueryData(StartTimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, CurrentTimestamp, 1, StartTimestampQueryHeapBuffer, 8 * CurrentTimestamp);
		StartTimestampListHandles[NewTimestampIndex] = CmdContext.CommandListHandle;
		CurrentTimestamp = NewTimestampIndex;
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FD3D12BufferedGPUTiming::EndTiming()
{
	// Issue a timestamp query for the 'end' time.
	if (GIsSupported && bIsTiming)
	{
		check(CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize);
        FD3D12CommandContext& CmdContext = GetParentDevice()->GetDefaultCommandContext();
		CmdContext.otherWorkCounter += 2;
		CmdContext.CommandListHandle->EndQuery(EndTimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, CurrentTimestamp);
		CmdContext.CommandListHandle->ResolveQueryData(EndTimestampQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, CurrentTimestamp, 1, EndTimestampQueryHeapBuffer, 8 * CurrentTimestamp);
		EndTimestampListHandles[CurrentTimestamp] = CmdContext.CommandListHandle;
		NumIssuedTimestamps = FMath::Min<int32>(NumIssuedTimestamps + 1, BufferSize);
		bIsTiming = false;
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
uint64 FD3D12BufferedGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{
	if ( GIsSupported )
	{
		check( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );
		uint64 StartTime, EndTime;

		FD3D12CommandListManager& CommandListManager = GetParentDevice()->GetCommandListManager();

		int32 TimestampIndex = CurrentTimestamp;
		if (!bGetCurrentResultsAndBlock)
		{
			// Quickly check the most recent measurements to see if any of them has been resolved.  Do not flush these queries.
			for ( int32 IssueIndex = 1; IssueIndex < NumIssuedTimestamps; ++IssueIndex )
			{
				if (CommandListManager.IsFinished(EndTimestampListHandles[TimestampIndex]))
				{
					check(CommandListManager.IsFinished(StartTimestampListHandles[TimestampIndex]));

					StartTime = StartTimestampQueryHeapBufferData[TimestampIndex];
					EndTime = EndTimestampQueryHeapBufferData[TimestampIndex];

					if (EndTime > StartTime)
						return EndTime - StartTime;
				}

				TimestampIndex = (TimestampIndex + BufferSize - 1) % BufferSize;
			}
		}
		
		if ( NumIssuedTimestamps > 0 || bGetCurrentResultsAndBlock )
		{
			// None of the (NumIssuedTimestamps - 1) measurements were ready yet,
			// so check the oldest measurement more thoroughly.
			// This really only happens if occlusion and frame sync event queries are disabled, otherwise those will block until the GPU catches up to 1 frame behind

			const bool bBlocking = ( NumIssuedTimestamps == BufferSize ) || bGetCurrentResultsAndBlock;
			uint32 IdleStart = FPlatformTime::Cycles();
			double StartTimeoutTime = FPlatformTime::Seconds();

			SCOPE_CYCLE_COUNTER( STAT_RenderQueryResultTime );

			if (bBlocking)
				GetParentDevice()->GetDefaultCommandContext().FlushCommands(true);

			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;

			StartTime = StartTimestampQueryHeapBufferData[TimestampIndex];
			EndTime = EndTimestampQueryHeapBufferData[TimestampIndex];

			if (EndTime > StartTime)
				return EndTime - StartTime;
		}
	}

	return 0;
}