/************************************************************************************

Filename    :   MetaDataManager.h
Content     :   A class to manage metadata used by FolderBrowser 
Created     :   January 26, 2015
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_MetaDataManager_h )
#define OVR_MetaDataManager_h

#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_StringHash.h"

namespace OVR {
class JSON;
class JsonReader;
//==============================================================
// OvrMetaData
struct OvrMetaDatum
{
	int						Id;
	Array< String >			Tags;
	String					Url;

protected:
	OvrMetaDatum() {}
};

enum TagAction
{
	TAG_ADDED,
	TAG_REMOVED,
	TAG_ERROR
};

struct OvrMetaDataFileExtensions
{
	Array< String > GoodExtensions;
	Array< String > BadExtensions;
};

class OvrMetaData
{
public:
	struct Category
	{
		Category()
			: Dirty( true )
		{}
		String 			CategoryTag;
		String 			Label;
		Array< int > 	DatumIndicies;
		bool 			Dirty;
	};

	OvrMetaData()
		: Version( -1.0 )
	{}

	// Init meta data from contents on disk
	void					InitFromDirectory( const char * relativePath, const Array< String > & searchPaths, const OvrMetaDataFileExtensions & fileExtensions );

	// Init meta data from a passed in list of files
	void					InitFromFileList( const Array< String > & fileList, const OvrMetaDataFileExtensions & fileExtensions );

	// Check specific paths for media and reconcile against stored/new metadata (Maintained for SDK)
	void					InitFromDirectoryMergeMeta( const char * relativePath, const Array< String > & searchPaths,
		const OvrMetaDataFileExtensions & fileExtensions, const char * metaFile, const char * packageName );

	// File list passed in and we reconcile against stored/new metadata
	void					InitFromFileListMergeMeta( const Array< String > & fileList, const Array< String > & searchPaths,
		const OvrMetaDataFileExtensions & fileExtensions, const char * appFileStoragePath, const char * metaFile, JSON * storedMetaData );

	void					ProcessRemoteMetaFile( const char * metaFileString, const int startInsertionIndex /* index to insert remote categories*/ );

	// Rename a category after construction 
	void					RenameCategory( const char * currentTag, const char * newName );

	// Adds or removes tag and returns action taken
	TagAction				ToggleTag( OvrMetaDatum * data, const String & tag );

	// Returns metaData file if one is found, otherwise creates one using the default meta.json in the assets folder
	JSON *					CreateOrGetStoredMetaFile( const char * appFileStoragePath, const char * metaFile );
	void					AddCategory( const String & name );

	const Array< Category > GetCategories() const 							{ return Categories; }
	const Category & 		GetCategory( const int index ) const 			{ return Categories.At( index ); }
	Category & 				GetCategory( const int index )   				{ return Categories.At( index ); }
	const OvrMetaDatum &	GetMetaDatum( const int index ) const;
	bool 					GetMetaData( const Category & category, Array< const OvrMetaDatum * > & outMetaData ) const;
	void					SetCategoryDatumIndicies( const int index, const Array< int >& datumIndicies );

protected:
	// Overload to fill extended data during initialization
	virtual OvrMetaDatum *	CreateMetaDatum( const char* fileName ) const = 0;
	virtual	void			ExtractExtendedData( const JsonReader & jsonDatum, OvrMetaDatum & outDatum ) const = 0;
	virtual	void			ExtendedDataToJson( const OvrMetaDatum & datum, JSON * outDatumObject ) const = 0;
	virtual void			SwapExtendedData( OvrMetaDatum * left, OvrMetaDatum * right ) const = 0;
	
	// Optional protected interface
	virtual bool			IsRemote( const OvrMetaDatum * datum ) const	{	return true; } 

private:
	Category * 				GetCategory( const String & categoryName );
	void					ProcessMetaData( JSON * dataFile, const Array< String > & searchPaths, const char * metaFile );
	void					RegenerateCategoryIndices();
	void					ReconcileMetaData( StringHash< OvrMetaDatum * > & storedMetaData );
	void					ReconcileCategories( Array< Category > & storedCategories );

	JSON *					MetaDataToJson() const;
	void					WriteMetaFile( const char * metaFile ) const;
	bool 					ShouldAddFile( const char * filename, const OvrMetaDataFileExtensions & fileExtensions ) const;
	void					ExtractVersion( JSON * dataFile, double & outVersion ) const;
	void					ExtractCategories( JSON * dataFile, Array< Category > & outCategories ) const;
	void					ExtractMetaData( JSON * dataFile, const Array< String > & searchPaths, StringHash< OvrMetaDatum * > & outMetaData ) const;
	void					ExtractRemoteMetaData( JSON * dataFile, StringHash< OvrMetaDatum * > & outMetaData ) const;

	String 					FilePath;
	Array< Category >		Categories;
	Array< OvrMetaDatum * >	MetaData;
	StringHash< int >		UrlToIndex;
	double					Version;
};

}

#endif // OVR_MetaDataManager_h
