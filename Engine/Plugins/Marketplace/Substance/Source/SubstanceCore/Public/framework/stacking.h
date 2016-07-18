//! @file stacking.h
//! @brief Substance package stacking class definition
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_FRAMEWORK_STACKING_H
#define _SUBSTANCE_FRAMEWORK_STACKING_H

#include "SubstanceFPackage.h"

#include <utility>
#include <vector>

namespace Substance
{

//! @brief The non connected outputs behavior policy enumeration
//! Used by ConnectionsOptions::mNonConnected member
enum NonConnected
{
	NonConnected_Remove,        //!< Remove all non connected outputs of pre graph
	NonConnected_Keep,          //!< Keep all non connected outputs of pre graph
};

//! @brief Connections options structure
struct ConnectionsOptions
{
	//! @brief Pair of two UIDs definition (pre output UID,post input UID)
	//! Output UIDs must be defined in the 'pre' graph. Input UIDs must be
	//!	defined in the 'post' graph.
	typedef std::pair<uint32,uint32> PairInOut;

	//! @brief List of connections (list of pre output UID/post input UID)
	typedef std::vector<PairInOut> Connections;

	//! @brief Explicit input/output connections list
	//! If this list is leave empty, inputs and outputs will be automatically 
	//!	wired using identifiers and/or label and/or channel (OutputDesc::mChannel)
	Connections mConnections;
	
	//! @brief Non connected pre outputs graph behavior policy option
	//! Default value: NonConnected_Remove, all non connected pre graph
	//! outputs are removed.
	NonConnected mNonConnected;
	
	//! @brief Default constructor
	ConnectionsOptions() : 
		mNonConnected(NonConnected_Remove) 
	{
	}
	
	//! @brief Fill connections automatically from options and graphs to stack
	//! @param preGraph Source pre graph.
	//! @param postGraph Source post graph.
	//! @note This method is called by PackageStackDesc constructor if the 
	//! 	connections list is empty.
	bool connectAutomatically(
		const FGraphDesc& preGraph,
		const FGraphDesc& postGraph);
		
	//! @brief Remove invalid connections (UID not present in graphs to stack)
	//! @param preGraph Source pre graph.
	//! @param postGraph Source post graph.
	//! @note This method is called by PackageStackDesc constructor if the 
	//! 	connections list is NOT empty.
	void fixConnections(
		const FGraphDesc& preGraph,
		const FGraphDesc& postGraph);
};


//! @brief Class that represents a stacking of two Graphs of two packages
//! Allows to apply post effect (e.g. illumination, shuffling, etc.) or
//! pre effects (e.g. make it tile, bitmap to material, etc.)
class PackageStackDesc : public FPackage
{
public:
	//! @brief Construct from pre and post Graphs, and connection options.
	//! @param preGraph Source pre graph.
	//! @param postGraph Source post graph.
	//! @param connOptions Connections options. Default, auto connections.
	//!
	//! This package is the result of the connection of one or several outputs
	//! of pre Graph to inputs of post Graph.
	PackageStackDesc(
		const FGraphDesc& preGraph,
		const FGraphDesc& postGraph,
		const ConnectionsOptions& connOptions = ConnectionsOptions());
		
	//! @brief Destructor
	~PackageStackDesc();
};

} // namespace Substance

#endif //_SUBSTANCE_FRAMEWORK_STACKING_H
