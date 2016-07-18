//! @file detailsrendererimpl.h
//! @brief The Substance renderer concrete implementation class
//! @author Christophe Soum - Allegorithmic
//! @date 20111102
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSRENDERERIMPL_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSRENDERERIMPL_H

#include "detailsstates.h"
#include "detailsengine.h"
#include "detailssync.h"

#include "SubstanceCallbacks.h"
#include "SubstanceFGraph.h"
#include "Engine.h"

#include <deque>

namespace Substance
{
namespace Details
{

//! @brief Concrete renderer implementation
class RendererImpl
{
public:
	//! @brief Default constructor
	//! @param renderOptions Initial render options.
	RendererImpl(const RenderOptions& renderOptions);
	
	//! @brief Destructor
	~RendererImpl();
	
	//! @brief Push graph instance current changes to render
	//! @param graphInstance The instance to push dirty outputs
	//! @return Return true if at least one dirty output
	bool push(FGraphInstance* graphInstance);
	
	//! @brief Launch computation
	//! @param options Renderer::RunOption flags combination
	//! @return Return UID of render job or 0 if not pushed computation to run
	uint32 run(unsigned int options);
	
	//! @brief Cancel a computation or all computations
	//! @param runUid UID of the render job to cancel (returned by run()), set
	//!		to 0 to cancel ALL jobs.
	//! @return Return true if the job is retrieved (pending)
	bool cancel(uint32 runUid = 0);

	//! @brief Clear the substance cache
	void clearCache();
	
	//! @brief Return if a computation is pending
	//! @param runUid UID of the render job to retrieve state (returned by run())
	bool isPending(uint32 runUid) const;
	
	//! @brief Hold rendering
	void hold();
	
	//! @brief Continue held rendering
	void resume();
	
	//! @brief Flush computation, wait for all render jobs to be complete
	void flush();

	//! @brief Set new memory budget and CPU usage.
	//! @param renderOptions New render options to use.
	//! @note This function can be called at any time, from any thread.
	void setOptions(const RenderOptions& renderOptions);

	//! @brief Set user render callbacks
	//! @param callbacks Pointer on the user callbacks concrete structure 
	//! 	instance or NULL.
	//! @warning The callbacks instance must remains valid until all
	//!		render job created w/ this callback instance set are processed.
	void setRenderCallbacks(RenderCallbacks* callbacks);

protected:
	//! @brief Render render process states enumeration
	enum RenderState
	{
		RenderState_Idle,      //!< No render thread or process
		RenderState_Wait,      //!< Thread waiting (blocked in mCondVarRender)
		RenderState_OnGoing    //!< Processing/ready to process
	};

	//! @brief Render jobs list container
	typedef std::deque<RenderJob*> RenderJobs;
	
	//! @brief Current graphs state
	States mStates;
	
	//! @brief Engine
	Engine mEngine;
	
	//! @brief Render jobs list
	//! @note Container modification are always done in user thread
	RenderJobs mRenderJobs;
	
	//! @brief First render job to proceed by render thread
	//! R/W access thread safety ensure by mMainMutex.
	//! If NULL Render thread is waiting for pending render job (rendering 
	//! thread is blocked in mCondVarRender).
	RenderJob *volatile mCurrentJob;
	
	//! @brief Current/required render process state
	//! R/W access thread safety ensure by mMainMutex.
	volatile RenderState mRenderState;
	
	//! @brief Currently hold
	volatile bool mHold;
	
	//! @brief Cancel action occur
	//! Set from user thread (cancel action), unset when taken into account
	//!	by render loop (render thread).
	volatile bool mCancelOccur;
	
	//! @brief Pending engine hard resources change
	//! Can be set from any thread. Unset from render or user thread.
	volatile bool mPendingHardRsc;

	//! @brief Exit render and engine release required by user
	//! Set from user thread (destroy or switch engine), unset when taken into
	//!	account by render loop (render thread).
	volatile bool mExitRender;

	//! @brief User thread waiting for render completion flag
	//! Set and unset from user thread when blocked in mCondVarUser
	volatile bool mUserWaiting;

	//! @brief Engine is initialized flag
	//! User thread usage only.
	bool mEngineInitialized;
	
	//! @brief Condition variable used by render thread for waiting
	Sync::condition_variable mCondVarRender;
	
	//! @brief Condition variable used by user thread for waiting
	Sync::condition_variable mCondVarUser;

	//! @brief Mutex for mCurrentJob R/W access
	Sync::mutex mMainMutex;
	
	//! @brief Rendering thread, used if no rendering process callback
	Sync::thread mThread;
	
	//! @brief Current User render callbacks instance (can be NULL, none)
	RenderCallbacks* mRenderCallbacks;

	//! @brief Current Render job UID
	uint32 mRenderJobUid;
		
	//! @brief Call process callback or create up render thread
	//! @pre mMainMutex Must be NOT locked, render thread must be currently
	//!		idle.
	//! @note Called from user thread
	void launchRender();

	//! @brief Wake up render thread if currently locked into mCondVarRender
	//! @pre mMainMutex Must be locked
	//! @note Called from user thread
	//! @brief Return false if wakeup failed: the render thread is idle and
	//!		a launch is necessary
	bool wakeupRender();
	
	//! @brief Clean consumed render jobs
	//! @note Called from user thread
	void cleanup();
	
	//! @brief Release engine and terminates render thread
	//! If no thread created and engine need to be released, call
	//! wakeupRender before thread termination.
	//! @pre mMainMutex Must be NOT locked
	//! @note Called from user thread
	void exitRender();

	//! @brief Wait for render thread exit/sleep if currently ongoing
	//! @pre mMainMutex Must be NOT locked
	//! @note Called from user thread
	void waitRender();
	
	//! @brief Rendering thread call function
	static void renderThread(RendererImpl*);

	//! @brief Render process call function
	static void renderProcess(void*);
	
	//! @brief Rendering loop function
	//! @param waitAndLoop Starvation behavior flag: if true, wait using 
	//!		mCondVarRender for new job to process (or exit), otherwise exit
	//!		immediately (revert to State_Idle state).
	void renderLoop(bool waitAndLoop);
	
	//! @brief Process job and next ones
	//! @param begjob The first job to process
	//! @note Called from render thread
	//! @post Job is in 'Done' state if not canceled/hold
	//! @warning The job can be deleted by user thread BEFORE returning process().
	//! @return Return next job to proceed if the job is fully completed 
	//!		(may be NULL) and is possible to jump to next job. Return begjob if
	//!		NOT completed.
	RenderJob* processJob(RenderJob *begjob);
};


} // namespace Details
} // namespace Substance

#endif // _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSRENDERERIMPL_H
