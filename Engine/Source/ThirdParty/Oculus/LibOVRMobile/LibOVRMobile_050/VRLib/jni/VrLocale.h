/************************************************************************************

Filename    :   VrLocale.h
Content     :   Localization and internationalization (i18n) functionality.
Created     :   11/24/2914
Authors     :   Jonathan Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#if !defined( OVR_LOCALE_H )
#define OVR_LOCALE_H

#include "Kernel/OVR_String.h"
#include <stdarg.h>
#include "jni.h"

struct ovrMobile;

namespace OVR {

//==============================================================
// VrLocale
//
// Holds all localization functions.
class VrLocale
{
public:
	static char const *	LOCALIZED_KEY_PREFIX;
	static size_t		LOCALIZED_KEY_PREFIX_LEN;
		
	// Get's a localized UTF-8-encoded string from the string table.
	static bool 	GetString( JNIEnv* jni, jobject activityObject, char const * key, char const * defaultOut, String & out );

	// Takes a UTF8 string and returns an identifier that can be used as an Android string id.
	static String	MakeStringIdFromUTF8( char const * str );

	// Takes an ANSI string and returns an identifier that can be used as an Android string id. 
	static String	MakeStringIdFromANSI( char const * str );

	// Localization : Returns xliff formatted string
	// These are set to const char * to make sure that's all that's passed in - we support up to 9, add more functions as needed
	static String GetXliffFormattedString( const String & inXliffStr, const char * arg1 );
	static String GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2 );
	static String GetXliffFormattedString( const String & inXliffStr, const char * arg1, const char * arg2, const char * arg3 );

	static String ToString( char const * fmt, float const f );
	static String ToString( char const * fmt, int const i );

	static jclass VrActivityClass;
};

} // namespace OVR

#endif	// OVR_LOCALE_H
