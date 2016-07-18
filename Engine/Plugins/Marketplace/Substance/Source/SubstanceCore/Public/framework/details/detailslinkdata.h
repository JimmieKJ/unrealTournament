//! @file detailslinkdata.h
//! @brief Substance Framework Link data classes definition
//! @author Christophe Soum - Allegorithmic
//! @date 20111031
//! @copyright Allegorithmic. All rights reserved.
//!
 
#ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSLINKDATA_H
#define _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSLINKDATA_H

#include "SubstanceCoreTypedefs.h"
#include "SubstanceFPackage.h"
#include "framework/stacking.h"

#include <vector>
#include <string>
#include <utility>

namespace Substance
{
namespace Details
{

struct LinkContext;


//! @brief Link data abstract base class
class LinkData
{
public:
	//! @brief Default constructor
	LinkData() {}

	//! @brief Virtual destructor
	virtual ~LinkData();
		
	//! @brief Push data to link
	//! @param cxt Used to push link data
	virtual bool push(LinkContext& cxt) const = 0;

private:
	LinkData(const LinkData&);
	const LinkData& operator=(const LinkData&);

};  // class LinkData


//! @brief Link data simple package from assembly class
class LinkDataAssembly : public LinkData
{
public:
	//! @brief Constructor from assembly data
	//! @param ptr Pointer on assembly data
	//! @param size Size of assembly data in bytes
	LinkDataAssembly(const uint8* ptr, uint32 size);

	virtual ~LinkDataAssembly() { clear(); }
		
	//! @brief Push data to link
	//! @param cxt Used to push link data
	bool push(LinkContext& cxt) const;
	
	//! @brief Force output format/mipmap
	//! @param uid Output uid
	//! @param format New output format
	//! @param mipmap New mipmap count
	void setOutputFormat(uint32 uid, int32 format, int32 mipmap=0);

	//! @brief Size of the resource, in bytes
	int32 getSize() {return mAssembly.size();}

	//! @brief Remove the content of the LinkData
	void clear()
	{
		mAssembly.clear();
	}

	void zeroAssembly()
	{
		mAssembly.assign(mAssembly.size(), 0);
	}

	//! @brief Accessor to the assembly
	const std::string& getAssembly() const
	{
		return mAssembly;
	}
	
protected:
	//! @brief Output format force entry
	struct OutputFormat
	{
		uint32 uid;             //!< Output uid
		int32 format;           //!< New output format
		int32 mipmap;           //!< New mipmap count
	};  // struct OutputFormat
	
	//! @brief Output format force entries container
	typedef std::vector< OutputFormat > OutputFormats;

	//! @brief Assembly data
	std::string mAssembly;
	
	//! @brief Output formats override
	OutputFormats mOutputFormats;
	
};  // class LinkDataAssembly


//! @brief Link data simple package from stacking class
class LinkDataStacking : public LinkData
{
public:
	//! @brief UID translation container (initial,translated pair)
	typedef std::vector< std::pair<uint32,uint32> > UidTranslates;

	//! @brief Construct from pre and post Graphs, and connection options.
	//! @param preGraph Source pre graph.
	//! @param postGraph Source post graph.
	//! @param connOptions Connections options.
	LinkDataStacking(
		const FGraphDesc* preGraph,
		const FGraphDesc* postGraph,
		const ConnectionsOptions& connOptions);
		
	//! @brief Push data to link
	//! @param cxt Used to push link data
	bool push(LinkContext& cxt) const;
	
	//! @brief Connections options.
	ConnectionsOptions mOptions;
	
	//! @brief Pair of pre,post UIDs to fuse (initial UIDs)
	//! Post UID sorted (second)
	ConnectionsOptions::Connections mFuseInputs;  
	
	//! @brief Disabled UIDs (initial UIDs)
	//! UID sorted
	Uids mDisabledOutputs;
	
	//! @brief Post graph Inputs UID translation (initial->translated)
	//! initial UID sorted
	UidTranslates mTrPostInputs;
	
	//! @brief Pre graph Outputs UID translation (initial->translated)
	//! initial UID sorted
	UidTranslates mTrPreOutputs;
	
protected:
	//! @brief Link data of Source pre graph.
	std::shared_ptr<LinkData> mPreLinkData;
	
	//! @brief Link data of Source post graph.
	std::shared_ptr<LinkData> mPostLinkData;
	
};  // class LinkDataStacking


} // namespace Details
} // namespace Substance

#endif // ifndef _SUBSTANCE_FRAMEWORK_DETAILS_DETAILSLINKDATA_H
