/************************************************************************************

Filename    :   NativeBuildStrings.cpp
Content     :   Fetch the Android BUILD strings with JNI
Created     :   August, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/


#include "NativeBuildStrings.h"
#include "Log.h"

namespace OVR {

NativeBuildStrings::NativeBuildStrings( JNIEnv *env_ )
{
	env = env_;

	// find the class
	BuildClass = env->FindClass("android/os/Build");

	// find and copy out the fields
	BuildStrings[BUILDSTR_BRAND] = GetFieldString( "BRAND" );
	BuildStrings[BUILDSTR_DEVICE] = GetFieldString( "DEVICE" );
	BuildStrings[BUILDSTR_DISPLAY] = GetFieldString( "DISPLAY" );
	BuildStrings[BUILDSTR_FINGERPRINT] = GetFieldString( "FINGERPRINT" );
	BuildStrings[BUILDSTR_HARDWARE] = GetFieldString( "HARDWARE" );
	BuildStrings[BUILDSTR_HOST] = GetFieldString( "HOST" );
	BuildStrings[BUILDSTR_ID] = GetFieldString( "ID" );
	BuildStrings[BUILDSTR_MODEL] = GetFieldString( "MODEL" );
	BuildStrings[BUILDSTR_PRODUCT] = GetFieldString( "PRODUCT" );
	BuildStrings[BUILDSTR_SERIAL] = GetFieldString( "SERIAL" );
	BuildStrings[BUILDSTR_TAGS] = GetFieldString( "TAGS" );
	BuildStrings[BUILDSTR_TYPE] = GetFieldString( "TYPE" );

	// don't pollute the local reference table
	env->DeleteLocalRef( BuildClass );
	BuildClass = 0;
}

String NativeBuildStrings::GetFieldString( const char * name )
{
	jfieldID field = env->GetStaticFieldID(BuildClass, name , "Ljava/lang/String;");

	// get reference to the string
	jstring jstr = (jstring)env->GetStaticObjectField(BuildClass, field);

	jboolean isCopy;
	const char * cstr = env->GetStringUTFChars(jstr, &isCopy);
	DROIDLOG( "VrApp", "Buildstring %s: %s", name, cstr );

	String	str = String( cstr );

	env->ReleaseStringUTFChars(jstr, cstr);


	return str;
}

char const * NativeBuildStrings::GetBuildString( eBuildString const str ) const
{
	if ( str < 0 || str >= BUILDSTR_MAX )
	{
		return "";
	}

	return BuildStrings[str].ToCStr();
}

} // namespace OVR

