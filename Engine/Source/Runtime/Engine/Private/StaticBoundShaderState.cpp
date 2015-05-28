// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticBoundShaderState.cpp: Static bound shader state implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "StaticBoundShaderState.h"

TLinkedList<FGlobalBoundShaderStateResource*>*& FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()
{
	static TLinkedList<FGlobalBoundShaderStateResource*>* List = NULL;
	return List;
}

FGlobalBoundShaderStateResource::FGlobalBoundShaderStateResource()
	: GlobalListLink(this)
#if DO_CHECK
	, BoundVertexDeclaration(nullptr)
	, BoundVertexShader(nullptr)
	, BoundPixelShader(nullptr)
	, BoundGeometryShader(nullptr)
#endif 
{
	// Add this resource to the global list in the rendering thread.
	if(IsInRenderingThread())
	{
		GlobalListLink.Link(GetGlobalBoundShaderStateList());
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			LinkGlobalBoundShaderStateResource,FGlobalBoundShaderStateResource*,Resource,this,
			{
				Resource->GlobalListLink.Link(GetGlobalBoundShaderStateList());
			});
	}
}

FGlobalBoundShaderStateResource::~FGlobalBoundShaderStateResource()
{
	// Remove this resource from the global list.
	GlobalListLink.Unlink();
}

/**
 * Initializes a global bound shader state with a vanilla bound shader state and required information.
 */
FBoundShaderStateRHIParamRef FGlobalBoundShaderStateResource::GetInitializedRHI(
	FVertexDeclarationRHIParamRef VertexDeclaration,
	FVertexShaderRHIParamRef VertexShader, 
	FPixelShaderRHIParamRef PixelShader,
	FGeometryShaderRHIParamRef GeometryShader
	)
{
	check(IsInitialized());

	// This should only be called in the rendering thread after the RHI has been initialized.
	check(GIsRHIInitialized);
	check(IsInRenderingThread());

	// Create the bound shader state if it hasn't been cached yet.
	if(!IsValidRef(BoundShaderState))
	{
#if DO_CHECK
		BoundVertexDeclaration = VertexDeclaration;
		BoundVertexShader = VertexShader;
		BoundPixelShader = PixelShader;
		BoundGeometryShader = GeometryShader;
#endif 
		BoundShaderState = 
			RHICreateBoundShaderState(
			VertexDeclaration,
			VertexShader,
			FHullShaderRHIRef(), 
			FDomainShaderRHIRef(),
			PixelShader,
			GeometryShader);
	}
#if DO_CHECK
	// Verify that the passed in shaders will actually be used
	// This will catch cases where one bound shader state is being used with more than one combination of shaders
	// Otherwise setting the shader will just silently fail once the bound shader state has been initialized with a different shader 
	check(BoundVertexDeclaration == VertexDeclaration &&
		BoundVertexShader == VertexShader &&
		BoundPixelShader == PixelShader &&
		BoundGeometryShader == GeometryShader);
#endif 

	return BoundShaderState;
}

FBoundShaderStateRHIParamRef FGlobalBoundShaderStateResource::GetPreinitializedRHI()
{
	check(IsInitialized());
	check(GIsRHIInitialized);
	check(IsInParallelRenderingThread());
	return BoundShaderState;
}

void FGlobalBoundShaderStateResource::ReleaseRHI()
{
	// Release the cached bound shader state.
	BoundShaderState.SafeRelease();
}

static FBoundShaderStateRHIParamRef GetGlobalBoundShaderState_Internal(FGlobalBoundShaderState& GlobalBoundShaderState, ERHIFeatureLevel::Type InFeatureLevel)
{
	auto WorkArea = GlobalBoundShaderState.Get(InFeatureLevel);

	// Check for unset uniform buffer parameters
	// Technically you can set uniform buffer parameters after calling RHISetBoundShaderState, but this is the most global place to check for unset parameters
	WorkArea->Args.VertexShader->VerifyBoundUniformBufferParameters();
	WorkArea->Args.PixelShader->VerifyBoundUniformBufferParameters();
	WorkArea->Args.GeometryShader->VerifyBoundUniformBufferParameters();

	FGlobalBoundShaderState_Internal* BSS = WorkArea->BSS;
	bool bNewBSS = false;
	if (!BSS)
	{
		BSS = new FGlobalBoundShaderState_Internal(); // these are simply leaked and never freed
		bNewBSS = true;
	}
	FBoundShaderStateRHIParamRef Result = BSS->GetInitializedRHI(
		WorkArea->Args.VertexDeclarationRHI,
		GETSAFERHISHADER_VERTEX(WorkArea->Args.VertexShader),
		GETSAFERHISHADER_PIXEL(WorkArea->Args.PixelShader),
		(FGeometryShaderRHIParamRef)GETSAFERHISHADER_GEOMETRY(WorkArea->Args.GeometryShader));
	if (bNewBSS)
	{
		FPlatformMisc::MemoryBarrier();
		// this probably doesn't need to be an atomic, assuming a pointer write is atomic, but this checks alignment, etc
		FGlobalBoundShaderState_Internal* OldBSS = (FGlobalBoundShaderState_Internal*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)&WorkArea->BSS, BSS, nullptr);
		check(!OldBSS); // should not be any concurrent writes
	}
	return Result;
}

class FSetGlobalBoundShaderStateRenderThreadTask
{
	FRHICommandList& RHICmdList;
	FGlobalBoundShaderState& GlobalBoundShaderState;
	ERHIFeatureLevel::Type FeatureLevel;

public:

	FSetGlobalBoundShaderStateRenderThreadTask(FRHICommandList& InRHICmdList, FGlobalBoundShaderState& InGlobalBoundShaderState, ERHIFeatureLevel::Type InFeatureLevel)
		: RHICmdList(InRHICmdList)
		, GlobalBoundShaderState(InGlobalBoundShaderState)
		, FeatureLevel(InFeatureLevel)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSetGlobalBoundShaderStateRenderThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::RenderThread_Local;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		RHICmdList.SetBoundShaderState(GetGlobalBoundShaderState_Internal(GlobalBoundShaderState, FeatureLevel));
	}
};

void SetGlobalBoundShaderState(
	FRHICommandList& RHICmdList,
	ERHIFeatureLevel::Type FeatureLevel, 
	FGlobalBoundShaderState& GlobalBoundShaderState,
	FVertexDeclarationRHIParamRef VertexDeclarationRHI,
	FShader* VertexShader,
	FShader* PixelShader,
	FShader* GeometryShader
	)
{
	FGlobalBoundShaderStateWorkArea* ExistingGlobalBoundShaderState = GlobalBoundShaderState.Get(FeatureLevel);
	if (!ExistingGlobalBoundShaderState)
	{
		FGlobalBoundShaderStateWorkArea* NewGlobalBoundShaderState = new FGlobalBoundShaderStateWorkArea();
		NewGlobalBoundShaderState->Args.VertexDeclarationRHI = VertexDeclarationRHI;
		NewGlobalBoundShaderState->Args.VertexShader = VertexShader;
		NewGlobalBoundShaderState->Args.PixelShader = PixelShader;
		NewGlobalBoundShaderState->Args.GeometryShader = GeometryShader;
		FPlatformMisc::MemoryBarrier();

		FGlobalBoundShaderStateWorkArea* OldGlobalBoundShaderState = (FGlobalBoundShaderStateWorkArea*)FPlatformAtomics::InterlockedCompareExchangePointer((void**)GlobalBoundShaderState.GetPtr(FeatureLevel), NewGlobalBoundShaderState, nullptr);

		if (OldGlobalBoundShaderState != nullptr)
		{
			//we lost
			delete NewGlobalBoundShaderState;
			check(OldGlobalBoundShaderState == GlobalBoundShaderState.Get(FeatureLevel));
			ExistingGlobalBoundShaderState = OldGlobalBoundShaderState;
		}
		else
		{
			check(NewGlobalBoundShaderState == GlobalBoundShaderState.Get(FeatureLevel));
			ExistingGlobalBoundShaderState = NewGlobalBoundShaderState;
		}
	}
	else if (!(VertexDeclarationRHI == ExistingGlobalBoundShaderState->Args.VertexDeclarationRHI &&
		VertexShader == ExistingGlobalBoundShaderState->Args.VertexShader &&
		PixelShader == ExistingGlobalBoundShaderState->Args.PixelShader &&
		GeometryShader == ExistingGlobalBoundShaderState->Args.GeometryShader))
	{
		// this is sketchy from a parallel perspective, but assuming the writes are atomic, and probably even if they aren't, this should be ok
		ExistingGlobalBoundShaderState->Args.VertexDeclarationRHI = VertexDeclarationRHI;
		ExistingGlobalBoundShaderState->Args.VertexShader = VertexShader;
		ExistingGlobalBoundShaderState->Args.PixelShader = PixelShader;
		ExistingGlobalBoundShaderState->Args.GeometryShader = GeometryShader;
	}

	if (RHICmdList.Bypass() || IsInRenderingThread())
	{
		// we can just do it as usual
		RHICmdList.SetBoundShaderState(GetGlobalBoundShaderState_Internal(GlobalBoundShaderState, FeatureLevel));
		return;
	}
	if (ExistingGlobalBoundShaderState->BSS)
	{
		FBoundShaderStateRHIParamRef BoundShaderState = ExistingGlobalBoundShaderState->BSS->GetPreinitializedRHI();
		if (BoundShaderState)
		{
			// this is on a thread, but we already have a valid BSS
			RHICmdList.SetBoundShaderState(BoundShaderState);
			return;
		}
	}
	// We need to do this on the render thread

	FRHICommandList* CmdList = new FRHICommandList;
	FGraphEventRef RenderThreadCompletionEvent = TGraphTask<FSetGlobalBoundShaderStateRenderThreadTask>::CreateTask().ConstructAndDispatchWhenReady(*CmdList, GlobalBoundShaderState, FeatureLevel);
	RHICmdList.QueueRenderThreadCommandListSubmit(RenderThreadCompletionEvent, CmdList);
}


