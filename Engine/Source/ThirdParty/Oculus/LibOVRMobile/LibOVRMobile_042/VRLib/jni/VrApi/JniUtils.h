/************************************************************************************

Filename    :   JniUtils.h
Content     :   JNI Utility functions
Created     :   October 21, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVR_JniUtils_h
#define OVR_JniUtils_h

#include <jni.h>

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
			DROIDLOG( "OvrJNI", "JNI exception occured calling DeleteLocalRef!" );
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
		SetJObject( GetJNI()->NewStringUTF( str ) );
		if ( GetJNI()->ExceptionOccurred() )
		{
			DROIDLOG( "OvrJNI", "JNI exception occured calling NewStringUTF!" );
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
		UTFString = GetJNI()->GetStringUTFChars( GetJString(), NULL );
		if ( GetJNI()->ExceptionOccurred() )
		{
			DROIDLOG( "OvrJNI", "JNI exception occured calling GetStringUTFChars!" );
		}
	}

	~JavaUTFChars()
	{
		OVR_ASSERT( UTFString != NULL );
		GetJNI()->ReleaseStringUTFChars( GetJString(), UTFString );
		if ( GetJNI()->ExceptionOccurred() )
		{
			DROIDLOG( "OvrJNI", "JNI exception occured calling ReleaseStringUTFChars!" );
		}
	}

	operator char const * () const { return UTFString; }

private:
	char const *	UTFString;
};

jclass		ovr_GetGlobalClassReference( JNIEnv * jni, const char * className );
jmethodID	ovr_GetStaticMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature );
jmethodID	ovr_GetMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature );

jclass		ovr_GetVrActivityClass();
void		ovr_SetVrActivityClass( jclass clazz );

void		ovr_LoadDevConfig( bool const forceReload );

// Tries to read the Home package name from the dev config. Defaults to "com.oculus.home".
const char * ovr_GetHomePackageName( char * packageName, int const maxLen );

// Get the current package name, for instance "com.oculus.home".
const char * ovr_GetCurrentPackageName( JNIEnv * jni, jobject activityObject, char * packageName, int const maxLen );

// Get the current activity class name, for instance "com.oculusvr.vrlib.PlatformActivity".
const char * ovr_GetCurrentActivityName( JNIEnv * jni, jobject activityObject, char * className, int const maxLen );

// For instance packageName = "com.oculus.home".
bool ovr_IsCurrentPackage( JNIEnv * jni, jobject activityObject, const char * packageName );

// For instance className = "com.oculusvr.vrlib.PlatformActivity".
bool ovr_IsCurrentActivity( JNIEnv * jni, jobject activityObject, const char * className );

// Returns true if this is the Home package.
bool ovr_IsOculusHomePackage( JNIEnv * jni, jobject activityObject );

#endif	// OVR_JniUtils_h
