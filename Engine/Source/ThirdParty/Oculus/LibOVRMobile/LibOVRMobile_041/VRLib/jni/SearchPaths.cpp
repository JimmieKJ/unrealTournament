/************************************************************************************

Filename    :   SearchPaths.cpp
Content     :
Created     :
Authors     :

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include "SearchPaths.h"
#include "VrCommon.h"

namespace OVR {

// TODO: Load from data
SearchPaths::SearchPaths()
{
	Paths.PushBack( "/storage/extSdCard/RetailMedia/" );
	Paths.PushBack( "/storage/extSdCard/" );
	Paths.PushBack( "/sdcard/RetailMedia/" );
	Paths.PushBack( "/sdcard/" );
}

String SearchPaths::GetFullPath( const String & relativePath ) const
{
	if ( FileExists( relativePath ) )
	{
		return relativePath;
	}

	const int numSearchPaths = Paths.GetSizeI();
	for ( int index = 0; index < numSearchPaths; ++index )
	{
		const String fullPath = SearchPaths::Paths.At( index ) + String( relativePath );
		if ( FileExists( fullPath ) )
		{
			return fullPath;
		}
	}

	return String();
}

bool SearchPaths::GetFullPath( char const * relativePath, char * outPath, const int outMaxLen ) const
{
	OVR_ASSERT( outPath != NULL && outMaxLen >= 1 );

	if ( FileExists( relativePath ) )
	{
		OVR_sprintf( outPath, OVR_strlen( relativePath ) + 1, "%s", relativePath );
		return true;
	}

	for ( int i = 0; i < Paths.GetSizeI(); ++i )
	{
		OVR_sprintf( outPath, outMaxLen, "%s%s", Paths[i].ToCStr(), relativePath );
		if ( FileExists( outPath ) )
		{
			return true;	// outpath is now set to the full path
		}
	}
	// just return the relative path if we never found the file
	OVR_sprintf( outPath, outMaxLen, "%s", relativePath );
	return false;
}

bool SearchPaths::GetFullPath( char const * relativePath, String & outPath ) const
{
	char largePath[1024];
	bool result = GetFullPath( relativePath, largePath, sizeof( largePath ) );
	outPath = largePath;
	return result;
}

bool SearchPaths::ToRelativePath( char const * fullPath, char * outPath, const int outMaxLen ) const
{
	// check if the path starts with any of the search paths
	const int n = Paths.GetSizeI();
	for ( int i = 0; i < n; ++i )
	{
		char const * path = Paths[i].ToCStr();
		if ( strstr( fullPath, path ) == fullPath )
		{
			size_t len = OVR_strlen( path );
			OVR_sprintf( outPath, outMaxLen, "%s", fullPath + len );
			return true;
		}
	}
	OVR_sprintf( outPath, outMaxLen, "%s", fullPath );
	return false;
}

bool SearchPaths::ToRelativePath( char const * fullPath, String & outPath ) const
{
	char largePath[1024];
	bool result = ToRelativePath( fullPath, largePath, sizeof( largePath ) );
	outPath = largePath;
	return result;
}

} // namespace OVR
