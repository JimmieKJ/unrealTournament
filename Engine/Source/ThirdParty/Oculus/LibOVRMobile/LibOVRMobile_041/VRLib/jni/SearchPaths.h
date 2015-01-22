
/************************************************************************************

Filename    :   SearchPaths.h
Content     :   Manages multiple content locations
Created     :   Septembet 05, 2014
Authors     :   Warsam Osman

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


************************************************************************************/
#ifndef OVR_SearchPaths_h
#define OVR_SearchPaths_h

#include "Kernel/OVR_String.h"
#include "Kernel/OVR_Array.h"

namespace OVR {

//==============================================================
// SearchPaths
// encapsulates several paths that need to be checked for content
// the paths are stored in priority
class SearchPaths
{
public:
					SearchPaths();

	String			GetFullPath( const String & relativePath ) const;

	// Return false if it fails to find the relativePath in any of the search locations
	bool			GetFullPath( char const * relativePath, char * outPath, const int outMaxLen ) const;
	bool			GetFullPath( char const * relativePath, String & outPath ) const;

	bool			ToRelativePath( char const * fullPath, char * outPath, const int outMaxLen ) const;
	bool			ToRelativePath( char const * fullPath, String & outPath ) const;

	int				NumPaths() const { return Paths.GetSizeI(); }
	char const *	GetPath( int const i ) const { return Paths[i]; }

private:
	Array< String > Paths;
};

}	// namespace OVR

#endif	// OVR_SearchPaths_h