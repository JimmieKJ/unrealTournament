//! @file preset.h
//! @brief Substance preset definitions
//! @author Christophe Soum - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceCoreTypedefs.h"

#include "substance_public.h"

namespace Substance
{

//! @brief Class that represents a preset of inputs
//! Presets can be loaded from a preset file (.SBSPRS) or can be embedded in a
//! graph description (GraphDesc::getPresets()) 
struct FPreset
{
	//! @brief One input value structure
	struct FInputValue
	{
		//! @brief 32 bits unsigned Unique Identifier 
		//! Used for exact match.
		uint32 mUid;
		
		//! @brief User-readable identifier of the input
		//! Used for matching a preset created from a graph to an another one.
		FString mIdentifier;

		//! @brief Type of the input
		//! Used to check values types consistency.
		//! @see SubstanceInputType enum in substance/inputdesc.h
		SubstanceInputType mType;
		
		//! @brief Value of the input, stored as string
		FString mValue;
	};
	
	//! @brief Container of input values
	typedef Substance::List<FInputValue> inputvalues_t;
	
	//! @brief Apply mode, used by apply()
	enum ApplyMode
	{
		Apply_Reset,  //!< Graph inputs not listed in input values are reset
		Apply_Merge   //!< Non concerned graph inputs are not reset, NYI
	};
	
	//! @brief URL of the graph corresponding to this preset
	//! A preset can be applied to any graph, only input values that match per 
	//!	identifier and per type are effectively set.
	FString mPackageUrl;
	
	//! @brief Label of this preset (UI) UTF8 format
	FString mLabel;
	
	//! @brief Description of this preset (UI) UTF8 format
	FString mDescription;

	//! @brief List of preset inputs
	//! Only non default input values are recorded.
	inputvalues_t mInputValues;
	
	//! @brief Apply this preset to a graph instance
	//! @param graph The target graph instance to apply this preset
	//! @param mode Reset to default other inputs of merge w/ previous values
	//! @return Return true whether at least one input value is applied
	SUBSTANCECORE_API bool Apply(graph_inst_t* Graph, ApplyMode Mode = Apply_Reset) const;

	//! @brief Fill this preset from a graph instance input values
	//! @param graph The source graph instance.
	SUBSTANCECORE_API void ReadFrom(graph_inst_t* Graph);
};

//! @brief Parse presets from a XML preset file (.SBSPRS)
//! @param[out] presets Container filled with loaded presets
//! @param xmlPreset String that contains presets XML description
//! @return Return whether preset parsing succeed
SUBSTANCECORE_API bool ParsePresets(presets_t& Presets, const FString& xmlPreset);

//! @brief Translates a list of presets to a presets XML description
SUBSTANCECORE_API void WritePreset(preset_t& Preset, FString& xmlPreset);

}  // namespace Substance

