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

#include "OVR_Android_HIDDevice.h"	// Oculus_VendorId and Device_Tracker_ProductId

#include "VrCommon.h"
#include "OVR.h"
#include "Log.h"



namespace OVR
{

enum hmdType_t
{
	HMD_GALAXY_OCULUS,	 // Galaxy S4 in J3 printed holder
	HMD_S4_GALAXY,		 // Galaxy S4 in Samsung's holder
	HMD_NOTE,			 // Note3
	HMD_NOTE_4,          // Note4
	HMD_PM_GALAXY,       // Galaxy S5 1080 with lens version 1
	HMD_QM_GALAXY,       // Galaxy S5 1080 with lens version 2
	HMD_PM_GALAXY_WQHD,  // Galaxy S5 1440 with lens version 1
	HMD_QM_GALAXY_WQHD   // Galaxy S5 1440 with lens version 2
};

enum hmdVersion_t
{
	HMD_VERSION_PM,		// Lens version 1, 13mm eye relief.
	HMD_VERSION_QM		// Lens version 2, 11mm eye relief.
};

#define CASE_FOR_TYPE(NAME) case NAME: str = #NAME; break;

static void LogHmdType(hmdType_t hmdType)
{
	String str;

	switch(hmdType)
	{
		CASE_FOR_TYPE(HMD_GALAXY_OCULUS);
		CASE_FOR_TYPE(HMD_S4_GALAXY);
		CASE_FOR_TYPE(HMD_NOTE);
		CASE_FOR_TYPE(HMD_NOTE_4);
		CASE_FOR_TYPE(HMD_PM_GALAXY);
		CASE_FOR_TYPE(HMD_QM_GALAXY);
		CASE_FOR_TYPE(HMD_PM_GALAXY_WQHD);
		CASE_FOR_TYPE(HMD_QM_GALAXY_WQHD);
	default:
		str = "<type not known - add to LogHmdType function>";
	}

	LogText("Detected HMD/Phone version: %s", str.ToCStr());
}

static hmdVersion_t GetHmdVersion(unsigned int firmwareVersion)
{
	if (firmwareVersion == 0)
	{
		// LDC - Happens when device not detected for some reason. Let's default to QM in this case.
		return HMD_VERSION_QM;
	}

	if (firmwareVersion == 1)
	{
		return HMD_VERSION_PM;
	}

	if (firmwareVersion == 3)
	{
		return HMD_VERSION_QM;
	}

	// Above were early version numbers. Going forward bit 9 indicates PM (0) or QM (1).
	if (firmwareVersion & 0x0100)	
	{
		return HMD_VERSION_QM;
	}

	return HMD_VERSION_PM;
}

static hmdType_t IdentifyHmdType( const char *buildModel, const ovrSensorDesc & sdesc )
{
	// Determine HMD type to select correct HmdInfo params.

	// model GT-I9500 is SGX Galaxy
	// model GT-I9506 is Adreno Galaxy
	// model SM-N9000 is Mali Note 3
	// model SM-N9005 is Adreno Note 3
	// model SM-N910 is Note 4
	// model SM-N916 is Korean Note 4
	
	if ( ( strcmp( buildModel, "SM-G900F")  == 0 ) || ( strcmp( buildModel, "SM-G900X")  == 0 ) )
	{
		if ( GetHmdVersion( sdesc.Version ) == HMD_VERSION_PM )
		{
			return HMD_PM_GALAXY;
		}

		return HMD_QM_GALAXY;
	}

	if ( strcmp( buildModel, "GT-I9506" )  == 0 )
	{
		return HMD_S4_GALAXY;
	}

	if ( ( strcmp( buildModel, "SM-N900" )  == 0 ) || ( strcmp( buildModel, "SM-N9005" )  == 0 ) )
	{
		return HMD_NOTE;
	}

	if ( ( strstr( buildModel, "SM-N910" ) != NULL ) || ( strstr( buildModel, "SM-N916" ) != NULL ) )
	{
		return HMD_NOTE_4;
	}

	if ( strcmp( buildModel, "SM-G906S" )  == 0 )
	{

		if ( GetHmdVersion( sdesc.Version ) == HMD_VERSION_PM )
		{
			return HMD_PM_GALAXY_WQHD;			
		}

		return HMD_QM_GALAXY_WQHD;
	}

	if ( sdesc.VendorId == Oculus_VendorId &&
        sdesc.ProductId == Device_Tracker_ProductId )
	{
		return HMD_GALAXY_OCULUS;
	}

	LOG( "IdentifyHmdType: Model %s not found. Defaulting to Note4", buildModel );
	return HMD_NOTE_4;
}

static hmdInfoInternal_t GetHmdInfo( const hmdType_t hmdType )
{
	hmdInfoInternal_t hmdInfo = {};

	// General.
    hmdInfo.lens.Eqn = Distortion_RecipPoly4;
    hmdInfo.lens.MaxR = 1.0f;
    hmdInfo.lens.MaxInvR = 1.0f;
	hmdInfo.lens.MetersPerTanAngleAtCenter = 0.043875f;

	hmdInfo.lens.ChromaticAberration[0] = -0.006f;
	hmdInfo.lens.ChromaticAberration[2] =  0.014f;
	hmdInfo.lens.ChromaticAberration[3] =  0.0f;

	// Screen params.
	switch( hmdType )
	{
	case HMD_GALAXY_OCULUS:		// J3
	default:
		hmdInfo.lensSeparation = 0.059f;

		hmdInfo.eyeTextureFov = 90.0f;

		hmdInfo.lens.K[0] = 1.0f;
		hmdInfo.lens.K[1] = -0.3999f;
		hmdInfo.lens.K[2] =  0.2408f;
		hmdInfo.lens.K[3] = -0.4589f;
		break;

	case HMD_S4_GALAXY:
		hmdInfo.lensSeparation = 0.062f;

		// original polynomial hand tuning - the scale seems a bit too small
		hmdInfo.eyeTextureFov = 95.0f;
		hmdInfo.lens.Eqn = Distortion_RecipPoly4;
		hmdInfo.lens.MetersPerTanAngleAtCenter = 0.043875f;
	    hmdInfo.lens.K[0] = 0.756f;
		hmdInfo.lens.K[1] = -0.266f;
		hmdInfo.lens.K[2] =  -0.389f;
		hmdInfo.lens.K[3] = 0.158f;
		break;

	case HMD_PM_GALAXY:       // Galaxy S5 1080 paired with version 1 lenses
		hmdInfo.lensSeparation = 0.062f;

#if 1
		// 15 mm eye-relief G5 spline tuning for GDC 2014
		// Also the default setting for the S5 Dev Kit release June 2014
		hmdInfo.eyeTextureFov = 90.0f;
		hmdInfo.lens.Eqn = Distortion_CatmullRom10;
		hmdInfo.lens.MetersPerTanAngleAtCenter     = 0.039f;
		hmdInfo.lens.K[0]                          = 1.0f;
		hmdInfo.lens.K[1]                          = 1.022f;
		hmdInfo.lens.K[2]                          = 1.049f;
		hmdInfo.lens.K[3]                          = 1.081f;
		hmdInfo.lens.K[4]                          = 1.117f;
		hmdInfo.lens.K[5]                          = 1.159f;
		hmdInfo.lens.K[6]                          = 1.204f;
		hmdInfo.lens.K[7]                          = 1.258f;
		hmdInfo.lens.K[8]                          = 1.32f;
		hmdInfo.lens.K[9]                          = 1.39f;
		hmdInfo.lens.K[10]                         = 1.47f;
#elif 0
		// 8 mm eye-relief G5 spline tuning for GDC 2014 (tuned for Matt's hacked up prototype)
		hmdInfo.eyeTextureFov = 90.0f;
		hmdInfo.lens.Eqn = Distortion_CatmullRom10;
		hmdInfo.lens.MetersPerTanAngleAtCenter     = 0.039f;
		hmdInfo.lens.K[0]                          = 1.0f;
		hmdInfo.lens.K[1]                          = 1.016f;
		hmdInfo.lens.K[2]                          = 1.04f;
		hmdInfo.lens.K[3]                          = 1.07f;
		hmdInfo.lens.K[4]                          = 1.108f;
		hmdInfo.lens.K[5]                          = 1.152f;
		hmdInfo.lens.K[6]                          = 1.202f;
		hmdInfo.lens.K[7]                          = 1.258f;
		hmdInfo.lens.K[8]                          = 1.32f;
		hmdInfo.lens.K[9]                          = 1.39f;
		hmdInfo.lens.K[10]                         = 1.47f;
#else
	    // Spline tuning for S5 DK2 prototype with S4 lens
		hmdInfo.eyeTextureFov = 90.0f;
		hmdInfo.lens.Eqn = Distortion_CatmullRom10;
		hmdInfo.lens.MetersPerTanAngleAtCenter     = 0.0375f;
		hmdInfo.lens.K[0]                          = 1.0f;
		hmdInfo.lens.K[1]                          = 1.016f;
		hmdInfo.lens.K[2]                          = 1.04f;
		hmdInfo.lens.K[3]                          = 1.07f;
		hmdInfo.lens.K[4]                          = 1.108f;
		hmdInfo.lens.K[5]                          = 1.152f;
		hmdInfo.lens.K[6]                          = 1.202f;
		hmdInfo.lens.K[7]                          = 1.258f;
		hmdInfo.lens.K[8]                          = 1.32f;
		hmdInfo.lens.K[9]                          = 1.39f;
		hmdInfo.lens.K[10]                         = 1.47f;
#endif
		break;

	case HMD_QM_GALAXY:      // Galaxy S5 1080 paired with version 2 lenses
		hmdInfo.lensSeparation = 0.062f;
		hmdInfo.eyeTextureFov = 90.0f;

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

	case HMD_NOTE_4:      // Note 4 prototype 6/9/2014
		hmdInfo.lensSeparation = 0.063f;	// JDC: measured on 8/23/2014
		hmdInfo.eyeTextureFov = 90.0f;
		hmdInfo.widthMeters = 0.125f;		// not reported correctly by display metrics!
		hmdInfo.heightMeters = 0.0707f;

		// This was for the revision 0 of the GS5 lens and was used at IFA and Oculus Connect
		/*
		hmdInfo.eyeTextureFov = 90.0f;
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
		*/

		/*
		// This is the tuning for rev 2 of the Note 4 lens (single cavity)
		hmdInfo.eyeTextureFov = 90.0f;
		hmdInfo.lens.Eqn = Distortion_CatmullRom10;
		hmdInfo.lens.MetersPerTanAngleAtCenter     = 0.0365f;
		hmdInfo.lens.K[0]                          = 1.0f;
		hmdInfo.lens.K[1]                          = 1.03f;
		hmdInfo.lens.K[2]                          = 1.057f;
		hmdInfo.lens.K[3]                          = 1.088f;
		hmdInfo.lens.K[4]                          = 1.125f;
		hmdInfo.lens.K[5]                          = 1.170f;
		hmdInfo.lens.K[6]                          = 1.225f;
		hmdInfo.lens.K[7]                          = 1.290f;
		hmdInfo.lens.K[8]                          = 1.368f;
		hmdInfo.lens.K[9]                          = 1.457f;
		hmdInfo.lens.K[10]                         = 1.565f;
		*/

		// Note 4 Rev 3 (4 cavity) lens
		hmdInfo.eyeTextureFov = 90.0f;
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
		
	case HMD_PM_GALAXY_WQHD:         // Galaxy S5 1440 paired with version 1 lenses
		hmdInfo.lensSeparation = 0.062f;

		hmdInfo.eyeTextureFov = 90.0f;  // 95.0f

		// Copied settings from the G5 Dev Kit (June 2014)
		hmdInfo.eyeTextureFov = 90.0f;
		hmdInfo.lens.Eqn = Distortion_CatmullRom10;
		hmdInfo.lens.MetersPerTanAngleAtCenter     = 0.039f;
		hmdInfo.lens.K[0]                          = 1.0f;
		hmdInfo.lens.K[1]                          = 1.022f;
		hmdInfo.lens.K[2]                          = 1.049f;
		hmdInfo.lens.K[3]                          = 1.081f;
		hmdInfo.lens.K[4]                          = 1.117f;
		hmdInfo.lens.K[5]                          = 1.159f;
		hmdInfo.lens.K[6]                          = 1.204f;
		hmdInfo.lens.K[7]                          = 1.258f;
		hmdInfo.lens.K[8]                          = 1.32f;
		hmdInfo.lens.K[9]                          = 1.39f;
		hmdInfo.lens.K[10]                         = 1.47f;
		break;

	case HMD_QM_GALAXY_WQHD:            // Galaxy S5 1440 paired with version 2 lenses
		hmdInfo.lensSeparation = 0.062f;

		hmdInfo.eyeTextureFov = 90.0f;  // 95.0f

		// Tuned for S5 DK2 with lens version 2 for E3 2014 (06-06-14)
		hmdInfo.eyeTextureFov = 90.0f;
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
	}
	return hmdInfo;
}


// We need to do java calls to get certain device parameters, but for Unity we
// won't be called by java code we control, so explicitly query for the info.
static void QueryAndroidInfo( JNIEnv *env, jobject activity, jclass vrActivityClass,
		const char * buildModel, float & screenWidthMeters, float & screenHeightMeters, int & screenWidthPixels, int & screenHeightPixels )
{
	LOG( "QueryAndroidInfo (%p,%p,%p)", env, activity, vrActivityClass );

	if(env->ExceptionOccurred())
	{
		env->ExceptionClear();
		LOG("Cleared JNI exception");
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

hmdInfoInternal_t GetDeviceHmdInfo( JNIEnv *env, jobject activity, jclass vrActivityClass,
	ovrHmd hmd, const char * buildModel)
{
	float screenWidthMeters, screenHeightMeters;
	int screenWidthPixels, screenHeightPixels;

	QueryAndroidInfo( env, activity, vrActivityClass, buildModel, screenWidthMeters, screenHeightMeters, screenWidthPixels, screenHeightPixels );

	// We might be running without a sensor plugged in at all
    ovrSensorDesc sdesc = {};
    if ( hmd )
    {
    	ovrHmd_GetSensorDesc(hmd, &sdesc);
    }

    hmdInfoInternal_t hmdInfo = {};
	hmdType_t hmdType = IdentifyHmdType( buildModel, sdesc );
	LogHmdType( hmdType );
	hmdInfo = GetHmdInfo( hmdType );

	// Only use the Android info if we haven't explicit set the screenWidth / height,
	// because they are reported wrong on the note.

	if ( hmdInfo.widthMeters == 0 )
	{
		hmdInfo.widthMeters = screenWidthMeters;
		hmdInfo.heightMeters = screenHeightMeters;
	}
	hmdInfo.widthPixels = screenWidthPixels;
	hmdInfo.heightPixels = screenHeightPixels;

    return hmdInfo;
}

}
