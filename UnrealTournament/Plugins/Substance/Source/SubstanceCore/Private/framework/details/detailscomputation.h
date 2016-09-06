//! @file detailscomputation.h
//! @brief Substance Framework Computation action definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111109
//! @copyright Allegorithmic. All rights reserved.
//!

#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSCOMPUTATION_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSCOMPUTATION_H

#include "SubstanceCorePrivatePCH.h"
#include "substance_public.h"

#include <vector>


namespace Substance
{
namespace Details
{

class Engine;
struct InputState;
struct ImageInputToken;

//! @brief Substance Engine Computation (render) action
class Computation
{
public:
	//! @brief Container of indices
	typedef std::vector<uint32> Indices;
	
	//! @brief Constructor from engine
	//! @param flush Flush internal engine queue if true
	//! @pre Engine is valid and correctly linked
	//! @post Engine render queue is flushed if 'flush' is true
	Computation(Engine& engine,bool flush);
	
	//! @brief Set current user data to push w/ I/O
	void setUserData(size_t userData) { mUserData = userData; }
	
	//! @brief Get current user data to push w/ I/O
	size_t getUserData() const { return mUserData; }
	
	//! @brief Push input
	//! @param index SBSBIN index
	//! @param inputState Input type, value and other flags
	//! @param imgToken Image token pointer, used if input image
	//! @return Return if input really pushed
	bool pushInput(
		uint32 index,
		const InputState& inputState,
		ImageInputToken* imgToken);
	
	//! @brief Push outputs SBSBIN indices to compute 
	void pushOutputs(const Indices& indices);
	
	//! @brief Run computation
	//! Push hints I/O and run
	void run();
	
	//! @brief Accessor on current engine
	Engine& getEngine() { return mEngine; }

protected:

	//! @brief Reference on parent Engine
	Engine &mEngine;
	
	//! @brief Hints outputs indices (SBSBIN indices)
	Indices mHintOutputs;
	
	//! @brief Hints inputs SBSBIN indices (24 MSB bits) and types (8 LSB bits)
	Indices mHintInputs;
	
	//! @brief Current pushed user data
	size_t mUserData;

	//! @brief Get hint input type from mHintInputs value
	static SubstanceInputType getType(uint32 v) { return (SubstanceInputType)(v&0xFF); }

	//! @brief Get hint index from mHintInputs value
	static uint32 getIndex(uint32 v) { return v>>8; }

	//! @brief Get mHintInputs value from type and index
	static uint32 getHintInput(SubstanceInputType t, uint32 i) { return (i<<8)|(uint32)t; }
	
private:
	Computation(const Computation&);
	const Computation& operator=(const Computation&);
};  // class Computation


} // namespace Details
} // namespace Substance

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSCOMPUTATION_H
