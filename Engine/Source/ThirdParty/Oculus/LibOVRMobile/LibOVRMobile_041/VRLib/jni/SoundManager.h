/************************************************************************************

Filename    :   SoundManager.h
Content     :   Sound asset manager via json definitions
Created     :   October 22, 2013
Authors     :   Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#if !defined( OVR_SoundManager_h )
#define OVR_SoundManager_h

#include "Kernel/OVR_StringHash.h"

namespace OVR {

class JSON;

class OvrSoundManager
{
public:
	OvrSoundManager() {}

	void	LoadSoundAssets();
	bool	GetSound( const char * soundName, String & outSound );
	
private:
	void	LoadSoundAssetsFromJsonObject( const String & url, JSON * dataFile );
	void	LoadSoundAssetsFromPackage( const String & url, const char * jsonFile );

	StringHash< String >  SoundMap;	// Maps hashed sound name to sound asset url
};

}

#endif