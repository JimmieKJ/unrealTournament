//! @file SubstanceEditorHelpers.h
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceCoreTypedefs.h"

namespace SubstanceEditor
{
	//! @brief Contains helper functions to call from the main thread
	namespace Helpers
	{
		//! @brief Create an Unreal Material for the given graph-instance
		UMaterial* CreateMaterial(
			graph_inst_t* GraphInstance, 
			const FString& MatName,
			UObject* Outer = NULL);

		bool ExportPresetFromGraph(USubstanceGraphInstance*);
		bool ImportAndApplyPresetForGraph(USubstanceGraphInstance*);

		FString ImportPresetFile();
		FString ExportPresetFile(FString SuggestedFilename);
	} // namespace Helpers
} // namespace Substance
