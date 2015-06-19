// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderingCompositionGraph.cpp: Scene pass order and dependency system.
=============================================================================*/

#include "RendererPrivate.h"
#include "RenderingCompositionGraph.h"
#include "HighResScreenshot.h"

// render thread, 0:off, >0 next n frames should be debugged
uint32 GDebugCompositionGraphFrames = 0;

class FGMLFileWriter
{
public:
	// constructor
	FGMLFileWriter()
		: GMLFile(0)
	{		
	}

	void OpenGMLFile(const TCHAR* Name)
	{
#if !UE_BUILD_SHIPPING
		// todo: do we need to create the directory?
		FString FilePath = FPaths::ScreenShotDir() + TEXT("/") + Name + TEXT(".gml");
		GMLFile = IFileManager::Get().CreateDebugFileWriter(*FilePath);
#endif
	}

	void CloseGMLFile()
	{
#if !UE_BUILD_SHIPPING
		if(GMLFile)
		{
			delete GMLFile;
			GMLFile = 0;
		}
#endif
	}

	// .GML file is to visualize the post processing graph as a 2d graph
	void WriteLine(const char* Line)
	{
#if !UE_BUILD_SHIPPING
		if(GMLFile)
		{
			GMLFile->Serialize((void*)Line, FCStringAnsi::Strlen(Line));
			GMLFile->Serialize((void*)"\r\n", 2);
		}
#endif
	}

private:
	FArchive* GMLFile;
};

FGMLFileWriter GGMLFileWriter;


bool ShouldDebugCompositionGraph()
{
#if !UE_BUILD_SHIPPING
	return GDebugCompositionGraphFrames > 0;
#else 
	return false;
#endif
}

void ExecuteCompositionGraphDebug()
{
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		StartDebugCompositionGraph,
	{
		GDebugCompositionGraphFrames = 1;
	}
	);
}

// main thread
void CompositionGraph_OnStartFrame()
{
#if !UE_BUILD_SHIPPING
	ENQUEUE_UNIQUE_RENDER_COMMAND(
		DebugCompositionGraphDec,
	{
		if(GDebugCompositionGraphFrames)
		{
			--GDebugCompositionGraphFrames;
		}
	}
	);		
#endif
}


#if !UE_BUILD_SHIPPING
FAutoConsoleCommand CmdCompositionGraphDebug(
	TEXT("r.CompositionGraphDebug"),
	TEXT("Execute to get a single frame dump of the composition graph of one frame (post processing and lighting)."),
	FConsoleCommandDelegate::CreateStatic(ExecuteCompositionGraphDebug)
	);
#endif

FRenderingCompositePassContext::FRenderingCompositePassContext(FRHICommandListImmediate& InRHICmdList, FViewInfo& InView/*, const FSceneRenderTargetItem& InRenderTargetItem*/)
	: View(InView)
	, ViewState((FSceneViewState*)InView.State)
	//		, CompositingOutputRTItem(InRenderTargetItem)
	, Pass(0)
	, RHICmdList(InRHICmdList)
	, ViewPortRect(0, 0, 0 ,0)
	, FeatureLevel(View.GetFeatureLevel())
	, ShaderMap(InView.ShaderMap)
{
	check(!IsViewportValid());

	Root = Graph.RegisterPass(new(FMemStack::Get()) FRCPassPostProcessRoot());

	Graph.ResetDependencies();
}

FRenderingCompositePassContext::~FRenderingCompositePassContext()
{
	Graph.Free();
}

void FRenderingCompositePassContext::Process(const TCHAR *GraphDebugName)
{
	if(Root)
	{
		if(ShouldDebugCompositionGraph())
		{
			UE_LOG(LogConsoleResponse,Log, TEXT(""));
			UE_LOG(LogConsoleResponse,Log, TEXT("FRenderingCompositePassContext:Debug '%s' ---------"), GraphDebugName);
			UE_LOG(LogConsoleResponse,Log, TEXT(""));

			GGMLFileWriter.OpenGMLFile(GraphDebugName);
			GGMLFileWriter.WriteLine("Creator \"UnrealEngine4\"");
			GGMLFileWriter.WriteLine("Version \"2.10\"");
			GGMLFileWriter.WriteLine("graph");
			GGMLFileWriter.WriteLine("[");
			GGMLFileWriter.WriteLine("\tcomment\t\"This file can be viewed with yEd from yWorks. Run Layout/Hierarchical after loading.\"");
			GGMLFileWriter.WriteLine("\thierarchic\t1");
			GGMLFileWriter.WriteLine("\tdirected\t1");
		}

		FRenderingCompositeOutputRef Output(Root);

		Graph.RecursivelyGatherDependencies(Output);
		Graph.RecursivelyProcess(Root, *this);

		if(ShouldDebugCompositionGraph())
		{
			UE_LOG(LogConsoleResponse,Log, TEXT(""));

			GGMLFileWriter.WriteLine("]");
			GGMLFileWriter.CloseGMLFile();
		}
	}
}

// --------------------------------------------------------------------------

FRenderingCompositionGraph::FRenderingCompositionGraph()
{
}

FRenderingCompositionGraph::~FRenderingCompositionGraph()
{
	Free();
}

void FRenderingCompositionGraph::Free()
{
	for(uint32 i = 0; i < (uint32)Nodes.Num(); ++i)
	{
		FRenderingCompositePass *Element = Nodes[i];
		if (FMemStack::Get().ContainsPointer(Element))
		{
			Element->~FRenderingCompositePass();
		}
		else
		{
			// Call release on non-stack allocated elements
			Element->Release();
		}
	}

	Nodes.Empty();
}

void FRenderingCompositionGraph::RecursivelyGatherDependencies(const FRenderingCompositeOutputRef& InOutputRef)
{
	FRenderingCompositePass *Pass = InOutputRef.GetPass();

	if(Pass->bGraphMarker)
	{
		// already processed
		return;
	}
	Pass->bGraphMarker = true;

	// iterate through all inputs and additional dependencies of this pass
	uint32 Index = 0;
	while(const FRenderingCompositeOutputRef* OutputRefIt = Pass->GetDependency(Index++))
	{
		FRenderingCompositeOutput* InputOutput = OutputRefIt->GetOutput();
				
		if(InputOutput)
		{
			// add a dependency to this output as we are referencing to it
			InputOutput->AddDependency();
		}
		
		if(OutputRefIt->GetPass())
		{
			// recursively process all inputs of this Pass
			RecursivelyGatherDependencies(*OutputRefIt);
		}
	}

	// the pass is asked what the intermediate surface/texture format needs to be for all its outputs.
	for(uint32 OutputId = 0; ; ++OutputId)
	{
		EPassOutputId PassOutputId = (EPassOutputId)(OutputId);
		FRenderingCompositeOutput* Output = Pass->GetOutput(PassOutputId);

		if(!Output)
		{
			break;
		}

		Output->RenderTargetDesc = Pass->ComputeOutputDesc(PassOutputId);
	}
}

void FRenderingCompositionGraph::DumpOutputToFile(FRenderingCompositePassContext& Context, const FString& Filename, FRenderingCompositeOutput* Output) const
{
	FSceneRenderTargetItem& RenderTargetItem = Output->PooledRenderTarget->GetRenderTargetItem();
	FHighResScreenshotConfig& HighResScreenshotConfig = GetHighResScreenshotConfig();
	FTextureRHIRef Texture = RenderTargetItem.TargetableTexture ? RenderTargetItem.TargetableTexture : RenderTargetItem.ShaderResourceTexture;
	check(Texture);
	check(Texture->GetTexture2D());

	FIntRect SourceRect;
	int32 MSAAXSamples = Texture->GetNumSamples();
	if (GIsHighResScreenshot)
	{
		SourceRect = HighResScreenshotConfig.CaptureRegion;
		if (SourceRect.Area() == 0)
		{
			SourceRect = Context.View.ViewRect;
		}
		else
		{
			SourceRect.Min.X *= MSAAXSamples;
			SourceRect.Max.X *= MSAAXSamples;
		}
	}

	FIntPoint DestSize(SourceRect.Width(), SourceRect.Height());

	EPixelFormat PixelFormat = Texture->GetFormat();
	FString ResultPath;
	switch (PixelFormat)
	{
		case PF_FloatRGBA:
		{
			TArray<FFloat16Color> Bitmap;
			Context.RHICmdList.ReadSurfaceFloatData(Texture, SourceRect, Bitmap, (ECubeFace)0, 0, 0);
			HighResScreenshotConfig.SaveImage(Filename, Bitmap, DestSize, &ResultPath);
		}
		break;
		case PF_R8G8B8A8:
		case PF_B8G8R8A8:
		{
			FReadSurfaceDataFlags ReadDataFlags;
			ReadDataFlags.SetLinearToGamma(false);
			TArray<FColor> Bitmap;
			Context.RHICmdList.ReadSurfaceData(Texture, SourceRect, Bitmap, ReadDataFlags);
			FColor* Pixel = Bitmap.GetData();
			for (int32 i = 0, Count = Bitmap.Num(); i < Count; i++, Pixel++)
			{
				Pixel->A = 255;
			}
			HighResScreenshotConfig.SaveImage(Filename, Bitmap, DestSize, &ResultPath);
		}
		break;
	}

	UE_LOG(LogConsoleResponse, Display, TEXT("Content was saved to \"%s\""), *ResultPath);
}

void FRenderingCompositionGraph::RecursivelyProcess(const FRenderingCompositeOutputRef& InOutputRef, FRenderingCompositePassContext& Context) const
{
	FRenderingCompositePass *Pass = InOutputRef.GetPass();
	FRenderingCompositeOutput *Output = InOutputRef.GetOutput();

#if !UE_BUILD_SHIPPING
	if(!Pass || !Output)
	{
		// to track down a crash bug
		if(Context.Pass)
		{
			UE_LOG(LogRenderer,Fatal, TEXT("FRenderingCompositionGraph::RecursivelyProcess %s"), *Context.Pass->ConstructDebugName());
		}
	}
#endif

	check(Pass);
	check(Output);

	if(!Pass->bGraphMarker)
	{
		// already processed
		return;
	}
	Pass->bGraphMarker = false;

	// iterate through all inputs and additional dependencies of this pass
	{
		uint32 Index = 0;

		while(const FRenderingCompositeOutputRef* OutputRefIt = Pass->GetDependency(Index++))
		{
			if(OutputRefIt->GetPass())
			{
				if(!OutputRefIt)
				{
					// Pass doesn't have more inputs
					break;
				}

				FRenderingCompositeOutput* Input = OutputRefIt->GetOutput();
				
				// to track down an issue, should never happen
				check(OutputRefIt->GetPass());

				if(GRenderTargetPool.IsEventRecordingEnabled() && Pass != Context.Root)
				{
					GRenderTargetPool.AddPhaseEvent(*Pass->ConstructDebugName());
				}

				Context.Pass = Pass;
				RecursivelyProcess(*OutputRefIt, Context);
			}
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(ShouldDebugCompositionGraph())
	{
		GGMLFileWriter.WriteLine("\tnode");
		GGMLFileWriter.WriteLine("\t[");

		int32 PassId = ComputeUniquePassId(Pass);
		FString PassDebugName = Pass->ConstructDebugName();

		ANSICHAR Line[MAX_SPRINTF];

		{
			GGMLFileWriter.WriteLine("\t\tgraphics");
			GGMLFileWriter.WriteLine("\t\t[");
			FCStringAnsi::Sprintf(Line, "\t\t\tw\t%d", 200);
			GGMLFileWriter.WriteLine(Line);
			FCStringAnsi::Sprintf(Line, "\t\t\th\t%d", 80);
			GGMLFileWriter.WriteLine(Line);
			GGMLFileWriter.WriteLine("\t\t\tfill\t\"#FFCCCC\"");
			GGMLFileWriter.WriteLine("\t\t]");
		}

		{
			FCStringAnsi::Sprintf(Line, "\t\tid\t%d", PassId);
			GGMLFileWriter.WriteLine(Line);
			GGMLFileWriter.WriteLine("\t\tLabelGraphics");
			GGMLFileWriter.WriteLine("\t\t[");
			FCStringAnsi::Sprintf(Line, "\t\t\ttext\t\"#%d\r%s\"", PassId, (const char *)TCHAR_TO_ANSI(*PassDebugName));
			GGMLFileWriter.WriteLine(Line);
			GGMLFileWriter.WriteLine("\t\t\tanchor\t\"t\"");	// put label internally on top
			GGMLFileWriter.WriteLine("\t\t\tfontSize\t14");
			GGMLFileWriter.WriteLine("\t\t\tfontStyle\t\"bold\"");
			GGMLFileWriter.WriteLine("\t\t]");
		}

		UE_LOG(LogConsoleResponse,Log, TEXT("Node#%d '%s'"), PassId, *PassDebugName);
	
		GGMLFileWriter.WriteLine("\t\tisGroup\t1");
		GGMLFileWriter.WriteLine("\t]");

		uint32 InputId = 0;
		while(FRenderingCompositeOutputRef* OutputRefIt = Pass->GetInput((EPassInputId)(InputId++)))
		{
			if(OutputRefIt->Source)
			{
				// source is hooked up 
				FString InputName = OutputRefIt->Source->ConstructDebugName();

				int32 TargetPassId = ComputeUniquePassId(OutputRefIt->Source);

				UE_LOG(LogConsoleResponse,Log, TEXT("  ePId_Input%d: Node#%d @ ePId_Output%d '%s'"), InputId - 1, TargetPassId, (uint32)OutputRefIt->PassOutputId, *InputName);

				// input connection to another node
				{
					GGMLFileWriter.WriteLine("\tedge");
					GGMLFileWriter.WriteLine("\t[");
					{
						FCStringAnsi::Sprintf(Line, "\t\tsource\t%d", ComputeUniqueOutputId(OutputRefIt->Source, OutputRefIt->PassOutputId));
						GGMLFileWriter.WriteLine(Line);
						FCStringAnsi::Sprintf(Line, "\t\ttarget\t%d", PassId);
						GGMLFileWriter.WriteLine(Line);
					}
					{
						FString EdgeName = FString::Printf(TEXT("ePId_Input%d"), InputId - 1);

						GGMLFileWriter.WriteLine("\t\tLabelGraphics");
						GGMLFileWriter.WriteLine("\t\t[");
						FCStringAnsi::Sprintf(Line, "\t\t\ttext\t\"%s\"", (const char *)TCHAR_TO_ANSI(*EdgeName));
						GGMLFileWriter.WriteLine(Line);
						GGMLFileWriter.WriteLine("\t\t\tmodel\t\"three_center\"");
						GGMLFileWriter.WriteLine("\t\t\tposition\t\"tcentr\"");
						GGMLFileWriter.WriteLine("\t\t]");
					}
					GGMLFileWriter.WriteLine("\t]");
				}
			}
			else
			{
				// source is not hooked up 
				UE_LOG(LogConsoleResponse,Log, TEXT("  ePId_Input%d:"), InputId - 1);
			}
		}

		uint32 DepId = 0;
		while(FRenderingCompositeOutputRef* OutputRefIt = Pass->GetAdditionalDependency(DepId++))
		{
			check(OutputRefIt->Source);

			FString InputName = OutputRefIt->Source->ConstructDebugName();
			int32 TargetPassId = ComputeUniquePassId(OutputRefIt->Source);

			UE_LOG(LogConsoleResponse,Log, TEXT("  Dependency: Node#%d @ ePId_Output%d '%s'"), TargetPassId, (uint32)OutputRefIt->PassOutputId, *InputName);

			// dependency connection to another node
			{
				GGMLFileWriter.WriteLine("\tedge");
				GGMLFileWriter.WriteLine("\t[");
				{
					FCStringAnsi::Sprintf(Line, "\t\tsource\t%d", ComputeUniqueOutputId(OutputRefIt->Source, OutputRefIt->PassOutputId));
					GGMLFileWriter.WriteLine(Line);
					FCStringAnsi::Sprintf(Line, "\t\ttarget\t%d", PassId);
					GGMLFileWriter.WriteLine(Line);
				}
				// dashed line
				{
					GGMLFileWriter.WriteLine("\t\tgraphics");
					GGMLFileWriter.WriteLine("\t\t[");
					GGMLFileWriter.WriteLine("\t\t\tstyle\t\"dashed\"");
					GGMLFileWriter.WriteLine("\t\t]");
				}
				{
					FString EdgeName = TEXT("Dependency");

					GGMLFileWriter.WriteLine("\t\tLabelGraphics");
					GGMLFileWriter.WriteLine("\t\t[");
					FCStringAnsi::Sprintf(Line, "\t\t\ttext\t\"%s\"", (const char *)TCHAR_TO_ANSI(*EdgeName));
					GGMLFileWriter.WriteLine(Line);
					GGMLFileWriter.WriteLine("\t\t\tmodel\t\"three_center\"");
					GGMLFileWriter.WriteLine("\t\t\tposition\t\"tcentr\"");
					GGMLFileWriter.WriteLine("\t\t]");
				}
				GGMLFileWriter.WriteLine("\t]");
			}
		}

		uint32 OutputId = 0;
		while(FRenderingCompositeOutput* PassOutput = Pass->GetOutput((EPassOutputId)(OutputId)))
		{
			UE_LOG(LogConsoleResponse,Log, TEXT("  ePId_Output%d %s %s Dep: %d"), OutputId, *PassOutput->RenderTargetDesc.GenerateInfoString(), PassOutput->RenderTargetDesc.DebugName, PassOutput->GetDependencyCount());

			GGMLFileWriter.WriteLine("\tnode");
			GGMLFileWriter.WriteLine("\t[");

			{
				GGMLFileWriter.WriteLine("\t\tgraphics");
				GGMLFileWriter.WriteLine("\t\t[");
				FCStringAnsi::Sprintf(Line, "\t\t\tw\t%d", 220);
				GGMLFileWriter.WriteLine(Line);
				FCStringAnsi::Sprintf(Line, "\t\t\th\t%d", 40);
				GGMLFileWriter.WriteLine(Line);
				GGMLFileWriter.WriteLine("\t\t]");
			}

			{
				FCStringAnsi::Sprintf(Line, "\t\tid\t%d", ComputeUniqueOutputId(Pass, (EPassOutputId)(OutputId)));
				GGMLFileWriter.WriteLine(Line);
				GGMLFileWriter.WriteLine("\t\tLabelGraphics");
				GGMLFileWriter.WriteLine("\t\t[");
				FCStringAnsi::Sprintf(Line, "\t\t\ttext\t\"ePId_Output%d '%s'\r%s\"", 
					OutputId, 
					(const char *)TCHAR_TO_ANSI(PassOutput->RenderTargetDesc.DebugName),
					(const char *)TCHAR_TO_ANSI(*PassOutput->RenderTargetDesc.GenerateInfoString()));
				GGMLFileWriter.WriteLine(Line);
				GGMLFileWriter.WriteLine("\t\t]");
			}


			{
				FCStringAnsi::Sprintf(Line, "\t\tgid\t%d", PassId);
				GGMLFileWriter.WriteLine(Line);
			}

			GGMLFileWriter.WriteLine("\t]");

			++OutputId;
		}

		UE_LOG(LogConsoleResponse,Log, TEXT(""));
	}
#endif

	Context.Pass = Pass;
	Context.SetViewportInvalid();

	// then process the pass itself
	Pass->Process(Context);

	// for VisualizeTexture and output buffer dumping
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	{
		uint32 OutputId = 0;

		while(FRenderingCompositeOutput* PassOutput = Pass->GetOutput((EPassOutputId)OutputId))
		{
			// use intermediate texture unless it's the last one where we render to the final output
			if(PassOutput->PooledRenderTarget)
			{
				GRenderTargetPool.VisualizeTexture.SetCheckPoint(Context.RHICmdList, PassOutput->PooledRenderTarget);

				// If this buffer was given a dump filename, write it out
				const FString& Filename = Pass->GetOutputDumpFilename((EPassOutputId)OutputId);
				if (!Filename.IsEmpty())
				{
					DumpOutputToFile(Context, Filename, PassOutput);
				}

				// If we've been asked to write out the pixel data for this pass to an external array, do it now
				TArray<FColor>* OutputColorArray = Pass->GetOutputColorArray((EPassOutputId)OutputId);
				if (OutputColorArray)
				{
					Context.RHICmdList.ReadSurfaceData(
						PassOutput->PooledRenderTarget->GetRenderTargetItem().TargetableTexture,
						Context.View.ViewRect,
						*OutputColorArray,
						FReadSurfaceDataFlags()
						);
				}
			}

			OutputId++;
		}
	}
#endif

	// iterate through all inputs of tghis pass and decrement the references for it's inputs
	// this can release some intermediate RT so they can be reused
	{
		uint32 InputId = 0;

		while(const FRenderingCompositeOutputRef* OutputRefIt = Pass->GetDependency(InputId++))
		{
			FRenderingCompositeOutput* Input = OutputRefIt->GetOutput();

			if(Input)
			{
				Input->ResolveDependencies();
			}
		}
	}
}

// for debugging purpose O(n)
int32 FRenderingCompositionGraph::ComputeUniquePassId(FRenderingCompositePass* Pass) const
{
	for(uint32 i = 0; i < (uint32)Nodes.Num(); ++i)
	{
		FRenderingCompositePass *Element = Nodes[i];

		if(Element == Pass)
		{
			return i;
		}
	}

	return -1;
}

int32 FRenderingCompositionGraph::ComputeUniqueOutputId(FRenderingCompositePass* Pass, EPassOutputId OutputId) const
{
	uint32 Ret = Nodes.Num();

	for(uint32 i = 0; i < (uint32)Nodes.Num(); ++i)
	{
		FRenderingCompositePass *Element = Nodes[i];

		if(Element == Pass)
		{
			return (int32)(Ret + (uint32)OutputId);
		}

		uint32 OutputCount = 0;
		while(Pass->GetOutput((EPassOutputId)OutputCount))
		{
			++OutputCount;
		}

		Ret += OutputCount;
	}

	return -1;
}


void FRenderingCompositionGraph::ResetDependencies()
{
	for(uint32 i = 0; i < (uint32)Nodes.Num(); ++i)
	{
		FRenderingCompositePass *Element = Nodes[i];

		Element->bGraphMarker = false;

		uint32 OutputId = 0;

		while(FRenderingCompositeOutput* Output = Element->GetOutput((EPassOutputId)(OutputId++)))
		{
			Output->ResetDependency();
		}
	}
}

FRenderingCompositeOutput *FRenderingCompositeOutputRef::GetOutput() const
{
	if(Source == 0)
	{
		return 0;
	}

	return Source->GetOutput(PassOutputId); 
}

FRenderingCompositePass* FRenderingCompositeOutputRef::GetPass() const
{
	return Source;
}

// -----------------------------------------------------------------

void FPostProcessPassParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	BilinearTextureSampler0.Bind(ParameterMap,TEXT("BilinearTextureSampler0"));
	BilinearTextureSampler1.Bind(ParameterMap,TEXT("BilinearTextureSampler1"));
	ViewportSize.Bind(ParameterMap,TEXT("ViewportSize"));
	ViewportRect.Bind(ParameterMap,TEXT("ViewportRect"));
	ScreenPosToPixel.Bind(ParameterMap,TEXT("ScreenPosToPixel"));
	
	for(uint32 i = 0; i < ePId_Input_MAX; ++i)
	{
		PostprocessInputParameter[i].Bind(ParameterMap, *FString::Printf(TEXT("PostprocessInput%d"), i));
		PostprocessInputParameterSampler[i].Bind(ParameterMap, *FString::Printf(TEXT("PostprocessInput%dSampler"), i));
		PostprocessInputSizeParameter[i].Bind(ParameterMap, *FString::Printf(TEXT("PostprocessInput%dSize"), i));
		PostProcessInputMinMaxParameter[i].Bind(ParameterMap, *FString::Printf(TEXT("PostprocessInput%dMinMax"), i));
	}
}

void FPostProcessPassParameters::SetPS(const FPixelShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context, FSamplerStateRHIParamRef Filter, bool bWhiteIfNoTexture, FSamplerStateRHIParamRef* FilterOverrideArray)
{
	Set(ShaderRHI, Context, Filter, bWhiteIfNoTexture, FilterOverrideArray);
}

void FPostProcessPassParameters::SetCS(const FComputeShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context, FSamplerStateRHIParamRef Filter, bool bWhiteIfNoTexture, FSamplerStateRHIParamRef* FilterOverrideArray)
{
	Set(ShaderRHI, Context, Filter, bWhiteIfNoTexture, FilterOverrideArray);
}

void FPostProcessPassParameters::SetVS(const FVertexShaderRHIParamRef& ShaderRHI, const FRenderingCompositePassContext& Context, FSamplerStateRHIParamRef Filter, bool bWhiteIfNoTexture, FSamplerStateRHIParamRef* FilterOverrideArray)
{
	Set(ShaderRHI, Context, Filter, bWhiteIfNoTexture, FilterOverrideArray);
}

template< typename ShaderRHIParamRef >
void FPostProcessPassParameters::Set(
	const ShaderRHIParamRef& ShaderRHI,
	const FRenderingCompositePassContext& Context,
	FSamplerStateRHIParamRef Filter,
	bool bWhiteIfNoTexture,
	FSamplerStateRHIParamRef* FilterOverrideArray)
{
	// assuming all outputs have the same size
	FRenderingCompositeOutput* Output = Context.Pass->GetOutput(ePId_Output0);

	// Output0 should always exist
	check(Output);

	// one should be on
	check(FilterOverrideArray || Filter);
	// but not both
	check(!FilterOverrideArray || !Filter);

	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;

	if(BilinearTextureSampler0.IsBound())
	{
		RHICmdList.SetShaderSampler(
			ShaderRHI, 
			BilinearTextureSampler0.GetBaseIndex(), 
			TStaticSamplerState<SF_Bilinear>::GetRHI()
			);
	}

	if(BilinearTextureSampler1.IsBound())
	{
		RHICmdList.SetShaderSampler(
			ShaderRHI, 
			BilinearTextureSampler1.GetBaseIndex(), 
			TStaticSamplerState<SF_Bilinear>::GetRHI()
			);
	}

	if(ViewportSize.IsBound() || ScreenPosToPixel.IsBound() || ViewportRect.IsBound())
	{
		FIntRect LocalViewport = Context.GetViewport();

		FIntPoint ViewportOffset = LocalViewport.Min;
		FIntPoint ViewportExtent = LocalViewport.Size();

		{
			FVector4 Value(ViewportExtent.X, ViewportExtent.Y, 1.0f / ViewportExtent.X, 1.0f / ViewportExtent.Y);

			SetShaderValue(RHICmdList, ShaderRHI, ViewportSize, Value);
		}

		{
			FVector4 Value(LocalViewport.Min.X, LocalViewport.Min.Y, LocalViewport.Max.X, LocalViewport.Max.Y);

			SetShaderValue(RHICmdList, ShaderRHI, ViewportRect, Value);
		}

		{
			FVector4 ScreenPosToPixelValue(
				ViewportExtent.X * 0.5f,
				-ViewportExtent.Y * 0.5f, 
				ViewportExtent.X * 0.5f - 0.5f + ViewportOffset.X,
				ViewportExtent.Y * 0.5f - 0.5f + ViewportOffset.Y);
			SetShaderValue(RHICmdList, ShaderRHI, ScreenPosToPixel, ScreenPosToPixelValue);
		}
	}

	//Calculate a base scene texture min max which will be pulled in by a pixel for each PP input.
	FIntRect ContextViewportRect = Context.IsViewportValid() ? Context.GetViewport() : FIntRect(0,0,0,0);
	const FIntPoint SceneRTSize = GSceneRenderTargets.GetBufferSizeXY();
	FVector4 BaseSceneTexMinMax(	((float)ContextViewportRect.Min.X/SceneRTSize.X), 
									((float)ContextViewportRect.Min.Y/SceneRTSize.Y), 
									((float)ContextViewportRect.Max.X/SceneRTSize.X), 
									((float)ContextViewportRect.Max.Y/SceneRTSize.Y) );

	// ePId_Input0, ePId_Input1, ...
	for(uint32 Id = 0; Id < (uint32)ePId_Input_MAX; ++Id)
	{
		FRenderingCompositeOutputRef* OutputRef = Context.Pass->GetInput((EPassInputId)Id);

		if(!OutputRef)
		{
			// Pass doesn't have more inputs
			break;
		}

		const auto FeatureLevel = Context.GetFeatureLevel();

		FRenderingCompositeOutput* Input = OutputRef->GetOutput();

		TRefCountPtr<IPooledRenderTarget> InputPooledElement;

		if(Input)
		{
			InputPooledElement = Input->RequestInput();
		}

		FSamplerStateRHIParamRef LocalFilter = FilterOverrideArray ? FilterOverrideArray[Id] : Filter;

		if(InputPooledElement)
		{
			check(!InputPooledElement->IsFree());

			const FTextureRHIRef& SrcTexture = InputPooledElement->GetRenderTargetItem().ShaderResourceTexture;

			SetTextureParameter(RHICmdList, ShaderRHI, PostprocessInputParameter[Id], PostprocessInputParameterSampler[Id], LocalFilter, SrcTexture);

			if(PostprocessInputSizeParameter[Id].IsBound() || PostProcessInputMinMaxParameter[Id].IsBound())
			{
				float Width = InputPooledElement->GetDesc().Extent.X;
				float Height = InputPooledElement->GetDesc().Extent.Y;
				
				FVector2D OnePPInputPixelUVSize = FVector2D(1.0f / Width, 1.0f / Height);

				FVector4 TextureSize(Width, Height, OnePPInputPixelUVSize.X, OnePPInputPixelUVSize.Y);
				SetShaderValue(RHICmdList, ShaderRHI, PostprocessInputSizeParameter[Id], TextureSize);

				//We could use the main scene min max here if it weren't that we need to pull the max in by a pixel on a per input basis.
				FVector4 PPInputMinMax = BaseSceneTexMinMax;
				PPInputMinMax.Z -= OnePPInputPixelUVSize.X;
				PPInputMinMax.W -= OnePPInputPixelUVSize.Y;
				SetShaderValue(RHICmdList, ShaderRHI, PostProcessInputMinMaxParameter[Id], PPInputMinMax);
			}
		}
		else
		{
			IPooledRenderTarget* Texture = bWhiteIfNoTexture ? GSystemTextures.WhiteDummy : GSystemTextures.BlackDummy;

			// if the input is not there but the shader request it we give it at least some data to avoid d3ddebug errors and shader permutations
			// to make features optional we use default black for additive passes without shader permutations
			SetTextureParameter(RHICmdList, ShaderRHI, PostprocessInputParameter[Id], PostprocessInputParameterSampler[Id], LocalFilter, Texture->GetRenderTargetItem().TargetableTexture);

			FVector4 Dummy(1, 1, 1, 1);
			SetShaderValue(RHICmdList, ShaderRHI, PostprocessInputSizeParameter[Id], Dummy);
			SetShaderValue(RHICmdList, ShaderRHI, PostProcessInputMinMaxParameter[Id], Dummy);
		}
	}

	// todo warning if Input[] or InputSize[] is bound but not available, maybe set a specific input texture (blinking?)
}

#define IMPLEMENT_POST_PROCESS_PARAM_SET( ShaderRHIParamRef ) \
	template void FPostProcessPassParameters::Set< ShaderRHIParamRef >( \
		const ShaderRHIParamRef& ShaderRHI,				\
		const FRenderingCompositePassContext& Context,	\
		FSamplerStateRHIParamRef Filter,				\
		bool bWhiteIfNoTexture,							\
		FSamplerStateRHIParamRef* FilterOverrideArray	\
	);

IMPLEMENT_POST_PROCESS_PARAM_SET( FVertexShaderRHIParamRef );
IMPLEMENT_POST_PROCESS_PARAM_SET( FHullShaderRHIParamRef );
IMPLEMENT_POST_PROCESS_PARAM_SET( FDomainShaderRHIParamRef );
IMPLEMENT_POST_PROCESS_PARAM_SET( FGeometryShaderRHIParamRef );
IMPLEMENT_POST_PROCESS_PARAM_SET( FPixelShaderRHIParamRef );
IMPLEMENT_POST_PROCESS_PARAM_SET( FComputeShaderRHIParamRef );

FArchive& operator<<(FArchive& Ar, FPostProcessPassParameters& P)
{
	Ar << P.BilinearTextureSampler0 << P.BilinearTextureSampler1 << P.ViewportSize << P.ScreenPosToPixel << P.ViewportRect;

	for(uint32 i = 0; i < ePId_Input_MAX; ++i)
	{
		Ar << P.PostprocessInputParameter[i];
		Ar << P.PostprocessInputParameterSampler[i];
		Ar << P.PostprocessInputSizeParameter[i];
		Ar << P.PostProcessInputMinMaxParameter[i];
	}

	return Ar;
}

// -----------------------------------------------------------------

const FSceneRenderTargetItem& FRenderingCompositeOutput::RequestSurface(const FRenderingCompositePassContext& Context)
{
	if(PooledRenderTarget)
	{
		return PooledRenderTarget->GetRenderTargetItem();
	}

	if(!RenderTargetDesc.IsValid())
	{
		// useful to use the CompositingGraph dependency resolve but pass the data between nodes differently
		static FSceneRenderTargetItem Null;

		return Null;
	}

	if(!PooledRenderTarget)
	{
		GRenderTargetPool.FindFreeElement(RenderTargetDesc, PooledRenderTarget, RenderTargetDesc.DebugName);
	}

	check(!PooledRenderTarget->IsFree());

	FSceneRenderTargetItem& RenderTargetItem = PooledRenderTarget->GetRenderTargetItem();

	return RenderTargetItem;
}


const FPooledRenderTargetDesc* FRenderingCompositePass::GetInputDesc(EPassInputId InPassInputId) const
{
	// to overcome const issues, this way it's kept local
	FRenderingCompositePass* This = (FRenderingCompositePass*)this;

	const FRenderingCompositeOutputRef* OutputRef = This->GetInput(InPassInputId);

	if(!OutputRef)
	{
		return 0;
	}

	FRenderingCompositeOutput* Input = OutputRef->GetOutput();

	if(!Input)
	{
		return 0;
	}

	return &Input->RenderTargetDesc;
}

uint32 FRenderingCompositePass::ComputeInputCount()
{
	for(uint32 i = 0; ; ++i)
	{
		if(!GetInput((EPassInputId)i))
		{
			return i;
		}
	}
}

uint32 FRenderingCompositePass::ComputeOutputCount()
{
	for(uint32 i = 0; ; ++i)
	{
		if(!GetOutput((EPassOutputId)i))
		{
			return i;
		}
	}
}

FString FRenderingCompositePass::ConstructDebugName()
{
	FString Name;

	uint32 OutputId = 0;
	while(FRenderingCompositeOutput* Output = GetOutput((EPassOutputId)OutputId))
	{
		Name += Output->RenderTargetDesc.DebugName;

		++OutputId;
	}

	if(Name.IsEmpty())
	{
		Name = TEXT("UnknownName");
	}

	return Name;
}
