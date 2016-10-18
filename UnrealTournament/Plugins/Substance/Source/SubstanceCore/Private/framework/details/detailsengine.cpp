//! @file detailsengine.cpp
//! @brief Substance Framework Engine/Linker wrapper implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111116
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceCorePrivatePCH.h"

#include "framework/details/detailsengine.h"
#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailsrenderpushio.h"
#include "framework/details/detailsgraphbinary.h"
#include "framework/details/detailsgraphstate.h"
#include "framework/details/detailsinputimagetoken.h"
#include "framework/details/detailslinkgraphs.h"
#include "framework/details/detailslinkcontext.h"
#include "framework/details/detailslinkdata.h"
#include "framework/details/detailsrendererimpl.h"
#include "framework/renderresult.h"
#include "framework/renderopt.h"

#include "substance_public.h"

#include "SubstanceSettings.h"

#include "ThreadingBase.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <assert.h>
#include <memory.h>

using namespace Substance::Details;


//! @brief Substance_Linker_Callback_UIDCollision linker callback impl.
//! Fill GraphBinary translated UIDs
SUBSTANCE_EXTERNC void SUBSTANCE_CALLBACK 
substanceDetailsLinkerCallbackUIDCollision(
	SubstanceLinkerHandle *handle,
	SubstanceLinkerUIDCollisionType collisionType,
	unsigned int previousUid,
	unsigned int newUid)
{
	unsigned int res;
	(void)res;
	Substance::Details::Engine *engine = NULL;
	res = Substance::gAPI->SubstanceLinkerGetUserData(handle,(size_t*)&engine);
	check(res==0);

	engine->callbackLinkerUIDCollision(
		collisionType,
		previousUid,
		newUid);
}


//! @brief Substance_Callback_OutputCompleted engine callback impl.
SUBSTANCE_EXTERNC void SUBSTANCE_CALLBACK
substanceDetailsEngineCallbackOutputCompleted(
	SubstanceHandle *handle,
	unsigned int outputIndex,
	size_t jobUserData)
{
	Substance::Details::RenderPushIO *pushio = 
		(Substance::Details::RenderPushIO*)jobUserData;
	check(pushio!=NULL);

	Substance::Details::Engine *engine = pushio->getEngine();
	check(engine != NULL);

	// Grab result
	SubstanceTexture texture;
	SubstanceContext* context;
	unsigned int res = Substance::gAPI->SubstanceHandleGetTexture(handle, outputIndex, &texture, &context);
	check(res == 0);
	
	// Create render result
	Substance::RenderResult *renderresult = 
		new Substance::RenderResult(texture, context, engine);
	
	// Transmit it to Push I/O
	pushio->callbackOutputComplete(outputIndex,renderresult);
}

	
//! @brief Substance_Callback_JobCompleted engine callback impl.
SUBSTANCE_EXTERNC void SUBSTANCE_CALLBACK
substanceDetailsEngineCallbackJobCompleted(
	SubstanceHandle*,
	size_t jobUserData)
{
	Substance::Details::RenderPushIO *pushio = 
		(Substance::Details::RenderPushIO*)jobUserData;
	if (pushio!=NULL)
	{
		pushio->callbackJobComplete();
	}
}


//! @brief Substance_Callback_InputImageLock engine callback impl.
SUBSTANCE_EXTERNC void SUBSTANCE_CALLBACK
substanceDetailsEngineCallbackInputImageLock(
	SubstanceHandle*,
	size_t,
	unsigned int inputIndex,
	SubstanceTextureInput **currentTextureInputDesc,
	const SubstanceTextureInput*)
{
	TextureInput *const texinp =(TextureInput*)(*currentTextureInputDesc);

	if (texinp != NULL)
	{
		texinp->parentToken->lock();
		*currentTextureInputDesc = (SubstanceTextureInput*)(texinp);
	}
}


//! @brief Substance_Callback_InputImageUnlock engine callback impl.
SUBSTANCE_EXTERNC void SUBSTANCE_CALLBACK
substanceDetailsEngineCallbackInputImageUnlock(
	SubstanceHandle*,
	size_t,
	unsigned int,
	SubstanceTextureInput* textureInputDesc)
{
	TextureInput *const texinp = (TextureInput*)(textureInputDesc);
	if (texinp != NULL)
	{
		texinp->parentToken->unlock();
	}
}	


/*
//! @brief Substance_Callback_InputImageUnlock engine callback impl.
static void SUBSTANCE_CALLBACK substanceAirDetailsEngineCallbackCacheEvict(
	SubstanceHandle* handle,
	unsigned int flags,
	unsigned int itemUid,
	void *buffer,
	size_t bytesCount)
{
	size_t userData = 0;
	unsigned int res = substanceHandleGetUserData(handle,&userData);

	const uint3264_t itemUid64 = 
		static_cast<uint3264_t>(userData)<<32 | 
		static_cast<uint3264_t>(itemUid);

	Substance::GlobalCallbacks *const callbacks = 
		Substance::Details::RendererImpl::getGlobalCallbacks();
	(void)res;
	
	assert(res==0);
	assert(sizeof(uint3264_t)>=8);
	assert(callbacks!=NULL);
		
	if ((flags&Substance_CacheEvict_Evict)!=0)
	{
		callbacks->renderCacheEvict(itemUid64,buffer,bytesCount);
	}
	else if ((flags&Substance_CacheEvict_Fetch)!=0)
	{
		callbacks->renderCacheFetch(itemUid64,buffer,bytesCount);
	}
	if ((flags&Substance_CacheEvict_Remove)!=0)
	{
		callbacks->renderCacheRemove(itemUid64);
	}
}
*/

//! @brief Substance_Callback_Malloc engine callback impl.
SUBSTANCE_EXTERNC void* SUBSTANCE_CALLBACK substanceDetailsEngineCallbackMalloc(
	size_t bytesCount,
	size_t alignment)
{
#if SUBSTANCE_MEMORY_STAT
	INC_MEMORY_STAT_BY(STAT_SubstanceEngineMemory, bytesCount);
#endif

	return FMemory::Malloc(bytesCount, alignment);
}


//! @brief Substance_Callback_Malloc engine callback impl.
SUBSTANCE_EXTERNC void SUBSTANCE_CALLBACK substanceDetailsEngineCallbackFree(
	void* bufferPtr)
{
#if SUBSTANCE_MEMORY_STAT
	DEC_MEMORY_STAT_BY(STAT_SubstanceEngineMemory, FMemory::GetAllocSize(bufferPtr));
#endif

	FMemory::Free(bufferPtr);
}


//! @brief Context singleton instance
std::weak_ptr<Substance::Details::Engine::Context>
	Substance::Details::Engine::mContextGlobal;

//! @brief Last UID used, used to generate Engine::mInstanceUid
uint32 Substance::Details::Engine::mLastInstanceUid = uint32();


//! @brief Constructor
//! @param renderOptions Initial render options.
Substance::Details::Engine::Engine(const RenderOptions& renderOptions) :
	mInstanceUid((++mLastInstanceUid)|0x80000000u),
	mLinkerCacheData(NULL),
	mHandle(NULL),
	mLinkerContext(NULL),
	mLinkerHandle(NULL),
	mCurrentLinkContext(NULL),
	mPendingReleaseTextures(false)
{
	fillHardResources(mHardResources,renderOptions);
	createLinker();
}


//! @brief Create Linker handle and context
void Substance::Details::Engine::createLinker()
{
	uint32 res;

#if PLATFORM_PS4
	res = Substance::gAPI->SubstanceLinkerContextInit(&mLinkerContext, Substance_Linker_Context_Init_NonMobile);
#else
	res = Substance::gAPI->SubstanceLinkerContextInit(&mLinkerContext, Substance_Linker_Context_Init_Auto);
#endif

	check(res==0);	
	
	// Set UID collision callback
	res = Substance::gAPI->SubstanceLinkerContextSetUIDCollisionCallback(mLinkerContext, (void*)substanceDetailsLinkerCallbackUIDCollision);
	check(res==0);	
	
	// Create linker handle
	res = Substance::gAPI->SubstanceLinkerHandleInit(&mLinkerHandle, mLinkerContext, (size_t)this);
	check(res==0);
}


//! @brief Destructor
Substance::Details::Engine::~Engine()
{
	// Must be already released from render thread
	check(mHandle==NULL);
	check(mContextInstance.get()==NULL);
	
	releaseLinker();
}


//! @brief Release linker handle and context
void Substance::Details::Engine::releaseLinker()
{
	unsigned int res;
	res = Substance::gAPI->SubstanceLinkerRelease(mLinkerHandle, mLinkerContext);
	mLinkerCacheData = NULL;
	check(res==0);
}


//! @brief Destroy engine (handle and context)
//! @pre No render currently running
void Substance::Details::Engine::releaseEngine()
{
	// Release engine handle
	unsigned int res;
	(void)res;
	if (mHandle!=NULL)
	{
		res = Substance::gAPI->SubstanceHandleRelease(mHandle);
		mHandle = NULL;
		check(res==0);
	}

	// Release pending textures
	releaseTextures();

	// Release engine context
	mContextInstance.reset();
}


//! @brief Link/Relink all states in pending render jobs
//! @param renderJobBegin Begin of chained list of render jobs to link
//! @return Return true if link process succeed
bool Substance::Details::Engine::link(
	RenderJob* renderJobBegin,
	const RenderJob* renderJobLast)
{
	uint32 res;
	(void)res;

	RenderCallbacks* callbacks = renderJobBegin->getCallbacks();
	
	// Merge all graph states
	LinkGraphs linkgraphs;
	while (true)
	{
		linkgraphs.merge(renderJobBegin->getLinkGraphs());
		if (renderJobBegin == renderJobLast)
		{
			break;
		}
		renderJobBegin = renderJobBegin->getNextJob();
	}

	// Disable all outputs by default
	res = Substance::gAPI->SubstanceLinkerEnableOutputs(
		mLinkerHandle,
		NULL,
		0);
	assert(res == 0);

	TArray<unsigned int> enabledIds;

	// Push all states to link
	SBS_VECTOR_FOREACH (
		const LinkGraphs::GraphStatePtr& graphstateptr,
		linkgraphs.graphStates)
	{
		LinkContext linkContext(
			mLinkerHandle,
			graphstateptr->getBinary(),
			graphstateptr->getUid());
		linkContext.graphBinary.resetTranslatedUids();  // Reset translated UID
		mCurrentLinkContext = &linkContext;  // Set as current context to fill
		graphstateptr->getLinkData()->push(linkContext);

		// Select outputs
		SBS_VECTOR_FOREACH (
			const GraphBinary::Entry& entry,
			linkContext.graphBinary.outputs)
		{		
			enabledIds.Push(entry.uidTranslated);
		}
	}

	res = Substance::gAPI->SubstanceLinkerEnableOutputs(
		mLinkerHandle,
		enabledIds.GetData(),
		enabledIds.Num());

	if (res)
	{
		assert(0);
	}
		
	// Link, Grab assembly
	size_t sbsbindataindex = mSbsbinDatas[0].empty() ? 0 : 1;
	std::string &sbsbindata = mSbsbinDatas[sbsbindataindex];
	check(mSbsbinDatas[0].empty()||mSbsbinDatas[1].empty());
	{
		const unsigned char* resultData = NULL;
		size_t resultSize = 0;
		res = Substance::gAPI->SubstanceLinkerLink(
			mLinkerHandle,
			&resultData,
			&resultSize);
		check(res==0);
	
		sbsbindata.assign((const char*)resultData,resultSize);
	}

	// Grab new cache data blob
	res = Substance::gAPI->SubstanceLinkerGetCacheMapping(
		mLinkerHandle,
		&mLinkerCacheData,
		mLinkerCacheData);
	check(res==0);

	// Create Substance context if necessary, done on render thread(required
	//	by some impl.) 
	if (mContextInstance.get() == NULL)
	{
		mContextInstance.reset(new Context(*this, callbacks));
	}

	// Create new handle
	SubstanceHandle *newhandle = NULL;
	res = Substance::gAPI->SubstanceHandleInit(&newhandle,
				mContextInstance->getContext(),
				(unsigned char*)sbsbindata.data(),
				sbsbindata.size(),
				mHardResources,
				(size_t)this);
	check(res==0);

	// Switch handle
	{
		// Scoped modification
		Sync::unique_lock slock(mMutexHandle);
		
		if (mHandle!=NULL)
		{
			// Transfer
			res = Substance::gAPI->SubstanceHandleTransferCache(
				newhandle,
				mHandle,
				mLinkerCacheData);
			check(res==0);
	
			// Delete previous handle
			res = Substance::gAPI->SubstanceHandleRelease(mHandle);
			check(res==0);
			
			// Erase previous sbsbin data
			mSbsbinDatas[sbsbindataindex^1].resize(0);
		}
		
		// Use new one
		mHandle = newhandle;
	}
	
	// Fill Graph binary SBSBIN indices
	fillIndices(linkgraphs);
	
	return true;
}


//! @brief Function called just before pushing I/O to compute
//! @post Engine render queue is flushed
void Substance::Details::Engine::flush()
{
	check(mHandle!=NULL);
	
	unsigned int res = Substance::gAPI->SubstanceHandleFlush(mHandle);
	(void)res;
	check(res==0);
}


//! @brief Stop any generation
//! Thread safe stop call on current handle if present 
//! @note Called from user thread
void Substance::Details::Engine::stop()
{
	Sync::unique_lock slock(mMutexHandle);
	if (mHandle!=NULL)
	{
		Substance::gAPI->SubstanceHandleStop(mHandle);
	}
}


//! @brief Set new memory budget and CPU usage.
//! @param renderOptions New render options to use.
//! @note This function can be called at any time, from any thread.
//!	@return Return true if options are immediately processed.
bool Substance::Details::Engine::setOptions(
	const RenderOptions& renderOptions)
{
	unsigned int res, state = 0;
	(void)res;
	
	Sync::unique_lock slock(mMutexHandle);
	
	fillHardResources(mHardResources,renderOptions);
	
	if (mHandle!=NULL)
	{
		// Switch resources
		res = Substance::gAPI->SubstanceHandleSwitchHard(mHandle, Substance_Sync_Asynchronous, &mHardResources);
		check(res==0);
	
		// Get current engine state
		res = Substance::gAPI->SubstanceHandleGetState(
			mHandle,
			&state);

		assert(res==0);
	}
	
	// New memory will be processed if engine render job not empty
	return (state&Substance_State_NoMoreRender) == 0;
}


void Substance::Details::Engine::clearCache()
{
	Sync::unique_lock slock(mMutexHandle);
	
	if (mHandle != NULL)
	{
		SubstanceHardResources substanceRscnNoRes;
		memset(&substanceRscnNoRes, 0, sizeof(SubstanceHardResources));

		for (auto i = SUBSTANCE_CPU_COUNT_MAX-1; i >= 0; --i)
		{
			substanceRscnNoRes.cpusUse[i] = Substance_Resource_DoNotUse;
		}

		substanceRscnNoRes.systemMemoryBudget = substanceRscnNoRes.videoMemoryBudget[0] = SUBSTANCE_MEMORYBUDGET_FORCEFLUSH;

		// cold run with a zero hardware budget to clear cache
		Substance::gAPI->SubstanceHandleSwitchHard(mHandle, Substance_Sync_Synchronous, &substanceRscnNoRes);
		Substance::gAPI->SubstanceHandleStart(mHandle);

		// reset linker handle to free some memory
		releaseLinker();
		createLinker();
		
		// then restore previous hardware budget
		Substance::gAPI->SubstanceHandleSwitchHard(mHandle, Substance_Sync_Synchronous, &mHardResources);
		Substance::gAPI->SubstanceHandleStart(mHandle);
	}
}


void Substance::Details::Engine::fillHardResources(
	SubstanceHardResources& hardRsc,
	const RenderOptions& renderOptions)
{
	memset(&hardRsc, 0, sizeof(SubstanceHardResources));

	// 16kb minimum
	hardRsc.systemMemoryBudget = hardRsc.videoMemoryBudget[0] =
		renderOptions.mMemoryBudget | 0x4000;

	// Switch on/off CPU usage
	for (size_t k = 0; k < SUBSTANCE_CPU_COUNT_MAX; ++k)
	{
		hardRsc.cpusUse[k] = k < std::max<size_t>(renderOptions.mCoresCount, 1) ?
			Substance_Resource_FullUse :
			Substance_Resource_DoNotUse;
	}
}


//! @brief Linker Collision UID callback implementation
//! @param collisionType Output or input collision flag
//! @param previousUid Initial UID that collide
//! @param newUid New UID generated
//! @note Called by linker callback
void Substance::Details::Engine::callbackLinkerUIDCollision(
	SubstanceLinkerUIDCollisionType collisionType,
	uint32 previousUid,
	uint32 newUid)
{
	check(mCurrentLinkContext!=NULL);
	mCurrentLinkContext->notifyLinkerUIDCollision(
		collisionType,
		previousUid,
		newUid);
}


//! @brief Context constructor, create Substance engine context
Substance::Details::Engine::Context::Context(
		Engine &parent,
		RenderCallbacks* callbacks) :
	mParent(parent),
	mContext(NULL)
{
	unsigned int res;
	(void)res;

	// Create context
	res = Substance::gAPI->SubstanceContextInit(&mContext,
		(void*)substanceDetailsEngineCallbackOutputCompleted, 
		(void*)substanceDetailsEngineCallbackJobCompleted, 
		(void*)substanceDetailsEngineCallbackInputImageLock, 
		(void*)substanceDetailsEngineCallbackInputImageUnlock,
		(void*)substanceDetailsEngineCallbackMalloc,
		(void*)substanceDetailsEngineCallbackFree);

	check(res == 0);
}


//! @brief Destructor, release Substance Context
Substance::Details::Engine::Context::~Context()
{
	unsigned int res = Substance::gAPI->SubstanceContextRelease(mContext);
	check(res==0);
}


//! @brief Fill Graph binaries w/ new Engine handle SBSBIN indices
//! @param linkGraphs Contains Graph binaries to fill indices
void Substance::Details::Engine::fillIndices(LinkGraphs& linkGraphs) const
{
	typedef std::vector<std::pair<uint32,uint32> > UidIndexPairs;
	UidIndexPairs trinputs,troutputs;

	// Get indices/UIDs pairs
	{
		unsigned int dindex;
		SubstanceDataDesc datadesc;
		unsigned int res = Substance::gAPI->SubstanceHandleGetDesc(
			mHandle,
			&datadesc);
		(void)res;
		assert(res == 0);

		// Parse Inputs
		trinputs.reserve(datadesc.inputsCount);
		for (dindex = 0; dindex < datadesc.inputsCount; ++dindex)
		{
			SubstanceInputDesc indesc;
			res = Substance::gAPI->SubstanceHandleGetInputDesc(
				mHandle,
				dindex,
				&indesc);
			assert(res == 0);

			trinputs.push_back(std::make_pair(indesc.inputId, dindex));
		}

		// Parse outputs
		troutputs.reserve(datadesc.outputsCount);
		for (dindex = 0; dindex < datadesc.outputsCount; ++dindex)
		{
			SubstanceOutputDesc outdesc;
			res = Substance::gAPI->SubstanceHandleGetOutputDesc(
				mHandle,
				dindex,
				&outdesc);
			assert(res == 0);

			troutputs.push_back(std::make_pair(outdesc.outputId, dindex));
		}
	}
	typedef std::vector<std::pair<uint32,GraphBinary::Entry*> > UidEntryPairs;
	UidEntryPairs einputs,eoutputs;

	// Get graph binaries entries per translated uids
	SBS_VECTOR_FOREACH (
		const LinkGraphs::GraphStatePtr& graphstateptr,
		linkGraphs.graphStates)
	{
		GraphBinary& binary = graphstateptr->getBinary();
		
		// Get all input entries
		SBS_VECTOR_FOREACH (GraphBinary::Entry& entry,binary.inputs)
		{
			einputs.push_back(std::make_pair(entry.uidTranslated,&entry));
		}
		
		// Get all output entries
		SBS_VECTOR_FOREACH (GraphBinary::Entry& entry,binary.outputs)
		{
			eoutputs.push_back(std::make_pair(entry.uidTranslated,&entry));
		}
		
		// Mark as linked
		binary.linked();
	}

	// Sort all, per translated UID
	std::sort(trinputs.begin(),trinputs.end());
	std::sort(troutputs.begin(),troutputs.end());
	std::sort(einputs.begin(),einputs.end());
	std::sort(eoutputs.begin(),eoutputs.end());

	// Fill input entries w/ indices
	UidIndexPairs::const_iterator trite = trinputs.begin();
	SBS_VECTOR_FOREACH (UidEntryPairs::value_type& epair,einputs)
	{
		// Skip unused inputs (multi-graph case)
		while (trite!=trinputs.end() && trite->first<epair.first)
		{
			++trite;
		}	
		check(trite!=trinputs.end() && trite->first==epair.first);
		if (trite!=trinputs.end() && trite->first==epair.first)
		{
			epair.second->index = (trite++)->second;
		}
	}
	
	// Fill output entries w/ indices
	trite = troutputs.begin();
	SBS_VECTOR_FOREACH (UidEntryPairs::value_type& epair,eoutputs)
	{
		check(trite!=troutputs.end() && trite->first==epair.first);
		while (trite!=troutputs.end() && trite->first<epair.first)
		{
			++trite;
		}	
		if (trite!=troutputs.end() && trite->first==epair.first)
		{
			epair.second->index = (trite++)->second;
		}
	}
}


//! @brief Enqueue texture for deletion, texture ownership is grabbed
//! @warning Can be called from user thread
void Substance::Details::Engine::enqueueRelease(
	const SubstanceTexture& texture)
{
	check(mContextInstance.get()!=NULL);

	{
		Sync::unique_lock slock(mMutexToRelease);
		mToReleaseTextures.Push(texture);
		mPendingReleaseTextures = true;
	}
}


//! @brief Release enqueued textures (enqueueRelease())
//! @brief Must be called from render thread
void Substance::Details::Engine::releaseTextures()
{
	if (mContextInstance.get()!=NULL &&
		mPendingReleaseTextures)
	{
		Sync::unique_lock slock(mMutexToRelease);

		for (auto textureIt = mToReleaseTextures.CreateIterator(); textureIt; ++textureIt)
		{
			FMemory::Free(textureIt->buffer);
		}

		mToReleaseTextures.Reset();
		mPendingReleaseTextures = false;
	}
}


