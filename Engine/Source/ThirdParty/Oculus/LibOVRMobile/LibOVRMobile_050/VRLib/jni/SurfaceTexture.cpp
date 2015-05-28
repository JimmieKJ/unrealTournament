/************************************************************************************

Filename    :   SurfaceTexture.cpp
Content     :   Interface to Android SurfaceTexture objects
Created     :   September 17, 2013
Authors		:	John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "SurfaceTexture.h"

#include <stdlib.h>

#include "Android/GlUtils.h"
#include "Android/LogUtils.h"


namespace OVR {


SurfaceTexture::SurfaceTexture( JNIEnv * jni_ ) : nanoTimeStamp( 0 ) {
	jni = jni_;

	// Gen a gl texture id for the java SurfaceTexture to use.
	glGenTextures( 1, &textureId );
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
	glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

	static const char * className = "android/graphics/SurfaceTexture";
	const jclass surfaceTextureClass = jni->FindClass(className);
	if ( surfaceTextureClass == 0 ) {
		FAIL( "FindClass( %s ) failed", className );
	}

	// find the constructor that takes an int
	const jmethodID constructor = jni->GetMethodID( surfaceTextureClass, "<init>", "(I)V" );
	if ( constructor == 0 ) {
		FAIL( "GetMethodID( <init> ) failed" );
	}

	jobject obj = jni->NewObject( surfaceTextureClass, constructor, textureId );
	if ( obj == 0 ) {
		FAIL( "NewObject() failed" );
	}

	javaObject = jni->NewGlobalRef( obj );
	if ( javaObject == 0 ) {
		FAIL( "NewGlobalRef() failed" );
	}

	// Now that we have a globalRef, we can free the localRef
	jni->DeleteLocalRef( obj );

    updateTexImageMethodId = jni->GetMethodID( surfaceTextureClass, "updateTexImage", "()V" );
    if ( !updateTexImageMethodId ) {
    	FAIL( "couldn't get updateTexImageMethodId" );
    }

    getTimestampMethodId = jni->GetMethodID( surfaceTextureClass, "getTimestamp", "()J" );
    if ( !getTimestampMethodId ) {
    	FAIL( "couldn't get getTimestampMethodId" );
    }

	setDefaultBufferSizeMethodId = jni->GetMethodID( surfaceTextureClass, "setDefaultBufferSize", "(II)V" );
    if ( !setDefaultBufferSizeMethodId ) {
		FAIL( "couldn't get setDefaultBufferSize" );
    }

	// jclass objects are localRefs that need to be freed
	jni->DeleteLocalRef( surfaceTextureClass );
}

SurfaceTexture::~SurfaceTexture() {
	if ( textureId ) {
		glDeleteTextures( 1, &textureId );
		textureId = 0;
	}
	if ( javaObject ) {
		jni->DeleteGlobalRef( javaObject );
		javaObject = 0;
	}
}

void SurfaceTexture::SetDefaultBufferSize( int width, int height ) {
	jni->CallVoidMethod( javaObject, setDefaultBufferSizeMethodId, width, height );
}

void SurfaceTexture::Update() {
    // latch the latest movie frame to the texture
    if ( !javaObject ) {
    	return;
    }

   jni->CallVoidMethod( javaObject, updateTexImageMethodId );
   nanoTimeStamp = jni->CallLongMethod( javaObject, getTimestampMethodId );
}


}	// namespace OVR
