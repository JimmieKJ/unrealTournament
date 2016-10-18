//! @file SubstanceOutput.h
//! @brief Substance output informations definition
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#pragma once

#include "SubstanceGraphInstance.h"
#include "framework/renderresult.h"
#include "framework/std_wrappers.h"

#include <memory> // for std_auto

class USubstanceTexture2D;
class USubstanceGraphInstance;

namespace Substance
{
	struct RenderResult;

	namespace Details
	{
		class RenderToken;
		class States;
	}

	//! @brief Description of a Substance output
	struct FOutputDesc
	{
		SUBSTANCECORE_API FOutputDesc();
		SUBSTANCECORE_API FOutputDesc(const FOutputDesc &);

		FOutputInstance Instantiate() const;

		FString		Identifier;
		FString		Label;

		uint32 		Uid;
		int32 		Format;	    //! @see SubstancePixelFormat enum in substance/pixelformat.h
		int32 		Channel;	//! @see ChannelUse

		//! @brief The inputs modifying this output
		//! @note FOutput is not the owner of those objects
		Substance::List<uint32> AlteringInputUids;
	};


	//! @brief Description of a Substance output instance
	struct FOutputInstance
	{
		typedef std::auto_ptr<RenderResult> Result;

		SUBSTANCECORE_API FOutputInstance();

		SUBSTANCECORE_API FOutputInstance(const FOutputInstance&);

		SUBSTANCECORE_API ~FOutputInstance();

		//! @brief Return the graph instance this output belongs to
		//! @return Null is the output is unable to know, this
		//! happens when its texture has not been created yet.
		SUBSTANCECORE_API graph_inst_t* GetParentGraphInstance() const;
		SUBSTANCECORE_API graph_desc_t* GetParentGraph() const;
		SUBSTANCECORE_API output_desc_t* GetOutputDesc() const;

		USubstanceGraphInstance* GetOuter() const;

		Result grabResult();

		void flagAsDirty() { bIsDirty = true; }

		bool isDirty() const { return bIsDirty; }

		//! @brief Substance UID of the reference output desc
		uint32		Uid;

		//! @brief Output of this specific instance
		int32		Format;

		//! @brief Used to link host-engine texture to output
		FGuid		OutputGuid;

		//! @brief The user can disable the generation of some outputs
		uint32	bIsEnabled:1;

		uint32	bIsDirty:1;

		//! @brief Actual texture class of the host engine
		MS_ALIGN(16) std::shared_ptr<USubstanceTexture2D*> Texture;

		MS_ALIGN(16) USubstanceGraphInstance* ParentInstance;

		typedef std::shared_ptr<Details::RenderToken> Token;	//!< Internal use only

		void push(const Token&);                                //!< Internal use only

		bool queueRender();                                     //!< Internal use only

	protected:
		void releaseTokensOwnedByEngine(uint32 engineUid);

		typedef Substance::Vector<Token> RenderTokens_t;

		RenderTokens_t RenderTokens;

		friend struct FGraphInstance;
		friend class Details::States;
	};

} // namespace Substance
