/************************************************************************************

Filename    :   VrApi.h
Content     :   Primary C level interface necessary for VR, App builds on this
Created     :   July, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "VrApi.h"

#include <unistd.h>						// gettid, usleep, etc
#include <jni.h>

#include "Log.h"
#include "Kernel/OVR_String.h"			// for ReadFreq()
#include "OVR_JSON.h"
#include "../SearchPaths.h"

static OVR::JSON * DevConfig = NULL;

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

jmethodID ovr_GetStaticMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature )
{
	const jmethodID method = jni->GetStaticMethodID( jniclass, name, signature );
	if ( !method )
	{
		FAIL( "couldn't get %s, %s", name, signature );
	}
	return method;
}

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
	OVR::SearchPaths sp;
	for ( int i = 0; i < sp.NumPaths(); i++ )
	{
		char configPath[1024];
		sp.GetFullPath( "Oculus/dev.cfg", configPath, sizeof( configPath ) );
		FILE * fp = fopen( configPath, "rb" );	// check for existence
		if ( fp != NULL )
		{
			fclose( fp );
			DevConfig = OVR::JSON::Load( configPath );
			if ( DevConfig != NULL )
			{
				break;
			}
		}
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

const char * ovr_GetCurrentPackageName( JNIEnv * jni, jobject activityObject, char * packageName, int const maxLen )
{
	packageName[0] = '\0';

	jclass curActivityClass = jni->GetObjectClass( activityObject );
	jmethodID getPackageNameId = jni->GetMethodID( curActivityClass, "getPackageName", "()Ljava/lang/String;");
	if ( getPackageNameId != 0 )
	{
		jstring result = (jstring)jni->CallObjectMethod( activityObject, getPackageNameId );
		if ( !jni->ExceptionOccurred() )
		{
			const char * currentPackageName = jni->GetStringUTFChars( result, NULL );
			if ( currentPackageName != NULL )
			{
				OVR::OVR_sprintf( packageName, maxLen, "%s", currentPackageName );
				jni->ReleaseStringUTFChars( result, currentPackageName );
			}
		}
		else
		{
			jni->ExceptionClear();
			LOG( "Cleared JNI exception" );
		}
	}

	jni->DeleteLocalRef( curActivityClass );

	LOG ( "ovr_GetCurrentPackageName() = %s", packageName );
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
			jstring javaClassName = (jstring)jni->CallObjectMethod( classObj, getNameMethodId );

			const char * currentClassName = jni->GetStringUTFChars( javaClassName, NULL );
			if ( currentClassName != NULL )
			{
				OVR::OVR_sprintf( className, maxLen, "%s", currentClassName );
				jni->ReleaseStringUTFChars( javaClassName, currentClassName );
			}
		}

		jni->DeleteLocalRef( classObj );
		jni->DeleteLocalRef( activityClass );
	}

	jni->DeleteLocalRef( curActivityClass );

	LOG ( "ovr_GetCurrentActivityName() = %s", className );
	return className;
}

bool ovr_IsCurrentPackage( JNIEnv * jni, jobject activityObject, const char * packageName )
{
	char currentPackageName[128];
	ovr_GetCurrentPackageName( jni, activityObject, currentPackageName, sizeof( currentPackageName ) );
	const bool isCurrentPackage = ( OVR::OVR_stricmp( currentPackageName, packageName ) == 0 );
	LOG ( "ovr_IsCurrentPackage( %s ) = %s", packageName, isCurrentPackage ? "true" : "false" );
	return isCurrentPackage;
}

bool ovr_IsCurrentActivity( JNIEnv * jni, jobject activityObject, const char * className )
{
	char currentClassName[128];
	ovr_GetCurrentActivityName( jni, activityObject, currentClassName, sizeof( currentClassName ) );
	const bool isCurrentActivity = ( OVR::OVR_stricmp( currentClassName, className ) == 0 );
	LOG ( "ovr_IsCurrentActivity( %s ) = %s", className, isCurrentActivity ? "true" : "false" );
	return isCurrentActivity;
}

bool ovr_IsOculusHomePackage( JNIEnv * jni, jobject activityObject )
{
	char homePackageName[128];
	ovr_GetHomePackageName( homePackageName, sizeof( homePackageName ) );
	return ovr_IsCurrentPackage( jni, activityObject, homePackageName );
}
