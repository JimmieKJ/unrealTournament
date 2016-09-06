//! @file SubstanceFGraphInstance.h
//! @brief Substance graph definition
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceFOutput.h"
#include "SubstanceInput.h"
#include "SubstanceCoreHelpers.h"

#include "framework/details/detailsstates.h"

class USubstanceGraphInstance;

namespace Substance
{
	struct FGraphDesc
	{
		FGraphDesc():PackageUrl(TEXT("")), Label(TEXT("")), Parent(0){}
		SUBSTANCECORE_API ~FGraphDesc();

		//! @brief Create a default instance of the Graph
		SUBSTANCECORE_API graph_inst_t* Instantiate(
			USubstanceGraphInstance* Outer,
			bool bCreateOutputs=true,
			bool bSubscribeInstance=true);
		
		//! @brief Return the input desc with the given Substance Uid
		SUBSTANCECORE_API input_desc_ptr GetInputDesc(const uint32 Uid);

		//! @brief Instances are registered after loading or instancing
		//! @note Their UID has been registered during creation (@see InstanceUids)
		void Subscribe(graph_inst_t* Inst);

		//! @brief Instances are registered when unloaded or destroyed
		//! @note Their UID still has to be removed after that (@see InstanceUids)
		void UnSubscribe(graph_inst_t* Inst);

		FString PackageUrl;
		FString Label;
		FString Description;

		//! @brief The outputs of the graph
		List<output_desc_t> OutputDescs;

		//! @brief The inputs of the graph
		List<input_desc_ptr> InputDescs;

		//! @brief Instances of graph currently loaded
		//! @note There is at least one, the instances are owned by the USubstanceGraphInstance
		List<graph_inst_t*> LoadedInstances;

		//! @brief UIDs of the all existing instances
		List<substanceGuid_t> InstanceUids;

		//! @brief the Parent package
		package_t* Parent;

	private:
		//disable copy constructor
		FGraphDesc(const FGraphDesc&);
		FGraphDesc& operator = (const FGraphDesc&);
	};


	struct FGraphInstance
	{
		FGraphInstance():InstanceGuid(0,0,0,0), Desc(NULL), ParentInstance(NULL), bHasPendingImageInputRendering(false){}

		FGraphInstance(FGraphDesc*, USubstanceGraphInstance* Outer);

		SUBSTANCECORE_API ~FGraphInstance();

		//! @brief Update the input with that name with the given values
		//! @return Number of modified outputs
		template< typename T > int32 UpdateInput(
			const FString& ParameterName,
			const TArray< T >& Value);

		//! @return Number of modified outputs
		template< typename T > int32 UpdateInput(
			const uint32& Uid,
			const TArray< T >& Value);

		//! @brief Update the input with that name with the given values
		//! @return Number of modified outputs
		SUBSTANCECORE_API int32 UpdateInput(
			const uint32& Uid,
			class UObject* Value);

		//! @return Number of modified outputs
		int32 UpdateInput(
			const FString& ParameterName,
			class UObject* Value);

		//! @brief Return the instance of the input with the given UID
		input_inst_t* GetInput(const uint32 Uid);

		//! @brief Return the instance of the input with the given UID
		input_inst_t* GetInput(const FString& ParameterName);

		//! @brief Return the instance of the output with the given UID
		SUBSTANCECORE_API output_inst_t* GetOutput(const uint32 Uid);

		void plugState(Details::States*);         //!< Internal use only
		void unplugState(Details::States*);       //!< Internal use only

		//! @brief Array of output instances
		Substance::List<output_inst_t> Outputs;

		//! @brief Array of input instances
		Substance::List<TSharedPtr<input_inst_t>> Inputs;

		//! @brief GUID of this instance
		substanceGuid_t InstanceGuid;

		//! @brief URL of the parent Desc
		FString ParentUrl;

		//! @brief the graph's description
		graph_desc_t* Desc;

		//! @brief the UObject based interface object
		USubstanceGraphInstance* ParentInstance;

		//! @brief Does the graph need some ImageInput to be loaded
		uint32 bHasPendingImageInputRendering:1;

		typedef std::vector<Details::States*> States_t;

		States_t States;
	protected:
		template< typename T > int32 UpdateInputHelper(
			input_inst_t* InputInst,
			input_desc_t* InputDesc,
			const TArray< T > & InValue );
	};

} // namespace Substance

#include "SubstanceFGraph.inl"
