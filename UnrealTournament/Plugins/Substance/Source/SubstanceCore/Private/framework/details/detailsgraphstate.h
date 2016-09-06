//! @file detailsgraphstate.h
//! @brief Substance Framework Graph Instance State definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111026
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSGRAPHSTATE_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSGRAPHSTATE_H

#include "detailsinputstate.h"
#include "detailsgraphbinary.h"

#include <deque>
#include <vector>
#include <set>

namespace Substance
{
namespace Details
{

class DeltaState;
class LinkData;

//! @brief Graph Instance State
//! Represent the state of one or several Graph instance. 
//! This is the state of input values w/ all render job PUSHED. 
//!
//! Also contains a pointer on associated GraphBinary that contains UID/index
//! translation of linked SBSBIN.
class GraphState
{
public:
	//! @brief Pointer on LinkData type
	typedef std::shared_ptr<LinkData> LinkDataPtr;
	
	//! Create from graph instance
	//! @param graphInstance The source graph instance, initialize state
	//! @note Called from user thread 
	GraphState(FGraphInstance* graphInstance);
	
	//! @brief Destructor
	~GraphState();
	
	//! @brief Accessor on input state
	const InputState& operator[](size_t i) const { return mInputStates.at(i); }
	
	//! @brief Accessor on pointers on ImageInput indexed by input states
	const ImageInputPtr& getInputImage(size_t i) const { return mInputImagePtrs.at(i); }
	
	//! @brief Apply delta state to current state
	void apply(const DeltaState& deltaState);
	
	//! @brief This state is currently linked (present in current SBSBIN data)
	bool isLinked() const;
	
	//! @brief Accessor on Graph state uid
	uint32 getUid() const { return mUid; }
	
	//! @brief Accessor on current GraphBinary
	GraphBinary& getBinary() const { return *mGraphBinary; }
	
	//! @brief Accessor on Pointer to link data corresponding to graph instance
	const LinkDataPtr& getLinkData() const { return mLinkData; }
	
protected:
	//! @brief Vector of InputState
	typedef std::vector<InputState> InputStates;
	
	//! @brief Graph state uid
	const uint32 mUid;
	
	//! @brief Pointer link data corresponding to graph instance
	//! Allows to keep ownership on data required at link time
	const LinkDataPtr mLinkData;
	
	//! @brief Array of input states in GraphInstance/GraphDesc order
	InputStates mInputStates;

	////! @brief Array of pointers on ImageInput indexed by mInputStates
	ImageInputPtrs mInputImagePtrs;
	
	//! @brief Index of the first NULL pointer in mInputImagePtrs
	size_t mImageInputFirstNullPtr;
	
	//! @brief Graph binary associated with this instance of GraphState.
	//! Owned by GraphState.
	GraphBinary* mGraphBinary;
	
	//! @brief Store new image pointer
	//! @return Return image pointer index in mInputImagePtrs array
	size_t store(const ImageInputPtr& imgPtr);
	
	//! @brief Remove image pointer
	//! @param Index of the image pointer to remove
	void remove(size_t index);

private:
	GraphState(const GraphState&);
	const GraphState& operator=(const GraphState&);	

};  // class GraphState


} // namespace Details
} // namespace Substance

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSGRAPHSTATE_H
