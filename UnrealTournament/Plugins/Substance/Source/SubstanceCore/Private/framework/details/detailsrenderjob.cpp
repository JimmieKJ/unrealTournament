//! @file detailsrenderjob.cpp
//! @brief Substance Framework Render Job implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111107
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceCorePrivatePCH.h"

#include "SubstanceFGraph.h"

#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailsduplicatejob.h"
#include "framework/details/detailsrenderpushio.h"
#include "framework/details/detailsgraphstate.h"
#include "framework/details/detailsstates.h"
#include "framework/details/detailsengine.h"
#include "framework/details/detailscomputation.h"

#include <algorithm>



//! @brief Constructor
//! @param callbacks User callbacks instance (or NULL if none)
Substance::Details::RenderJob::RenderJob(
		uint32 uid,
		RenderCallbacks *callbacks) :
	mUid(uid|0x80000000u),
	mState(State_Setup),
	mCanceled(false),
	mNextJob(NULL),
	mCallbacks(callbacks),
	mEngine(NULL)
{
}


//! @brief Constructor from job to duplicate and outputs filtering
//! @param src The canceled job to copy
//! @param dup Duplicate job context
//! @param callbacks User callbacks instance (or NULL if none)
//!
//! Build a pending job from a canceled one and optionnaly pointer on an
//! other job used to filter outputs.
//! Push again SRC render tokens (of not filtered outputs).
//! @warning Resulting job can be empty if all outputs are filtered. In this
//!		case this job is no more usefull and can be removed.
//! @note Called from user thread
Substance::Details::RenderJob::RenderJob(
		const RenderJob& src,
		DuplicateJob& dup,
		RenderCallbacks *callbacks) :
	mUid(src.getUid()),
	mState(State_Setup),
	mCanceled(false),
	mNextJob(NULL),
	mLinkGraphs(dup.linkGraphs),
	mCallbacks(callbacks)
{
	mRenderPushIOs.reserve(src.mRenderPushIOs.size());
	
	SBS_VECTOR_FOREACH (RenderPushIO *srcpushio,src.mRenderPushIOs)
	{
		RenderPushIO *newpushio = 
			new RenderPushIO(*this,*srcpushio,dup);
		if (newpushio->hasOutputs())
		{
			// Has outputs, push it in the list
			mRenderPushIOs.push_back(newpushio);
		}
		else
		{
			// No outputs, all filtered
			delete newpushio;
		}
	}
}


//! @brief Destructor
Substance::Details::RenderJob::~RenderJob()
{
	// Delete RenderPushIO elements
	for (size_t i=0; i<mRenderPushIOs.size(); i++)
	{
		delete mRenderPushIOs[i];
	}
}


//! @brief Take states snapshot (used by linker)
//! @param states Used to take a snapshot states to use at link time
//!
//! Must be done before activate it
void Substance::Details::RenderJob::snapshotStates(const States &states)
{
	states.fill(mLinkGraphs);        // Create snapshot from current states
}


//! @brief Push I/O to render: from current state & current instance
//! @param graphState The current graph state
//! @param graphInstance The pushed graph instance (not kept)
//! @pre Job must be in 'Setup' state
//! @note Called from user thread 
//! @return Return true if at least one dirty output
//!
//! Update states, create render tokens.
bool Substance::Details::RenderJob::push(
	GraphState &graphState,
	FGraphInstance* graphInstance)
{
	check(State_Setup==mState);

	// Get push IO index
	uint32 pushioindex = mStateUsageCount[graphState.getUid()]++;
	
	check(pushioindex<=mRenderPushIOs.size());
	const bool newpushio = mRenderPushIOs.size()==(size_t)pushioindex;
	if (newpushio)
	{
		// Create new push IO
		mRenderPushIOs.push_back(new RenderPushIO(*this));
	}
	
	// Push!
	if (mRenderPushIOs.at(pushioindex)->push(graphState,graphInstance))
	{
		return true;
	}
	else if (newpushio)
	{
		// No dirty outputs: Remove just created, not necessary
		delete mRenderPushIOs.back();
		
		mRenderPushIOs.pop_back();
		--mStateUsageCount[graphState.getUid()];
	}
	
	return false;
}


//! @brief Put the job in render state, build the render chained list
//! @param previous Previous job in render list or NULL if first
//! @pre Activation must be done in reverse order, this next job is already
//!		activated.
//! @pre Job must be in 'Setup' state
//! @post Job is in 'Pending' state
//! @note Called from user thread
void Substance::Details::RenderJob::activate(RenderJob* previous)
{
	check(State_Setup==mState);
	
	mState = State_Pending;
	
	if (previous!=NULL)
	{
		check(previous->mNextJob==NULL);
		previous->mNextJob = this;
	}
}


//! @brief Cancel this job (or this job and next ones)
//! @param cancelList If true this job and next ones (mNextJob chained list)
//! 	are canceled (in reverse order).
//! @note Called from user thread
//! @return Return true if at least one job is effectively canceled
//!
//! Cancel if Pending or Computing.
//! Outputs of canceled jobs are not computed, only inputs are set (state
//! coherency). RenderToken's are notified as canceled.
bool Substance::Details::RenderJob::cancel(bool cancelList)
{
	bool res = false;
	if (cancelList && mNextJob!=NULL)
	{
		// Recursive call, reverse chained list order canceling
		res = mNextJob->cancel(true);
	}
	
	if (!mCanceled)
	{
		res = true;
		mCanceled = true;
		
		// notify cancel for each push I/O (needed for RenderToken counter decr)
		SBS_VECTOR_REVERSE_FOREACH (RenderPushIO *pushio,mRenderPushIOs)
		{
			pushio->cancel();
		}
	}
	
	return res;
}


//! @brief Prepend reverted input delta into duplicate job context
//! @param dup The duplicate job context to accumulate reversed delta
//! Use to restore the previous state of copied jobs.
//! @note Called from user thread
void Substance::Details::RenderJob::prependRevertedDelta(
	DuplicateJob& dup) const
{
	SBS_VECTOR_FOREACH (const RenderPushIO *pushio,mRenderPushIOs)
	{
		pushio->prependRevertedDelta(dup);
	}
}


//! @brief Fill filter outputs structure from this job
//! @param[in,out] filter The filter outputs structure to fill
void Substance::Details::RenderJob::fill(OutputsFilter& filter) const
{
	SBS_VECTOR_FOREACH (RenderPushIO *pushio,mRenderPushIOs)
	{
		pushio->fill(filter);
	}
}


//! @brief Push input and output in engine handle
void Substance::Details::RenderJob::pull(Computation &computation)
{
	mState = State_Computing;
	mEngine = &computation.getEngine();
	const bool canceled = mCanceled; 
	
	SBS_VECTOR_FOREACH (RenderPushIO *pushio,mRenderPushIOs)
	{

		// If canceled, only inputs
		if (!pushio->pull(computation, canceled) && !canceled)
		{
			// Not into process early exit
			break;
		}
	}
}


//! @brief Is render job complete
//! @return Return if all push I/O completed
bool Substance::Details::RenderJob::isComplete() const
{
	SBS_VECTOR_REVERSE_FOREACH (const RenderPushIO *pushio,mRenderPushIOs)
	{
		// Reverse test, ordered computation
		const RenderPushIO::Complete complete = pushio->isComplete(mCanceled);
		
		// Complete if last push I/O complete or only Inputs pushed only if
		// job canceled or no outputs to push
		if (RenderPushIO::Complete_DontKnow!=complete)
		{
			return RenderPushIO::Complete_Yes==complete;
		}
	}
	
	return true;	
}

bool Substance::Details::RenderJob::enqueueProcess(ProcessJobs& processJobs)
{
	check(mState == State_Pending || mState == State_Computing);

	mRenderPushIOs.begin();

	for (RenderPushIOs::const_iterator ioite = firstNonComplete(); // Skip complete
	ioite != mRenderPushIOs.end();
		++ioite)
	{
		switch ((*ioite)->enqueueProcess())
		{
		case RenderPushIO::Process_LinkRequired:
			processJobs.linkNeeded = true;
			// no break here

		case RenderPushIO::Process_Append:
			processJobs.last = this;          // At least one to process
			break;

		case RenderPushIO::Process_Collision:
		{
			return false;                     // Stop iterating
		}
		break;
		}
	}

	return getNextJob() != NULL;
}


bool Substance::Details::RenderJob::needResume() const
{
	assert(mState == State_Pending || mState == State_Computing);

	if (isCanceled())
	{
		// Do not resume if canceled
		return false;
	}

	// Skip complete
	RenderPushIOs::const_iterator ioite = firstNonComplete();

	assert(ioite != mRenderPushIOs.end() ||    // Should be skipped before
		mRenderPushIOs.empty());             // Or hard res. change only run

											 // True if first non complete is still in process
	return ioite != mRenderPushIOs.end() &&
		((*ioite)->getState()&RenderPushIO::State_Process) != 0;
}


bool Substance::Details::RenderJob::ProcessJobs::enqueue()
{
	bool resumeonly = true;

	check(first != NULL);

	// Test if not strict resume	
	for (RenderJob* curjob = first;
	curjob != NULL && resumeonly;
		curjob = curjob->getNextJob())
	{
		resumeonly = curjob->needResume();
		last = curjob;                         // Last job updated
	}

	if (!resumeonly)
	{
		// Enqueue to process job list if not strict resume
		RenderJob* curjob = first;
		last = first;                          // Restore last
		while (curjob->enqueueProcess(*this))
		{
			curjob = curjob->getNextJob();     // Next if no collision or end
			assert(curjob != NULL);
		}
	}

	return !resumeonly;
}


void Substance::Details::RenderJob::ProcessJobs::pull(
	Computation &computation) const
{
	RenderJob* curjob = first;
	do
	{
		// Push I/O
		curjob->pull(computation);
	} while (curjob != last && (curjob = curjob->getNextJob()) != NULL);
}


Substance::Details::RenderJob::RenderPushIOs::const_iterator
Substance::Details::RenderJob::firstNonComplete() const
{
	RenderPushIOs::const_iterator ioite = mRenderPushIOs.begin();

	// Skip complete
	while (ioite != mRenderPushIOs.end() &&
		(*ioite)->isComplete() != RenderPushIO::Complete_No)
	{
		++ioite;
	}

	return ioite;
}