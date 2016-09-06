//! @file SubstanceCallbacks.h
//! @brief Substance render callbacks implementation
//! @date 20120120
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceCallbacks.h"
#include "SubstanceTexture2D.h"
#include "SubstanceFGraph.h"
#include "SubstanceFOutput.h"

Substance::List<output_inst_t*> Substance::RenderCallbacks::mOutputQueue;
Substance::Details::Sync::mutex Substance::RenderCallbacks::mMutex;


void Substance::RenderCallbacks::outputComputed(
	uint32 Uid,
	const Substance::FGraphInstance* graph,
	Substance::FOutputInstance* output)
{
	Substance::Details::Sync::unique_lock slock(mMutex);
	mOutputQueue.AddUnique(output);
}


Substance::List<Substance::FOutputInstance*> Substance::RenderCallbacks::getComputedOutputs(bool throttleOutputGrab)
{
	Substance::Details::Sync::unique_lock slock(mMutex);
	Substance::List<Substance::FOutputInstance*> outputs;
	
	if (mOutputQueue.size())
	{
		if (throttleOutputGrab)
		{
			outputs.push(mOutputQueue.pop());
		}
		else
		{
			outputs = mOutputQueue;
			mOutputQueue.Empty();
		}
	}
	 
	return outputs;
}


bool Substance::RenderCallbacks::runRenderProcess(RenderFunction renderFunction, void* renderParams)
{		
	return false;
}


void Substance::RenderCallbacks::clearComputedOutputs(output_inst_t* Output)
{
	Substance::Details::Sync::unique_lock slock(mMutex);
	mOutputQueue.Remove(Output);
}
