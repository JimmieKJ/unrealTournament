/************************************************************************************

Filename    :   HmdInfo.cpp
Content     :   Head Mounted Display Info
Created     :   January 30, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "HmdInfo.h"

#include <math.h>
#include <jni.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>						// usleep, etc

#include "OVR.h"
#include "Android/LogUtils.h"

namespace OVR
{

enum hmdType_t
{
	HMD_GALAXY_S4,		// Galaxy S4 in Samsung's holder
	HMD_GALAXY_S5,		// Galaxy S5 1080 with lens version 2
	HMD_GALAXY_S5_WQHD,	// Galaxy S5 1440 with lens version 2
	HMD_NOTE_4,			// Note4
};

#define CASE_FOR_TYPE(NAME) case NAME: str = #NAME; break;

static void LogHmdType( hmdType_t hmdType )
{
	String str;

	switch ( hmdType )
	{
		CASE_FOR_TYPE( HMD_GALAXY_S4 );
		CASE_FOR_TYPE( HMD_GALAXY_S5 );
		CASE_FOR_TYPE( HMD_GALAXY_S5_WQHD );
		CASE_FOR_TYPE( HMD_NOTE_4 );
	default:
		str = "<type not known - add to LogHmdType function>";
	}

	LogText( "Detected HMD/Phone version: %s", str.ToCStr() );
}

static hmdType_t IdentifyHmdType( const char * buildModel )
{
	if ( strcmp( buildModel, "GT-I9506" )  == 0 )
	{
		return HMD_GALAXY_S4;
	}

	if ( ( strcmp( buildModel, "SM-G900F" )  == 0 ) || ( strcmp( buildModel, "SM-G900X" )  == 0 ) )
	{
		return HMD_GALAXY_S5;
	}

	if ( strcmp( buildModel, "SM-G906S" )  == 0 )
	{
		return HMD_GALAXY_S5_WQHD;
	}

	if ( ( strstr( buildModel, "SM-N910" ) != NULL ) || ( strstr( buildModel, "SM-N916" ) != NULL ) )
	{
		return HMD_NOTE_4;
	}

	LOG( "IdentifyHmdType: Model %s not found. Defaulting to Note4", buildModel );
	return HMD_NOTE_4;
}

static hmdInfoInternal_t GetHmdInfo( const hmdType_t hmdType )
{
	hmdInfoInternal_t hmdInfo = {};

	// Defaults.
	hmdInfo.lens.Eqn = Distortion_RecipPoly4;
	hmdInfo.lens.MaxR = 1.0f;
	hmdInfo.lens.MaxInvR = 1.0f;
	hmdInfo.lens.MetersPerTanAngleAtCenter = 0.043875f;

	hmdInfo.lens.ChromaticAberration[0] = -0.006f;
	hmdInfo.lens.ChromaticAberration[1] =  0.0f;
	hmdInfo.lens.ChromaticAberration[2] =  0.014f;
	hmdInfo.lens.ChromaticAberration[3] =  0.0f;
	hmdInfo.horizontalOffsetMeters = 0;

	hmdInfo.displayRefreshRate = 60.0f;
	hmdInfo.eyeTextureResolution[0] = 1024;
	hmdInfo.eyeTextureResolution[1] = 1024;
	hmdInfo.eyeTextureFov[0] = 90.0f;
	hmdInfo.eyeTextureFov[1] = 90.0f;

	// Screen params.
	switch( hmdType )
	{
	case HMD_GALAXY_S4:			// Galaxy S4 in Samsung's holder
		hmdInfo.lensSeparation = 0.062f;
		hmdInfo.eyeTextureFov[0] = 95.0f;
		hmdInfo.eyeTextureFov[1] = 95.0f;

		// original polynomial hand tuning - the scale seems a bit too small
		hmdInfo.lens.Eqn = Distortion_RecipPoly4;
		hmdInfo.lens.MetersPerTanAngleAtCenter = 0.043875f;
		hmdInfo.lens.K[0] = 0.756f;
		hmdInfo.lens.K[1] = -0.266f;
		hmdInfo.lens.K[2] = -0.389f;
		hmdInfo.lens.K[3] = 0.158f;
		break;

	case HMD_GALAXY_S5:      // Galaxy S5 1080 paired with version 2 lenses
		hmdInfo.lensSeparation = 0.062f;
		hmdInfo.eyeTextureFov[0] = 90.0f;
		hmdInfo.eyeTextureFov[1] = 90.0f;

		// Tuned for S5 DK2 with lens version 2 for E3 2014 (06-06-14)
		hmdInfo.lens.Eqn = Distortion_CatmullRom10;
		hmdInfo.lens.MetersPerTanAngleAtCenter     = 0.037f;
		hmdInfo.lens.K[0]                          = 1.0f;
		hmdInfo.lens.K[1]                          = 1.021f;
		hmdInfo.lens.K[2]                          = 1.051f;
		hmdInfo.lens.K[3]                          = 1.086f;
		hmdInfo.lens.K[4]                          = 1.128f;
		hmdInfo.lens.K[5]                          = 1.177f;
		hmdInfo.lens.K[6]                          = 1.232f;
		hmdInfo.lens.K[7]                          = 1.295f;
		hmdInfo.lens.K[8]                          = 1.368f;
		hmdInfo.lens.K[9]                          = 1.452f;
		hmdInfo.lens.K[10]                         = 1.560f;
		break;

	case HMD_GALAXY_S5_WQHD:            // Galaxy S5 1440 paired with version 2 lenses
		hmdInfo.lensSeparation = 0.062f;
		hmdInfo.eyeTextureFov[0] = 90.0f;  // 95.0f
		hmdInfo.eyeTextureFov[1] = 90.0f;  // 95.0f

		// Tuned for S5 DK2 with lens version 2 for E3 2014 (06-06-14)
		hmdInfo.lens.Eqn = Distortion_CatmullRom10;
		hmdInfo.lens.MetersPerTanAngleAtCenter     = 0.037f;
		hmdInfo.lens.K[0]                          = 1.0f;
		hmdInfo.lens.K[1]                          = 1.021f;
		hmdInfo.lens.K[2]                          = 1.051f;
		hmdInfo.lens.K[3]                          = 1.086f;
		hmdInfo.lens.K[4]                          = 1.128f;
		hmdInfo.lens.K[5]                          = 1.177f;
		hmdInfo.lens.K[6]                          = 1.232f;
		hmdInfo.lens.K[7]                          = 1.295f;
		hmdInfo.lens.K[8]                          = 1.368f;
		hmdInfo.lens.K[9]                          = 1.452f;
		hmdInfo.lens.K[10]                         = 1.560f;
		break;

	default:
	case HMD_NOTE_4:      // Note 4
		hmdInfo.lensSeparation = 0.063f;	// JDC: measured on 8/23/2014
		hmdInfo.eyeTextureFov[0] = 90.0f;
		hmdInfo.eyeTextureFov[1] = 90.0f;

		hmdInfo.widthMeters = 0.125f;		// not reported correctly by display metrics!
		hmdInfo.heightMeters = 0.0707f;

		// GearVR (Note 4)
		hmdInfo.lens.Eqn = Distortion_CatmullRom10;
		hmdInfo.lens.MetersPerTanAngleAtCenter     = 0.0365f;
		hmdInfo.lens.K[0]                          = 1.0f;
		hmdInfo.lens.K[1]                          = 1.029f;
		hmdInfo.lens.K[2]                          = 1.0565f;
		hmdInfo.lens.K[3]                          = 1.088f;
		hmdInfo.lens.K[4]                          = 1.127f;
		hmdInfo.lens.K[5]                          = 1.175f;
		hmdInfo.lens.K[6]                          = 1.232f;
		hmdInfo.lens.K[7]                          = 1.298f;
		hmdInfo.lens.K[8]                          = 1.375f;
		hmdInfo.lens.K[9]                          = 1.464f;
		hmdInfo.lens.K[10]                         = 1.570f;


		break;
	}
	return hmdInfo;
}

// We need to do java calls to get certain device parameters, but for Unity we
// won't be called by java code we control, so explicitly query for the info.
static void QueryAndroidInfo( JNIEnv *env, jobject activity, jclass vrActivityClass, const char * buildModel,
				float & screenWidthMeters, float & screenHeightMeters, int & screenWidthPixels, int & screenHeightPixels )
{
	LOG( "QueryAndroidInfo (%p,%p,%p)", env, activity, vrActivityClass );

	if ( env->ExceptionOccurred() )
	{
		env->ExceptionClear();
		LOG( "Cleared JNI exception" );
	}

	jmethodID getDisplayWidth = env->GetStaticMethodID( vrActivityClass, "getDisplayWidth", "(Landroid/app/Activity;)F" );
	if ( !getDisplayWidth )
	{
		FAIL( "couldn't get getDisplayWidth" );
	}
	const float widthMeters = env->CallStaticFloatMethod( vrActivityClass, getDisplayWidth, activity );

	jmethodID getDisplayHeight = env->GetStaticMethodID( vrActivityClass, "getDisplayHeight", "(Landroid/app/Activity;)F" );
	if ( !getDisplayHeight )
	{
		FAIL( "couldn't get getDisplayHeight" );
	}
	const float heightMeters = env->CallStaticFloatMethod( vrActivityClass, getDisplayHeight, activity );

	screenWidthMeters = widthMeters;
	screenHeightMeters = heightMeters;
	screenWidthPixels = 0;
	screenHeightPixels = 0;

	LOG( "%s %f x %f", buildModel, screenWidthMeters, screenHeightMeters );
}

hmdInfoInternal_t GetDeviceHmdInfo( const char * buildModel, JNIEnv *env, jobject activity, jclass vrActivityClass )
{
	hmdType_t hmdType = IdentifyHmdType( buildModel );
	LogHmdType( hmdType );

	hmdInfoInternal_t hmdInfo = {};
	hmdInfo = GetHmdInfo( hmdType );

	if ( env != NULL && activity != 0 && vrActivityClass != 0 )
	{
		float screenWidthMeters, screenHeightMeters;
		int screenWidthPixels, screenHeightPixels;

		QueryAndroidInfo( env, activity, vrActivityClass, buildModel, screenWidthMeters, screenHeightMeters, screenWidthPixels, screenHeightPixels );

		// Only use the Android info if we haven't explicitly set the screenWidth / height,
		// because they are reported wrong on the note.
		if ( hmdInfo.widthMeters == 0 )
		{
			hmdInfo.widthMeters = screenWidthMeters;
			hmdInfo.heightMeters = screenHeightMeters;
		}
		hmdInfo.widthPixels = screenWidthPixels;
		hmdInfo.heightPixels = screenHeightPixels;
	}

	return hmdInfo;
}

}
