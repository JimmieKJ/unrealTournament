/************************************************************************************

Filename    :   SoundManager.cpp
Content     :   Sound asset manager via json definitions
Created     :   October 22, 2013
Authors     :   Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#include "SoundManager.h"

#include "Kernel/OVR_JSON.h"
#include "Android/LogUtils.h"

#include "PathUtils.h"
#include "PackageFiles.h"

namespace OVR {

static const char * DEV_SOUNDS_RELATIVE = "Oculus/sound_assets.json";
static const char * VRLIB_SOUNDS = "res/raw/sound_assets.json";
static const char * APP_SOUNDS = "assets/sound_assets.json";

void OvrSoundManager::LoadSoundAssets()
{
	Array<String> searchPaths;
	searchPaths.PushBack( "/storage/extSdCard/" );
	searchPaths.PushBack( "/sdcard/" );

	// First look for sound definition using SearchPaths for dev
	String foundPath;
	if ( GetFullPath( searchPaths, DEV_SOUNDS_RELATIVE, foundPath ) )
	{
		JSON * dataFile = JSON::Load( foundPath.ToCStr() );
		if ( dataFile == NULL )
		{
			FAIL( "OvrSoundManager::LoadSoundAssets failed to load JSON meta file: %s", foundPath.ToCStr( ) );
		}
		foundPath.StripTrailing( "sound_assets.json" );
		LoadSoundAssetsFromJsonObject( foundPath, dataFile );
	}
	else // if that fails, we are in release - load sounds from vrlib/res/raw and the assets folder
	{
		if ( ovr_PackageFileExists( VRLIB_SOUNDS ) )
		{
			LoadSoundAssetsFromPackage( "res/raw/", VRLIB_SOUNDS );
		}
		if ( ovr_PackageFileExists( APP_SOUNDS ) )
		{
			LoadSoundAssetsFromPackage( "", APP_SOUNDS );
		}
	}

	if ( SoundMap.IsEmpty() )
	{
#if defined( OVR_BUILD_DEBUG )
		FAIL( "SoundManger - failed to load any sound definition files!" );
#else
		WARN( "SoundManger - failed to load any sound definition files!" );
#endif
	}
}

bool OvrSoundManager::HasSound( const char * soundName )
{
	StringHash< String >::ConstIterator soundMapping = SoundMap.Find( soundName );
	return ( soundMapping != SoundMap.End() );
}

bool OvrSoundManager::GetSound( const char * soundName, String & outSound )
{
	StringHash< String >::ConstIterator soundMapping = SoundMap.Find( soundName );
	if ( soundMapping != SoundMap.End() )
	{
		OVR_ASSERT( soundMapping->Second );
		outSound = soundMapping->Second;
		return true;
	}
	else
	{
		WARN( "OvrSoundManager::GetSound failed to find %s", soundName );
	}

	return false;
}

void OvrSoundManager::LoadSoundAssetsFromPackage( const String & url, const char * jsonFile )
{
	int bufferLength = 0;
	void * 	buffer = NULL;
	ovr_ReadFileFromApplicationPackage( jsonFile, bufferLength, buffer );
	if ( !buffer )
	{
		FAIL( "OvrSoundManager::LoadSoundAssetsFromPackage failed to read %s", jsonFile );
	}

	JSON * dataFile = JSON::Parse( reinterpret_cast< char * >( buffer ) );
	if ( !dataFile )
	{
		FAIL( "OvrSoundManager::LoadSoundAssetsFromPackage failed json parse on %s", jsonFile );
	}
	free( buffer );

	LoadSoundAssetsFromJsonObject( url, dataFile );
}

void OvrSoundManager::LoadSoundAssetsFromJsonObject( const String & url, JSON * dataFile )
{
	OVR_ASSERT( dataFile );

	// Read in sounds - add to map
	JSON* sounds = dataFile->GetItemByName( "Sounds" );
	OVR_ASSERT( sounds );
	
	const unsigned numSounds = sounds->GetItemCount();

	for ( unsigned i = 0; i < numSounds; ++i )
	{
		const JSON* sound = sounds->GetItemByIndex( i );
		OVR_ASSERT( sound );

		String fullPath( url );
		fullPath.AppendString( sound->GetStringValue( ) );

		// Do we already have this sound?
		StringHash< String >::ConstIterator soundMapping = SoundMap.Find( sound->Name );
		if ( soundMapping != SoundMap.End() )
		{
			LOG( "SoundManger - adding Duplicate sound %s with asset %s", sound->Name.ToCStr( ), fullPath.ToCStr( ) );
			SoundMap.Set( sound->Name, fullPath );
		}
		else // add new sound
		{
			LOG( "SoundManger read in: %s -> %s", sound->Name.ToCStr( ), fullPath.ToCStr( ) );
			SoundMap.Add( sound->Name, fullPath );
		}
	}

	dataFile->Release();
}

}
