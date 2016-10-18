//! @file detailslinkdata.cpp
//! @brief Substance Framework Link data classes implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111116
//! @copyright Allegorithmic. All rights reserved.
//!
 
#include "SubstanceCorePrivatePCH.h"

#include "SubstanceFGraph.h"

#include "framework/details/detailslinkdata.h"
#include "framework/details/detailslinkcontext.h"
#include "framework/details/detailsgraphbinary.h"


//! @brief Destructor
Substance::Details::LinkData::~LinkData()
{
}


//! @brief Constructor from assembly data
//! @param ptr Pointer on assembly data
//! @param size Size of assembly data in bytes
Substance::Details::LinkDataAssembly::LinkDataAssembly(
		const uint8* ptr,
		uint32 size) :
	mAssembly((const char*)ptr,size)
{
}


//! @brief Push data to link
//! @param cxt Used to push link data
bool Substance::Details::LinkDataAssembly::push(LinkContext& cxt) const
{
	int32 err;
	
	// Push assembly
	{
		err = Substance::gAPI->SubstanceLinkerPushAssembly(cxt.handle, cxt.stateUid, mAssembly.data(), mAssembly.size());
	}

	if (err)
	{
		// TODO2 error pushing assembly to linker
		check(0);
		return false;
	}

	// set output formats/mipmaps
	SBS_VECTOR_FOREACH (const OutputFormat& outfmt, mOutputFormats)
	{
		LinkContext::TrResult trres = cxt.translateUid(true,outfmt.uid,false);
		if (trres.second)
		{
			err = Substance::gAPI->SubstanceLinkerSetOutputFormat(
				cxt.handle,
				trres.first,
				outfmt.format,
				outfmt.mipmap);
				
			if (err)
			{
				// TODO2 Warning: output UID not valid
				check(0);
			}
		}
	}

	return true;
}
	
	
//! @brief Force output format/mipmap
//! @param uid Output uid
//! @param format New output format
//! @param mipmap New mipmap count
void Substance::Details::LinkDataAssembly::setOutputFormat(
	uint32 uid,
	int32 format,
	int32 mipmap)
{
	OutputFormat outfmt = {uid,format,mipmap};
	mOutputFormats.push_back(outfmt);
}
	
	
//! @brief Construct from pre and post Graphs, and connection options.
//! @param preGraph Source pre graph.
//! @param postGraph Source post graph.
//! @param connOptions Connections options.
Substance::Details::LinkDataStacking::LinkDataStacking(
		const FGraphDesc* preGraph,
		const FGraphDesc* postGraph,
		const ConnectionsOptions& connOptions) :
	mOptions(connOptions),
	mPreLinkData(preGraph->Parent->getLinkData()),
	mPostLinkData(postGraph->Parent->getLinkData())
{
}
	
	
//! @brief Push data to link
//! @param cxt Used to push link data
bool Substance::Details::LinkDataStacking::push(LinkContext& cxt) const
{
	// Push this stack level in context, popped at deletion (RAII)
	LinkContext::StackLevel level(cxt,*this);

	// Push pre to link
	if (mPreLinkData.get()==NULL || !mPreLinkData->push(cxt))
	{
		// TODO2 Error: cannot push pre
		check(0);
		return false;
	}
	
	// Fix fuse pre UIDs
	ConnectionsOptions::Connections fusefixed(mFuseInputs);

	for (auto fuse = fusefixed.begin(); fuse != fusefixed.end(); ++fuse)
	{
		LinkContext::TrResult trres = cxt.translateUid(false,fuse->first,true);
		check(trres.second);
		if (trres.second)
		{
			fuse->first = trres.first;
		}
	}
	// Switch to post
	level.beginPost();

	// Push post to link
	if (mPostLinkData.get()==NULL || !mPostLinkData->push(cxt))
	{
		// TODO2 Error: cannot push post
		check(0);
		return false;
	}
	
	// Push connections
	for (auto c = mOptions.mConnections.begin(); c != mOptions.mConnections.end(); ++c)
	{
		int32 err = Substance::gAPI->SubstanceLinkerConnectOutputToInput(
			cxt.handle,
			LinkContext::translate(level.trPostInputs,c->second).first,
			LinkContext::translate(level.trPreOutputs,c->first).first);
			
		if (err)
		{
			// TODO2 Warning: connect failed
			check(0);
		}
	}
	
	// Fuse all $ prefixed inputs
	for (auto fuse = fusefixed.begin(); fuse != fusefixed.end(); ++fuse)
	{
		Substance::gAPI->SubstanceLinkerFuseInputs(
			cxt.handle,
			fuse->first,
			LinkContext::translate(level.trPostInputs,fuse->second).first);
	}
	
	return true;
}
