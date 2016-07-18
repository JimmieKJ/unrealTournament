//! @file SubstanceFPackage.cpp
//! @brief Implementation of the USubstancePackage class
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceFGraph.h"
#include "SubstanceFPackage.h"
#include "SubstanceCorePreset.h"
#include "SubstanceCoreXmlHelper.h"
#include "SubstanceCache.h"
#include "substance_public.h"

#include "framework/details/detailslinkdata.h"

namespace
{
	TArray<FString> linkerArchiveXmlContent;
}

namespace Substance
{

FPackage::FPackage():
	Guid(0, 0, 0, 0),
	LoadedInstancesCount(0),
    Parent(0)
{

}


FPackage::~FPackage()
{
	for (int32 IdxGraph=0 ; IdxGraph<Graphs.Num() ; ++IdxGraph)
	{
		delete Graphs[IdxGraph];
		Graphs[IdxGraph] = 0;
	}

	Graphs.Empty();
	Parent = 0;
}


SUBSTANCE_EXTERNC void SUBSTANCE_CALLBACK linkerCallbackArchiveXml(  
	SubstanceLinkerHandle *handle,
	const unsigned short* basename,
	const char* xmlContent)
{
	linkerArchiveXmlContent.Add(UTF8_TO_TCHAR(xmlContent));
}


void FPackage::SetData(
	const uint8*& BinaryBuffer,
	const int32 BinaryBufferSize,
	const FString& FilePath)
{
	// save the sbsar
	LinkData.reset(
		new Details::LinkDataAssembly(
			BinaryBuffer,
			BinaryBufferSize));

	{
		int32 Err = 0;

		// grab the xmls using the linker
		SubstanceLinkerContext *context;

		SubstanceLinkerContextInitEnum mode = Substance_Linker_Context_Init_NonMobile;
#if IPHONE
		mode = Substance_Linker_Context_Init_Mobile;
#endif

		Err = Substance::gAPI->SubstanceLinkerContextInit(&context, mode);

		if (Err)
		{
			GWarn->Log((*FString::Printf(TEXT("Substance: error while initializing the linker context(%d)"), Err)));
			return;
		}

		Err = Substance::gAPI->SubstanceLinkerContextSetXMLCallback(context, (void*)linkerCallbackArchiveXml);
		if (Err)
		{
			GWarn->Log((*FString::Printf(TEXT("Substance: linker error (%d)"), Err)));
			return;
		}

		if (Err)
		{
			GWarn->Log((*FString::Printf(TEXT("Substance: linker error (%d)"), Err)));
			return;
		}

		SubstanceLinkerHandle *handle;
		Err = Substance::gAPI->SubstanceLinkerHandleInit(&handle,context, 0);
		if (Err)
		{
			GWarn->Log((*FString::Printf(TEXT("Substance: linker error (%d)"), Err)));
			return;
		}

		linkerArchiveXmlContent.Empty();

		Err	= Substance::gAPI->SubstanceLinkerPushMemory(
			handle,
			(const char*)BinaryBuffer,
			BinaryBufferSize);
		if (Err)
		{
			GWarn->Log(TEXT("Substance: invalid assembly, cannot import file."));
			return;
		}

		// Release
		Err = Substance::gAPI->SubstanceLinkerRelease(handle, context);
		if (Err)
		{
			GWarn->Log((*FString::Printf(TEXT("Substance: linker error (%d)"), Err)));
			return;
		}
	}

	// try to parse the xml file
	if (false == ParseXml(linkerArchiveXmlContent))
	{
		// abort if invalid
		GWarn->Log( TEXT("Substance: error while parsing xml companion file."));
		return;
	}

	Substance::List<graph_desc_t*>::TIterator itG(Graphs.itfront());
	for (; itG ; ++itG)
	{
		(*itG)->Parent = this;

		Substance::List<output_desc_t>::TIterator itO((*itG)->OutputDescs.itfront());
		for (; itO ; ++itO)
		{
			Details::LinkDataAssembly *linkdata = 
				(Details::LinkDataAssembly *)LinkData.get();

			linkdata->setOutputFormat(
				(*itO).Uid,
				(*itO).Format);
		}
	}

	SourceFilePath = FilePath;
	SourceFileTimestamp = IFileManager::Get().GetTimeStamp( *FilePath ).ToString();
}


bool FPackage::IsValid()
{
	Substance::Details::LinkDataAssembly *linkdata =
		static_cast<Substance::Details::LinkDataAssembly*>(&(*LinkData));

	return Graphs.Num() && linkdata->getSize() && substanceGuid_t(0, 0, 0, 0) != Guid;
}


bool FPackage::ParseXml(const TArray<FString>& XmlContent)
{
#if WITH_EDITOR
	return Helpers::ParseSubstanceXml(XmlContent, Graphs, SubstanceUids);
#else
	return false;
#endif
}


output_inst_t* FPackage::GetOutputInst(const struct FGuid OutputUid) const
{
	for (int32 IdxGraph=0 ; IdxGraph<Graphs.Num() ; ++IdxGraph)
	{
		Substance::List<graph_inst_t*>::TIterator
			InstanceIt(Graphs[IdxGraph]->LoadedInstances.itfront());

		for (; InstanceIt ; ++InstanceIt)
		{
			for (int32 IdxOut=0 ; IdxOut<(*InstanceIt)->Outputs.Num() ; ++IdxOut)
			{
				if (OutputUid == (*InstanceIt)->Outputs[IdxOut].OutputGuid)
				{
					return &(*InstanceIt)->Outputs[IdxOut];
				}
			}
		}
	}

	return NULL;
}


void FPackage::ConditionnalClearLinkData()
{
	for (auto itGr = Graphs.itfront(); itGr; ++itGr)
	{
		if ((*itGr)->Parent->Parent->GenerationMode != ESubstanceGenerationMode::SGM_Baked)
		{
			return;
		}

		if ((*itGr)->LoadedInstances.Num() == (*itGr)->InstanceUids.Num())
		{
			for (auto itInst = (*itGr)->LoadedInstances.itfront(); itInst; ++itInst)
			{
				if (Substance::SubstanceCache::Get()->CanReadFromCache((*itInst)))
				{
					continue;
				}
			}
		}
		else
		{
			return;
		}
	}

	//get rid of sbsar data if every graph can be loaded from cache
	LinkData.reset();
}


//! @brief Called when a Graph gains an instance
void FPackage::InstanceSubscribed()
{
	++LoadedInstancesCount;

	if (!GIsEditor)
	{
		ConditionnalClearLinkData();
	}
}


//! @brief Called when a Graph loses an instance
void FPackage::InstanceUnSubscribed()
{
	--LoadedInstancesCount;
}


uint32 FPackage::GetInstanceCount()
{
	uint32 InstanceCount = 0;

	Substance::List<graph_desc_t*>::TIterator 
		ItGraph(Graphs.itfront());
	for (; ItGraph ; ++ItGraph)
	{
		InstanceCount += (*ItGraph)->InstanceUids.Num();		
	}

	return InstanceCount;
}

} // namespace Substance
