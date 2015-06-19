/************************************************************************************

Filename    :   MetaDataManager.cpp
Content     :   A class to manage metadata used by FolderBrowser
Created     :   January 26, 2015
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "MetaDataManager.h"

#include "Kernel/OVR_JSON.h"
#include "Android/LogUtils.h"

#include "VrCommon.h"
#include "PackageFiles.h"
#include "unistd.h"

namespace OVR {

//==============================
// OvrMetaData

const char * const VERSION = "Version";
const char * const CATEGORIES = "Categories";
const char * const DATA = "Data";
const char * const TITLE = "Title";
const char * const URL = "Url";
const char * const FAVORITES_TAG = "Favorites";
const char * const TAG = "tag";
const char * const LABEL = "label";
const char * const TAGS = "tags";
const char * const CATEGORY = "category";
const char * const URL_INNER = "url";

void OvrMetaData::InitFromDirectory( const char * relativePath, const Array< String > & searchPaths, const OvrMetaDataFileExtensions & fileExtensions )
{
	LOG( "OvrMetaData::InitFromDirectory( %s )", relativePath );

	// Find all the files - checks all search paths
	StringHash< String > uniqueFileList = RelativeDirectoryFileList( searchPaths, relativePath );
	Array<String> fileList;
	for ( StringHash< String >::ConstIterator iter = uniqueFileList.Begin(); iter != uniqueFileList.End(); ++iter )
	{
		fileList.PushBack( iter->First );
	}
	SortStringArray( fileList );
	Category currentCategory;
	currentCategory.CategoryTag = ExtractFileBase( relativePath );
	// The label is the same as the tag by default. 
	//Will be replaced if definition found in loaded metadata
	currentCategory.Label = currentCategory.CategoryTag;

	LOG( "OvrMetaData start category: %s", currentCategory.CategoryTag.ToCStr() );
	Array< String > subDirs;
	// Grab the categories and loose files
	for ( int i = 0; i < fileList.GetSizeI(); i++ )
	{
		const String & s = fileList[ i ];
		const String fileBase = ExtractFileBase( s );
		// subdirectory - add category
		if ( MatchesExtension( s, "/" ) )
		{
			subDirs.PushBack( s );
			continue;
		}
		
		// See if we want this loose-file
		if ( !ShouldAddFile( s.ToCStr(), fileExtensions ) )
		{
			continue;
		}

		// Add loose file
		const int dataIndex = MetaData.GetSizeI();
		OvrMetaDatum * datum = CreateMetaDatum( fileBase );
		if ( datum )
		{
			datum->Id = dataIndex;
			datum->Tags.PushBack( currentCategory.CategoryTag );
			if ( GetFullPath( searchPaths, s, datum->Url ) )
			{
				StringHash< int >::ConstIterator iter = UrlToIndex.FindCaseInsensitive( datum->Url );
				if ( iter == UrlToIndex.End() )
				{
					UrlToIndex.Add( datum->Url, dataIndex );
					MetaData.PushBack( datum );
					LOG( "OvrMetaData adding datum %s with index %d to %s", datum->Url.ToCStr(), dataIndex, currentCategory.CategoryTag.ToCStr() );
					// Register with category
					currentCategory.DatumIndicies.PushBack( dataIndex );
				}
				else
				{
					WARN( "OvrMetaData::InitFromDirectory found duplicate url %s", datum->Url.ToCStr() );
				}
			}
			else
			{
				WARN( "OvrMetaData::InitFromDirectory failed to find %s", s.ToCStr() );
			}
		}
	}

	if ( !currentCategory.DatumIndicies.IsEmpty() )
	{
		Categories.PushBack( currentCategory );
	}

	// Recurse into subdirs
	for ( int i = 0; i < subDirs.GetSizeI(); ++i )
	{
		const String & subDir = subDirs.At( i );
		InitFromDirectory( subDir.ToCStr(), searchPaths, fileExtensions );
	}
}

void OvrMetaData::InitFromFileList( const Array< String > & fileList, const OvrMetaDataFileExtensions & fileExtensions )
{
	// Create unique categories
	StringHash< int > uniqueCategoryList;
	for ( int i = 0; i < fileList.GetSizeI(); ++i )
	{
		const String & filePath = fileList.At( i );
		const String categoryTag = ExtractDirectory( fileList.At( i ) );
		StringHash< int >::ConstIterator iter = uniqueCategoryList.Find( categoryTag );
		int catIndex = -1;
		if ( iter == uniqueCategoryList.End() )
		{
			LOG( " category: %s", categoryTag.ToCStr() );
			Category cat;
			cat.CategoryTag = categoryTag;
			// The label is the same as the tag by default. 
			// Will be replaced if definition found in loaded metadata
			cat.Label = cat.CategoryTag;
			catIndex = Categories.GetSizeI();
			Categories.PushBack( cat );
			uniqueCategoryList.Add( categoryTag, catIndex );
		}
		else
		{
			catIndex = iter->Second;
		}

		OVR_ASSERT( catIndex > -1 );
		Category & currentCategory = Categories.At( catIndex );

		// See if we want this loose-file
		if ( !ShouldAddFile( filePath.ToCStr(), fileExtensions ) )
		{
			continue;
		}

		// Add loose file
		const int dataIndex = MetaData.GetSizeI();
		OvrMetaDatum * datum = CreateMetaDatum( filePath );
		if ( datum )
		{
			datum->Id = dataIndex;
			datum->Url = filePath;
			datum->Tags.PushBack( currentCategory.CategoryTag );

			StringHash< int >::ConstIterator iter = UrlToIndex.FindCaseInsensitive( datum->Url );
			if ( iter == UrlToIndex.End() )
			{
				UrlToIndex.Add( datum->Url, dataIndex );
				MetaData.PushBack( datum );
				LOG( "OvrMetaData::InitFromFileList adding datum %s with index %d to %s", datum->Url.ToCStr(),
					dataIndex, currentCategory.CategoryTag.ToCStr() );
				// Register with category
				currentCategory.DatumIndicies.PushBack( dataIndex );
			}
			else
			{
				WARN( "OvrMetaData::InitFromFileList found duplicate url %s", datum->Url.ToCStr() );
			}
		}
	}
}

void OvrMetaData::RenameCategory( const char * currentTag, const char * newName )
{
	for ( int i = 0; i < Categories.GetSizeI(); ++i )
	{
		Category & cat = Categories.At( i );
		if ( cat.CategoryTag == currentTag )
		{
			cat.Label = newName;
			break;
		}
	}
}

JSON * LoadPackageMetaFile( const char * metaFile )
{
	int bufferLength = 0;
	void * 	buffer = NULL;
	String assetsMetaFile = "assets/";
	assetsMetaFile += metaFile;
	ovr_ReadFileFromApplicationPackage( assetsMetaFile.ToCStr(), bufferLength, buffer );
	if ( !buffer )
	{
		WARN( "LoadPackageMetaFile failed to read %s", assetsMetaFile.ToCStr() );
	}
	return JSON::Parse( static_cast< const char * >( buffer ) );
}

JSON * OvrMetaData::CreateOrGetStoredMetaFile( const char * appFileStoragePath, const char * metaFile )
{
	FilePath = appFileStoragePath;
	FilePath += metaFile;

	LOG( "CreateOrGetStoredMetaFile FilePath: %s", FilePath.ToCStr() );

	JSON * dataFile = JSON::Load( FilePath );
	if ( dataFile == NULL )
	{
		// If this is the first run, or we had an error loading the file, we copy the meta file from assets to app's cache
		WriteMetaFile( metaFile );

		// try loading it again
		dataFile = JSON::Load( FilePath );
		if ( dataFile == NULL )
		{
			WARN( "OvrMetaData failed to load JSON meta file: %s", metaFile );
		}
	}
	else
	{
		LOG( "OvrMetaData::CreateOrGetStoredMetaFile found %s", FilePath.ToCStr() );
	}
	return dataFile;
}

void OvrMetaData::WriteMetaFile( const char * metaFile ) const
{
	LOG( "Writing metafile from apk" );

	if ( FILE * newMetaFile = fopen( FilePath.ToCStr(), "w" ) )
	{
		int bufferLength = 0;
		void * 	buffer = NULL;
		String assetsMetaFile = "assets/";
		assetsMetaFile += metaFile;
		ovr_ReadFileFromApplicationPackage( assetsMetaFile.ToCStr(), bufferLength, buffer );
		if ( !buffer )
		{
			WARN( "OvrMetaData failed to read %s", assetsMetaFile.ToCStr() );
		}
		else
		{
			int writtenCount = fwrite( buffer, 1, bufferLength, newMetaFile );
			if ( writtenCount != bufferLength )
			{
				FAIL( "OvrMetaData::WriteMetaFile failed to write %s", metaFile );
			}
			free( buffer );
		}
		fclose( newMetaFile );
	}
	else
	{
		FAIL( "OvrMetaData failed to create %s - check app permissions", FilePath.ToCStr() );
	}
}

void OvrMetaData::InitFromDirectoryMergeMeta( const char * relativePath, const Array< String > & searchPaths,
	const OvrMetaDataFileExtensions & fileExtensions, const char * metaFile, const char * packageName )
{
	LOG( "OvrMetaData::InitFromDirectoryMergeMeta" );

	String appFileStoragePath = "/data/data/";
	appFileStoragePath += packageName;
	appFileStoragePath += "/files/";

	FilePath = appFileStoragePath + metaFile;

	OVR_ASSERT( HasPermission( FilePath, R_OK ) );

	JSON * dataFile = CreateOrGetStoredMetaFile( appFileStoragePath, metaFile );

	InitFromDirectory( relativePath, searchPaths, fileExtensions );
	ProcessMetaData( dataFile, searchPaths, metaFile );
}

void OvrMetaData::InitFromFileListMergeMeta( const Array< String > & fileList, const Array< String > & searchPaths,
	const OvrMetaDataFileExtensions & fileExtensions, const char * appFileStoragePath, const char * metaFile, JSON * storedMetaData )
{
	LOG( "OvrMetaData::InitFromFileListMergeMeta" );

	InitFromFileList( fileList, fileExtensions );
	ProcessMetaData( storedMetaData, searchPaths, metaFile );
}

void OvrMetaData::ProcessRemoteMetaFile( const char * metaFileString, const int startIndex )
{
	JSON * remoteMetaFile = JSON::Parse( metaFileString );
	if ( remoteMetaFile != NULL )
	{
		// First grab the version
		double remoteVersion = 0.0;
		ExtractVersion( remoteMetaFile, remoteVersion );

		if ( remoteVersion <= Version ) // We already have this metadata, don't need to process further
		{
			remoteMetaFile->Release();
			return;
		}

		Version = remoteVersion;

		Array< Category > remoteCategories;
		StringHash< OvrMetaDatum * > remoteMetaData;
		ExtractCategories( remoteMetaFile , remoteCategories );
		ExtractRemoteMetaData( remoteMetaFile , remoteMetaData );
		
		// Merge in the remote categories
		// Ignore any duplicate categories
		StringHash< bool > CurrentCategoriesSet; // using as set
		for ( int i = 0; i < Categories.GetSizeI(); ++i )
		{
			const Category & storedCategory = Categories.At( i );
			CurrentCategoriesSet.Add( storedCategory.CategoryTag, true );
		}

		for ( int remoteIndex = 0; remoteIndex < remoteCategories.GetSizeI(); ++remoteIndex )
		{
			const Category & remoteCat = remoteCategories.At( remoteIndex );

			StringHash< bool >::ConstIterator iter = CurrentCategoriesSet.Find( remoteCat.CategoryTag );
			if ( iter == CurrentCategoriesSet.End() )
			{
				const int targetIndex = startIndex + remoteIndex;
				LOG( "OvrMetaData::ProcessRemoteMetaFile merging %s into category index %d", remoteCat.CategoryTag.ToCStr(), targetIndex );
				if ( startIndex >= 0 && startIndex < Categories.GetSizeI() )
				{
					Categories.InsertAt( targetIndex, remoteCat );
				}
				else
				{
					Categories.PushBack( remoteCat );
				}
			}
			else
			{
				LOG( "OvrMetaData::ProcessRemoteMetaFile discarding duplicate category %s", remoteCat.CategoryTag.ToCStr() );
			}
		}

		// Append the remote data
		ReconcileMetaData( remoteMetaData );
		
		// Recreate indices which may have changed after reconciliation
		RegenerateCategoryIndices();

		remoteMetaFile->Release();

		// Serialize the new metadata
		JSON * dataFile = MetaDataToJson();
		if ( dataFile == NULL )
		{
			FAIL( "OvrMetaData::ProcessMetaData failed to generate JSON meta file" );
		}

		dataFile->Save( FilePath );

		LOG( "OvrMetaData::ProcessRemoteMetaFile updated %s", FilePath.ToCStr() );
		dataFile->Release();
	}
}

void OvrMetaData::ProcessMetaData( JSON * dataFile, const Array< String > & searchPaths, const char * metaFile )
{
	if ( dataFile != NULL )
	{
		// Grab the version from the loaded data
		ExtractVersion( dataFile, Version );

		Array< Category > storedCategories;
		StringHash< OvrMetaDatum * > storedMetaData;
		ExtractCategories( dataFile, storedCategories );

		// Read in package data first
		JSON * packageMeta = LoadPackageMetaFile( metaFile );
		if ( packageMeta )
		{
			// If we failed to find a version in the serialized data, need to set it from the assets version
			if ( Version < 0.0 ) 
			{
				ExtractVersion( packageMeta, Version );
				if ( Version < 0.0 )
				{
					Version = 0.0;
				}
			}
			ExtractCategories( packageMeta, storedCategories );
			ExtractMetaData( packageMeta, searchPaths, storedMetaData );
			packageMeta->Release();
		}
		else
		{
			WARN( "ProcessMetaData LoadPackageMetaFile failed for %s", metaFile );
		}

		// Read in the stored data - overriding any found in the package
		ExtractMetaData( dataFile, searchPaths, storedMetaData );

		// Reconcile the stored data vs the data read in
		ReconcileCategories( storedCategories );
		ReconcileMetaData( storedMetaData );
	
		// Recreate indices which may have changed after reconciliation
		RegenerateCategoryIndices();

		// Delete any newly empty categories except Favorites 
		if ( !Categories.IsEmpty() )
		{
			Array< Category > finalCategories;
			finalCategories.PushBack( Categories.At( 0 ) );
			for ( int catIndex = 1; catIndex < Categories.GetSizeI(); ++catIndex )
			{
				Category & cat = Categories.At( catIndex );
				if ( !cat.DatumIndicies.IsEmpty() )
				{
					finalCategories.PushBack( cat );
				}
				else
				{
					WARN( "OvrMetaData::ProcessMetaData discarding empty %s", cat.CategoryTag.ToCStr() );
				}
			}
			Alg::Swap( finalCategories, Categories );
		}
		dataFile->Release();
	}
	else
	{
		WARN( "OvrMetaData::ProcessMetaData NULL dataFile" );
	}

	// Rewrite new data
	dataFile = MetaDataToJson();
	if ( dataFile == NULL )
	{
		FAIL( "OvrMetaData::ProcessMetaData failed to generate JSON meta file" );
	}

	dataFile->Save( FilePath );

	LOG( "OvrMetaData::ProcessMetaData created %s", FilePath.ToCStr() );
	dataFile->Release();
}

void OvrMetaData::ReconcileMetaData( StringHash< OvrMetaDatum * > & storedMetaData )
{
	if ( storedMetaData.IsEmpty() )
	{
		return;
	}

	// Fix the read in meta data using the stored
	for ( int i = 0; i < MetaData.GetSizeI(); ++i )
	{
		OvrMetaDatum * metaDatum = MetaData.At( i );

		StringHash< OvrMetaDatum * >::Iterator iter = storedMetaData.FindCaseInsensitive( metaDatum->Url );

		if ( iter != storedMetaData.End() )
		{
			OvrMetaDatum * storedDatum = iter->Second;
			LOG( "ReconcileMetaData metadata for %s", storedDatum->Url.ToCStr() );
			Alg::Swap( storedDatum->Tags, metaDatum->Tags );
			SwapExtendedData( storedDatum, metaDatum );
			storedMetaData.Remove( iter->First );
		}
	}

	// Now for any remaining stored data - check if it's remote and just add it
	StringHash< OvrMetaDatum * >::Iterator storedIter = storedMetaData.Begin();
	for ( ; storedIter != storedMetaData.End(); ++storedIter )
	{
		OvrMetaDatum * storedDatum = storedIter->Second;
		if ( IsRemote( storedDatum ) )
		{
			LOG( "ReconcileMetaData metadata adding remote %s", storedDatum->Url.ToCStr() );
			MetaData.PushBack( storedDatum );
		}
	}
	storedMetaData.Clear();
}

void OvrMetaData::ReconcileCategories( Array< Category > & storedCategories )
{
	if ( storedCategories.IsEmpty() )
	{
		return;
	}

	// Reconcile categories
	// We want Favorites always at the top
	// Followed by user created categories
	// Finally we want to maintain the order of the retail categories (defined in assets/meta.json)
	Array< Category > finalCategories;

	Category favorites = storedCategories.At( 0 );
	if ( favorites.CategoryTag != FAVORITES_TAG )
	{
		WARN( "OvrMetaData::ReconcileCategories failed to find expected category order -- missing assets/meta.json?" );
	}

	finalCategories.PushBack( favorites );

	StringHash< bool > StoredCategoryMap; // using as set
	for ( int i = 0; i < storedCategories.GetSizeI(); ++i )
	{
		const Category & storedCategory = storedCategories.At( i );
		LOG( "OvrMetaData::ReconcileCategories storedCategory: %s", storedCategory.CategoryTag.ToCStr() );
		StoredCategoryMap.Add( storedCategory.CategoryTag, true );
	}

	// Now add the read in categories if they differ
	for ( int i = 0; i < Categories.GetSizeI(); ++i )
	{
		const Category & readInCategory = Categories.At( i );
		StringHash< bool >::ConstIterator iter = StoredCategoryMap.Find( readInCategory.CategoryTag );

		if ( iter == StoredCategoryMap.End() )
		{
			LOG( "OvrMetaData::ReconcileCategories adding %s", readInCategory.CategoryTag.ToCStr() );
			finalCategories.PushBack( readInCategory );
		}
	}

	// Finally fill in the stored in categories after user made ones
	for ( int i = 1; i < storedCategories.GetSizeI(); ++i )
	{
		const  Category & storedCat = storedCategories.At( i );
		LOG( "OvrMetaData::ReconcileCategories adding stored category %s", storedCat.CategoryTag.ToCStr() );
		finalCategories.PushBack( storedCat );
	}

	// Now replace Categories
	Alg::Swap( Categories, finalCategories );
}

void OvrMetaData::ExtractVersion( JSON * dataFile, double & outVersion ) const
{
	if ( dataFile == NULL )
	{
		return;
	}

	const JsonReader dataReader( dataFile );
	if ( dataReader.IsObject() )
	{
		outVersion = dataReader.GetChildDoubleByName( VERSION );
	}
}

void OvrMetaData::ExtractCategories( JSON * dataFile, Array< Category > & outCategories ) const
{
	if ( dataFile == NULL )
	{
		return;
	}

	const JsonReader categories( dataFile->GetItemByName( CATEGORIES ) );

	if ( categories.IsArray() )
	{
		while ( const JSON * nextElement = categories.GetNextArrayElement() )
		{
			const JsonReader category( nextElement );
			if ( category.IsObject() )
			{
				Category extractedCategory;
				extractedCategory.CategoryTag = category.GetChildStringByName( TAG );
				extractedCategory.Label = category.GetChildStringByName( LABEL );

				// Check if we already have this category
				bool exists = false;
				for ( int i = 0; i < outCategories.GetSizeI(); ++i )
				{
					const Category & existingCat = outCategories.At( i );
					if ( extractedCategory.CategoryTag == existingCat.CategoryTag )
					{
						exists = true;
						break;
					}
				}

				if ( !exists )
				{
					LOG( "Extracting category: %s", extractedCategory.CategoryTag.ToCStr() );
					outCategories.PushBack( extractedCategory );
				}
			}
		}
	}
}

void OvrMetaData::ExtractMetaData( JSON * dataFile, const Array< String > & searchPaths, StringHash< OvrMetaDatum * > & outMetaData ) const
{
	if ( dataFile == NULL )
	{
		return;
	}

	const JsonReader data( dataFile->GetItemByName( DATA ) );
	if ( data.IsArray() )
	{
		int jsonIndex = 0;
		while ( const JSON * nextElement = data.GetNextArrayElement() )
		{
			const JsonReader datum( nextElement );
			if ( datum.IsObject() )
			{
				OvrMetaDatum * metaDatum = CreateMetaDatum( "" );
				if ( !metaDatum )
				{
					continue;
				}

				metaDatum->Id = jsonIndex++;
				const JsonReader tags( datum.GetChildByName( TAGS ) );
				if ( tags.IsArray() )
				{
					while ( const JSON * tagElement = tags.GetNextArrayElement() )
					{
						const JsonReader tag( tagElement );
						if ( tag.IsObject() )
						{
							metaDatum->Tags.PushBack( tag.GetChildStringByName( CATEGORY ) );
						}
					}
				}

				OVR_ASSERT( !metaDatum->Tags.IsEmpty() );

				const String relativeUrl( datum.GetChildStringByName( URL_INNER ) );
				metaDatum->Url = relativeUrl;
				bool foundPath = false;
				const bool isRemote = IsRemote( metaDatum );
				
				// Get the absolute path if this is a local file
				if ( !isRemote )
				{
					foundPath = GetFullPath( searchPaths, relativeUrl, metaDatum->Url );
					if ( !foundPath )
					{
						// if we fail to find the file, check for encrypted extension (TODO: Might put this into a virtual function if necessary, benign for now)
						foundPath = GetFullPath( searchPaths, relativeUrl + ".x", metaDatum->Url );
					}
				}
				
				// if we fail to find the local file or it's a remote file, the Url is left as read in from the stored data
				if ( isRemote || !foundPath )
				{
					metaDatum->Url = relativeUrl;
				}

				ExtractExtendedData( datum, *metaDatum );
				LOG( "OvrMetaData::ExtractMetaData adding datum %s", metaDatum->Url.ToCStr() );

				StringHash< OvrMetaDatum * >::Iterator iter = outMetaData.FindCaseInsensitive( metaDatum->Url );
				if ( iter == outMetaData.End() )
				{
					outMetaData.Add( metaDatum->Url, metaDatum );
				}
				else
				{
					iter->Second = metaDatum;
				}
			}
		}
	}
}

void OvrMetaData::ExtractRemoteMetaData( JSON * dataFile, StringHash< OvrMetaDatum * > & outMetaData ) const
{
	if ( dataFile == NULL )
	{
		return;
	}

	const JsonReader data( dataFile->GetItemByName( DATA ) );
	if ( data.IsArray() )
	{
		int jsonIndex = 0;
		while ( const JSON * nextElement = data.GetNextArrayElement() )
		{
			const JsonReader jsonDatum( nextElement );
			if ( jsonDatum.IsObject() )
			{
				OvrMetaDatum * metaDatum = CreateMetaDatum( "" );
				if ( !metaDatum )
				{
					continue;
				}
				metaDatum->Id = jsonIndex++;
				const JsonReader tags( jsonDatum.GetChildByName( TAGS ) );
				if ( tags.IsArray() )
				{
					while ( const JSON * tagElement = tags.GetNextArrayElement() )
					{
						const JsonReader tag( tagElement );
						if ( tag.IsObject() )
						{
							metaDatum->Tags.PushBack( tag.GetChildStringByName( CATEGORY ) );
						}
					}
				}

				OVR_ASSERT( !metaDatum->Tags.IsEmpty() );

				metaDatum->Url = jsonDatum.GetChildStringByName( URL_INNER );
				ExtractExtendedData( jsonDatum, *metaDatum );

				StringHash< OvrMetaDatum * >::Iterator iter = outMetaData.FindCaseInsensitive( metaDatum->Url );
				if ( iter == outMetaData.End() )
				{
					outMetaData.Add( metaDatum->Url, metaDatum );
				}
				else
				{
					iter->Second = metaDatum;
				}
			}
		}
	}
}


void OvrMetaData::RegenerateCategoryIndices()
{
	for ( int catIndex = 0; catIndex < Categories.GetSizeI(); ++catIndex )
	{
		Category & cat = Categories.At( catIndex );
		cat.DatumIndicies.Clear();
	}

	// Delete any data only tagged as "Favorite" - this is a fix for user created "Favorite" folder which is a special case
	// Not doing this will show photos already favorited that the user cannot unfavorite
	for ( int metaDataIndex = 0; metaDataIndex < MetaData.GetSizeI(); ++metaDataIndex )
	{
		OvrMetaDatum & metaDatum = *MetaData.At( metaDataIndex );
		Array< String > & tags = metaDatum.Tags;

		OVR_ASSERT( metaDatum.Tags.GetSizeI() > 0 );
		if ( tags.GetSizeI() == 1 )
		{
			if ( tags.At( 0 ) == FAVORITES_TAG )
			{
				LOG( "Removing broken metadatum %s", metaDatum.Url.ToCStr() );
				MetaData.RemoveAtUnordered( metaDataIndex );
			}
		}
	}

	// Fix the indices
	for ( int metaDataIndex = 0; metaDataIndex < MetaData.GetSizeI(); ++metaDataIndex )
	{
		OvrMetaDatum & datum = *MetaData.At( metaDataIndex );
		Array< String > & tags = datum.Tags;

		OVR_ASSERT( tags.GetSizeI() > 0 );

		if ( tags.GetSizeI() == 1 )
		{
			OVR_ASSERT( tags.At( 0 ) != FAVORITES_TAG );
		}

		if ( tags.At( 0 ) == FAVORITES_TAG && tags.GetSizeI() > 1 )
		{
			Alg::Swap( tags.At( 0 ), tags.At( 1 ) );
		}

		for ( int tagIndex = 0; tagIndex < tags.GetSizeI(); ++tagIndex )
		{
			const String & tag = tags[ tagIndex ];
			if ( !tag.IsEmpty() )
			{
				if ( Category * category = GetCategory( tag ) )
				{
					LOG( "OvrMetaData inserting index %d for datum %s to %s", metaDataIndex, datum.Url.ToCStr(), category->CategoryTag.ToCStr() );

					// fix the metadata index itself
					datum.Id = metaDataIndex;
					
					// Update the category with the new index
					category->DatumIndicies.PushBack( metaDataIndex );
				}
				else
				{
					WARN( "OvrMetaData::RegenerateCategoryIndices failed to find category with tag %s for datum %s at index %d", 
						tag.ToCStr(), datum.Url.ToCStr(), metaDataIndex );
				}
			}
		}
	}
}

JSON * OvrMetaData::MetaDataToJson() const
{
	JSON * DataFile = JSON::CreateObject();

	// Add version
	DataFile->AddNumberItem( VERSION, Version );

	// Add categories
	JSON * newCategoriesObject = JSON::CreateArray();

	for ( int c = 0; c < Categories.GetSizeI(); ++c )
	{
		if ( JSON * catObject = JSON::CreateObject() )
		{
			const Category & cat = Categories.At( c );
			catObject->AddStringItem( TAG, cat.CategoryTag.ToCStr() );
			catObject->AddStringItem( LABEL, cat.Label.ToCStr() );
			LOG( "OvrMetaData::MetaDataToJson adding category %s", cat.CategoryTag.ToCStr() );
			newCategoriesObject->AddArrayElement( catObject );
		}
	}
	DataFile->AddItem( CATEGORIES, newCategoriesObject );

	// Add meta data
	JSON * newDataObject = JSON::CreateArray();

	for ( int i = 0; i < MetaData.GetSizeI(); ++i )
	{
		const OvrMetaDatum & metaDatum = *MetaData.At( i );

		if ( JSON * datumObject = JSON::CreateObject() )
		{
			ExtendedDataToJson( metaDatum, datumObject );
			datumObject->AddStringItem( URL_INNER, metaDatum.Url.ToCStr() );
			LOG( "OvrMetaData::MetaDataToJson adding datum url %s", metaDatum.Url.ToCStr() );
			if ( JSON * newTagsObject = JSON::CreateArray() )
			{
				for ( int t = 0; t < metaDatum.Tags.GetSizeI(); ++t )
				{
					if ( JSON * tagObject = JSON::CreateObject() )
					{
						tagObject->AddStringItem( CATEGORY, metaDatum.Tags.At( t ).ToCStr() );
						newTagsObject->AddArrayElement( tagObject );
					}
				}

				datumObject->AddItem( TAGS, newTagsObject );
			}
			newDataObject->AddArrayElement( datumObject );
		}
	}
	DataFile->AddItem( DATA, newDataObject );

	return DataFile;
}

TagAction OvrMetaData::ToggleTag( OvrMetaDatum * metaDatum, const String & newTag )
{
	JSON * DataFile = JSON::Load( FilePath );
	if ( DataFile == NULL )
	{
		FAIL( "OvrMetaData failed to load JSON meta file: %s", FilePath.ToCStr() );
	}

	OVR_ASSERT( DataFile );
	OVR_ASSERT( metaDatum );

	// First update the local data
	TagAction action = TAG_ERROR;
	for ( int t = 0; t < metaDatum->Tags.GetSizeI(); ++t )
	{
		if ( metaDatum->Tags.At( t ) == newTag )
		{
			// Handle case which leaves us with no tags - ie. broken state
			if ( metaDatum->Tags.GetSizeI() < 2 )
			{
				WARN( "ToggleTag attempt to remove only tag: %s on %s", newTag.ToCStr(), metaDatum->Url.ToCStr() );
				return TAG_ERROR;
			}
			LOG( "ToggleTag TAG_REMOVED tag: %s on %s", newTag.ToCStr(), metaDatum->Url.ToCStr() );
			action = TAG_REMOVED;
			metaDatum->Tags.RemoveAt( t );
			break;
		}
	}

	if ( action == TAG_ERROR )
	{
		LOG( "ToggleTag TAG_ADDED tag: %s on %s", newTag.ToCStr(), metaDatum->Url.ToCStr() );
		metaDatum->Tags.PushBack( newTag );
		action = TAG_ADDED;
	}

	// Then serialize 
	JSON * newTagsObject = JSON::CreateArray();
	OVR_ASSERT( newTagsObject );

	newTagsObject->Name = TAGS;

	for ( int t = 0; t < metaDatum->Tags.GetSizeI(); ++t )
	{
		if ( JSON * tagObject = JSON::CreateObject() )
		{
			tagObject->AddStringItem( CATEGORY, metaDatum->Tags.At( t ).ToCStr() );
			newTagsObject->AddArrayElement( tagObject );
		}
	}

	if ( JSON * data = DataFile->GetItemByName( DATA ) )
	{
		if ( JSON * datum = data->GetItemByIndex( metaDatum->Id ) )
		{
			if ( JSON * tags = datum->GetItemByName( TAGS ) )
			{
				tags->ReplaceNodeWith( newTagsObject );
				tags->Release();
				DataFile->Save( FilePath );
			}
		}
	}
	DataFile->Release();
	return action;
}

void OvrMetaData::AddCategory( const String & name )
{
	Category cat;
	cat.CategoryTag = name;
	Categories.PushBack( cat );
}

OvrMetaData::Category * OvrMetaData::GetCategory( const String & categoryName )
{
	const int numCategories = Categories.GetSizeI();
	for ( int i = 0; i < numCategories; ++i )
	{
		Category & category = Categories.At( i );
		if ( category.CategoryTag == categoryName )
		{
			return &category;
		}
	}
	return NULL;
}

const OvrMetaDatum & OvrMetaData::GetMetaDatum( const int index ) const
{
	OVR_ASSERT( index >= 0 && index < MetaData.GetSizeI() );
	return *MetaData.At( index );
}


bool OvrMetaData::GetMetaData( const Category & category, Array< const OvrMetaDatum * > & outMetaData ) const
{
	const int numPanos = category.DatumIndicies.GetSizeI();
	for ( int i = 0; i < numPanos; ++i )
	{
		const int metaDataIndex = category.DatumIndicies.At( i );
		OVR_ASSERT( metaDataIndex >= 0 && metaDataIndex < MetaData.GetSizeI() );
		//const OvrMetaDatum * panoData = &MetaData.At( metaDataIndex );
		//LOG( "Getting MetaData %d title %s from category %s", metaDataIndex, panoData->Title.ToCStr(), category.CategoryName.ToCStr() );
		outMetaData.PushBack( MetaData.At( metaDataIndex ) );
	}
	return true;
}

bool OvrMetaData::ShouldAddFile( const char * filename, const OvrMetaDataFileExtensions & fileExtensions ) const
{
	const int pathLen = OVR_strlen( filename );
	for ( int index = 0; index < fileExtensions.BadExtensions.GetSizeI(); ++index )
	{
		const String & ext = fileExtensions.BadExtensions.At( index );
		const int extLen = ext.GetLengthI();
		if ( pathLen > extLen && OVR_stricmp( filename + pathLen - extLen, ext.ToCStr() ) == 0 )
		{
			return false;
		}
	}

	for ( int index = 0; index < fileExtensions.GoodExtensions.GetSizeI(); ++index )
	{
		const String & ext = fileExtensions.GoodExtensions.At( index );
		const int extLen = ext.GetLengthI();
		if ( pathLen > extLen && OVR_stricmp( filename + pathLen - extLen, ext.ToCStr() ) == 0 )
		{
			return true;
		}
	}

	return false;
}

void OvrMetaData::SetCategoryDatumIndicies( const int index, const Array< int >& datumIndicies )
{
	OVR_ASSERT( index < Categories.GetSizeI() );

	if ( index < Categories.GetSizeI() )
	{
		Categories[index].DatumIndicies = datumIndicies;
	}
}

}
