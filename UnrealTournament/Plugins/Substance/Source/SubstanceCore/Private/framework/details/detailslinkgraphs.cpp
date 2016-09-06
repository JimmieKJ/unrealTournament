//! @file detailslinkgraphs.cpp
//! @brief Substance Framework global link time snapshot State implementation
//! @author Christophe Soum - Allegorithmic
//! @date 20111115
//! @copyright Allegorithmic. All rights reserved.
//!

#include "SubstanceCorePrivatePCH.h"

#include "framework/details/detailslinkgraphs.h"
#include "framework/details/detailsgraphstate.h"

#include <algorithm>
#include <iterator>


//! @brief Predicate used for sorting/union
bool Substance::Details::LinkGraphs::OrderPredicate::operator()(
	const GraphStatePtr& a,
	const GraphStatePtr& b) const
{
	return a->getUid()<b->getUid();
}


//! @brief Merge another link graph w\ redondancies
void Substance::Details::LinkGraphs::merge(const LinkGraphs& src)
{
	if (graphStates.empty())
	{
		// Simple copy if current empty
		graphStates = src.graphStates;
	}
	else
	{
		// Merge sorted lists in one sorted 
		const LinkGraphs::GraphStates prevgraphstates = graphStates;
		graphStates.resize(0);
		std::set_union(
			src.graphStates.begin(),
			src.graphStates.end(),
			prevgraphstates.begin(),
			prevgraphstates.end(),
			std::back_inserter(graphStates),
			OrderPredicate());
	}
}

