//! @file detailsdeltastate.h
//! @brief Substance Framework Instance Inputs Delta State definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111026
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSDELTASTATE_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSDELTASTATE_H

#include "detailsinputstate.h"
#include "detailsoutputstate.h"

#include "SubstanceFGraph.h"

#include <vector> 

namespace Substance
{
namespace Details
{

class GraphState;


//! @brief Delta of graph instance inputs and output state 
//! Represents a modification of a graph state: new and previous values of all
//! modified inputs and output format override
class DeltaState
{
public:
	//! @brief One I/O modification
	//! IOState is InputState or OutputState
	template <class IOState>
	struct Item
	{
		size_t index;      //!< Index in GraphState/GraphInstance order
		IOState previous;  //!< Previous state
		IOState modified;  //!< New state
		
	};  // struct Item

	typedef Item<InputState> Input;          //!< One input modification
	typedef std::vector<Input> Inputs;       //!< Vector of input states delta
	typedef Item<OutputState> Output;        //!< One output modification
	typedef std::vector<Output> Outputs;     //!< Vector of output states delta
	
	//! @brief Append modes
	enum AppendMode
	{
		Append_Default  = 0x0,    //!< Merge, use source modified values
		Append_Override = 0x1,    //!< Override previous values
		Append_Reverse  = 0x2     //!< Use source previous values
	};  // enum AppendMode
	
	
	//! @brief Accessor on inputs
	const Inputs& getInputs() const { return mInputs; }

	//! @brief Accessor on outputs
	const Outputs& getOutputs() const { return mOutputs; }

	//! @brief Returns if null delta
	bool isIdentity() const { return mInputs.empty() && mOutputs.empty(); }

	//! @brief Accessor on pointers on ImageInput indexed by mInputs
	const ImageInputPtr& getImageInput(size_t i) const { return mImageInputPtrs.at(i); }
	
	//! @brief Fill a delta state from current state & current instance values
	//! @param graphState The current graph state used to generate delta
	//! @param graphInstance The pushed graph instance (not keeped)
	void fill(
		const GraphState &graphState,
		const FGraphInstance* graphInstance);
		
	//! @brief Append a delta state to current
	//! @param src Source delta state to append
	//! @param mode Append policy flag
	void append(const DeltaState &src,AppendMode mode);

protected:
	//! @brief Order or inputs
	struct InputOrder 
	{ 
		bool operator()(const Input &a,const Input &b) const 
		{ 
			return a.index<b.index;
		} 
	};  // struct InputOrder

	//! @brief New and previous values of all modified inputs
	//! Input::index ordered
	Inputs mInputs;

	//! @brief Array of pointers on ImageInput indexed by mInputs
	ImageInputPtrs mImageInputPtrs;

	//! @brief New and previous values of all modified outputs
	//! Input::index ordered
	Outputs mOutputs;

	//! @brief Append a delta of I/O sub method
	//! @param destItems Destination inputs or outputs (from this)
	//! @param srcItems Source inputs or outputs (from src)
	//! @param src Source delta state to append
	//! @param mode Append policy flag
	template <typename Items>
	void append(
		Items& destItems,
		const Items& srcItems,
		const DeltaState &src,
		AppendMode mode);

	//! @brief Returns if states are equivalent
	//! Two implementations for input and output
	template <typename StateType>
	bool isEqual(
		const StateType& a,
		const StateType& b,
		const DeltaState &bparent) const;
	
	//! @brief Record child state
	//! @param[in,out] inputState The state to record image pointer
	//! @param parent Source delta state parent of inputState that contains
	//! 	image pointers.
	//! For inputs: record new image pointer if necessary, 
	//! do nothing for outputs.
	template <typename StateType>
	void record(
		StateType& inputState,
		const DeltaState &parent);



};  // class DeltaState

} // namespace Details
} // namespace Substance

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSDELTASTATE_H
