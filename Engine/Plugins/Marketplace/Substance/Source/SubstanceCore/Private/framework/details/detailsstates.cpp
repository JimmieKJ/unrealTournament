//! @file detailsstates.cpp
//! @brief Substance Framework All Instances global State impl.
//! @author Christophe Soum - Allegorithmic
//! @date 20111031
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceCorePrivatePCH.h"
#include "SubstanceFGraph.h"

#include "framework/details/detailsstates.h"
#include "framework/details/detailsgraphstate.h"
#include "framework/details/detailslinkgraphs.h"

#include <algorithm>
#include <utility>
#include <set>


//! @brief Destructor, remove pointer on this in recorded instances
Substance::Details::States::~States()
{
	clear();
}


//! @brief Clear states, remove pointer on this in recorded instances
//! @note Called from user thread 
void Substance::Details::States::clear()
{
	for (auto itMap = InstancesMap.begin(); itMap != InstancesMap.end(); ++itMap)
	{
		itMap->second.instance->unplugState(this);
	}
	
	InstancesMap.clear();
}


//! @brief Get the GraphState associated to this graph instance
//! @param graphInstance The source graph instance
//! @note Called from user thread 
//!
//! Create new state from graph instance if necessary (undefined state):
//! record the state into the instance.
Substance::Details::GraphState& 
	Substance::Details::States::operator[](FGraphInstance* graphInstance)
{
	InstancesMap_t::iterator ite = InstancesMap.find(graphInstance->InstanceGuid);
	if (ite==InstancesMap.end())
	{
		Instance inst;
		inst.state.reset(new GraphState(graphInstance));
		inst.instance = graphInstance;
		ite = InstancesMap.insert(
			std::make_pair(graphInstance->InstanceGuid,inst)).first;
		graphInstance->plugState(this);
	}
	
	return *ite->second.state.get();
}


//! @brief Return a graph instance from its UID or NULL if deleted 
//! @note Called from user thread 
Substance::FGraphInstance* 
Substance::Details::States::getInstanceFromUid(const substanceGuid_t& uid) const
{
	InstancesMap_t::const_iterator ite = InstancesMap.find(uid);
	return ite!=InstancesMap.end() ? ite->second.instance : NULL;
}


//! @brief Notify that an instance is deleted
//! @param uid The uid of the deleted graph instance
//! @note Called from user thread 
void Substance::Details::States::notifyDeleted(const substanceGuid_t& uid)
{
	InstancesMap_t::iterator ite = InstancesMap.find(uid);
	check (ite != InstancesMap.end());
	
	InstancesMap.erase(ite);
	
}


//! @brief Fill active graph snapshot: all active graph instance to link
//! @param snapshot The active graphs array to fill
void Substance::Details::States::fill(LinkGraphs& snapshot) const
{
	snapshot.graphStates.reserve(InstancesMap.size());
	std::set<uint32> pushedstates; 

	InstancesMap_t::const_iterator it;
	for (it=InstancesMap.begin(); it!=InstancesMap.end(); it++)
	{
		const InstancesMap_t::value_type &inst = *it;

		if (pushedstates.insert(inst.second.state->getUid()).second)
		{
			snapshot.graphStates.push_back(inst.second.state);
		}
	}
	
	// Sort list of states per State UID
	std::sort(
		snapshot.graphStates.begin(),
		snapshot.graphStates.end(),
		LinkGraphs::OrderPredicate());
}


//! @brief Clear all render token w/ render results from a specific engine
//! @brief Must be called from user thread Not thread safe, can't be 
//!		called if render ongoing.
void Substance::Details::States::releaseRenderResults(uint32 engineUid)
{
	for (auto inst = InstancesMap.begin(); inst != InstancesMap.end(); ++inst)
	{
		for (auto output = inst->second.instance->Outputs.itfront(); output ;  ++output)
		{
			output->releaseTokensOwnedByEngine(engineUid);
		}
	}
}

