//! @file SubstanceCallbacks.h
//! @brief Substance helper function declaration
//! @date 20120120
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCEFRAMEWORK_CALLBACKS_H
#define _SUBSTANCEFRAMEWORK_CALLBACKS_H

#include "framework/callbacks.h"

#include "framework/details/detailssync.h"

namespace Substance
{

struct FOutputInstance;
struct FGraphInstance;

struct RenderCallbacks : public Substance::Callbacks
{
public:
	//! @brief Output computed callback (render result available) OVERLOAD
	//! Overload of Substance::Callbacks::outputComputed
	void outputComputed(
		uint32 Uid,
		const Substance::FGraphInstance*,
		Substance::FOutputInstance*);

	static Substance::List<output_inst_t*> getComputedOutputs(bool throttleOutputGrab=true);

	static void clearComputedOutputs(output_inst_t*);

	bool runRenderProcess(
		Substance::RenderFunction renderFunction,
		void* renderParams);

	static bool isOutputQueueEmpty()
	{
		return mOutputQueue.size() == 0;
	}

protected:
	static Substance::List<output_inst_t*> mOutputQueue;
	static Substance::Details::Sync::mutex mMutex;
};

} // namespace Substance

#endif //_SUBSTANCEFRAMEWORK_CALLBACKS_H
