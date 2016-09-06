//! @file SubstanceCoreHelpers.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @date 20110105
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCoreHelpers.h"
#include "SubstanceCoreClasses.h"
#include "SubstanceFOutput.h"
#include "SubstanceFGraph.h"
#include "SubstanceFPackage.h"
#include "SubstanceCorePreset.h"
#include "SubstanceCache.h"
#include "SubstanceCallbacks.h"

#include "framework/renderer.h"
#include "framework/details/detailslinkdata.h"

#include "RenderCore.h"

#if WITH_EDITOR
#include "SubstanceCoreModule.h"
#include "AssetToolsModule.h"
#include "TexAlignTools.h"
#include "EditorReimportHandler.h"
#include "EditorSupportDelegates.h"
#include "UnrealEdClasses.h"
#include "ObjectTools.h"
#endif

#include "Materials/MaterialExpressionTextureSampleParameter.h"

namespace local
{
	TArray<USubstanceGraphInstance*> InstancesToDelete;
	TArray<UObject*> FactoriesToDelete;
	TArray<UObject*> TexturesToDelete;
}

namespace Substance
{

TSharedPtr<Substance::RenderCallbacks> gCallbacks(new Substance::RenderCallbacks());
TSharedPtr<Substance::Renderer> GSubstanceRenderer(NULL);

const int32 MinPlaceHolderMips = 4;

Substance::List<graph_inst_t*> LoadingQueue; // queue used by PushDelayedRender & PerformDelayedRender
Substance::List<graph_inst_t*> PriorityLoadingQueue; // queue used by PushDelayedRender & PerformDelayedRender

Substance::List<graph_inst_t*> AsyncQueue; //queue used by RenderAsync
Substance::List<graph_inst_t*> CurrentRenderQueue;

uint32 ASyncRunID = 0;

static uint32 GlobalInstancePendingCount = 0;
static uint32 GlobalInstanceCompletedCount = 0;

typedef pair_t< img_input_inst_t*, graph_inst_t* >::Type GraphImageInputPair_t;
TArray<GraphImageInputPair_t> DelayedImageInputs;

namespace Helpers
{

void RenderPush(graph_inst_t* Instance)
{
	if (false == Instance->bHasPendingImageInputRendering)
	{
		GSubstanceRenderer->push(Instance);
	}
}


void RenderAsync(Substance::List<graph_inst_t*>& Instances)
{
	Substance::List<graph_inst_t*>::TIterator Iter(Instances.itfront());
	for (; Iter; ++Iter)
	{
		RenderAsync(*Iter); 
	}
}


void RenderAsync(graph_inst_t* Instance)
{
#if WITH_EDITOR
	FSubstanceCoreModule& SubstanceModule = FModuleManager::LoadModuleChecked<FSubstanceCoreModule>(TEXT("SubstanceCore"));
	const bool PIE = SubstanceModule.isPie();
#else
	const bool PIE = false;
#endif

	//If this graph has been cached before, read from disk
	if (Instance->ParentInstance->bCooked && Instance->ParentInstance->Parent->ShouldCacheOutput())
	{
		if (Substance::SubstanceCache::Get()->ReadFromCache(Instance) && !PIE)
		{
			Instance->ParentInstance->Parent->SubstancePackage->ConditionnalClearLinkData();
			++GlobalInstanceCompletedCount;
			++GlobalInstancePendingCount;
			return;
		}
	}

	//no cache available, queue for pending
	uint32 OldSize = AsyncQueue.size();

	AsyncQueue.AddUnique(Instance);

	if (AsyncQueue.size() > OldSize)
	{
		++GlobalInstancePendingCount;
	}
}


void RenderSync(Substance::List<graph_inst_t*>& Instances)
{
	GSubstanceRenderer->setRenderCallbacks(NULL);

	GlobalInstanceCompletedCount += Instances.Num();
	GlobalInstancePendingCount += Instances.Num();

	GSubstanceRenderer->push(Instances);
	GSubstanceRenderer->run(Substance::Renderer::Run_Default);

	GSubstanceRenderer->setRenderCallbacks(gCallbacks.Get());
}


void RenderSync(graph_inst_t* Instance)
{
	GSubstanceRenderer->setRenderCallbacks(NULL);

	++GlobalInstanceCompletedCount;
	++GlobalInstancePendingCount;
	
	GSubstanceRenderer->push(Instance);
	GSubstanceRenderer->run(Substance::Renderer::Run_Default);

	GSubstanceRenderer->setRenderCallbacks(gCallbacks.Get());
}

void PushDelayedRender(graph_inst_t* Instance)
{
	if (false == Instance->bHasPendingImageInputRendering)
	{
		switch (Instance->ParentInstance->Parent->GetGenerationMode())
        {
        case SGM_OnLoadAsync:
        case SGM_OnLoadAsyncAndCache:
            LoadingQueue.AddUnique(Instance);
            break;
        default:
            PriorityLoadingQueue.AddUnique(Instance);
            break;
        }
	}
	else // instances waiting for image inputs are not filed in the priority queue
	{
		LoadingQueue.AddUnique(Instance);
	}
}

void PushDelayedRender(img_input_inst_t* ImgInput,graph_inst_t* Instance)
{
	if (false == Instance->bHasPendingImageInputRendering)
	{
		switch (Instance->ParentInstance->Parent->GetGenerationMode())
        {
		case SGM_OnLoadAsync:
		case SGM_OnLoadAsyncAndCache:
			LoadingQueue.AddUnique(Instance);
			break;
        default:
			PriorityLoadingQueue.AddUnique(Instance);
			break;
        }
	}
	else // instances waiting for image inputs are not filed in the priority queue
	{
		LoadingQueue.AddUnique(Instance);
	}
}


void PushDelayedImageInput(img_input_inst_t* ImgInput, graph_inst_t* Instance)
{
	DelayedImageInputs.AddUnique(std::make_pair(ImgInput, Instance));
}

void RemoveFromDelayedImageInputs(img_input_inst_t* ImgInput)
{
	TArray<GraphImageInputPair_t>::TIterator ItInp(DelayedImageInputs);

	while (ItInp)
	{
		if ((*ItInp).first == ImgInput)
		{
			LoadingQueue.Remove(ItInp->second);

			DelayedImageInputs.RemoveAt(ItInp.GetIndex());
			ItInp.Reset();
		}
		else
		{
			++ItInp;
		}
	}
}

void SetDelayedImageInput()
{
	TArray<GraphImageInputPair_t>::TIterator ItInp(DelayedImageInputs);

	for(;ItInp;++ItInp)
	{
		img_input_inst_t* ImgInput = (*ItInp).first;
		graph_inst_t* Instance = (*ItInp).second;

		ImgInput->SetImageInput(ImgInput->ImageSource, Instance, false);
		Instance->bHasPendingImageInputRendering = false;
	}

	DelayedImageInputs.Empty();
}


void PerformDelayedRender()
{
	if (PriorityLoadingQueue.Num())
	{
		Substance::List<graph_inst_t*> RemoveList;

		//see if we have cache entries first
		auto iter = PriorityLoadingQueue.itfront();
		while (iter)
		{
			graph_inst_t* graph = *iter;

			if (graph->ParentInstance->bCooked && graph->ParentInstance->Parent->ShouldCacheOutput())
			{
				if (Substance::SubstanceCache::Get()->ReadFromCache(graph))
				{
					++GlobalInstancePendingCount;
					++GlobalInstanceCompletedCount;
					RemoveList.push(graph);
				}
			}
			
			++GlobalInstancePendingCount;
			iter++;
		}

		auto iterr = RemoveList.itfront();
		while (iterr)
		{
			PriorityLoadingQueue.Remove(*iterr);
			iterr++;
		}

		GSubstanceRenderer->clearCache();

		if (PriorityLoadingQueue.Num())
		{
			RenderSync(PriorityLoadingQueue);
			Substance::Helpers::UpdateTextures(PriorityLoadingQueue);
			PriorityLoadingQueue.Empty();
		}
	}

	SetDelayedImageInput();

	if (LoadingQueue.Num())
	{
		RenderAsync(LoadingQueue);
		LoadingQueue.Empty();
	}
}


void UpdateSubstanceOutput(USubstanceTexture2D* Texture, const SubstanceTexture& ResultText)
{
	// make sure any outstanding resource update has been completed
	FlushRenderingCommands();

	// grab the Result computed in the Substance Thread
	const SIZE_T Mipstart = (SIZE_T) ResultText.buffer;
	SIZE_T MipOffset = 0;

	// prepare mip map data
	FTexture2DMipMap* MipMap = 0;

	Texture->Format = Substance::Helpers::SubstanceToUe4Format((SubstancePixelFormat)ResultText.pixelFormat, (SubstanceChannelsOrder)ResultText.channelsOrder);

	Texture->NumMips = ResultText.mipmapCount;
	
	// create as much mip as necessary
	if (Texture->Mips.Num() != ResultText.mipmapCount ||
		ResultText.level0Width != Texture->SizeX ||
		ResultText.level0Height != Texture->SizeY)
	{
		Texture->Mips.Empty();

		int32 MipSizeX = Texture->SizeX = ResultText.level0Width;
		int32 MipSizeY = Texture->SizeY = ResultText.level0Height;

		for (int32 IdxMip=0 ; IdxMip < ResultText.mipmapCount ; ++IdxMip)
		{
			MipMap = new(Texture->Mips) FTexture2DMipMap;
			MipMap->SizeX = MipSizeX;
			MipMap->SizeY = MipSizeY;

			// compute the next mip size
			MipSizeX = FMath::Max(MipSizeX>>1, 1);
			MipSizeY = FMath::Max(MipSizeY>>1, 1);

			// not smaller than the "block size"
			MipSizeX = FMath::Max((int32)GPixelFormats[Texture->Format].BlockSizeX,MipSizeX);
			MipSizeY = FMath::Max((int32)GPixelFormats[Texture->Format].BlockSizeY,MipSizeY);
		}
	}

	// fill up the mips
	for (int32 IdxMip=0 ; IdxMip < ResultText.mipmapCount ; ++IdxMip)
	{
		MipMap = &Texture->Mips[IdxMip];

		// get the size of the mip's content
		const SIZE_T ImageSize = CalculateImageBytes(
			MipMap->SizeX,
			MipMap->SizeY,
			0,
			Texture->Format);
		check(0 != ImageSize);

		void* TheMipDataPtr = NULL;

		// copy the data
		if (MipMap->BulkData.GetBulkDataSize() != ImageSize)
		{
			MipMap->BulkData = FByteBulkData();
			MipMap->BulkData.Lock(LOCK_READ_WRITE);
			TheMipDataPtr = MipMap->BulkData.Realloc(ImageSize);
		}
		else
		{
			TheMipDataPtr = MipMap->BulkData.Lock(LOCK_READ_WRITE);
		}		

		if (Texture->Format == PF_B8G8R8A8)
		{
			uint8* SrcPixels = (uint8*)(Mipstart + MipOffset);
			uint8* DstPixels = (uint8*)TheMipDataPtr;

			//swizzle channel order
			int indices[] = {
				ResultText.channelsOrder & 0x3,
				(ResultText.channelsOrder >> 2) & 0x3,
				(ResultText.channelsOrder >> 4) & 0x3,
				(ResultText.channelsOrder >> 6) & 0x3,
			};
			
			for (SIZE_T Idx = 0; Idx < ImageSize; Idx +=4)
			{
				// substance outputs rgba8, convert to bgra8
				// r, g, b, a = b, g, r, a
				DstPixels[Idx+0] = SrcPixels[Idx+indices[2]];
				DstPixels[Idx+1] = SrcPixels[Idx+indices[1]];
				DstPixels[Idx+2] = SrcPixels[Idx+indices[0]];
				DstPixels[Idx+3] = SrcPixels[Idx+indices[3]];
			}
		}
		else
		{
			FMemory::Memcpy(TheMipDataPtr, (void*)(Mipstart + MipOffset), ImageSize);
		}
	
		MipOffset += ImageSize;
		MipMap->BulkData.ClearBulkDataFlags( BULKDATA_SingleUse );
		MipMap->BulkData.Unlock();
	}
}


void UpdateTexture(const SubstanceTexture& result, output_inst_t* Output, bool bCacheResults /*= true*/)
{
	USubstanceTexture2D* Texture = *(Output->Texture.get());

	if (NULL == Texture)
	{
		if (bCacheResults)
		{
			Substance::SubstanceCache::Get()->CacheOutput(Output, result);
		}

		return;
	}

	//publish to cache if appropriate
	if (bCacheResults)
	{
		if (Texture->ParentInstance->bCooked)
		{
			if (Texture->ParentInstance->Parent->ShouldCacheOutput())
			{
				Substance::SubstanceCache::Get()->CacheOutput(Output, result);
			}
		}
	}

	Helpers::UpdateSubstanceOutput(Texture, result);
	Texture->UpdateResource();

	Texture->OutputCopy->bIsDirty = false;
}


void OnWorldInitialized()
{
	GlobalInstanceCompletedCount = 0;
	GlobalInstancePendingCount = 0;

	PerformDelayedRender();
}

void OnLevelAdded()
{
	PerformDelayedRender();
}

void StartPIE()
{
#if WITH_EDITOR
	GSubstanceRenderer->flush();
	
	//update outputs
	Substance::List<output_inst_t*> Outputs =
		RenderCallbacks::getComputedOutputs(false);

	Substance::List<output_inst_t*>::TIterator ItOut(Outputs.itfront());

	bool bUpdatedOutput = false;

	for (; ItOut; ++ItOut)
	{
		// Grab Result (auto pointer on RenderResult)
		output_inst_t::Result Result = ((*ItOut)->grabResult());

		if (Result.get())
		{
			UpdateTexture(Result->getTexture(), *ItOut);
			bUpdatedOutput = true;
		}
	}
#endif
}


void Tick()
{
	//update outputs
	Substance::List<output_inst_t*> Outputs =
		RenderCallbacks::getComputedOutputs(!GIsEditor);
		
	Substance::List<output_inst_t*>::TIterator ItOut(Outputs.itfront());

	bool bUpdatedOutput = false;

	for (; ItOut; ++ItOut)
	{
		// Grab Result (auto pointer on RenderResult)
		output_inst_t::Result Result = ((*ItOut)->grabResult());

		if (Result.get())
		{
			UpdateTexture(Result->getTexture(), *ItOut);
			bUpdatedOutput = true;
		}
	}

#if WITH_EDITOR
	if (bUpdatedOutput)
	{
		// Refresh viewports
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		ASyncRunID = 0;
	}
#endif

	//check if our last async job has completed
	if (!GIsEditor && CurrentRenderQueue.Num() && ASyncRunID != 0 && !GSubstanceRenderer->isPending(ASyncRunID) && RenderCallbacks::isOutputQueueEmpty())
	{
		ASyncRunID = 0;

		// free some memory by reseting the engine each batch of rendering
		GSubstanceRenderer = TSharedPtr<Substance::Renderer>(new Substance::Renderer());
		GSubstanceRenderer->setRenderCallbacks(gCallbacks.Get());

		for (auto graph = CurrentRenderQueue.itfront(); graph; ++graph)
		{
			check(*graph != NULL);
			check((*graph)->ParentInstance->IsValidLowLevel());
			check((*graph)->ParentInstance->Parent->IsValidLowLevel());
			check((*graph)->ParentInstance->Parent->SubstancePackage != NULL);

			(*graph)->ParentInstance->Parent->SubstancePackage->ConditionnalClearLinkData();
		}

		GlobalInstanceCompletedCount += CurrentRenderQueue.Num();
		CurrentRenderQueue.Empty();	
	}
	
#if WITH_EDITOR
	//push async substances to renderer
	if (AsyncQueue.Num() && 0 == ASyncRunID)
	{
		bool SmthgPushed = GSubstanceRenderer->push(AsyncQueue);
		AsyncQueue.Empty();
#else // WITH_EDITOR
	if (AsyncQueue.Num() && !CurrentRenderQueue.Num())
	{
		// limit the number of substances being rendered at the same time
		// to save some memory
		const int BatchSize = 4;

		while (AsyncQueue.Num() != 0 && CurrentRenderQueue.Num() <= BatchSize)
		{
			CurrentRenderQueue.AddUnique(AsyncQueue.pop());
		}

		bool SmthgPushed = GSubstanceRenderer->push(CurrentRenderQueue);

		if (!SmthgPushed)
		{
			CurrentRenderQueue.Empty();
		}

#endif //WITH_EDITOR

		if (SmthgPushed)
		{
			ASyncRunID = GSubstanceRenderer->run(
				Substance::Renderer::Run_Asynchronous |
				Substance::Renderer::Run_Replace |
				Substance::Renderer::Run_First |
				Substance::Renderer::Run_PreserveRun
				);
		}		
	}

	Substance::Helpers::PerformDelayedDeletion();
}


bool isSupportedByDefaultShader(output_inst_t* OutputInstance)
{
	check(OutputInstance);

	// find the description 
	output_desc_t* Desc = OutputInstance->GetOutputDesc();

	// create Output if it does match to a slot of the default material
	switch (Desc->Channel)
	{
	case CHAN_BaseColor:
	case CHAN_SpecularColor:
	case CHAN_Metallic:
	case CHAN_Specular:
	case CHAN_Roughness:
	case CHAN_Emissive:
	case CHAN_Normal:
	case CHAN_Mask:
	case CHAN_Opacity:
	case CHAN_Refraction:
	case CHAN_AmbientOcclusion:
		return true;
	default:
		return false;
	}
}


void Split_RGBA_8bpp(
	int32 Width, int32 Height,
	uint8* DecompressedImageRGBA, const int32 TextureDataSizeRGBA,
	uint8* DecompressedImageA, const int32 TextureDataSizeA22)
{
	uint8 Pixel[4] = { 0, 0, 0, 0 };

	if (DecompressedImageA)
	{
		uint8* ImagePtrRGBA = DecompressedImageRGBA + 3;
		uint8* ImagePtrA = DecompressedImageA;

		for (int32 Y = 0; Y < Height; Y++)
		{
			for (int32 X = 0; X < Width; X++)
			{
				Pixel[0] = *ImagePtrRGBA;
				ImagePtrRGBA += 4;
				*ImagePtrA++ = Pixel[0];
			}
		}
	}
}


void EnableTexture(output_inst_t* OutputInstance, graph_inst_t* GraphInstance)
{
#if WITH_EDITOR
	FString TextureName;
	FString PackageName;
	GetSuitableName(OutputInstance, TextureName, PackageName);
#else
	FString TextureName;
	FString PackageName;

	PackageName = GraphInstance->ParentInstance->GetName();
#endif // WITH_EDITOR

	UObject* TextureParent = CreatePackage(NULL, *PackageName);

	CreateSubstanceTexture2D(OutputInstance, false, TextureName, TextureParent);
}


void CreateTextures(graph_inst_t* GraphInstance)
{
	bool bCreateAllOutputs = false;
	GConfig->GetBool(TEXT("Substance"), TEXT("bCreateAllOutputs"), bCreateAllOutputs, GEngineIni);

	//special case for glossiness and diffuse
	bool hasRoughness = false;
	output_inst_t* GlossinessOutput = NULL;

	bool hasBaseColor = false;
	output_inst_t* DiffuseOutput = NULL;

	for(uint32 Idx=0 ; Idx<GraphInstance->Outputs.size() ; ++Idx)
	{
		output_inst_t* OutputInstance = &GraphInstance->Outputs[Idx];
		USubstanceTexture2D** ptr = OutputInstance->Texture.get();
		
		output_desc_t* Desc = OutputInstance->GetOutputDesc();
		if (Desc->Channel == CHAN_Roughness)
		{
			hasRoughness = true;
		}
		else if (Desc->Channel == CHAN_Glossiness)
		{
			GlossinessOutput = OutputInstance;
		}
		else if (Desc->Channel == CHAN_BaseColor)
		{
			hasBaseColor = true;
		}
		else if (Desc->Channel == CHAN_Diffuse)
		{
			DiffuseOutput = OutputInstance;
		}

		check(ptr && *ptr == NULL);

		if (bCreateAllOutputs || isSupportedByDefaultShader(OutputInstance))
		{
			EnableTexture(OutputInstance, GraphInstance);
		}
	}

	if (!hasRoughness && GlossinessOutput && !bCreateAllOutputs)
	{
		EnableTexture(GlossinessOutput, GraphInstance);
	}

	if (!hasBaseColor && DiffuseOutput && !bCreateAllOutputs)
	{
		EnableTexture(DiffuseOutput, GraphInstance);
	}
}


bool UpdateTextures(Substance::List<FGraphInstance*>& Instances)
{
	bool GotSomething = false;

	Substance::List<FGraphInstance*>::TIterator ItInst(Instances.itfront());
	
	for (;ItInst;++ItInst)
	{
		// Iterate on all Outputs
		Substance::List<output_inst_t>::TIterator ItOut((*ItInst)->Outputs.itfront());

		for (;ItOut;++ItOut)
		{
			// Grab Result (auto pointer on RenderResult)
			output_inst_t::Result Result = ((*ItOut).grabResult());

			if (Result.get())
			{
				(*ItInst)->ParentInstance->MarkPackageDirty();
				UpdateTexture(Result->getTexture(), &*ItOut);
				GotSomething = true;
			}
		}
	}

	return GotSomething;
}

void CreateSubstanceTexture2D(output_inst_t* OutputInstance, bool Transient, FString Name, UObject* InOuter)
{
	check(OutputInstance);
	check(OutputInstance->ParentInstance);
	check(OutputInstance->ParentInstance->Instance->Desc);
	check(InOuter);

	USubstanceTexture2D* Texture = NULL;

	Texture =
		NewObject<USubstanceTexture2D>(
			InOuter,
			Name.Len() ? *Name : TEXT(""),
			Transient ? RF_NoFlags : RF_Public | RF_Standalone
			);

	Texture->MarkPackageDirty();

	Texture->ParentInstance = OutputInstance->ParentInstance;
	Texture->OutputGuid = OutputInstance->OutputGuid;
	
	output_desc_t* Desc = OutputInstance->GetOutputDesc();

	if (Desc && Desc->Channel == CHAN_Normal)
	{
		Texture->SRGB = false;
		Texture->CompressionSettings = TC_Normalmap;
	}
	else if(Desc && (Desc->Channel == CHAN_BaseColor
		|| Desc->Channel == CHAN_Diffuse
		|| Desc->Channel == CHAN_Specular
		|| Desc->Channel == CHAN_Emissive))
	{
		Texture->SRGB = true;
	}
	else if (Desc && (Desc->Channel == CHAN_Roughness
		|| Desc->Channel == CHAN_Metallic
		|| Desc->Channel == CHAN_SpecularLevel
		|| Desc->Channel == CHAN_Glossiness
		|| Desc->Channel == CHAN_Opacity
		|| Desc->Channel == CHAN_Displacement
		|| Desc->Channel == CHAN_Height))
	{
		Texture->SRGB = false;
	}
	
	EPixelFormat Format = 
		SubstanceToUe4Format(
			(SubstancePixelFormat)OutputInstance->Format, Substance_ChanOrder_NC);

	// unsupported format
	if (PF_Unknown == Format)
	{
		Texture->ClearFlags(RF_Standalone);
		return;
	}

	switch (Format)
	{
	case PF_G8:
	case PF_G16:
		Texture->SRGB = false;
		Texture->CompressionSettings = TC_Grayscale;
		break;
#if WITH_EDITOR
	case PF_DXT1:
	case PF_ATC_RGB:
	case PF_ETC1:
	case PF_ETC2_RGB:
		Texture->CompressionNoAlpha = true;
		break;
#endif
	}

	Texture->Format = Format;
	Texture->SizeX = 0;
	Texture->SizeY = 0;

	OutputInstance->bIsEnabled = true;
	OutputInstance->flagAsDirty();
	*OutputInstance->Texture.get() = Texture;

	// make a copy of the Output instance in the texture
	(*OutputInstance->Texture.get())->OutputCopy =
		new FOutputInstance(*OutputInstance);
}


void CreatePlaceHolderMips(graph_inst_t* Instance)
{
	check(Instance);
	check(Instance->Outputs.Num());
		
	// Iterate on all Outputs
	Substance::List<output_inst_t>::TIterator ItOut(Instance->Outputs.itfront());

	// for each enabled output, create a placeholder mipmap pyramid 
	// with the following format & size : PF_B8G8R8A8 16x16
	for (; ItOut; ++ItOut)
	{
		if (false == ItOut->bIsEnabled)
		{
			continue;
		}

		USubstanceTexture2D* Texture = *ItOut->Texture;
		FTexture2DMipMap* MipMap = 0;

		Texture->Format = PF_B8G8R8A8;

		Texture->NumMips = MinPlaceHolderMips;	
		Texture->Mips.Empty();

		int32 MipSizeX = Texture->SizeX = FMath::Pow(2, MinPlaceHolderMips);
		int32 MipSizeY = Texture->SizeY = FMath::Pow(2, MinPlaceHolderMips);
		SIZE_T MipOffset = 0;

		for (int32 IdxMip = 0; IdxMip < Texture->NumMips; ++IdxMip)
		{
			MipMap = new(Texture->Mips) FTexture2DMipMap;
			MipMap->SizeX = MipSizeX;
			MipMap->SizeY = MipSizeY;

			// compute the next mip size
			MipSizeX = FMath::Max(MipSizeX >> 1, 1);
			MipSizeY = FMath::Max(MipSizeY >> 1, 1);

			// not smaller than the "block size"
			MipSizeX = FMath::Max((int32)GPixelFormats[Texture->Format].BlockSizeX, MipSizeX);
			MipSizeY = FMath::Max((int32)GPixelFormats[Texture->Format].BlockSizeY, MipSizeY);
		}
	

		// fill up the mips
		for (int32 IdxMip = 0; IdxMip < Texture->NumMips; ++IdxMip)
		{
			MipMap = &Texture->Mips[IdxMip];

			// get the size of the mip's content
			const SIZE_T ImageSize = CalculateImageBytes(
				MipMap->SizeX,
				MipMap->SizeY,
				0,
				Texture->Format);
			check(0 != ImageSize);

			// copy the data
			MipMap->BulkData = FByteBulkData();
			MipMap->BulkData.Lock(LOCK_READ_WRITE);
			void* TheMipDataPtr = MipMap->BulkData.Realloc(ImageSize);

			uint8* Pixels = (uint8*)TheMipDataPtr;

			for (SIZE_T Idx = 0; Idx < ImageSize; Idx += 4)
			{
				Pixels[0] = 128;
				Pixels[1] = 128;
				Pixels[2] = 128;
				Pixels[3] = 0;
				Pixels += 4;
			}
			
			MipOffset += ImageSize;
			MipMap->BulkData.ClearBulkDataFlags(BULKDATA_SingleUse);
			MipMap->BulkData.Unlock();
		}

		Texture->UpdateResource();
	}
}


SubstancePixelFormat ValidateFormat(const SubstancePixelFormat Format)
{
	int32 NewFormat = Format;

	// rgba16b is the default output format, fall back to DXT1 in that case
	if (Format == (Substance_PF_RAW|Substance_PF_16b)) 
	{
		NewFormat = Substance_PF_DXT1;
	}

	//! @note: SRGB is not implemented in Substance for the moment
	{
		NewFormat &= ~Substance_PF_sRGB;
	}

	switch(NewFormat)
	{
	case Substance_PF_RGBA:
		NewFormat = Substance_PF_RGBA;
		break;

	case Substance_PF_RGB:
	case Substance_PF_RGBx:
		NewFormat = Substance_PF_RGB;
		break;

	// we support the greyscale format
	case Substance_PF_L:
		NewFormat = Substance_PF_L;
		break;

	case Substance_PF_16b | Substance_PF_L:
		NewFormat = Substance_PF_L | Substance_PF_16b;
		break;

	case Substance_PF_DXT1:
		NewFormat = Substance_PF_DXT1;
		break;

	case Substance_PF_DXT2:
	case Substance_PF_DXT3:
		NewFormat = Substance_PF_DXT3;
		break;

	// the rest should be DXT5
	case Substance_PF_DXTn:
	case Substance_PF_DXT4:
	case Substance_PF_DXT5:
	default:
		NewFormat = Substance_PF_DXT5;
		break;
	}

	return (SubstancePixelFormat)NewFormat;
}


bool IsSupported(EPixelFormat Fmt)
{
	switch (Fmt)
	{
		case PF_DXT1:
		case PF_DXT3:
		case PF_DXT5:
		case PF_A8R8G8B8:
		case PF_R8G8B8A8:
		case PF_B8G8R8A8:
		case PF_G8:
		case PF_G16:
			return true;
		default:
			return false;
	}
}


SubstancePixelFormat UE4FormatToSubstance(EPixelFormat Fmt)
{
	switch (Fmt)
	{
	case PF_DXT1: return Substance_PF_DXT1;
	case PF_DXT3: return Substance_PF_DXT3;
	case PF_DXT5: return Substance_PF_DXT5;

	case PF_R8G8B8A8:
	case PF_B8G8R8A8:
	case PF_A8R8G8B8: return Substance_PF_RGBA;
	case PF_G8: return Substance_PF_L;
	case PF_G16: return SubstancePixelFormat(Substance_PF_L|Substance_PF_16b);

	default:
		return Substance_PF_RAW;
	}
}


EPixelFormat SubstanceToUe4Format(const SubstancePixelFormat Format, const SubstanceChannelsOrder ChanOrder)
{
	SubstancePixelFormat InFmt = SubstancePixelFormat(Format &~ Substance_PF_sRGB);

	EPixelFormat OutFormat = PF_Unknown;

	//! @see SubstanceXmlHelper.cpp, ValidateFormat(...) function
	switch ((unsigned int)InFmt)
	{

	case Substance_PF_L|Substance_PF_16b:
		OutFormat = PF_G16;
		break;

	case Substance_PF_L:
		OutFormat = PF_G8;
		break;

	case Substance_PF_DXT1:
		OutFormat = PF_DXT1;
		break;

	case Substance_PF_DXT3:
		OutFormat = PF_DXT3;
		break;

	case Substance_PF_DXT5:
		OutFormat = PF_DXT5;
		break;

	case Substance_PF_RGBA:
	case Substance_PF_RGBx:
	case Substance_PF_RGB:
		{
			if (ChanOrder == Substance_ChanOrder_ARGB)
				OutFormat = PF_A8R8G8B8;
			else if (ChanOrder == Substance_ChanOrder_RGBA)
				OutFormat = PF_R8G8B8A8;
			else
				OutFormat = PF_B8G8R8A8;//We special case BGRA to read swizzling later (ABGR etc)
		}
		break;

	// all other formats are replaced by 
	// one of the previous ones (DXT5 mostly)
	default: 
		OutFormat = PF_Unknown;
		break;
	}

	return OutFormat;
}


bool AreInputValuesEqual(
	TSharedPtr<input_inst_t> &A, TSharedPtr<input_inst_t> &B)
{
	input_inst_t* InstanceA = A.Get();
	input_inst_t* InstanceB = B.Get();

	if (!InstanceA || !InstanceB)
	{
		check(0);
		return false;
	}
	
	// don't bother comparing values of inputs that don't relate to the same Input
	if ((InstanceA->Uid != InstanceB->Uid) || 
		(InstanceA->Type != InstanceB->Type))   // if the UIDs match, the type
	{											// should also match, but better
		return false;							// safe than sorry...
	}

	switch((SubstanceInputType)InstanceA->Type)
	{
	case Substance_IType_Float:
		{
			FNumericalInputInstance<float>* InputA = 
				(FNumericalInputInstance<float>*)InstanceA;
				
			FNumericalInputInstance<float>* InputB = 
				(FNumericalInputInstance<float>*)InstanceB;

			return FMath::IsNearlyEqual(InputA->Value, InputB->Value,(float)DELTA);
		}
		break;
	case Substance_IType_Float2:
		{
			FNumericalInputInstance<vec2float_t>* InputA = 
				(FNumericalInputInstance<vec2float_t>*)InstanceA;
				
			FNumericalInputInstance<vec2float_t>* InputB = 
				(FNumericalInputInstance<vec2float_t>*)InstanceB;

			return 
				FMath::IsNearlyEqual(InputA->Value.X, InputB->Value.X,(float)DELTA) && 
				FMath::IsNearlyEqual(InputA->Value.Y, InputB->Value.Y,(float)DELTA);
		}
		break;
	case Substance_IType_Float3:
		{
			FNumericalInputInstance<vec3float_t>* InputA = 
				(FNumericalInputInstance<vec3float_t>*)InstanceA;
				
			FNumericalInputInstance<vec3float_t>* InputB = 
				(FNumericalInputInstance<vec3float_t>*)InstanceB;
				
			return
				FMath::IsNearlyEqual(InputA->Value.X, InputB->Value.X,(float)DELTA) && 
				FMath::IsNearlyEqual(InputA->Value.Y, InputB->Value.Y,(float)DELTA) && 
				FMath::IsNearlyEqual(InputA->Value.Z, InputB->Value.Z,(float)DELTA);
		}
		break;
	case Substance_IType_Float4:
		{
			FNumericalInputInstance<vec4float_t>* InputA = 
				(FNumericalInputInstance<vec4float_t>*)InstanceA;
				
			FNumericalInputInstance<vec4float_t>* InputB = 
				(FNumericalInputInstance<vec4float_t>*)InstanceB;
				
			return
				FMath::IsNearlyEqual(InputA->Value.X, InputB->Value.X,(float)DELTA) && 
				FMath::IsNearlyEqual(InputA->Value.Y, InputB->Value.Y,(float)DELTA) && 
				FMath::IsNearlyEqual(InputA->Value.Z, InputB->Value.Z,(float)DELTA) && 
				FMath::IsNearlyEqual(InputA->Value.W, InputB->Value.W,(float)DELTA);
		}
		break;
	case Substance_IType_Integer:
		{
			FNumericalInputInstance<int32>* InputA = 
				(FNumericalInputInstance<int32>*)InstanceA;
				
			FNumericalInputInstance<int32>* InputB = 
				(FNumericalInputInstance<int32>*)InstanceB;
				
			return InputA->Value == InputB->Value;
		}
		break;
	case Substance_IType_Integer2:
		{
			FNumericalInputInstance<vec2int_t>* InputA = 
				(FNumericalInputInstance<vec2int_t>*)InstanceA;
				
			FNumericalInputInstance<vec2int_t>* InputB = 
				(FNumericalInputInstance<vec2int_t>*)InstanceB;
				
			return InputA->Value == InputB->Value;

		}
		break;
	case Substance_IType_Integer3:
		{
			FNumericalInputInstance<vec3int_t>* InputA = 
				(FNumericalInputInstance<vec3int_t>*)InstanceA;
				
			FNumericalInputInstance<vec3int_t>* InputB = 
				(FNumericalInputInstance<vec3int_t>*)InstanceB;
				
			return InputA->Value == InputB->Value;
		}
		break;
	case Substance_IType_Integer4:
		{
			FNumericalInputInstance<vec4int_t>* InputA = 
				(FNumericalInputInstance<vec4int_t>*)InstanceA;
				
			FNumericalInputInstance<vec4int_t>* InputB = 
				(FNumericalInputInstance<vec4int_t>*)InstanceB;
				
			return InputA->Value == InputB->Value;
		}
		break;
	case Substance_IType_Image:
		{
			FImageInputInstance* ImgInputA =
				(FImageInputInstance*)InstanceA;
			
			FImageInputInstance* ImgInputB =
				(FImageInputInstance*)InstanceB;

			return (ImgInputA->ImageSource == ImgInputB->ImageSource) ? true : false;
		}
		break;
	default:
		break;
	}

	check(0);
	return false;
}


void SetupSubstance()
{
	GSubstanceRenderer = TSharedPtr<Substance::Renderer>(new Substance::Renderer());
	GSubstanceRenderer->setRenderCallbacks(gCallbacks.Get());
}


void TearDownSubstance()
{
	LoadingQueue.Empty();
	PriorityLoadingQueue.Empty();
	AsyncQueue.Empty();
	CurrentRenderQueue.Empty();
	
	DelayedImageInputs.Empty();

	GSubstanceRenderer.Reset();
	SubstanceCache::Shutdown();
}


void Disable(output_inst_t* Output)
{
	Output->bIsEnabled = false;

	if (*Output->Texture.get())
	{
		Clear(Output->Texture);
	}
}


void Clear(std::shared_ptr<USubstanceTexture2D*>& OtherTexture)
{
	check(*(OtherTexture).get() != NULL);
	check(OtherTexture.unique() == false);

	if (IsGarbageCollecting())
	{
		(*OtherTexture)->ClearFlags(RF_Standalone);
	}

	delete (*OtherTexture)->OutputCopy;
	(*OtherTexture)->OutputCopy = NULL;
	*(OtherTexture).get() = NULL;
}


graph_desc_t* FindParentGraph(
	Substance::List<graph_desc_t*>& Graphs,
	graph_inst_t* Instance)
{
	Substance::List<graph_desc_t*>::TIterator GraphIt(Graphs.itfront());
	
	for ( ; GraphIt ; ++GraphIt)
	{
		Substance::List<substanceGuid_t>::TConstIterator 
			UidIt((*GraphIt)->InstanceUids.itfrontconst());

		for (; UidIt ; ++UidIt )
		{
			if (*UidIt == Instance->InstanceGuid)
			{
				return (*GraphIt);
			}
		}
	}

	return NULL;
}


graph_desc_t* FindParentGraph(
	Substance::List<graph_desc_t*>& Graphs,
	const FString& ParentUrl)
{
	Substance::List<graph_desc_t*>::TIterator GraphIt(Graphs.itfront());

	for ( ; GraphIt ; ++GraphIt)
	{
		if ((*GraphIt)->PackageUrl == ParentUrl)
		{
			return (*GraphIt);
		}
	}

	return NULL;
}


void DecompressJpeg(
	const void* Buffer, const int32 Length,
	TArray<uint8>& outRawData,
	int32* outWidth, int32* outHeight, int32 NumComp )
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );

	IImageWrapperPtr ImageWrapper;

	if (NumComp == 1)
	{
		ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::GrayscaleJPEG);
	}
	else
	{
		ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::JPEG  );
	}

	ImageWrapper->SetCompressed(Buffer, Length);

	const TArray<uint8>* outRawDataPtr;
	ImageWrapper->GetRaw(NumComp == 1 ? ERGBFormat::Gray : ERGBFormat::BGRA, 8, outRawDataPtr);
	outRawData = *outRawDataPtr;

	*outWidth = ImageWrapper->GetWidth();
	*outHeight = ImageWrapper->GetHeight();
}


//! @brief Compress a jpeg buffer in RAW RGBA
void CompressJpeg(
	const void* InBuffer, const int32 InLength, 
	const int32 W, const int32 H, const int32 NumComp,
	TArray<uint8>& outCompressedData,
	int32 Quality)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
	IImageWrapperPtr ImageWrapper;

	if (NumComp == 1)
	{
		ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::GrayscaleJPEG);
	}
	else
	{
		ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);
	}

	ImageWrapper->SetRaw(InBuffer, InLength, W, H, NumComp == 1 ? ERGBFormat::Gray : ERGBFormat::BGRA, 8);

	const int32 JPEGQuality = Quality;
	
	outCompressedData = ImageWrapper->GetCompressed(JPEGQuality);
}


void Join_RGBA_8bpp(
	int32 Width, int32 Height,
	uint8* DecompressedImageRGBA, const int32 TextureDataSizeRGBA,
	uint8* DecompressedImageA, const int32 TextureDataSizeA)
{
	check(DecompressedImageRGBA);
	
	if (DecompressedImageA)
	{
		uint8* DstImagePtrA = DecompressedImageRGBA + 3;
		uint8* ImagePtrA = DecompressedImageA;

		for(int32 Y = 0; Y < Height; Y++)
		{					
			for(int32 X = 0;X < Width;X++)
			{	
				*DstImagePtrA = *ImagePtrA++;
				DstImagePtrA += 4;
			}
		}
	}
}


void PrepareFileImageInput_GetBGRA(USubstanceImageInput* Input, uint8** Outptr, int32& outWidth, int32& outHeight)
{
	TArray<uint8> UncompressedImageRGBA;
	TArray<uint8> UncompressedImageA;
	
	// decompress both parts, color and alpha
	if (Input->ImageRGB.GetBulkDataSize())
	{
		if (Input->CompressionRGB)
		{
			uint8* CompressedImageRGB =
				(uint8 *)Input->ImageRGB.Lock(LOCK_READ_ONLY);

			int32 SizeCompressedImageRGB = Input->ImageRGB.GetBulkDataSize();

			Substance::Helpers::DecompressJpeg(
				CompressedImageRGB,
				SizeCompressedImageRGB,
				UncompressedImageRGBA,
				&outWidth,
				&outHeight);

			Input->ImageRGB.Unlock();
		}
		else
		{
			UncompressedImageRGBA.Empty();
			UncompressedImageRGBA.AddUninitialized(Input->ImageRGB.GetBulkDataSize());
			FMemory::Memcpy(UncompressedImageRGBA.GetData(), Input->ImageRGB.Lock(LOCK_READ_ONLY), Input->ImageRGB.GetBulkDataSize());
			Input->ImageRGB.Unlock();

			outWidth = Input->SizeX;
			outHeight = Input->SizeY;
		}
	}

	if (Input->ImageA.GetBulkDataSize())
	{
		if (Input->CompressionAlpha)
		{
			uint8* CompressedImageA =
				(uint8 *)Input->ImageA.Lock(LOCK_READ_ONLY);

			int32 SizeCompressedImageA = Input->ImageA.GetBulkDataSize();

			int32 WidthA;
			int32 HeightA;

			Substance::Helpers::DecompressJpeg(
				CompressedImageA,
				SizeCompressedImageA,
				UncompressedImageA,
				&WidthA,
				&HeightA,
				1);

			check(WidthA == outWidth);
			check(HeightA == outHeight);

			Input->ImageA.Unlock();
		}
		else
		{
			UncompressedImageA.Empty();
			UncompressedImageA.AddUninitialized(Input->ImageA.GetBulkDataSize());
			FMemory::Memcpy(UncompressedImageA.GetData(), Input->ImageA.Lock(LOCK_READ_ONLY), Input->ImageA.GetBulkDataSize());
			Input->ImageA.Unlock();

			outWidth = Input->SizeX;
			outHeight = Input->SizeY;
		}
	}

	// recompose the two parts in one single image buffer
	if (UncompressedImageA.Num() && UncompressedImageRGBA.Num())
	{
		int32 OutptrSize = outWidth * outHeight * 4;

		*Outptr = (uint8*)FMemory::Malloc(OutptrSize);
#if SUBSTANCE_MEMORY_STAT
		INC_MEMORY_STAT_BY(STAT_SubstanceMemory, OutptrSize);
#endif

		FMemory::Memcpy(*Outptr, UncompressedImageRGBA.GetData(), UncompressedImageRGBA.Num());

		Join_RGBA_8bpp(
			outWidth, outHeight,
			*Outptr, UncompressedImageRGBA.Num(),
			&UncompressedImageA[0], UncompressedImageA.Num());

		UncompressedImageA.Empty();
	}
	else if (UncompressedImageRGBA.Num())
	{
		int32 OutptrSize = outWidth * outHeight * 4;

		*Outptr = (uint8*)FMemory::Malloc(OutptrSize);
#if SUBSTANCE_MEMORY_STAT
		INC_MEMORY_STAT_BY(STAT_SubstanceMemory, OutptrSize);
#endif

		FMemory::Memcpy(*Outptr, &UncompressedImageRGBA[0], UncompressedImageRGBA.Num());

	}
	else if (UncompressedImageA.Num())
	{
		// unsupported case for the moment
		UncompressedImageA.Empty();
		check(0);
	}
}


std::shared_ptr<ImageInput> PrepareFileImageInput(USubstanceImageInput* Input)
{
	if (NULL == Input)
	{
		return std::shared_ptr<ImageInput>();
	}

	int32 Width = 0;
	int32 Height = 0;
	uint8* UncompressedDataPtr = NULL;

	PrepareFileImageInput_GetBGRA(Input, &UncompressedDataPtr, Width, Height);

	if (UncompressedDataPtr)
	{
        uint16 sWidth = Width;
        uint16 sHeight = Height;
        
		SubstanceTexture texture = {
			UncompressedDataPtr,
			sWidth,
			sHeight,
			Substance_PF_RGBA,
			Substance_ChanOrder_BGRA,
			1};

		std::shared_ptr<ImageInput> res = ImageInput::create(texture);
#if SUBSTANCE_MEMORY_STAT
		DEC_MEMORY_STAT_BY(STAT_SubstanceMemory, FMemory::GetAllocSize(UncompressedDataPtr));
#endif
		FMemory::Free(UncompressedDataPtr);

		return res;
	}
	else
	{
		return std::shared_ptr<ImageInput>();
	}
}


void LinkImageInput(img_input_inst_t* ImgInputInst, USubstanceImageInput* SrcImageInput)
{
#if WITH_EDITOR
	// don't keep track of graph using the image input when playing in editor
	FSubstanceCoreModule& SubstanceModule = FModuleManager::LoadModuleChecked<FSubstanceCoreModule>(TEXT("SubstanceCore"));
	const bool PIE = SubstanceModule.isPie();
#else
	const bool PIE = false;
#endif

	// source image input changed, unlink previous one
	USubstanceImageInput* PrevBmpInput = Cast<USubstanceImageInput>(ImgInputInst->ImageSource);

	// at load, the image src is the same but still needs to be registered
	if (PrevBmpInput == SrcImageInput && SrcImageInput != NULL)
	{
		if (!SrcImageInput->Consumers.Contains(ImgInputInst->Parent->ParentInstance))
		{
			if (!PIE)
			{
				SrcImageInput->Consumers.AddUnique(ImgInputInst->Parent->ParentInstance);
			}
		}
		return;
	}

	if (PrevBmpInput &&
		ImgInputInst->ImageSource != SrcImageInput)
	{
		TArray< USubstanceGraphInstance* > DeprecatedConsumers;
		
		// iterate through consumers of the previous image input object,
		for (auto itConsumer = PrevBmpInput->Consumers.CreateIterator(); itConsumer; ++itConsumer)
		{
			int32 ConsumeCount = 0;

			for (auto itInput = (*itConsumer)->Instance->Inputs.itfront(); itInput; ++itInput)
			{
				// if there is only one image input instance using the previous image input, remove it from the consumers
				if (!(*itInput)->IsNumerical())
				{
					if (((FImageInputInstance*)itInput->Get())->ImageSource == PrevBmpInput)
					{
						++ConsumeCount;
					}
				}
			}

			if (ConsumeCount == 1)
			{
				DeprecatedConsumers.Add(*itConsumer);
			}
		}

		for (auto itDepConsumers = DeprecatedConsumers.CreateIterator(); itDepConsumers; ++itDepConsumers)
		{
			PrevBmpInput->Consumers.Remove(*itDepConsumers);
		}
	}
	
	if (SrcImageInput && !PIE)
	{
		SrcImageInput->Consumers.AddUnique(ImgInputInst->Parent->ParentInstance);
	}
}


std::shared_ptr<ImageInput> PrepareImageInput(
	UObject* Image, 
	FImageInputInstance* ImgInputInst,
	FGraphInstance* ImgInputInstParent)
{
	{
		USubstanceImageInput* BmpImageInput = Cast<USubstanceImageInput>(Image);

		if (BmpImageInput)
		{
			// link the image Input with the Input
			LinkImageInput(ImgInputInst, BmpImageInput);
			return PrepareFileImageInput(BmpImageInput);
		}
	}

	return std::shared_ptr<ImageInput>();
}


TArray< int32 > GetValueInt(const TSharedPtr<input_inst_t>& Input)
{
	TArray< int32 > Value;

	switch(Input->Desc->Type)
	{
	case Substance_IType_Integer:
		{
			Substance::FNumericalInputInstance<int32>* TypedInst =
				(Substance::FNumericalInputInstance<int32>*)&(*Input);
			Value.Add(TypedInst->Value);
		}
		break;
	case Substance_IType_Integer2:
		{
			Substance::FNumericalInputInstance<vec2int_t>* TypedInst =
				(Substance::FNumericalInputInstance<vec2int_t>*)&(*Input);

			Value.Add(TypedInst->Value.X);
			Value.Add(TypedInst->Value.Y);
		}
		break;

	case Substance_IType_Integer3:
		{
			Substance::FNumericalInputInstance<vec3int_t>* TypedInst = 
				(Substance::FNumericalInputInstance<vec3int_t>*)&(*Input);

			Value.Add(TypedInst->Value.X);
			Value.Add(TypedInst->Value.Y);
			Value.Add(TypedInst->Value.Z);
		}
		break;

	case Substance_IType_Integer4:
		{
			Substance::FNumericalInputInstance<vec4int_t>* TypedInst = 
				(Substance::FNumericalInputInstance<vec4int_t>*)&(*Input);
			Value.Add(TypedInst->Value.X);
			Value.Add(TypedInst->Value.Y);
			Value.Add(TypedInst->Value.Z);
			Value.Add(TypedInst->Value.W);
		}
		break;
	}

	return Value;
}


TArray< float > GetValueFloat(const TSharedPtr<input_inst_t>& Input)
{
	TArray< float > Value;

	switch(Input->Desc->Type)
	{
	case Substance_IType_Float:
		{
			Substance::FNumericalInputInstance<float>* TypedInst =
				(Substance::FNumericalInputInstance<float>*)&(*Input);
			Value.Add(TypedInst->Value);
		}
		break;
	case Substance_IType_Float2:
		{
			Substance::FNumericalInputInstance<vec2float_t>* TypedInst =
				(Substance::FNumericalInputInstance<vec2float_t>*)&(Input);

			Value.Add(TypedInst->Value.X);
			Value.Add(TypedInst->Value.Y);
		}
		break;

	case Substance_IType_Float3:
		{
			Substance::FNumericalInputInstance<vec3float_t>* TypedInst = 
				(Substance::FNumericalInputInstance<vec3float_t>*)&(*Input);

			Value.Add(TypedInst->Value.X);
			Value.Add(TypedInst->Value.Y);
			Value.Add(TypedInst->Value.Z);
		}
		break;

	case Substance_IType_Float4:
		{
			Substance::FNumericalInputInstance<vec4float_t>* TypedInst = 
				(Substance::FNumericalInputInstance<vec4float_t>*)&(*Input);
			Value.Add(TypedInst->Value.X);
			Value.Add(TypedInst->Value.Y);
			Value.Add(TypedInst->Value.Z);
			Value.Add(TypedInst->Value.W);
		}
		break;
	}

	return Value;
}


FString GetValueString(const TSharedPtr<input_inst_t>& InInput)
{
	FString ValueStr;

	switch((SubstanceInputType)InInput->Type)
	{
	case Substance_IType_Float:
		{
			FNumericalInputInstance<float>* Input =
				(FNumericalInputInstance<float>*)InInput.Get();

			ValueStr = FString::Printf(
				TEXT("%f"), 
				Input->Value);
		}
		break;
	case Substance_IType_Float2:
		{
			FNumericalInputInstance<vec2float_t>* Input =
				(FNumericalInputInstance<vec2float_t>*)InInput.Get();

			ValueStr = FString::Printf(
				TEXT("%f,%f"),
				Input->Value.X,
				Input->Value.Y);
		}
		break;
	case Substance_IType_Float3:
		{
			FNumericalInputInstance<vec3float_t>* Input =
				(FNumericalInputInstance<vec3float_t>*)InInput.Get();

			ValueStr = FString::Printf(
				TEXT("%f,%f,%f"),
				Input->Value.X,
				Input->Value.Y,
				Input->Value.Z);
		}
		break;
	case Substance_IType_Float4:
		{
			FNumericalInputInstance<vec4float_t>* Input =
				(FNumericalInputInstance<vec4float_t>*)InInput.Get();

			ValueStr = FString::Printf(
				TEXT("%f,%f,%f,%f"),
				Input->Value.X,
				Input->Value.Y,
				Input->Value.Z,
				Input->Value.W);
		}
		break;
	case Substance_IType_Integer:
		{
			FNumericalInputInstance<int32>* Input =
				(FNumericalInputInstance<int32>*)InInput.Get();

			ValueStr = FString::Printf(
				TEXT("%d"), 
				(int32)Input->Value);
		}
		break;
	case Substance_IType_Integer2:
		{
			FNumericalInputInstance<vec2int_t>* Input =
				(FNumericalInputInstance<vec2int_t>*)InInput.Get();

			ValueStr = FString::Printf(
				TEXT("%d,%d"),
				Input->Value.X,
				Input->Value.Y);
		}
		break;
	case Substance_IType_Integer3:
		{
			FNumericalInputInstance<vec3int_t>* Input =
				(FNumericalInputInstance<vec3int_t>*)InInput.Get();

			ValueStr = FString::Printf(
				TEXT("%d,%d,%d"),
				Input->Value.X,
				Input->Value.Y,
				Input->Value.Z);
		}
		break;
	case Substance_IType_Integer4:
		{
			FNumericalInputInstance<vec4int_t>* Input =
				(FNumericalInputInstance<vec4int_t>*)InInput.Get();

			ValueStr = FString::Printf(
				TEXT("%d,%d,%d,%d"),
				Input->Value.X,
				Input->Value.Y,
				Input->Value.Z,
				Input->Value.W);
		}
		break;
	case Substance_IType_Image:
		{
			FImageInputInstance* Input =
				(FImageInputInstance*)InInput.Get();

			if (Input->ImageSource && Input->ImageSource->IsValidLowLevel())
			{
				Input->ImageSource->GetFullName().Split(TEXT(" "), NULL, &ValueStr);
			}
			else
			{
				ValueStr = TEXT("NULL");
			}			
		}
		break;
	default:
		break;
	}

	return ValueStr;
}


void ClearFromRender(USubstanceGraphInstance* GraphInstance)
{
	LoadingQueue.Remove(GraphInstance->Instance);
	PriorityLoadingQueue.Remove(GraphInstance->Instance);
	CurrentRenderQueue.Remove(GraphInstance->Instance);
	AsyncQueue.Remove(GraphInstance->Instance);

	Substance::List<output_inst_t>::TIterator
		ItOut(GraphInstance->Instance->Outputs.itfront());

	// if some of the outputs are still waiting for a rendering,
	// wait for it before finishing destruction
	bool needFlush = false;
	for (; ItOut; ++ItOut)
	{
		if (*(ItOut->Texture) && (*ItOut->Texture)->OutputCopy->bIsDirty)
		{
			needFlush = true;
			break;
		}
	}

	if (needFlush && GSubstanceRenderer.IsValid())
	{
		GSubstanceRenderer->cancelAll();
		if (!IsGarbageCollecting())
		{
			Substance::Helpers::Tick();
		}
	}
}


void Cleanup( USubstanceGraphInstance* InstanceContainer )
{
	if (InstanceContainer->Instance != NULL)
	{
		TArray<GraphImageInputPair_t>::TIterator ItInp(DelayedImageInputs);

		for(;ItInp;++ItInp)
		{
			img_input_inst_t* ImgInput = (*ItInp).first;
			graph_inst_t* Instance = (*ItInp).second;

			if (Instance == InstanceContainer->Instance)
			{
				DelayedImageInputs.RemoveAt(ItInp.GetIndex());
				ItInp.Reset();
			}
		}

		ClearFromRender(InstanceContainer);

		for (auto ItOut = InstanceContainer->Instance->Outputs.itfront(); ItOut; ++ItOut)
		{
			Substance::Helpers::Disable(&(*ItOut));
		}
		
		// the desc may have already forced
		// the UnSubscribtion for its instances
		if(InstanceContainer->Instance->Desc)
		{
			InstanceContainer->Instance->Desc->InstanceUids.Remove(
				InstanceContainer->Instance->InstanceGuid);

			InstanceContainer->Instance->Desc->UnSubscribe(
				InstanceContainer->Instance);
		}

		if (InstanceContainer->Parent)
		{
			InstanceContainer->Parent->MarkPackageDirty();
		}

		delete InstanceContainer->Instance;
		InstanceContainer->Instance = 0;
		InstanceContainer->Parent = 0;
	}
}


bool IsSupportedImageInput( UObject* CandidateImageInput)
{
	if (NULL == CandidateImageInput)
	{
		return false;
	}

	if (CandidateImageInput->IsPendingKill())
	{
		return false;
	}

	if (Cast<USubstanceImageInput>(CandidateImageInput) /*|| 
		Cast<USubstanceTexture2D>(CandidateImageInput)*/)
	{
		return true;
	}

	return false;
}


template<typename T> void resetInputNumInstance(input_inst_t* Input)
{
	FNumericalInputInstance<T>* TypedInput = (FNumericalInputInstance<T>*)Input;

	FNumericalInputDesc<T>* InputDesc= 
		(FNumericalInputDesc<T>*)Input->Desc;

	TypedInput->Value = InputDesc->DefaultValue;
}


void ResetToDefault(TSharedPtr<input_inst_t> Input)
{
	switch ((SubstanceInputType)Input->Desc->Type)
	{
	case Substance_IType_Float:
	{
		resetInputNumInstance<float>(Input.Get());
	}
	break;
	case Substance_IType_Float2:
	{
		resetInputNumInstance<vec2float_t>(Input.Get());
	}
	break;
	case Substance_IType_Float3:
	{
		resetInputNumInstance<vec3float_t>(Input.Get());
	}
	break;
	case Substance_IType_Float4:
	{
		resetInputNumInstance<vec4float_t>(Input.Get());
	}
	break;
	case Substance_IType_Integer:
	{
		resetInputNumInstance<int32>(Input.Get());
	}
	break;
	case Substance_IType_Integer2:
	{
		resetInputNumInstance<vec2int_t>(Input.Get());
	}
	break;
	case Substance_IType_Integer3:
	{
		resetInputNumInstance<vec3int_t>(Input.Get());
	}
	break;
	case Substance_IType_Integer4:
	{
		resetInputNumInstance<vec4int_t>(Input.Get());
	}
	break;
	case  Substance_IType_Image:
	{
		FImageInputInstance* TypedInput = (FImageInputInstance*)Input.Get();

		FImageInputDesc* InputDesc =
			(FImageInputDesc*)Input->Desc;

		TypedInput->SetImageInput(NULL, TypedInput->Parent);
	}
	break;
	default:
		break;
	}
}


void ResetToDefault(graph_inst_t* Instance)
{
	Substance::List<TSharedPtr<input_inst_t>>::TIterator ItInput(Instance->Inputs.itfront());

	for (;ItInput;++ItInput)
	{
		ResetToDefault(*ItInput);
	}

	// force outputs to dirty so they can be updated
	Substance::List<output_inst_t>::TIterator 
		ItOut(Instance->Outputs.itfront());

	for (;ItOut;++ItOut)
	{
		ItOut->flagAsDirty();
	}
}


#if WITH_EDITOR
void GetSuitableName(output_inst_t* Instance, FString& OutAssetName, FString& OutPackageName)
{
	graph_desc_t* Graph = Instance->ParentInstance->Instance->Desc;

	static FName AssetToolsModuleName = FName("AssetTools");
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolsModuleName);

	for (uint32 IdxOut=0 ; IdxOut<Graph->OutputDescs.size() ; ++IdxOut)
	{
		//look for the original description
		if (Graph->OutputDescs[IdxOut].Uid == Instance->Uid)
		{
			FString BaseName = Instance->ParentInstance->GetName() + TEXT("_") + Graph->OutputDescs[IdxOut].Identifier;
			FString PackageName = Instance->ParentInstance->GetOuter()->GetName() + TEXT("_") + Graph->OutputDescs[IdxOut].Identifier;
			AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), OutPackageName, OutAssetName);
			return;
		}
	}

	// this should not happen
	check(0);

	// still got to find a name, and still got to have it unique
	FString BaseName(TEXT("TEXTURE_NAME_NOT_FOUND"));
	AssetToolsModule.Get().CreateUniqueAssetName(Instance->ParentInstance->GetPathName() + TEXT("/") + BaseName, TEXT(""), OutPackageName, OutAssetName);
}
#endif


USubstanceGraphInstance* DuplicateGraphInstance(
	USubstanceGraphInstance* SourceGraphInstance)
{
	FString BasePath;
	FString ParentName = SourceGraphInstance->Parent->GetOuter()->GetPathName();
	
	ParentName.Split(TEXT("/"), &(BasePath), NULL, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	
	FString AssetNameStr;
	FString PackageNameStr;

#if WITH_EDITOR
	FString Name = ObjectTools::SanitizeObjectName(SourceGraphInstance->Instance->Desc->Label);
	static FName AssetToolsModuleName = FName("AssetTools");
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolsModuleName);
	AssetToolsModule.Get().CreateUniqueAssetName(BasePath + TEXT("/") + Name + TEXT("_INST"), TEXT(""), PackageNameStr, AssetNameStr);
#else
	FString Name = SourceGraphInstance->Instance->Desc->Label;
	PackageNameStr = BasePath;
#endif

	UObject* InstanceParent = CreatePackage(NULL, *PackageNameStr);

	graph_inst_t* NewInstance = 
		Substance::Helpers::InstantiateGraph(
		SourceGraphInstance->Instance->Desc, InstanceParent, AssetNameStr, false /*bCreateOutputs*/, SourceGraphInstance->GetFlags());
	check(NewInstance);

	CopyInstance(SourceGraphInstance->Instance, NewInstance, false);
	
	return NewInstance->ParentInstance;
}


graph_inst_t* InstantiateGraph(
	graph_desc_t* Graph,
	UObject* Outer,
	FString InstanceName,
	bool bCreateOutputs,
	EObjectFlags Flags)
{
	graph_inst_t* NewInstance = NULL;

	USubstanceGraphInstance* GraphInstance = NewObject<USubstanceGraphInstance>(
			Outer,
			*InstanceName,
			Flags);

	GraphInstance->MarkPackageDirty();

	NewInstance = Graph->Instantiate(GraphInstance, bCreateOutputs);

	return NewInstance;
}


void CopyInstance( 
	graph_inst_t* RefInstance, 
	graph_inst_t* NewInstance,
	bool bCopyOutputs) 
{
#if WITH_EDITOR
	FSubstanceCoreModule& SubstanceModule = FModuleManager::LoadModuleChecked<FSubstanceCoreModule>(TEXT("SubstanceCore"));
	const bool PIE = SubstanceModule.isPie();
#else
	const bool PIE = false;
#endif

	// copy values from previous
	preset_t Preset;
	Preset.ReadFrom(RefInstance);
	Preset.Apply(NewInstance, Substance::FPreset::Apply_Merge);

	if (bCopyOutputs)
	{
		//! create same outputs as ref instance
		for(uint32 Idx=0 ; Idx<NewInstance->Outputs.size() ; ++Idx)
		{
			output_inst_t* OutputRefInstance = &RefInstance->Outputs[Idx];
			output_inst_t* OutputInstance = &NewInstance->Outputs[Idx];

			if (OutputRefInstance->bIsEnabled)
			{
#if WITH_EDITOR
				FString TextureName;
				FString PackageName;
				Substance::Helpers::GetSuitableName(OutputInstance, TextureName, PackageName);
#else // WITH_EDITOR
				FString TextureName;
				FString PackageName;
				PackageName = RefInstance->ParentInstance->GetName();
#endif
				UObject* TextureParent = CreatePackage(NULL, *PackageName);

				// create new texture
				Substance::Helpers::CreateSubstanceTexture2D(
					OutputInstance, 
					PIE ? true : false, // if play in editor, this means the instance is dynamic, ie transient
					TextureName,
					TextureParent);
			}
		}
	}
}


void RegisterForDeletion( USubstanceGraphInstance* InstanceContainer )
{
	InstanceContainer->GetOutermost()->FullyLoad();
	local::InstancesToDelete.AddUnique(InstanceContainer);
}


void RegisterForDeletion( USubstanceInstanceFactory* Factory )
{
	Factory->GetOutermost()->FullyLoad();
	local::FactoriesToDelete.AddUnique(Cast<UObject>(Factory));
}


void RegisterForDeletion( USubstanceTexture2D* Texture)
{
	Texture->GetOutermost()->FullyLoad();
	local::TexturesToDelete.AddUnique(Cast<UObject>(Texture));
}


void PerformDelayedDeletion()
{
	bool bDeletedSomething = false;

#if WITH_EDITOR
	if (local::TexturesToDelete.Num() ||
		local::FactoriesToDelete.Num() ||
		local::InstancesToDelete.Num())
	{
		GEditor->Trans->Reset(NSLOCTEXT("UnrealEd", "DeleteSelectedItem", "Delete Selected Item"));
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	else
	{
		return;
	}

	// start by deleting the textures
	if (local::TexturesToDelete.Num())
	{
		for (auto it = local::TexturesToDelete.CreateConstIterator(); it; ++it )
		{
			CastChecked<USubstanceTexture2D>(*it)->ParentInstance->MarkPackageDirty();
			(*it)->MarkPackageDirty();
		}
		
		bDeletedSomething = (ObjectTools::DeleteObjectsUnchecked(local::TexturesToDelete) != 0) || bDeletedSomething;
		local::TexturesToDelete.Empty();
	}
	
	for (auto ItFact = local::FactoriesToDelete.CreateIterator(); ItFact; ++ItFact)
	{
		USubstanceInstanceFactory* Factory = CastChecked<USubstanceInstanceFactory>(*ItFact);
		Factory->ConditionalPostLoad();

		// collect the instances which are loaded and subscribe them for deletion
		for (auto ItDesc = Factory->SubstancePackage->Graphs.itfront(); ItDesc; ++ItDesc)
		{
			Substance::List<graph_inst_t*>::TIterator 
				ItInst((*ItDesc)->LoadedInstances.itfront());

			for (;ItInst;++ItInst)
			{
				if ((*ItInst)->ParentInstance->IsValidLowLevel())
				{
					local::InstancesToDelete.AddUnique((*ItInst)->ParentInstance);
				}
			}
		}
	}

	for (auto ItInst = local::InstancesToDelete.CreateIterator(); ItInst; ++ItInst)
	{
		USubstanceGraphInstance* GraphInstance = *ItInst;

		if (!GraphInstance->IsValidLowLevel())
		{
			continue;
		}

		GraphInstance->ConditionalPostLoad();

		auto ItOut = GraphInstance->Instance->Outputs.itfront();
		// delete every child textures
		for (; ItOut ; ++ItOut)
		{
			UTexture* Texture = *((*ItOut).Texture).get();

			if (Texture && Texture->IsValidLowLevel())
			{
				local::TexturesToDelete.AddUnique(Texture);
			}
		}

		int32 DeletedObjectsCount = ObjectTools::ForceDeleteObjects(local::TexturesToDelete, false);
		
		bDeletedSomething = bDeletedSomething || (DeletedObjectsCount > 0);

		// if not all textures were deleted, cancel eventual instance or factory deletion
		if (local::TexturesToDelete.Num() != DeletedObjectsCount)
		{
			if (local::FactoriesToDelete.Contains(GraphInstance->Parent))
			{
				local::FactoriesToDelete.Remove(GraphInstance->Parent);
			}
		}
		else // otherwise, delete the instance if needed
		{
			local::TexturesToDelete.Empty();

			TArray<UObject*> Instances;
			if (GraphInstance->IsValidLowLevel())
			{
				Instances.AddUnique(GraphInstance);
				DeletedObjectsCount = ObjectTools::ForceDeleteObjects(Instances, false);
				bDeletedSomething = (DeletedObjectsCount > 0) || bDeletedSomething;

				// do not delete instance factory if the instance has not been deleted
				if (Instances.Num() != DeletedObjectsCount && local::FactoriesToDelete.Contains(GraphInstance->Parent))
				{
					local::FactoriesToDelete.Remove(GraphInstance->Parent);
				}
			}
		}
	}

	if (local::FactoriesToDelete.Num())
	{
		bDeletedSomething = (ObjectTools::ForceDeleteObjects(local::FactoriesToDelete, false) > 0) || bDeletedSomething;
	}

#endif //WITH_EDITOR

	local::TexturesToDelete.Empty();
	local::InstancesToDelete.Empty();
	local::FactoriesToDelete.Empty();
}


UPackage* CreateObjectPackage(UObject* Outer, FString ObjectName)
{
	FString Left;
	FString Right;
	Outer->GetName().Split(TEXT("/"), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	FString PkgName = Left + TEXT("/") + ObjectName;
	UPackage* Pkg = CreatePackage(NULL, *PkgName);
	Pkg->FullyLoad();
	return Pkg;
}


float GetSubstanceLoadingProgress()
{
	// when no substances are used, return 100%
	if (GlobalInstancePendingCount == 0)
	{
		return 1.0f;
	}

	return (float)GlobalInstanceCompletedCount / (float)GlobalInstancePendingCount;
}

} // namespace Helpers
} // namespace Substance

