// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"

DEFINE_STAT(STAT_MetalMakeDrawableTime);
DEFINE_STAT(STAT_MetalDrawCallTime);
DEFINE_STAT(STAT_MetalPrepareDrawTime);
DEFINE_STAT(STAT_MetalUniformBufferCleanupTime);
DEFINE_STAT(STAT_MetalTotalUniformBufferMemory);
DEFINE_STAT(STAT_MetalFreeUniformBufferMemory);
DEFINE_STAT(STAT_MetalNumFreeUniformBuffers);
DEFINE_STAT(STAT_MetalPipelineStateTime);
DEFINE_STAT(STAT_MetalBoundShaderStateTime);
DEFINE_STAT(STAT_MetalVertexDeclarationTime);
DEFINE_STAT(STAT_MetalBufferPageOffTime);
DEFINE_STAT(STAT_MetalTexturePageOffTime);
DEFINE_STAT(STAT_MetalBufferCount);
DEFINE_STAT(STAT_MetalTextureCount);
DEFINE_STAT(STAT_MetalCommandBufferCount);
DEFINE_STAT(STAT_MetalSamplerStateCount);
DEFINE_STAT(STAT_MetalDepthStencilStateCount);
DEFINE_STAT(STAT_MetalRenderPipelineStateCount);
DEFINE_STAT(STAT_MetalRenderPipelineColorAttachmentDescriptor);
DEFINE_STAT(STAT_MetalRenderPassDescriptorCount);
DEFINE_STAT(STAT_MetalRenderPassColorAttachmentDescriptorCount);
DEFINE_STAT(STAT_MetalRenderPassDepthAttachmentDescriptorCount);
DEFINE_STAT(STAT_MetalRenderPassStencilAttachmentDescriptorCount);
DEFINE_STAT(STAT_MetalVertexDescriptorCount);
DEFINE_STAT(STAT_MetalComputePipelineStateCount);
DEFINE_STAT(STAT_MetalFunctionCount);
DEFINE_STAT(STAT_MetalFreePooledBufferCount);
DEFINE_STAT(STAT_MetalPooledBufferCount);

DEFINE_STAT(STAT_MetalPooledBufferMem);
DEFINE_STAT(STAT_MetalUsedPooledBufferMem);
DEFINE_STAT(STAT_MetalFreePooledBufferMem);
DEFINE_STAT(STAT_MetalWastedPooledBufferMem);
DEFINE_STAT(STAT_MetalBufferAlloctations);
DEFINE_STAT(STAT_MetalBufferFreed);
DEFINE_STAT(STAT_MetalBufferMemAlloc);
DEFINE_STAT(STAT_MetalBufferMemFreed);
DEFINE_STAT(STAT_MetalBufferNativeAlloctations);
DEFINE_STAT(STAT_MetalBufferNativeFreed);
DEFINE_STAT(STAT_MetalBufferNativeMemAlloc);
DEFINE_STAT(STAT_MetalBufferNativeMemFreed);


#if METAL_STATISTICS
void FMetalEventNode::GetStats(FMetalPipelineStats& OutStats)
{
	for(auto Entry : DrawStats)
	{
		FMetalPipelineStats const& DrawStat = Entry->GetResult();
		OutStats.RHIPrimitives += DrawStat.RHIPrimitives;
		OutStats.RHIVertices += DrawStat.RHIVertices;
		OutStats.VertexFunctionCost += DrawStat.VertexFunctionCost;
		OutStats.FragmentFunctionCost += DrawStat.FragmentFunctionCost;
		OutStats.DrawCallTime += DrawStat.DrawCallTime;
		OutStats.InputVertices += DrawStat.InputVertices;
		OutStats.InputPrimitives += DrawStat.InputPrimitives;
		OutStats.VertexFunctionInvocations += DrawStat.VertexFunctionInvocations;
		OutStats.ClipperInvocations += DrawStat.ClipperInvocations;
		OutStats.ClipperPrimitives += DrawStat.ClipperPrimitives;
		OutStats.FragmentFunctionInvocations += DrawStat.FragmentFunctionInvocations;
	}
}

/** Recursively dumps stats for each node with a depth first traversal. */
static void DumpStatsEventNode(FMetalEventNode* Node, float RootResult, int32 Depth, int32& NumNodes, int32& NumDraws)
{
	NumNodes++;
	if (Node->NumDraws > 0 || Node->Children.Num() > 0)
	{
		NumDraws += Node->NumDraws;
		// Percent that this node was of the total frame time
		const float Percent = Node->TimingResult * 100.0f / (RootResult * 1000.0f);
		
		const int32 EffectiveDepth = FMath::Max(Depth - 1, 0);
		
		FMetalPipelineStats Stats;
		FMemory::Memzero(Stats);
		Node->GetStats(Stats);
		
		// Print information about this node, padded to its depth in the tree
		float DrawCallTime = FPlatformTime::ToMilliseconds(Stats.DrawCallTime);
		UE_LOG(LogRHI, Warning, TEXT("%s%4.1f%%%5.2fms (%5.2fms)   %s %u draws %u (%u) prims %u (%u) verts %u vert invoke %u vert cost %u clip invoke %u clip prims %u pixel invoke %u pixel cost"),
			   *FString(TEXT("")).LeftPad(EffectiveDepth * 3),
			   Percent,
			   Node->TimingResult,
			   DrawCallTime,
			   *Node->Name,
			   Node->NumDraws,
			   Stats.RHIPrimitives,
			   Stats.InputPrimitives,
			   Stats.RHIVertices,
			   Stats.InputVertices,
			   Stats.VertexFunctionInvocations,
			   Stats.VertexFunctionCost,
			   Stats.ClipperInvocations,
			   Stats.ClipperPrimitives,
			   Stats.FragmentFunctionInvocations,
			   Stats.FragmentFunctionCost
			   );
		
		float TotalChildTime = 0;
		uint32 TotalChildDraws = 0;
		for (int32 ChildIndex = 0; ChildIndex < Node->Children.Num(); ChildIndex++)
		{
			FMetalEventNode* ChildNode = (FMetalEventNode*)(Node->Children[ChildIndex].GetReference());
			
			int32 NumChildDraws = 0;
			// Traverse children
			DumpStatsEventNode(ChildNode, RootResult, Depth + 1, NumNodes, NumChildDraws);
			NumDraws += NumChildDraws;
			
			TotalChildTime += ChildNode->TimingResult;
			TotalChildDraws += NumChildDraws;
		}
		
		const float UnaccountedTime = FMath::Max(Node->TimingResult - TotalChildTime, 0.0f);
		const float UnaccountedPercent = UnaccountedTime * 100.0f / (RootResult * 1000.0f);
		
		// Add an 'Other Children' node if necessary to show time spent in the current node that is not in any of its children
		if (Node->Children.Num() > 0 && TotalChildDraws > 0 && (UnaccountedPercent > 2.0f || UnaccountedTime > .2f))
		{
			UE_LOG(LogRHI, Warning, TEXT("%s%4.1f%%%5.2fms Unaccounted"),
				   *FString(TEXT("")).LeftPad((EffectiveDepth + 1) * 3),
				   UnaccountedPercent,
				   UnaccountedTime);
		}
	}
}
#endif

FMetalDrawProfiler::FMetalDrawProfiler(struct FMetalGPUProfiler* InProfiler, uint32 InStartPoint, uint32 InEndPoint, uint32 NumPrimitives, uint32 NumVertices)
: Profiler(InProfiler)
, StartPoint(InStartPoint)
, EndPoint(InEndPoint)
{
	if(Profiler)
	{
		Profiler->StartGPUWork(InStartPoint, InEndPoint, NumPrimitives, NumVertices);
	}
}

FMetalDrawProfiler::~FMetalDrawProfiler()
{
	if(Profiler)
	{
		Profiler->FinishGPUWork();
	}
}

FMetalEventNode::~FMetalEventNode()
{
#if METAL_STATISTICS
	for(auto Entry : DrawStats)
	{
		delete Entry;
	}
#endif
}

float FMetalEventNode::GetTiming()
{
	return FPlatformTime::ToSeconds(EndTime - StartTime);
}

void FMetalEventNode::StartTiming()
{
	StartTime = 0;
	EndTime = 0;
	
	Context->StartTiming(this);
}

void FMetalEventNode::Start(id<MTLCommandBuffer> Buffer)
{
	check(Buffer);
	[Buffer addCompletedHandler:^(id<MTLCommandBuffer>)
	{
		StartTime = mach_absolute_time();
	}];
}

void FMetalEventNode::StopTiming()
{
	Context->EndTiming(this);
}

void FMetalEventNode::Stop(id<MTLCommandBuffer> Buffer)
{
	[Buffer addCompletedHandler:^(id<MTLCommandBuffer>)
	{
		EndTime = mach_absolute_time();
     
        if(bRoot)
        {
            GGPUFrameTime = FMath::TruncToInt( double(GetTiming()) / double(FPlatformTime::GetSecondsPerCycle()) );
            if(!bFullProfiling)
            {
                delete this;
            }
        }
	}];
}

#if METAL_STATISTICS
void FMetalEventNode::StartDraw(bool bActiveStats, uint32 StartPoint, uint32 EndPoint, uint32 NumPrimitives, uint32 NumVertices)
{
	if(Context->GetCommandQueue().GetStatistics() && bActiveStats)
	{
		DrawStats.Add(Context->GetCommandQueue().GetStatistics()->CreateDrawStats(Context->GetCurrentCommandBuffer(), StartPoint, EndPoint, NumPrimitives, NumVertices));
	}
}

void FMetalEventNode::StopDraw(void)
{
	if(DrawStats.Num())
	{
		DrawStats.Last()->End();
	}
}
#endif
	
/** Start this frame of per tracking */
void FMetalEventNodeFrame::StartFrame()
{
	RootNode->StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FMetalEventNodeFrame::EndFrame()
{
	RootNode->StopTiming();
}

/** Calculates root timing base frequency (if needed by this RHI) */
float FMetalEventNodeFrame::GetRootTimingResults()
{
    return RootNode->GetTiming();
}

void FMetalEventNodeFrame::LogDisjointQuery()
{
	
}

FGPUProfilerEventNode* FMetalGPUProfiler::CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent)
{
	FMetalEventNode* EventNode = new FMetalEventNode(FMetalContext::GetCurrentContext(), InName, InParent, false, false);
	return EventNode;
}

void FMetalGPUProfiler::Cleanup()
{
	
}

void FMetalGPUProfiler::PushEvent(const TCHAR* Name, FColor Color)
{
	FGPUProfiler::PushEvent(Name, Color);
}

void FMetalGPUProfiler::PopEvent()
{
	FGPUProfiler::PopEvent();
}

//TGlobalResource<FVector4VertexDeclaration> GMetalVector4VertexDeclaration;
TGlobalResource<FTexture> GMetalLongTaskRT;

static FGlobalBoundShaderState LongGPUTaskBoundShaderState;

void FMetalGPUProfiler::BeginFrame()
{
    if(!CurrentEventNodeFrame)
    {
        // Start tracking the frame
        CurrentEventNodeFrame = new FMetalEventNodeFrame(Context, GTriggerGPUProfile);
        CurrentEventNodeFrame->StartFrame();
        
        if(GTriggerGPUProfile)
        {
            bTrackingEvents = true;
            bLatchedGProfilingGPU = true;
            
            GTriggerGPUProfile = false;

#if METAL_STATISTICS
			if(Context->GetCommandQueue().GetStatistics())
            {
				Context->GetCommandQueue().GetStatistics()->BeginSamplingStatistics();
				bActiveStats = true;
            }
#endif
			
			/*if (bLatchedGProfilingGPU)
			{
				// Issue a bunch of GPU work at the beginning of the frame, to make sure that we are GPU bound
				// We can't isolate idle time from GPU timestamps
				const auto FeatureLevel = GMaxRHIFeatureLevel;
				
				FRHICommandList_RecursiveHazardous RHICmdList(InRHI);
				if(!IsValidRef(GMetalLongTaskRT.TextureRHI))
				{
					FRHIResourceCreateInfo Info;
					GMetalLongTaskRT.TextureRHI = RHICreateTexture2D(1920, 1080, PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable, Info);
				}
				SetRenderTarget(RHICmdList, GMetalLongTaskRT.TextureRHI->GetTexture2D(), FTextureRHIRef());
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One>::GetRHI(), FLinearColor::Black);
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI(), 0);
				RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
				
				auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
				TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);
				TShaderMapRef<FLongGPUTaskPS> PixelShader(ShaderMap);
				
				SetGlobalBoundShaderState(RHICmdList, FeatureLevel, LongGPUTaskBoundShaderState, GMetalVector4VertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, 0);
				
				// Draw a fullscreen quad
				FVector4 Vertices[4];
				Vertices[0].Set( -1.0f,  1.0f, 0, 1.0f );
				Vertices[1].Set(  1.0f,  1.0f, 0, 1.0f );
				Vertices[2].Set( -1.0f, -1.0f, 0, 1.0f );
				Vertices[3].Set(  1.0f, -1.0f, 0, 1.0f );
				DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Vertices, sizeof(Vertices[0]));
				
				RHICmdList.SubmitCommandsHint();
				// RHICmdList flushes on destruction
			}*/
        }
        
        PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
    }
    NumNestedFrames++;
}

void FMetalGPUProfiler::EndFrame()
{
    if(--NumNestedFrames == 0)
    {
		PopEvent();
		
#if PLATFORM_MAC
        FPlatformMisc::UpdateDriverMonitorStatistics(GetMetalDeviceContext().GetDeviceIndex());
#endif
		
        if(CurrentEventNodeFrame)
        {
            CurrentEventNodeFrame->EndFrame();
			
			if(bLatchedGProfilingGPU)
			{
#if METAL_STATISTICS
				if(Context->GetCommandQueue().GetStatistics())
				{
					Context->GetCommandQueue().GetStatistics()->FinishSamplingStatistics();
					bActiveStats = false;
				}
#endif
				
                bTrackingEvents = false;
                bLatchedGProfilingGPU = false;
            
                UE_LOG(LogRHI, Warning, TEXT(""));
                UE_LOG(LogRHI, Warning, TEXT(""));
                CurrentEventNodeFrame->DumpEventTree();
				
#if METAL_STATISTICS
				if(Context->GetCommandQueue().GetStatistics())
				{
					float RootResult = CurrentEventNodeFrame->GetRootTimingResults();
					
					UE_LOG(LogRHI, Warning, TEXT(""));
					UE_LOG(LogRHI, Warning, TEXT(""));
					UE_LOG(LogRHI, Warning, TEXT("Pipeline stats hierarchy, total GPU time %.2fms"), RootResult * 1000.0f);
					
					int32 NumNodes = 0;
					int32 NumDraws = 0;
					for (int32 BaseNodeIndex = 0; BaseNodeIndex < CurrentEventNodeFrame->EventTree.Num(); BaseNodeIndex++)
					{
						DumpStatsEventNode((FMetalEventNode*)(CurrentEventNodeFrame->EventTree[BaseNodeIndex].GetReference()), RootResult, 0, NumNodes, NumDraws);
					}
				}
#endif
            }
			
            delete CurrentEventNodeFrame;
            CurrentEventNodeFrame = NULL;
        }
    }
}

void FMetalGPUProfiler::StartGPUWork(uint32 StartPoint, uint32 EndPoint, uint32 NumPrimitives, uint32 NumVertices)
{
	if(CurrentEventNode)
	{
		RegisterGPUWork(NumPrimitives, NumVertices);
#if METAL_STATISTICS
		FMetalEventNode* EventNode = (FMetalEventNode*)CurrentEventNode;
		EventNode->StartDraw(bActiveStats, StartPoint, EndPoint, NumPrimitives, NumVertices);
#endif
	}
}

void FMetalGPUProfiler::FinishGPUWork(void)
{
#if METAL_STATISTICS
	if(CurrentEventNode)
	{
		FMetalEventNode* EventNode = (FMetalEventNode*)CurrentEventNode;
		EventNode->StopDraw();
	}
#endif
}
