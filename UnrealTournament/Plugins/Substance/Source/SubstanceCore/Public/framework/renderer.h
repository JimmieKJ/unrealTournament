//! @file renderer.h
//! @brief The Substance renderer class, used to render instances
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_FRAMEWORK_RENDERER_H
#define _SUBSTANCE_FRAMEWORK_RENDERER_H

#include "SubstanceFGraph.h"
#include "SubstanceCallbacks.h"
#include "renderopt.h"

#include <map>

namespace Substance
{

namespace Details
{
	class RendererImpl;
}

//! @brief Class used to render graph instances
class Renderer
{
public:
	//! @brief Run options enumeration used as argument by run() method
	//! Options can be combined. Run_PreserveRun is only useful w/ Run_Replace
	//! and/or Run_First flags (useful w/ burst run() calls).
	enum RunOption
	{
		Run_Default      = 0,  //!< Synchronous, preserve previous push
		Run_Asynchronous = 1,  //!< Asynchronous computation 
		Run_Replace      = 2,  //!< Discard computation on deprecated outputs
		Run_First        = 4,  //!< Run w/ maximum priority on other computation
		Run_PreserveRun  = 8   //!< In any case, preserve currently running job 
	};

	//! @brief Default constructor
	//! @param renderOptions Optional render options. Allows to set initial
	//!		memory consumption budget.
	Renderer(const RenderOptions& renderOptions = RenderOptions());

	//! @brief Destructor
	~Renderer();

	//! @brief Push graph instance current changes to render
	//! The current GraphInstance state is used to enqueue a render job: input 
	//!	values and flags, output flags.
	//!
	//! Only the outputs of the graph flagged as dirty (outdated) are rendered
	//!	(use OutputInstance::flagAsDirty() to force rendering).<BR>
	//! When a GraphInstance is pushed for the first time, all outputs are
	//! considered dirty (and therefore are computed later).
	//! @note Note that the rendering process is not started by this command but
	//! with the run() method.
	bool push(FGraphInstance*);

	//! @brief Helper: Push graph instance arrays to render
	bool push(Substance::List<Substance::FGraphInstance*>&);

	//! @brief Launch synchronous/asynchronous computation
	//! @param runOptions Combination of RunOption flags
	//! @post If synchronous mode selected (default): Render results are 
	//!		available in output instances (OutputInstance::grabResult()).
	//! @return Return UID of render job or 0 if not pushed computation to run.
	//!
	//! Render previously pushed GraphInstance's.
	//! Returns after computation end in synchronous mode, otherwise
	//! returns immediatly.
	int32 run(uint32 runOptions = Run_Default);
	
	//! @brief Cancel a computation
	//! @param runUid UID of the computation to cancel (returned by run())
	//! @return Return true if the job is retrieved (pending)
	bool cancel(uint32 runUid);
	
	//! @brief Cancel all active/pending computations
	void cancelAll();

	//! @brief Clear the substance cache
	void clearCache();

	//! @brief Flush computation, wait for all render jobs to be complete
	void flush();
	
	//! @brief Return if a computation is pending
	//! @param runUid UID of the render job to retrieve state (returned by run())
	bool isPending(uint32 runUid) const;
	
	//! @brief Hold rendering
	void hold();
	
	//! @brief Continue held rendering
	void resume();
	
	//! @brief Set per-renderer user callbacks
	//! @param callbacks Pointer on the user callbacks concrete structure 
	//! 	instance that will be used for this renderer instance callbacks
	//! 	calls. Can be NULL pointer: callbacks no more used.
	//! @warning A set per-renderer callback instance is used by all computation
	//! 	created by run() calls, even if another callback instance is set
	//!		just after. Then take care of RenderCallbacks instance lifetime
	//!		(to avoid issues, should be deleted AFTER Renderer instance).
	void setRenderCallbacks(RenderCallbacks* callbacks);

protected:
	Details::RendererImpl* mRendererImpl;
};

} // namespace Substance

#endif // _SUBSTANCE_FRAMEWORK_RENDERER_H
