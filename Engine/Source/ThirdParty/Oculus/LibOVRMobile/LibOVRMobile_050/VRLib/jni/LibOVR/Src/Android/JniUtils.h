/************************************************************************************

Filename    :   JniUtils.h
Content     :   JNI utility functions
Created     :   October 21, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_JniUtils_h
#define OVR_JniUtils_h

#include <jni.h>
#include "Kernel/OVR_Types.h"
#include "LogUtils.h"

// Use this EVERYWHERE and you can insert your own catch here if you have string references leaking.
// Even better, use the JavaString / JavaUTFChars classes instead and they will free resources for
// you automatically.
jobject ovr_NewStringUTF( JNIEnv * jni, char const * str );
char const * ovr_GetStringUTFChars( JNIEnv * jni, jstring javaStr, jboolean * isCopy );

//==============================================================
// JavaObject
//
// Releases a java object reference on destruction
class JavaObject
{
public:
	JavaObject( JNIEnv * jni_, jobject const JObject_ ) :
		jni( jni_ ),
		JObject( JObject_ )
	{
		OVR_ASSERT( jni != NULL );
	}
	~JavaObject()
	{
		OVR_ASSERT( jni != NULL && JObject != NULL );
		jni->DeleteLocalRef( JObject );
		if ( jni->ExceptionOccurred() )
		{
			LOG( "JNI exception occured calling DeleteLocalRef!" );
		}
		jni = NULL;
		JObject = NULL;
	}

	jobject			GetJObject() const { return JObject; }
	JNIEnv *		GetJNI() const { return jni; }

protected:
	void			SetJObject( jobject const & obj ) { JObject = obj; }

private:
	JNIEnv *		jni;
	jobject			JObject;
};

//==============================================================
// JavaString
//
// Creates a java string on construction and releases it on destruction.
class JavaString : public JavaObject
{
public:
	JavaString( JNIEnv * jni_, char const * str ) :
		JavaObject( jni_, NULL )
	{
		SetJObject( ovr_NewStringUTF( GetJNI(), str ) );
		if ( GetJNI()->ExceptionOccurred() )
		{
			LOG( "JNI exception occured calling NewStringUTF!" );
		}
	}

	JavaString( JNIEnv * jni_, jstring JString_ ) :
		JavaObject( jni_, JString_ )
	{
		OVR_ASSERT( JString_ != NULL );
	}

	jstring			GetJString() const { return static_cast< jstring >( GetJObject() ); }
};

//==============================================================
// JavaUTFChars
//
// Gets a java string object as a buffer of UTF characters and
// releases the buffer on destruction.
// Use this only if you need to store a string reference and access
// the string as a char buffer of UTF8 chars.  If you only need
// to store and release a reference to a string, use JavaString.
class JavaUTFChars : public JavaString
{
public:
	JavaUTFChars( JNIEnv * jni_, jstring const JString_ ) :
		JavaString( jni_, JString_ ),
		UTFString( NULL )
	{
		UTFString = ovr_GetStringUTFChars( GetJNI(), GetJString(), NULL );
		if ( GetJNI()->ExceptionOccurred() )
		{
			LOG( "JNI exception occured calling GetStringUTFChars!" );
		}
	}

	~JavaUTFChars()
	{
		OVR_ASSERT( UTFString != NULL );
		GetJNI()->ReleaseStringUTFChars( GetJString(), UTFString );
		if ( GetJNI()->ExceptionOccurred() )
		{
			LOG( "JNI exception occured calling ReleaseStringUTFChars!" );
		}
	}

	char const * ToStr() const { return UTFString; }
	operator char const * () const { return UTFString; }

private:
	char const *	UTFString;
};

// This must be called by a function called directly from a java thread,
// preferably from JNI_OnLoad().  It will fail if called from a pthread created
// in native code, or from a NativeActivity due to the class-lookup issue:
// http://developer.android.com/training/articles/perf-jni.html#faq_FindClass
jclass		ovr_GetGlobalClassReference( JNIEnv * jni, const char * className );

jmethodID	ovr_GetMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature );
jmethodID	ovr_GetStaticMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature );

// get the code path of the current package.
const char * ovr_GetPackageCodePath( JNIEnv * jni, jclass activityClass, jobject activityObject, char * packageCodePath, int const maxLen );

// Get the current package name, for instance "com.oculus.home".
const char * ovr_GetCurrentPackageName( JNIEnv * jni, jclass activityClass, jobject activityObject, char * packageName, int const maxLen );

// Get the current activity class name, for instance "com.oculusvr.vrlib.PlatformActivity".
const char * ovr_GetCurrentActivityName( JNIEnv * jni, jobject activityObject, char * className, int const maxLen );

// For instance packageName = "com.oculus.home".
bool ovr_IsCurrentPackage( JNIEnv * jni, jclass activityClass, jobject activityObject, const char * packageName );

// For instance className = "com.oculusvr.vrlib.PlatformActivity".
bool ovr_IsCurrentActivity( JNIEnv * jni, jobject activityObject, const char * className );

void ovr_LoadDevConfig( bool const forceReload );
const char * ovr_GetHomePackageName( char * packageName, int const maxLen );
bool ovr_IsOculusHomePackage( JNIEnv * jni, jclass activityClass, jobject activityObject );

#endif	// OVR_JniUtils_h
