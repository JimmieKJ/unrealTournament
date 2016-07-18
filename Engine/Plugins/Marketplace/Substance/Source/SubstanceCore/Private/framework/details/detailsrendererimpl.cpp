//! @file detailsrendererimpl.cpp
//! @brief The Substance renderer class implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111103
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceSettings.h"

#include "framework/details/detailscomputation.h"
#include "framework/details/detailsduplicatejob.h"
#include "framework/details/detailsrendererimpl.h"
#include "framework/details/detailsrenderjob.h"
#include "framework/details/detailsoutputsfilter.h"
#include "framework/renderer.h"

#include <algorithm>


//! @brief Default constructor
//! @param renderOptions Initial render options.
Substance::Details::RendererImpl::RendererImpl(
		const RenderOptions& renderOptions) :
	mEngine(renderOptions),
	mCurrentJob(NULL),
	mRenderState(RenderState_Idle),
	mHold(false),
	mCancelOccur(false),
	mPendingHardRsc(false),
	mExitRender(false),
	mUserWaiting(false),
	mEngineInitialized(false),
	mRenderCallbacks(NULL),
	mRenderJobUid(0)
{
}


//! @brief Destructor
Substance::Details::RendererImpl::~RendererImpl()
{
	// Cancel all
	cancel();

	// Wait for cancel effective
	flush();

	// Clear all render results created w/ this engine instance
	mStates.releaseRenderResults(mEngine.getInstanceUid());
	
	// Exit thread / release engine
	exitRender();
	
	// Delete render jobs
	for (size_t i = 0; i < mRenderJobs.size(); i++)
	{
		delete mRenderJobs[i];
		mRenderJobs[i] = NULL;
	}
}

//! @brief Release engine and terminates render thread
//! If no thread created and engine need to be released, call
//! wakeupRender before thread termination.
void Substance::Details::RendererImpl::exitRender()
{
	bool needlaunch = false;

	// Exit required, try to wake up render thread
	{
		Sync::unique_lock slock(mMainMutex);
		
		mExitRender = mRenderState!=RenderState_Idle || mEngineInitialized;
		needlaunch = mExitRender && !wakeupRender();
	}

	// Launch render just for releasing engine if necessary
	if (needlaunch)
	{
		check(mExitRender);

		launchRender();
	}

	// Wait that rendering thread ends (loop exit)
	{
		Sync::unique_lock slock(mMainMutex);
		
		if (mExitRender)
		{
			mUserWaiting = true;
			mCondVarUser.wait(slock);
			mUserWaiting = false;
		}

		check(!mExitRender);
	}

	mEngineInitialized = false;

	if (mThread.joinable())
	{
		mThread.join();
	}
}


//! @brief Wait for render thread exit/sleep if currently ongoing
//! @pre mMainMutex Must be NOT locked
//! @note Called from user thread
void Substance::Details::RendererImpl::waitRender()
{
	{
		Sync::unique_lock slock(mMainMutex);
		
		if (mRenderState==RenderState_OnGoing || (mCurrentJob!=NULL && !mHold))
		{
			mUserWaiting = true;
			mCondVarUser.wait(slock);
			mUserWaiting = false;
		}
	}
}


//! @brief Push graph instance current changes to render
//! @param graphInstance The instance to push dirty outputs
//! @return Return true if at least one dirty output
bool Substance::Details::RendererImpl::push(FGraphInstance* graphInstance)
{
	if (mRenderJobs.empty() || 
		mRenderJobs.back()->getState()!=RenderJob::State_Setup)
	{
		// Create setup render job if necessary
		mRenderJobs.push_back(new RenderJob(++mRenderJobUid,mRenderCallbacks));
	}

	return mRenderJobs.back()->push(mStates[graphInstance],graphInstance);
}


//! @brief Launch computation
//! @param options RunOptions flags combination
//! @return Return UID of render job or 0 if not pushed computation to run
uint32 Substance::Details::RendererImpl::run(
	unsigned int options)
{
	// Cleanup deprecated jobs
	cleanup();

	// Force run if pending hard resource switch
	if (mPendingHardRsc)
	{
		mPendingHardRsc = false;
		if (mRenderJobs.empty())
		{
			// Create empty render job (implies engine start w\ outputs)
			mRenderJobs.push_back(new RenderJob(++mRenderJobUid,mRenderCallbacks));
		}
	}
	else
	{
		// Otherwise, run only if non-empty jobs list
		if (mRenderJobs.empty() ||
			mRenderJobs.back()->getState()!=RenderJob::State_Setup ||
			mRenderJobs.back()->isEmpty())
		{
			// No pushed jobs
			return 0;
		}
	}

	// Skip already canceled jobs
	// and currently computed job if Run_PreserveRun flag is present
	RenderJob *curjob = mCurrentJob;    // Current job (can diverge)
	RenderJob *lastjob = curjob;        // Last job in render list
	while (curjob!=NULL && 
		(curjob->isCanceled() ||
		((options&Renderer::Run_PreserveRun)!=0 &&
		curjob->getState()==RenderJob::State_Computing)))	
	{
		lastjob = curjob;
		curjob = curjob->getNextJob();
	}

	// Retrieve last job in render list
	while (lastjob!=NULL && lastjob->getNextJob()!=NULL)
	{
		lastjob = lastjob->getNextJob();
	}
	
	RenderJob *newjob = mRenderJobs.back(); // Currently pushed render job
	RenderJob *begjob = newjob;             // List of new jobs: begin
	
	// Fill list of graphs to link
	newjob->snapshotStates(mStates);

	if (curjob!=NULL && 
		(options&(Renderer::Run_Replace|Renderer::Run_First))!=0 &&
		curjob->cancel(true))      // Proceed only if really canceled (diverge)
	{
		// Non empty render job list
		// And must cancel previous computation
		
		// Notify render loop that cancel operation occur
		mCancelOccur = true;
		
		if ((options&Renderer::Run_PreserveRun)==0)
		{
			// Stop engine if current computation is not preserved
			mEngine.stop();
		}
		
		const bool newfirst = (options&Renderer::Run_First)!=0; 
		std::auto_ptr<OutputsFilter> filter((options&Renderer::Run_Replace)!=0 ? 
			new OutputsFilter(*newjob) : 
			NULL);
			
		// Duplicate job context / accumulation structure
		DuplicateJob dup(
			newjob->getLinkGraphs(),
			filter.get(),
			mStates,
			newfirst);   // Update state if new first
		
		if (!newfirst)
		{
			mRenderJobs.pop_back();       // Insert other jobs before
		}
		
		// Accumulate reversed state -> just before curjob
		RenderJob *srcjob;
		for (srcjob=curjob;srcjob!=NULL;srcjob=srcjob->getNextJob()) 
		{
			srcjob->prependRevertedDelta(dup);
		}
		
		// Duplicate job
		// Iterate on canceled jobs
		RenderJob *prevdupjob = NULL;
		for (srcjob=curjob;srcjob!=NULL;srcjob=srcjob->getNextJob()) 
		{
			// Duplicate canceled
			RenderJob *dupjob = new RenderJob(*srcjob,dup,mRenderCallbacks);
				
			if (dupjob->isEmpty())
			{
				// All outputs filtered
				delete dupjob;
			}
			else 
			{
				// Valid, at least one output
				if (prevdupjob==NULL)
				{
					// First job created, update inputs state
					if (newfirst)
					{	
						// Activate duplicated job w/ current as previous
						dupjob->activate(newjob);
					}
					else
					{
						// Record as first
						begjob = dupjob;
					}   
				}
				else
				{
					check(prevdupjob!=NULL);
					dupjob->activate(prevdupjob);
				}
				
				prevdupjob = dupjob;      // Store prev for next activation
			}	   
		}
		
		// Push new job at the end except if First flag is set
		if (!newfirst)
		{
			// New job at the end of list
			mRenderJobs.push_back(newjob);   // At the end
			
			// Activate
			if (begjob!=newjob)              
			{
				// Not in case that nothing pushed
				check(prevdupjob!=NULL);
				newjob->activate(prevdupjob);  // Activate (last, not first)
			}
			
			check(!dup.hasDelta());         // State must be reverted
		}
	}
		
	// Activate the first job (render thread can view and consume them)
	// Active the first in LAST! Otherwise thread unsafe behavior
	begjob->activate(lastjob);
		
	bool needlaunch = false;
	const bool synchrun = (options&Renderer::Run_Asynchronous)==0;

	{
		// Thread safe modifications	
		Sync::unique_lock slock(mMainMutex);
		if (mCurrentJob==NULL &&                           // No more computation
			begjob->getState()==RenderJob::State_Pending)  // Not already processed
		{
			// Set as current job if no current computations
			mCurrentJob = begjob;
		}
		
		mHold = mHold && !synchrun;
		if (!mHold)
		{
			needlaunch = !wakeupRender();
		}
	}

	if (needlaunch)
	{
		launchRender();
	}

	if (synchrun)
	{
		check(!mHold);

		// If synchronous run
		// Wait to be wakeup by rendering thread (render finished)
		waitRender();
	}
	
	return newjob->getUid();
}


//! @brief Cancel a computation or all computations
//! @param runUid UID of the render job to cancel (returned by run()), set
//!		to 0 to cancel ALL jobs.
//! @return Return true if the job is retrieved (pending)
bool Substance::Details::RendererImpl::cancel(uint32 runUid)
{
	bool hasCancel = false;
	Sync::unique_lock slock(mMainMutex);
	
	if (mCurrentJob!=NULL)
	{
		if (runUid!=0)
		{	
			// Search for job to cancel
			for (RenderJob *rjob=mCurrentJob;rjob!=NULL;rjob=rjob->getNextJob())
			{
				if (runUid==rjob->getUid())
				{
					// Cancel this job
					hasCancel = rjob->cancel();
					break;
				}
			}
		}
		else
		{
			// Cancel all
			hasCancel = mCurrentJob->cancel(true);
		}
		
		if (hasCancel)
		{
			// Notify render loop that cancel operation occur
			mCancelOccur = true;
		
			// Stop engine if necessary
			mEngine.stop();
		}
	}
	
	return hasCancel;
}


void Substance::Details::RendererImpl::clearCache()
{
	Sync::unique_lock slock(mMainMutex);
	mEngine.clearCache();	
}


//! @brief Return if a computation is pending
//! @param runUid UID of the render job to retreive state (returned by run())
bool Substance::Details::RendererImpl::isPending(uint32 runUid) const
{
	SBS_VECTOR_REVERSE_FOREACH (const RenderJob*const renderJob,mRenderJobs)
	{
		if (renderJob->getUid()==runUid)
		{
			return (renderJob->getState()==RenderJob::State_Pending || 
				renderJob->getState()==RenderJob::State_Computing) &&
				!renderJob->isCanceled();
		}
	}
	
	return false;
}


//! @brief Hold rendering
void Substance::Details::RendererImpl::hold()
{
	mHold = true;
	mEngine.stop();
}


//! @brief Continue held rendering
void Substance::Details::RendererImpl::resume()
{
	bool needlaunch = false;

	// Wake up render if really hold
	{
		Sync::unique_lock slock(mMainMutex);
	
		const bool needwakeup = mHold && mCurrentJob!=NULL;

		mHold = false;

		if (needwakeup)
		{
			needlaunch = !wakeupRender();
		}
	}

	// Launch render process
	if (needlaunch)
	{
		check(!mHold);
		check(mCurrentJob != NULL);

		launchRender();
	}
}


//! @brief Flush computation, wait for all render jobs to be complete
void Substance::Details::RendererImpl::flush()
{
	// Resume if hold
	resume();

	check(!mHold);
	
	// Wait for active/pending end
	waitRender();
	
	// Cleanup deprecated jobs
	cleanup();
}


//! @brief Set new memory budget and CPU usage.
//! @param renderOptions New render options to use.
//! @note This function can be called at any time, from any thread.
void Substance::Details::RendererImpl::setOptions(
	const RenderOptions& renderOptions)
{
	mPendingHardRsc = !mEngine.setOptions(renderOptions);
}

//! @brief Set user callbacks
//! @param callbacks Pointer on the user callbacks concrete structure 
//! 	instance or NULL.
//! @warning The callbacks instance must remains valid until all
//!		render job created w/ this callback instance set are processed.
void Substance::Details::RendererImpl::setRenderCallbacks(
	RenderCallbacks* callbacks)
{
	mRenderCallbacks = callbacks;
}


//! @brief Wake up rendering thread if necessary
//! @brief Wake up render thread if currently locked into mCondVarRender
//! @pre mMainMutex Must be locked
//! @note Called from user thread
bool Substance::Details::RendererImpl::wakeupRender()
{
	const bool res = mRenderState!=RenderState_Idle;

	if (mRenderState==RenderState_Wait)
	{
		// Render thread used, resume render loop
		mCondVarRender.notify_one(); 
	}

	return res;
}

//! @brief Call process callback or create up render thread
//! @pre mMainMutex Must be NOT locked, render thread must be currently
//!		idle.
//! @note Called from user thread
void Substance::Details::RendererImpl::launchRender()
{
	check(mRenderState==RenderState_Idle);

	mEngineInitialized = true;

	// Try to use process callback
	if (mRenderCallbacks==NULL || !mRenderCallbacks->runRenderProcess(
		&RendererImpl::renderProcess,
		this))
	{
		// Otherwise create render thread
		mThread = Sync::thread(&renderThread,this);
	}
}


//! @brief Clean consumed render jobs
//! @note Called from user thread
void Substance::Details::RendererImpl::cleanup()
{
	while (!mRenderJobs.empty() &&
		mRenderJobs.front()->getState()==RenderJob::State_Done)
	{
		delete mRenderJobs.front();
		mRenderJobs.pop_front();
	}
}


//! @brief Rendering thread call function
void Substance::Details::RendererImpl::renderThread(RendererImpl* impl)
{
	impl->renderLoop(true);
}


//! @brief Rendering process call function
void Substance::Details::RendererImpl::renderProcess(void* impl)
{
	((RendererImpl*)impl)->renderLoop(false);
}


//! @brief Rendering loop function
//! @param waitAndLoop Starvation behavior flag: if true, wait using 
//!		mCondVarRender for new job to process (or exit), otherwise exit
//!		immediately (revert to State_Idle state).
void Substance::Details::RendererImpl::renderLoop(bool waitAndLoop)
{
	RenderJob* nextJob = NULL;       // Next job to proceed
	bool nextAvailable = false;      // Flag: next job available
	
	while (1)
	{
		// Job processing loop, exit when Renderer is deleted
		{
			// Thread safe operations
			Sync::unique_lock slock(mMainMutex); // mState/mHold safety
			
			if (nextAvailable)
			{
				// Jump to next job
				check(mCurrentJob!=NULL);
				mCurrentJob = nextJob;
				nextAvailable = false;
			}
			
			while (mExitRender || 
				mHold || 
				mCurrentJob==NULL)
			{
				if (mExitRender)
				{
					mEngine.releaseEngine();        // Release engine
					mExitRender = false;            // Exit render taken account
					waitAndLoop = false;            // Exit loop
				}
				
				if (mUserWaiting)
				{
					mCondVarUser.notify_one();      // Wake up user thread
				}
				
				if (waitAndLoop)
				{
					mRenderState = RenderState_Wait;
					mCondVarRender.wait(slock);     // Wait to be wakeup
				}
				else
				{
					mRenderState = RenderState_Idle;
					return;                         // Exit function
				}
			}

			mRenderState = RenderState_OnGoing;
		}

		// Release pending textures
		mEngine.releaseTextures();
		
		// memory limitation for the handle, the RenderOptions object 
		// is created using the values set in the settings
		Substance::RenderOptions renderOptions; 
		
		// Update render options
		mPendingHardRsc = mEngine.setOptions(renderOptions);

		// Process current job
		nextJob = processJob(mCurrentJob);
		nextAvailable = nextJob!=mCurrentJob;
	}
}

//! @brief Process job and next ones
//! @param begjob The first job to process
//! @note Called from render thread
//! @post Job is in 'Done' state if not canceled/hold
//! @warning The job can be deleted by user thread BEFORE returning process().
//! @return Return next job to proceed if the job is fully completed 
//!		(may be NULL) and is possible to jump to next job. Return begjob if
//!		NOT completed.
Substance::Details::RenderJob*
Substance::Details::RendererImpl::processJob(RenderJob *begjob)
{
	// Iterate on jobs to render, find last one or first w/ link collision
	// Check if not strict resume (jobs are already pulled, w\ cancel or addon)
	// Check if link required
	RenderJob::ProcessJobs process(begjob);
	bool strictresume = !mCancelOccur;
	
	// Cancel will be accounted for
	mCancelOccur = false;

	// Enqueue jobs, returns if strict resume
	strictresume = !process.enqueue() && strictresume;
	
	if (process.linkNeeded)
	{
		// Link if necessary
		assert(!strictresume);		
		mEngine.link(process.first,process.last);
	}

	// Compute (only if something linked, test necessary w/ run forced to 
	// update hardware resources)
	if (mEngine.getHandle()!=NULL)
	{
		// Create computation context
		// Flush internal engine queue if push I/O needed
		Computation computation(mEngine,!strictresume);
		
		// If pull is necessary (not strict resume)
		if (!strictresume)
		{
			// Push I/O to engine (not already pushed, until end or collision)
			process.pull(computation);
		}

		// Run if no cancel occurs during push/link
		if (!mCancelOccur) 
		{
			computation.run();
		}
	}
	
	// Mark completed jobs as Done
	bool allcomplete = false;
	RenderJob* curjob = process.first;
	while (!allcomplete && curjob->isComplete())
	{
		RenderJob* nextjob = curjob->getNextJob();
		curjob->setComplete();                // may be destroyed just here!
		allcomplete = curjob==process.last;
		curjob = nextjob;
	}

	return curjob;
}
