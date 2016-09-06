//! @file SubstanceCoreXmlHelper.h
//! @brief Substance description XML parsing helper
//! @author Gonzalez Antoine - Allegorithmic
//! @date 20101229
//! @copyright Allegorithmic. All rights reserved.

#pragma once

namespace Substance
{
namespace Helpers
{

//! @brief Parse a Substance xml comp
//! @param[in] InOuter The unreal package in which to create SbsOutput structures
//! @param[in] XmlFilePath The path of the xml file, must be valid
//! @param[in/out] The package to store Graph in
//! @param[out] IdxOutputMap The substance index (valid only in the 
//! loaded substance package) to output structure map
SUBSTANCECORE_API bool ParseSubstanceXml(
	const TArray<FString>& XmlContent,
	Substance::List<graph_desc_t*>& graphs,
	TArray<uint32>& assembly_uid);

SUBSTANCECORE_API bool ParseSubstanceXmlPreset(
	presets_t& presets,
	const FString& XmlContent,
	const graph_desc_t* graphDesc);

SUBSTANCECORE_API void WriteSubstanceXmlPreset(
	preset_t& preset,
	FString& XmlContent);

} //namespace Substance
} //namespace Helpers

FORCEINLINE uint32 appAtoul( const TCHAR* String ) { return strtoul( TCHAR_TO_ANSI(String), NULL, 10 ); }
