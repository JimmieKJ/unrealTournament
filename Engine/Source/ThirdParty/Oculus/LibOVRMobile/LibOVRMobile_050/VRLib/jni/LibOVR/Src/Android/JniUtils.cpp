/************************************************************************************

Filename    :   JniUtils.h
Content     :   JNI utility functions
Created     :   October 21, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "JniUtils.h"

#include "Kernel/OVR_Std.h"
#include "Kernel/OVR_JSON.h"

jobject ovr_NewStringUTF( JNIEnv * jni, char const * str )
{
	//if ( strstr( str, "com.oculus.home" ) != NULL || strstr( str, "globalMenu" ) != NULL ) {
	//	DROIDLOG( "OvrJNI", "ovr_NewStringUTF: These are not the strings your looking for: %s", str );
	//}
	return jni->NewStringUTF( str );
}

char const * ovr_GetStringUTFChars( JNIEnv * jni, jstring javaStr, jboolean * isCopy ) 
{
	char const * str = jni->GetStringUTFChars( javaStr, isCopy );
	//if ( strstr( str, "com.oculus.home" ) != NULL || strstr( str, "globalMenu" ) != NULL ) {
	//	DROIDLOG( "OvrJNI", "ovr_GetStringUTFChars: These are not the strings your looking for: %s", str );
	//}
	return str;
}

jclass ovr_GetGlobalClassReference( JNIEnv * jni, const char * className )
{
	jclass lc = jni->FindClass(className);
	if ( lc == 0 )
	{
		FAIL( "FindClass( %s ) failed", className );
	}
	// Turn it into a global ref, so we can safely use it in other threads
	jclass gc = (jclass)jni->NewGlobalRef( lc );

	jni->DeleteLocalRef( lc );

	return gc;
}

jmethodID ovr_GetMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature )
{
	const jmethodID methodId = jni->GetMethodID( jniclass, name, signature );
	if ( !methodId )
	{
		FAIL( "couldn't get %s, %s", name, signature );
	}
	return methodId;
}

jmethodID ovr_GetStaticMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature )
{
	const jmethodID method = jni->GetStaticMethodID( jniclass, name, signature );
	if ( !method )
	{
		FAIL( "couldn't get %s, %s", name, signature );
	}
	return method;
}

const char * ovr_GetPackageCodePath( JNIEnv * jni, jclass activityClass, jobject activityObject, char * packageCodePath, int const maxLen )
{
	if ( packageCodePath == NULL || maxLen < 1 )
	{
		return packageCodePath;
	}

	packageCodePath[0] = '\0';

	jmethodID getPackageCodePathId = jni->GetMethodID( activityClass, "getPackageCodePath", "()Ljava/lang/String;" );
	if ( getPackageCodePathId == 0 )
	{
		LOG( "Failed to find getPackageCodePath on class %llu, object %llu", (long long unsigned int)activityClass, (long long unsigned int)activityObject );
		return packageCodePath;
	}

	JavaUTFChars result( jni, (jstring)jni->CallObjectMethod( activityObject, getPackageCodePathId ) );
	if ( !jni->ExceptionOccurred() )
	{
		const char * path = result.ToStr();
		if ( path != NULL ) 
		{
			OVR::OVR_sprintf( packageCodePath, maxLen, "%s", path );
		}
	}
	else 
	{
		jni->ExceptionClear();
		LOG( "Cleared JNI exception" );
	}

	LOG( "ovr_GetPackageCodePath() = '%s'", packageCodePath );
	return packageCodePath;
}

const char * ovr_GetCurrentPackageName( JNIEnv * jni, jclass activityClass, jobject activityObject, char * packageName, int const maxLen )
{
	packageName[0] = '\0';

	jclass curActivityClass = jni->GetObjectClass( activityObject );
	jmethodID getPackageNameId = jni->GetMethodID( curActivityClass, "getPackageName", "()Ljava/lang/String;");
	if ( getPackageNameId != 0 )
	{
		JavaUTFChars result( jni, (jstring)jni->CallObjectMethod( activityObject, getPackageNameId ) );
		if ( !jni->ExceptionOccurred() )
		{
			const char * currentPackageName = result.ToStr();
			if ( currentPackageName != NULL )
			{
				OVR::OVR_sprintf( packageName, maxLen, "%s", currentPackageName );
			}
		}
		else
		{
			jni->ExceptionClear();
			LOG( "Cleared JNI exception" );
		}
	}

	jni->DeleteLocalRef( curActivityClass );

	LOG( "ovr_GetCurrentPackageName() = %s", packageName );
	return packageName;
}

const char * ovr_GetCurrentActivityName( JNIEnv * jni, jobject activityObject, char * className, int const maxLen )
{
	className[0] = '\0';

	jclass curActivityClass = jni->GetObjectClass( activityObject );
	jmethodID getClassMethodId = jni->GetMethodID( curActivityClass, "getClass", "()Ljava/lang/Class;" );
	if ( getClassMethodId != 0 )
	{
		jobject classObj = jni->CallObjectMethod( activityObject, getClassMethodId );
		jclass activityClass = jni->GetObjectClass( classObj );

		jmethodID getNameMethodId = jni->GetMethodID( activityClass, "getName", "()Ljava/lang/String;" );
		if ( getNameMethodId != 0 )
		{
			JavaUTFChars utfCurrentClassName( jni, (jstring)jni->CallObjectMethod( classObj, getNameMethodId ) );
			const char * currentClassName = utfCurrentClassName.ToStr();
			if ( currentClassName != NULL )
			{
				OVR::OVR_sprintf( className, maxLen, "%s", currentClassName );
			}
		}

		jni->DeleteLocalRef( classObj );
		jni->DeleteLocalRef( activityClass );
	}

	jni->DeleteLocalRef( curActivityClass );

	LOG( "ovr_GetCurrentActivityName() = %s", className );
	return className;
}

bool ovr_IsCurrentPackage( JNIEnv * jni, jclass activityClass, jobject activityObject, const char * packageName )
{
	char currentPackageName[128];
	ovr_GetCurrentPackageName( jni, activityClass, activityObject, currentPackageName, sizeof( currentPackageName ) );
	const bool isCurrentPackage = ( OVR::OVR_stricmp( currentPackageName, packageName ) == 0 );
	LOG( "ovr_IsCurrentPackage( %s ) = %s", packageName, isCurrentPackage ? "true" : "false" );
	return isCurrentPackage;
}

bool ovr_IsCurrentActivity( JNIEnv * jni, jobject activityObject, const char * className )
{
	char currentClassName[128];
	ovr_GetCurrentActivityName( jni, activityObject, currentClassName, sizeof( currentClassName ) );
	const bool isCurrentActivity = ( OVR::OVR_stricmp( currentClassName, className ) == 0 );
	LOG( "ovr_IsCurrentActivity( %s ) = %s", className, isCurrentActivity ? "true" : "false" );
	return isCurrentActivity;
}

static OVR::JSON *	DevConfig = NULL;

void ovr_LoadDevConfig( bool const forceReload )
{
#if !defined( RETAIL )
	if ( DevConfig != NULL )
	{
		if ( !forceReload )
		{
			return;	// already loading and not forcing a reload
		}
		DevConfig->Release();
		DevConfig = NULL;
	}

	// load the dev config if possible
	FILE * fp = fopen( "/storage/extSdCard/Oculus/dev.cfg", "rb" );	// check for existence
	if ( fp != NULL )
	{
		fclose( fp );
		DevConfig = OVR::JSON::Load( "/storage/extSdCard/Oculus/dev.cfg" );
	}
#endif
}

//#define DEFAULT_DASHBOARD_PACKAGE_NAME "com.Oculus.UnitySample" -- for testing only
#define DEFAULT_DASHBOARD_PACKAGE_NAME "com.oculus.home"

const char * ovr_GetHomePackageName( char * packageName, int const maxLen )
{
#if defined( RETAIL )
	OVR::OVR_sprintf( packageName, maxLen, "%s", DEFAULT_DASHBOARD_PACKAGE_NAME );
	return packageName;
#else
	// make sure the dev config is loaded
	ovr_LoadDevConfig( false );

	// set to default value
	OVR::OVR_sprintf( packageName, maxLen, "%s", DEFAULT_DASHBOARD_PACKAGE_NAME );

	if ( DevConfig != NULL )
	{
		// try to get it from the Launcher/Packagename JSON entry
		OVR::JSON * jsonLauncher = DevConfig->GetItemByName( "Launcher" );
		if ( jsonLauncher != NULL ) 
		{
			OVR::JSON * jsonPackageName = jsonLauncher->GetItemByName( "PackageName" );
			if ( jsonPackageName != NULL )
			{
				OVR::OVR_sprintf( packageName, maxLen, "%s", jsonPackageName->GetStringValue().ToCStr() );
				LOG( "Found Home package name: '%s'", packageName );
			}
			else
			{
				LOG( "No override for Home package name found." );
			}
		}
	}
	return packageName;
#endif
}

bool ovr_IsOculusHomePackage( JNIEnv * jni, jclass activityClass, jobject activityObject )
{
	char homePackageName[128];
	ovr_GetHomePackageName( homePackageName, sizeof( homePackageName ) );
	return ovr_IsCurrentPackage( jni, activityClass, activityObject, homePackageName );
}
