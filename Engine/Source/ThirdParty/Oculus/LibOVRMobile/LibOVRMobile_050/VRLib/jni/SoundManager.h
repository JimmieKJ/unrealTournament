/************************************************************************************

Filename    :   SoundManager.h
Content     :   Sound asset manager via json definitions
Created     :   October 22, 2013
Authors     :   Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

Description :

SoundManager is a simple sound asset management class which allows sound assets to be easily replaced without recompilation. 
SoundManager is driven by a JSON file in which sounds are mapped as key-value pairs, where a value is the actual path to the wav file. 

For example:

	"sv_touch_active" : "sv_touch_active.wav"

These are defined in a json file: sound_assets.json.
Typically, we first load "res/raw/sound_assets.json" which references the wav files within that folder in VRLib.
In addition, we also load an app specific definition file in the app: "assets/sound_assets.json". 
The app file may define app specific sounds placed next to it in the assets folder. Additionally, it may
also be used to override the VRLib sounds by either redefining the sound as an empty string to remove the sound or
define it to point at a new sound file - without the need to modify the code that actually plays the sound.

For development, SoundManger checks for a sound_assets.json within the Oculus folder either on the internal or external
sdcard. If this file is found, it is solely used to load sounds.  

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
	bool	HasSound( const char * soundName );
	bool	GetSound( const char * soundName, String & outSound );
	
private:
	void	LoadSoundAssetsFromJsonObject( const String & url, JSON * dataFile );
	void	LoadSoundAssetsFromPackage( const String & url, const char * jsonFile );

	StringHash< String >  SoundMap;	// Maps hashed sound name to sound asset url
};

}

#endif