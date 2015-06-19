/************************************************************************************

Filename    :   LocalPreferences.cpp
Content     :   Trivial local key / value data store
Created     :   August, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "LocalPreferences.h"

#include <stdio.h>

#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_String.h"
#include "Kernel/OVR_MemBuffer.h"
#include "Android/LogUtils.h"

static bool AllowLocalPreferencesFile = false;
static const char * localPrefsFile = "/sdcard/.oculusprefs";

class KeyPair
{
public:
	OVR::String	Key;
	OVR::String	Value;
};

OVR::Array<KeyPair>	LocalPreferences;

static OVR::String ParseToken( const char * txt, const int start, const int stop, int & stopPoint  )
{
	// skip leading whitespace
	int	startPoint = start;
	for ( ; startPoint < stop && txt[startPoint] <= ' ' ; startPoint++ )
	{
	}

	for ( stopPoint = startPoint ; stopPoint < stop && txt[stopPoint] > ' ' ; stopPoint++ )
	{
	}

	return OVR::String( txt + startPoint, stopPoint - startPoint );
}

// Called on each resume, synchronously fetches the data.
void ovr_UpdateLocalPreferences()
{
	if ( !AllowLocalPreferencesFile )
	{
		return;
	}

	LocalPreferences.Clear();
	OVR::MemBufferFile	file( localPrefsFile );

	// Don't bother checking for duplicate names, later ones
	// will be ignored and removed on write.
	const char * txt = (const char *)file.Buffer;
	LOG( "%s is length %i", localPrefsFile, file.Length );
	for ( int ofs = 0 ; ofs < file.Length ; )
	{
		KeyPair	kp;
		int stopPos;
		kp.Key = ParseToken( txt, ofs, file.Length, stopPos );
		ofs = stopPos;
		kp.Value = ParseToken( txt, ofs, file.Length, stopPos );
		ofs = stopPos;

		if ( kp.Key.GetSize() > 0 )
		{
			LOG( "%s = %s", kp.Key.ToCStr(), kp.Value.ToCStr() );
			LocalPreferences.PushBack( kp );
		}
	}
}

// Query the in-memory preferences for a key / value pair.
// If the returned string is not defaultKeyValue, it will remain valid until the next ovr_UpdateLocalPreferences().
const char * ovr_GetLocalPreferenceValueForKey( const char * keyName, const char * defaultKeyValue )
{
	for ( int i = 0 ; i < LocalPreferences.GetSizeI() ; i++ )
	{
		if ( 0 == LocalPreferences[i].Key.CompareNoCase( keyName ) )
		{
			LOG( "Key %s = %s", keyName, LocalPreferences[i].Value.ToCStr() );
			return LocalPreferences[i].Value.ToCStr();
		}
	}
	LOG( "Key %s not found, returning default %s", keyName, defaultKeyValue );

	return defaultKeyValue;
}

// Updates the in-memory data and synchronously writes it to storage.
void ovr_SetLocalPreferenceValueForKey( const char * keyName, const char * keyValue )
{
	LOG( "Set( %s, %s )", keyName, keyValue );

	int i = 0;
	for ( ; i < LocalPreferences.GetSizeI() ; i++ )
	{
		if ( !strcmp( keyName, LocalPreferences[i].Key.ToCStr() ) )
		{
			LocalPreferences[i].Value = keyValue;
		}
	}
	if ( i == LocalPreferences.GetSizeI() )
	{
		KeyPair	kp;
		kp.Key = keyName;
		kp.Value = keyValue;
		LocalPreferences.PushBack( kp );
	}

	// don't write out if prefs are disabled because we could overwrite with default values
	if ( !AllowLocalPreferencesFile )
	{
		return;
	}

	// write the file
	FILE * f = fopen( localPrefsFile, "w" );
	if ( !f )
	{
		LOG( "Couldn't open %s for writing", localPrefsFile );
		return;
	}
	for ( int i = 0 ; i < LocalPreferences.GetSizeI() ; i++ )
	{
		fprintf( f, "%s %s", LocalPreferences[i].Key.ToCStr(), LocalPreferences[i].Value.ToCStr() );
	}
	fclose( f );
}

void ovr_SetAllowLocalPreferencesFile( const bool allow )
{
	LOG( "ovr_SetAllowLocalPreferences : %d", allow );
	AllowLocalPreferencesFile = allow;
}

void ovr_ShutdownLocalPreferences()
{
	LOG( "ovr_ShutdownLocalPreferences" );
	LocalPreferences.ClearAndRelease();
}
