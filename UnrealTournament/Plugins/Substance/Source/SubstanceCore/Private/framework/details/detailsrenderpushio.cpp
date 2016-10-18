//! @file detailsrenderpushio.cpp
//! @brief Substance Framework Render Push Input/Output implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111109
//! @copyright Allegorithmic. All rights reserved.
//!
 
#include "SubstanceCorePrivatePCH.h"
#include "SubstanceFOutput.h"
#include "SubstanceFGraph.h"

#include "framework/details/detailsrenderpushio.h"
#include "framework/details/detailsduplicatejob.h"
#include "framework/details/detailscomputation.h"
#include "framework/details/detailsgraphbinary.h"
#include "framework/details/detailsgraphstate.h"
#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailsrendertoken.h"
#include "framework/details/detailsoutputsfilter.h"
#include "framework/details/detailsstates.h"

#include "SubstanceCallbacks.h"

#include <iterator>
#include <algorithm>


using namespace Substance::Details;

//! @brief Create from render job
//! @param renderJob Parent render job that owns this instance
//! @note Called from user thread 
Substance::Details::RenderPushIO::RenderPushIO(RenderJob &renderJob) :
	mRenderJob(renderJob),
	mState(State_None)
{
}

//! @brief Constructor from push I/O to duplicate
//! @param renderJob Parent render job that owns this instance
//! @param src The canceled push I/O to copy
//! @param dup Duplicate job context
//! Build a push I/O from a canceled one and optionally filter outputs.
//! Push again SRC render tokens (of not filtered outputs).
//! @warning Resulting push I/O can be empty (hasOutputs()==false) if all
//!		outputs are filtered OR already computed. In this case this push IO 
//!		is no more useful and can be removed.
//! @note Called from user thread
Substance::Details::RenderPushIO::RenderPushIO(
		RenderJob &renderJob,
		const RenderPushIO& src,
		DuplicateJob& dup) :
	mRenderJob(renderJob),
	mState(State_None)
{
	OutputsFilter::Outputs foutsdummy;
	
	mInstances.reserve(src.mInstances.size());
	SBS_VECTOR_FOREACH (const Instance *srcinst,src.mInstances)
	{
		// Skip if instance deleted
		const substanceGuid_t uid = srcinst->instanceGuid;
		FGraphInstance* ginst = dup.states.getInstanceFromUid(uid);
		if (ginst==NULL)
		{
			// Deleted
			continue;
		}
	
		Instance *newinst = new Instance(*srcinst);

		// Filter outputs
		OutputsFilter::Outputs::const_iterator foutitecur = foutsdummy.end();
		OutputsFilter::Outputs::const_iterator foutiteend = foutitecur;
		if (dup.filter!=NULL)
		{
			OutputsFilter::Instances::const_iterator finstite = 
				dup.filter->instances.find(uid);
			if (finstite!=dup.filter->instances.end())
			{
				// If filtered and instance found, get iterator on indices
				foutitecur = finstite->second.begin();
				foutiteend = finstite->second.end();
			}
		}
		
		newinst->outputs.reserve(srcinst->outputs.size());
		SBS_VECTOR_FOREACH (const Output& srcout,srcinst->outputs)
		{
			// Iterate on filtered outputs indices list in same time
			while (foutitecur!=foutiteend && (*foutitecur)<srcout.index)
			{
				++foutitecur;
			}
		
			if (!srcout.renderToken->isComputed() &&  // Skip if is computed
				(foutitecur==foutiteend || 
					(*foutitecur)!=srcout.index))     // Skip if filtered
			{
				// Duplicate output
				newinst->outputs.push_back(srcout);
				
				// Push again SRC render tokens
				srcout.renderToken->incrRef();
				check(ginst->Outputs.size()>srcout.index);
				ginst->Outputs[srcout.index].push(srcout.renderToken);
			}
		}
		
		if (newinst->outputs.empty())
		{
			// Pruned, accumulate delta state for next one
			dup.append(uid,newinst->deltaState);
		
			// Delete instance
			delete newinst;
		}
		else
		{
			// Has outputs, pust it
			mInstances.push_back(newinst);
			
			// Fix delta state w/ accumulated delta
			dup.fix(uid,newinst->deltaState);
			
			// Update state if necessary
			if (dup.needUpdateState())
			{
				newinst->graphState.apply(newinst->deltaState);
			}
		}
	}
}


//! @brief Destructor
//! @note Called from user thread 
Substance::Details::RenderPushIO::~RenderPushIO()
{
	// Delete instances elements
	for (size_t i=0; i<mInstances.size(); i++)
	{
		delete mInstances[i];
	}
}

	
//! @brief Create a push I/O pair from current state & current instance
//! @param graphState The current graph state
//! @param graphInstance The pushed graph instance (not keeped)
//! @note Called from user thread 
//! @return Return true if at least one dirty output
bool Substance::Details::RenderPushIO::push(
	GraphState &graphState,
	FGraphInstance* graphInstance)
{
	Instance *instance = new Instance(graphState,graphInstance);
	
	if (instance->outputs.empty())
	{
		// No outputs, (not necessary to use hasOutputs()), delete it
		delete instance;
		return false;
	}
	
	mInstances.push_back(instance);
	
	return true;
}


//! @brief Prepend reverted input delta into duplicate job context
//! @param dup The duplicate job context to accumulate reversed delta
//! Use to restore the previous state of copied jobs.
//! @note Called from user thread
void Substance::Details::RenderPushIO::prependRevertedDelta(
	DuplicateJob& dup) const
{
	SBS_VECTOR_FOREACH (const Instance *instance,mInstances)
	{
		dup.prepend(instance->instanceGuid,instance->deltaState);
	}
}


bool Substance::Details::RenderPushIO::pull(
	Computation &computation,
	bool inputsOnly)
{
	if ((mState&State_Process)==0 || inputsOnly)
	{
		// Not into process, pull iteration should end
		return false;
	}

	if (isComplete(false)==Complete_Yes)
	{
		// Skip completed push I/O
		return true;
	}

	// Set user data for callback
	computation.setUserData((size_t)this);
	
	// Reset data built during pull
	check(inputsOnly || (mState&State_Process)!=0);

	std::fill(
		mDestOutputs.begin(),
		mDestOutputs.end(),
		(const Output*)NULL);
	mInputJobPendingCount = 0;
	
	// Output SBSBIN indices to push
	std::vector<uint32> indicesout;

	SBS_VECTOR_FOREACH (Instance *instance,mInstances)
	{
		const GraphBinary& binary = instance->graphState.getBinary();
	
		// Push inputs
		SBS_VECTOR_FOREACH (
			const DeltaState::Input& input,
			instance->deltaState.getInputs())
		{
			check(binary.inputs.size()>input.index);
			const uint32 binindex = binary.inputs[input.index].index;
			check(binindex!=GraphBinary::invalidIndex);
			
			if (binindex!=GraphBinary::invalidIndex)  
			{
				// Only if present in SBSBIN 
				ImageInputToken *imgtkn = NULL;
				if (input.modified.isValidImage())
				{
					// Image input, get pointer
					ImageInputPtr imgptr = instance->deltaState.getImageInput(
						input.modified.value.imagePtrIndex);
					check(imgptr);
					if (imgptr)
					{
						imgtkn = imgptr->getToken();
					}
				}
				
				if (computation.pushInput(binindex,input.modified,imgtkn)) 
				{
					// If NOT only hint
					++mInputJobPendingCount;       // One more pending input job
					mState |= State_InputsPending; // Has inputs pending
				}

			}
		}
		
		if (!inputsOnly)
		{
			// Append to outputs SBSBIN indices list
			indicesout.reserve(indicesout.size()+instance->outputs.size());
			SBS_VECTOR_FOREACH (const Output& output,instance->outputs)
			{
				check(binary.outputs.size()>output.index);
				if (!output.renderToken->isComputed())
				{
					// Only if not already computed
					const uint32 binindex = binary.outputs[output.index].index;
					check(binindex!=GraphBinary::invalidIndex);
					if (binindex!=GraphBinary::invalidIndex)
					{
						// Only if present in SBSBIN
						indicesout.push_back(binindex);
						
						// Has outputs pending
						mState |= State_OutputsPending;
						
						// Fill render token
						if (mDestOutputs.size()<=binindex)
						{
							std::fill_n(
								std::back_inserter(mDestOutputs),
								1+(size_t)binindex-mDestOutputs.size(),
								(const Output*)NULL);	
						}
						mDestOutputs[binindex] = &output;
					}
				}
			}
		}
	}
	
	if (!indicesout.empty())
	{
		// Push outputs
		computation.pushOutputs(indicesout);
	}

	return true;
}


Substance::Details::Engine*
Substance::Details::RenderPushIO::getEngine() const
{ 
	return mRenderJob.getEngine();
}


//! @brief Fill filter outputs structure from this push IO
//! @param[in,out] filter The filter outputs structure to fill
void Substance::Details::RenderPushIO::fill(OutputsFilter& filter) const
{
	SBS_VECTOR_FOREACH (const Instance *instance,mInstances)
	{
		OutputsFilter::Outputs intermouts;
		OutputsFilter::Outputs &instouts = 
			filter.instances[instance->instanceGuid];
		OutputsFilter::Outputs* foutputs = instouts.empty() ?
			&instouts :
			&intermouts;
		
		// Get output indices
		foutputs->reserve(instance->outputs.size());
		SBS_VECTOR_FOREACH (const Output& output,instance->outputs)
		{
			foutputs->push_back(output.index);
		}
		
		if (foutputs==&intermouts)
		{
			// If previous data, unite both lists
			OutputsFilter::Outputs mergedouts;
			mergedouts.reserve(instouts.size()+intermouts.size());
			
			std::set_union(
				instouts.begin(),
				instouts.end(),
				intermouts.begin(),
				intermouts.end(),
				std::back_inserter(mergedouts));
				
			check(mergedouts.size()>=instouts.size() && 
				mergedouts.size()>=intermouts.size());
			instouts = mergedouts;
		}
	}
}


Substance::Details::RenderPushIO::Process
Substance::Details::RenderPushIO::enqueueProcess()
{
	bool linkneeded = false;
	const bool alreadyprocess = (mState&State_Process)!=0;

	if (!alreadyprocess)
	{
		// Verify first if no collision
		SBS_VECTOR_FOREACH(Instance* instance, mInstances)
		{
			const GraphBinary& graphbin = instance->graphState.getBinary();

			SBS_VECTOR_FOREACH(
				const DeltaState::Output& outstate, instance->deltaState.getOutputs())
			{
				if (graphbin.outputs.at(outstate.index).enqueuedLink)
				{
					// Already pending override for this link 
					return Process_Collision;
				}
			}
		}
	}

	SBS_VECTOR_FOREACH(Instance* instance,mInstances)
	{
		GraphBinary& graphbin = instance->graphState.getBinary();

		if (!alreadyprocess)
		{
			linkneeded = linkneeded || !graphbin.isLinked();
			SBS_VECTOR_FOREACH(const DeltaState::Output& outstate, instance->deltaState.getOutputs())
			{
				GraphBinary::OutputEntry& outbin = graphbin.outputs.at(outstate.index);
				assert(!outbin.enqueuedLink);
				outbin.state = outstate.modified;
				linkneeded = true;
			}
		}

		// Mark all output of instances as used
		SBS_VECTOR_FOREACH(RenderPushIO::Output& output,instance->outputs)
		{
			GraphBinary::OutputEntry& outbin = graphbin.outputs.at(output.index);
			outbin.enqueuedLink = true;
		}
	}

	assert(!alreadyprocess || !linkneeded);

	mState |= State_Process;
	
	return linkneeded ? 
		Process_LinkRequired :
		Process_Append;
}


//! @brief Cancel notification, decrease RenderToken counters
void Substance::Details::RenderPushIO::cancel()
{
	SBS_VECTOR_FOREACH (Instance *instance, mInstances)
	{
		SBS_VECTOR_FOREACH (Output& output, instance->outputs)
		{
			output.renderToken->canceled();
		}
	}
}


//! @brief Accessor: At least one output to compute
//! Check all render tokens if not already filled
bool Substance::Details::RenderPushIO::hasOutputs() const
{
	SBS_VECTOR_FOREACH (const Instance *instance,mInstances)
	{
		if (instance->hasOutputs())
		{
			return true;
		}
	}

	return false;
}


//! @brief Return if the current Push I/O is completed
//! @param inputOnly Only inputs are required
//! @note Called from Render queue thread
//! @return Can return "don't know" if no pending I/O
Substance::Details::RenderPushIO::Complete 
Substance::Details::RenderPushIO::isComplete(bool inputOnly) const
{
	if (State_None==mState)
	{
		return Complete_No;
	}
	else if (State_Process==mState)
	{
		return Complete_DontKnow;
	}
	
	const uint32 flags = inputOnly || (mState&State_OutputsPending)==0 ?
		State_InputsPushed|State_InputsPending :
		State_OutputsComputed|State_OutputsPending;
	return (flags&mState)==flags ?
		Complete_Yes :
		Complete_No;
}


//! @brief Engine job completed
//! @note Called by engine callback from Render queue thread
void Substance::Details::RenderPushIO::callbackJobComplete()
{
	check(mInputJobPendingCount>=0);
	--mInputJobPendingCount;
	
	if (mInputJobPendingCount==0)
	{
		// 0 reached, all inputs processed
		check((mState&State_InputsPending)!=0);
		check((mState&State_InputsPushed)==0);
		mState |= State_InputsPushed;
	}
	else if (mInputJobPendingCount==-1)
	{
		// -1 reached, outputs computed
		check((mState&State_OutputsPending)!=0);
		check((mState&State_OutputsComputed)==0);
		mState |= State_OutputsComputed;		
	}
}


//! @brief Engine output completed
//! @param index Output SBSBIN index of completed output
//! @param renderResult The result texture, ownership transfered
//! @note Called by engine callback from Render queue thread
void Substance::Details::RenderPushIO::callbackOutputComplete(
	uint32 index,
	RenderResult* renderResult)
{
	check(index<mDestOutputs.size());
	check(mDestOutputs[index]!=NULL);
	
	const Output& output = *mDestOutputs[index];
	output.renderToken->fill(renderResult);

	// Emit callback if plugged
	RenderCallbacks* callbacks = mRenderJob.getCallbacks();
	if (callbacks!=NULL)
	{
		callbacks->outputComputed(
			mRenderJob.getUid(),
			output.graphInstance,
			output.outputInstance);
	}
}


//! @brief Build a push I/O pair from current state & current instance
//! @param state The current graph state
//! @param graphInstance The pushed graph instance (not kept)
Substance::Details::RenderPushIO::Instance::Instance(
		GraphState &state,
		FGraphInstance* graphInstance) :
	graphState(state),
	instanceGuid(graphInstance->InstanceGuid)
{
	uint32 outindex = 0;
	outputs.reserve(graphInstance->Outputs.size());

	Substance::List<output_inst_t>::TIterator ItOut(graphInstance->Outputs.itfront());
	for (;ItOut;++ItOut)
	{
		if (ItOut->bIsEnabled && ItOut->queueRender())
		{
			// Push output to compute
			outputs.resize(outputs.size()+1);
			Output &newout = outputs.back();
			newout.index = outindex;
			newout.outputInstance = &(*ItOut);
			newout.graphInstance = graphInstance;
			
			// Create render token
			newout.renderToken.reset(new RenderToken());
			ItOut->push(newout.renderToken);
		}
		++outindex;
	}

	if (!outputs.empty())
	{
		// Create input delta
		deltaState.fill(graphState,graphInstance);
		
		// Update state
		graphState.apply(deltaState);
	}
}


//! @brief Duplicate instance (except outputs)
Substance::Details::RenderPushIO::Instance::Instance(const Instance &src) :
	graphState(src.graphState),
	instanceGuid(src.instanceGuid),
	deltaState(src.deltaState)
{
}


//! @brief Accessor: At least one output to compute
//! Check all render tokens if not already filled
bool Substance::Details::RenderPushIO::Instance::hasOutputs() const
{
	SBS_VECTOR_FOREACH (const Output& output,outputs)
	{
		if (!output.renderToken->isComputed())
		{
			return true;
		}
	}
	
	return false;
}

