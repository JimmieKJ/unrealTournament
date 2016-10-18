//! @file detailsrenderjob.h
//! @brief Substance Framework Render Job definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111027
//! @copyright Allegorithmic. All rights reserved.
//!
 
#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSRENDERJOB_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSRENDERJOB_H

#include "detailslinkgraphs.h"
#include "detailsstates.h"

#include <vector>
#include <map>

namespace Substance
{

struct FGraphInstance;
struct RenderCallbacks;

namespace Details
{

class Computation;
struct DuplicateJob;
class Engine;
struct OutputsFilter;
class RenderPushIO;

//! @brief Render job: set of push Input/Output to render
//! One render job corresponds to all Renderer::push beetween two run 
class RenderJob
{
public:	

	//! @brief Process jobs enqueue context structure
	struct ProcessJobs
	{
		RenderJob *first;    //!< Jobs list begin
		RenderJob *last;     //!< Jobs list last element
		bool linkNeeded;     //!< Link process is needed before run

		//! @brief Constructor from first job to process
		ProcessJobs(RenderJob* f) : 
			first(f),
			last(f),
			linkNeeded(false)
		{
		}

		//! @brief Enqueue jobs from first one
		//! @post Set last (determine jobs range) & linkNeeded member.
		//! @return Return false if strict resume of previous process
		bool enqueue();

		//! @brief Pull jobs to computation
		void pull(Computation &computation) const;
	};



	//! @brief States enumeration
	enum State
	{
		State_Setup,     //!< Job currently built, initial state
		State_Pending,   //!< Not already processed by engine (to run)
		State_Computing, //!< Pushed into engine render list, not finished
		State_Done       //!< Completely processed, to destroy in user thread
	};
	
	//! @brief Constructor
	//! @param callbacks User callbacks instance (or NULL if none)
	RenderJob(uint32 uid,RenderCallbacks *callbacks);
	
	//! @brief Constructor from job to duplicate and outputs filtering
	//! @param src The canceled job to copy
	//! @param dup Duplicate job context
	//! @param callbacks User callbacks instance (or NULL if none)
	//!
	//! Build a pending job from a canceled one and optionnaly pointer on an
	//! other job used to filter outputs
	//! Push again SRC render tokens (of not filtered outputs).
	//! @warning Resulting job can be empty if all outputs are filtered. In this
	//!		case this job is no more usefull and can be removed.
	//! @note Called from user thread
	RenderJob(const RenderJob& src,DuplicateJob& dup,RenderCallbacks *callbacks);

	//! @brief Destructor
	~RenderJob();
	
	//! @brief Take states snapshot (used by linker)
	//! @param states Used to take a snapshot states to use at link time
	//!
	//! Must be done before activate it
	void snapshotStates(const States &states);

	//! @brief Push I/O to render: from current state & current instance
	//! @param graphState The current graph state
	//! @param graphInstance The pushed graph instance (not keeped)
	//! @pre Job must be in 'Setup' state
	//! @note Called from user thread 
	//! @return Return true if at least one dirty output
	//!
	//! Update states, create render tokens.
	bool push(GraphState &graphState, FGraphInstance* graphInstance);
	
	//! @brief Put the job in render state, build the render chained list
	//! @param previous Previous job in render list or NULL if first
	//! @pre Activation must be done in reverse order, this next job is already
	//!		activated.
	//! @pre Job must be in 'Setup' state
	//! @post Job is in 'Pending' state
	//! @note Called from user thread
	void activate(RenderJob* previous);
	
	//! @brief Cancel this job (or this job and next ones)
	//! @param cancelList If true this job and next ones (mNextJob chained list)
	//! 	are canceled (in reverse order).
	//! @note Called from user thread
	//! @return Return true if at least one job is effectively canceled
	//!
	//! Cancel if Pending or Computing.
	//! Outputs of canceled jobs are not computed, only inputs are set (state
	//! coherency). RenderToken's are notified as canceled.
	bool cancel(bool cancelList = false);
	
	//! @brief Prepend reverted input delta into duplicate job context
	//! @param dup The duplicate job context to accumulate reversed delta
	//! Use to restore the previous state of copied jobs.
	//! @note Called from user thread
	void prependRevertedDelta(DuplicateJob& dup) const;
	
	//! @brief Fill filter outputs structure from this job
	//! @param[in,out] filter The filter outputs structure to fill
	void fill(OutputsFilter& filter) const;
	
	//! @brief Get render job UID
	uint32 getUid() const { return mUid; }
	
	//! @brief Return the current job state
	State getState() const { return mState; }
	
	//! @brief Return if the current job is canceled
	bool isCanceled() const { return mCanceled; }

	//! @brief Accessor on next job to process
	//! @note Called from render thread 
	RenderJob* getNextJob() const { return mNextJob; }

	//! @brief Accessor on engine pointer filled when job pulled
	Engine* getEngine() const { return mEngine; }
	
	//! @brief Return if the job is empty (no pushed RenderPushIOs)
	bool isEmpty() const { return mRenderPushIOs.empty(); }
	
	//! @brief Accessor on link graphs (Read only)
	const LinkGraphs& getLinkGraphs() const { return mLinkGraphs; }
	
	//! @brief Accessor on User callbacks instance (can be NULL if none)
	RenderCallbacks* getCallbacks() const { return mCallbacks; }
	
	//! @brief Mark as complete
	//! Called from render thread.
	//! @warning When called, can be immediatly destroyed by user thread
	//!		(complete jobs cleanup)
	void setComplete() { mState = State_Done; }
	
	//! @brief Push input and output in engine handle
	void pull(Computation &computation);
	
	//! @brief Is render job complete
	//! @return Return if all push I/O completed
	bool isComplete() const;
	
	//! @brief Enqueue current job to current process if possible 
	//! @param processJobs Updated in function of job requirements. Set last
	//!		member to this if this job can be processed.
	//! @post Flatten output state changes in binary graph.
	//! @return Return if next job should be processed (always returns false
	//! 	if next job is NULL).
	bool enqueueProcess(ProcessJobs& processJobs);

	//! @brief Returns if this job has in process not computed push I/O
	bool needResume() const;
	
protected:

	//! @brief Vector of push I/O
	typedef std::vector<RenderPushIO*> RenderPushIOs;
	
	//! @brief Map of UIDs -> count
	typedef std::map<uint32,uint32> UidsMap;
	
	//! @brief Render job UID
	const uint32 mUid;
	
	//! @brief Current state
	volatile State mState;

	//! @brief Canceled by user, push only Inputs
	volatile bool mCanceled;
	
	//! @brief Pointer on next job to process
	//! Used by render thread to avoid container thread safety stuff
	RenderJob*volatile mNextJob;
	
	//! @brief Vector of push I/O (this instance ownership)
	//! In sequential engine push order
	RenderPushIOs mRenderPushIOs;
	
	//! @brief Per graph states UID usage count (used to determinate seq. index)
	UidsMap mStateUsageCount;
	
	//! @brief All graph states valid at render job creation to use at link time
	//! Allows to keep ownership on datas required at link time
	LinkGraphs mLinkGraphs;
	
	//! @brief User callbacks instance (can be NULL if none)
	RenderCallbacks* mCallbacks;

	//! @brief Engine used for computation, filled when render job pulled
	Engine* mEngine;


	//! @brief Returns iterator on first non complete Push I/O
	//! Returns push I/O end if fully complete.
	RenderPushIOs::const_iterator firstNonComplete() const;
		
private:
	RenderJob(const RenderJob&);
	const RenderJob& operator=(const RenderJob&);	
};  // class RenderJob


} // namespace Details
} // namespace Substance

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSRENDERJOB_H
