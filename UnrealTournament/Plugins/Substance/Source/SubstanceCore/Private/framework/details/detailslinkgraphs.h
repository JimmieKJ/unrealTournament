//! @file detailslinkgraphs.h
//! @brief Substance Framework global link time snapshot State definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111107
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSLINKGRAPHS_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSLINKGRAPHS_H

#include "framework/details/detailsgraphstate.h"

#include <vector>
#include <utility>

namespace Substance
{
namespace Details
{

//! @brief All valid graph states snapshot, used at link time
//! Represent the state of all valid Graph states. 
struct LinkGraphs
{
	//! @brief Pointer on graph state
	typedef std::shared_ptr<GraphState> GraphStatePtr;
	
	//! @brief Vector of graph states, ordered by GraphState::mUid
	typedef std::vector<GraphStatePtr> GraphStates;
	
	//! @brief Predicate used for sorting/union
	struct OrderPredicate
	{
		bool operator()(const GraphStatePtr& a,const GraphStatePtr& b) const;
	};  // struct OrderPredicate
	
	//! @brief Graph states pointer, ordered by GraphState::mUid
	GraphStates graphStates;
	
	//! @brief Merge another link graph w\ redundancies
	void merge(const LinkGraphs& src);
	
}; // class LinkGraphs

} // namespace Details
} // namespace SubstanceAir

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSLINKGRAPHS_H
