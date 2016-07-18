//! @file callbacks.h
//! @brief Substance callbacks structure definition
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_FRAMEWORK_CALLBACKS_H
#define _SUBSTANCE_FRAMEWORK_CALLBACKS_H

#include "SubstanceCoreTypedefs.h"

namespace Substance
{

	//! @brief Pointer to function used as render function definition
	typedef void(*RenderFunction)(void *);

//! @brief Abstract base structure of user callbacks
//! Inherit from this base struct and overload the virtual methods to be 
//! notified by framework callbacks.
//!
//! Use concrete callbacks instance in Renderer::setCallbacks().
//! @warning Callback methods can be called from a different thread, not 
//!		necessary the user thread (that call Renderer::run()).
struct Callbacks
{
	//! @brief Destructor
	virtual ~Callbacks() {}

	//! @brief Output computed callback (render result available)
	//! @param runUid The UID of the corresponding computation (returned by 
	//! 	Renderer::run()).
	//! @brief graphInstance Pointer on output parent graph instance
	//! @param outputInstance Pointer on computed output instance w/ new render
	//!		result just available (use OutputInstance::grabResult() to grab it).
	//! @note This callback must be implemented in concrete Callbacks structures.
	//!
	//! Called each time an new render result is just available.
	virtual void outputComputed(
		uint32 runUid,
		const FGraphInstance* graphInstance,
		FOutputInstance* outputInstance) = 0;

	//! @brief Render process execution callback
	//! @param renderFunction Pointer to function to call.
	//! @param renderParams Parameters of the function.
	//! @return Return true if render process execution is handled. If
	//!		returns false once, the renderer uses internal thread until
	//!		eventual engine switch.
	//!
	//! Allows custom thread allocation for rendering process. If handled,
	//! must call renderFunction w/ renderParams. 
	//!	This function returns when all pending render jobs are processed.
	//! Called each time a batch a jobs need to be rendered AND at renderer
	//! deletion (guarantee thread homogeneity for GPU Substance Engine
	//!	versions).
	//!
	//! Default implementation returns false: use internal thread.
	virtual bool runRenderProcess(
		RenderFunction renderFunction,
		void* renderParams) = 0;
};

} // namespace Substance

#endif //_SUBSTANCE_FRAMEWORK_CALLBACKS_H
