//! @file detailsduplicatejob.h
//! @brief Substance Framework Render Job duplication context structure def.
//! @author Christophe Soum - Allegorithmic
//! @date 20111110
//! @copyright Allegorithmic. All rights reserved.
//!
 
#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSDUPLICATEJOB_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSDUPLICATEJOB_H

#include "detailsdeltastate.h"

#include <map> 


namespace Substance
{
namespace Details
{

struct OutputsFilter;

//! @brief Render Job duplication context
struct DuplicateJob
{
	//! @brief Constructor
	//! @param linkGraphs_ Snapshot on graphs states to use at link time 
	//! @param filter_ Used to filter outputs (outputs replace) or NULL if none.
	//! @param states_ Used to retrieve graph instances
	//! @param newFirst_ True if new job must be run before canceled ones
	DuplicateJob(
		const LinkGraphs& linkGraphs_,
		const OutputsFilter* filter_,
		const States &states_,
		bool newFirst_);
	
	//! @param Snapshot on graphs states to use at link time 
	const LinkGraphs& linkGraphs;
	
	//! @param Used to filter outputs (outputs replace) or NULL if none.
	const OutputsFilter*const filter;
	
	//! @param Used to retrieve graph instances
	const States &states;
	
	
	//! @brief Append a pruned instance delta state
	//! @param instanceUid The GraphInstance UID corr. to delta state
	//! @param deltaState The delta state to append (values overriden, delete
	//!		input entry if return to identity)
	void append(const substanceGuid_t& instanceUid,const DeltaState& deltaState);
	
	//! @brief Prepend a canceled instance delta state
	//! @param instanceUid The GraphInstance UID corr. to delta state
	//! @param deltaState The delta state to prepend (reverse values merged)
	void prepend(const substanceGuid_t& instanceUid,const DeltaState& deltaState);
	
	//! @brief Fix instance input values w/ accumulated delta state
	//! @param instanceUid The GraphInstance UID corr. to delta state
	//! @param[in,out] deltaState The delta state to fix (merge)
	void fix(const substanceGuid_t& instanceUid,DeltaState& deltaState);
	
	//! @brief Return true if  accumulated input delta
	bool hasDelta() const { return !mDeltaStates.empty(); }
	
	//! @brief Return true if current state must be updated by duplicated jobs
	bool needUpdateState() const { return newFirst; }
	
protected:

	//! @brief Delta state per instance UID container
	typedef std::map<substanceGuid_t,DeltaState, guid_t_comp> DeltaStates;
	
	//! @brief Pruned inputs list per instance UID
	DeltaStates mDeltaStates;
	
	//! @param True if new job must be run before canceled ones
	const bool newFirst;

};  // struct DuplicateJob


} // namespace Details
} // namespace Substance

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSDUPLICATEJOB_H
