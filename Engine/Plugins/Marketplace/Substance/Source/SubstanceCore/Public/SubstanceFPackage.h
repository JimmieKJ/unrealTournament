//! @file SubstanceAirPackage.h
//! @brief Substance package definition
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#ifndef _SUBSTANCE_PACKAGE_H
#define _SUBSTANCE_PACKAGE_H

#include "SubstanceCoreTypedefs.h"

class USubstanceInstanceFactory;

namespace Substance
{

namespace Details
{
	class LinkData;
}

	struct FPackage
	{
		SUBSTANCECORE_API FPackage();
		~FPackage();

		//! @brief Initialize the package using the given binary data
		//! @note Supports only SBSAR content
		SUBSTANCECORE_API void SetData(const uint8*&, const int32, const FString&);

		//! @brief Tell is the package is valid
		SUBSTANCECORE_API bool IsValid();

		//! @brief Read the xml description of the assembly and creates the matching outputs and inputs
		bool ParseXml(const TArray<FString> & XmlContent);

		//! @brief Return the output instances with the given FGuid
		output_inst_t* GetOutputInst(const struct FGuid) const;

		//! @return the number of existing instances (loaded + unloaded)
		SUBSTANCECORE_API uint32 GetInstanceCount();

		//! @brief Called when a Graph gains an instance
		void InstanceSubscribed();

		//! @brief Called when a Graph looses an instance
		void InstanceUnSubscribed();

		void ConditionnalClearLinkData();

		//! @brief substance uid are not unique, someone could import a sbsar twice...
		TArray<uint32>	SubstanceUids;

		//! @brief ...that's why we also have unreal GUID, this one is unique
		substanceGuid_t			Guid;

		FString			SourceFilePath;

		FString			SourceFileTimestamp;

		uint32			LoadedInstancesCount;

		//! @brief The collection of graph descriptions
		//! @note This structure package owns those objects
		Substance::List<graph_desc_t*> Graphs;

		//! @brief Parent UObject
		//! @note Used as entry point for serialization and UI
		USubstanceInstanceFactory* Parent;

		//! @brief Internal use only
		std::shared_ptr<Details::LinkData> getLinkData() const { return LinkData; }

		//! @brief The sbsar content
		//! @note can be accessed by the substance rendering thread
		//! use the GSubstanceAirRendererMutex for safe access
		std::shared_ptr<Details::LinkData> LinkData;

	private:
		//! @brief Disabling package copy
		FPackage(const FPackage&);
		const FPackage& operator=( const FPackage& );
	};

} // namespace Substance

#endif //_SUBSTANCE_PACKAGE_H
