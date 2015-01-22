
/************************************************************************************

Filename    :   App.h
Content     :   Query Android Build strings through JNI
Created     :   July 12, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVR_NATIVE_BUILD_STRINGS_H
#define OVR_NATIVE_BUILD_STRINGS_H

#include <jni.h>
#include "Kernel/OVR_String.h"
#include "VrApi.h"

namespace OVR
{

class NativeBuildStrings
{
public:
	NativeBuildStrings( JNIEnv *env );

	char const * GetBuildString( eBuildString const str ) const;

private:
	String	BuildStrings[BUILDSTR_MAX];

	JNIEnv *env;
	jclass	BuildClass;
	String	GetFieldString( const char * name );
};

}	// namespace OVR

#endif // OVR_NATIVE_BUILD_STRINGS_H

