/************************************************************************************

Filename    :   JavaSample.java
Content     :   Simple rendering in Java based on Timewarp/VRShell platform
Created     :   September 29, 2013
Authors     :   John Carmack, Michael Antonov

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include <math.h>


#include "App.h"

using namespace OVR;


class JavaSample : public OVR::VrAppInterface
{
public:
    JavaSample();
    ~JavaSample();

    // This should be a per-user configuration parameter, along
    // with eye height.
    float		        InterpupillaryDistance;

    // JNI Variables for native drawFrame call to C++.
    // Since commands and Frame are called from render thread, they need custom Jni.
    JNIEnv			   *pJniRT;
    jmethodID           FrameNotifyThunkMethodId;
    jmethodID           DrawEyeThunkMethodId;

    // Current VRFrame in case we are called back from Java through JNI
    VrFrame 			vrFrame;

    // Overrides from OVR::App
    virtual void	OneTimeInit( const char * launchIntent, VRMenuObjectParms const *** additionalAppMenuItems );
    virtual Matrix4f	DrawEyeView( const int eye, const float fovDegrees );
    virtual Matrix4f	Frame( VrFrame vrFrame );


    //void initJavaVM(JavaVM* vm) { pJavaVM = vm; }
    void initJNIMethods();

    void callFrameNotifyThunk(const VrFrame& frame, const EyeParms& params);
    void callDrawEyeThunk(int eye, float fovDegrees);

};

//-----------------------------------------------------------------------------
// ***** JNI interface

extern "C"
{

long Java_com_oculusvr_vrlib_VrActivity_nativeSetJavaAppInterface(
	JNIEnv * jni, jclass clazz, jobject activity )
{
	LOG( "PureJava SetAppInterface" );
	return (new JavaSample())->SetActivity( jni, clazz, activity );
}

}	// extern "C"

//-----------------------------------------------------------------------------
// ***** JavaSample 

JavaSample::JavaSample()
{
	pJniRT = 0;
	FrameNotifyThunkMethodId  = 0;
    DrawEyeThunkMethodId = 0;
}

JavaSample::~JavaSample()
{
	LOG( "~JavaSample()" );
}


void JavaSample::OneTimeInit( const char * launchIntent, VRMenuObjectParms const *** additionalAppMenuItems )
{
	LOG("JavaSample::OneTimeInit()");
	
    // Clear out Jni and initialize.
    FrameNotifyThunkMethodId  = 0;
    DrawEyeThunkMethodId = 0;

    initJNIMethods();


    // Default IPD; should be configurable, along with eye height
	InterpupillaryDistance = 0.064f;

	LOG("JavaSample::OneTimeInit() completed");
}


//-----------------------------------------------------------------------------

Matrix4f JavaSample::DrawEyeView(const int eye, const float fovDegrees)
{
    //LOG("Frame - before callDrawFrameThunk");
    callDrawEyeThunk(eye, fovDegrees);

    return Matrix4f();	// FIXME
}

Matrix4f JavaSample::Frame( const VrFrame vrFrame )
{
    // Get the current vrParms for the buffer resolution.
    const EyeParms vrParms = app->GetEyeParms();

    callFrameNotifyThunk(vrFrame, vrParms);

    //-------------------------------------------
    // Render the two eye views, each to a separate texture, and TimeWarp
    // to the screen.
    //-------------------------------------------
    app->DrawEyeViewsPostDistorted( Matrix4f() );	// FIXME

    return Matrix4f();	// FIXME
}



// Called from JavaSample to initialize JNI.
void JavaSample::initJNIMethods()
{

    // The Java VM needs to be attached on each thread that will use it.    
    //const jint rtn = pJavaVM->AttachCurrentThread( &pJniRT, 0 );
    const jint rtn = app->GetJavaVM()->AttachCurrentThread( &pJniRT, 0 );
    if ( rtn != JNI_OK ) {
        FAIL( "GlobalJavaVM->AttachCurrentThread returned %i", rtn );
        return;
    }

/*
    // Note a global reference to the activity object with the method.
    ActivityObjectReference = pJniRT->NewGlobalRef( activity );
    if ( !ActivityObjectReference ) {
        FAIL( "RenderTest couldn't get reference" );
    }
    */
	//LOG("init JNI : %p, %p", pJniRT, javaObject);

    // Get the class, so we can find the method.
    const jclass activityObjectClass = pJniRT->GetObjectClass( app->GetJavaObject() );
    if ( !activityObjectClass ) {
        FAIL( "RenderTest couldn't get class" );
    }

    // This will be used to render each frame
    FrameNotifyThunkMethodId = pJniRT->GetMethodID( activityObjectClass,
    		                                    "frameNotifyThunk", "(IFFFFFFFFF)V" );
    if ( !FrameNotifyThunkMethodId ) {
        FAIL( "RenderTest couldn't get method frameNotifyThunk" );
    }

    DrawEyeThunkMethodId = pJniRT->GetMethodID( activityObjectClass,
                                                "drawEyeThunk", "(IF)V" );
    if ( !DrawEyeThunkMethodId ) {
        FAIL( "RenderTest couldn't get method drawEyeThunk" );
    }
}



void JavaSample::callFrameNotifyThunk(const VrFrame& frame, const EyeParms& params)
{
	if (!pJniRT)
		return;

	float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    Quatf hmdOrient = frame.PoseState.Pose.Orientation;
	hmdOrient.GetEulerAngles < Axis_Y, Axis_X, Axis_Z>
						   (&yaw, &pitch, &roll);


	pJniRT->CallVoidMethod( app->GetJavaObject(), FrameNotifyThunkMethodId,
    					 //frame.renderFrameBuffers[0], frame.renderFrameBuffers[1],
                         params.resolution,
    					 frame.PoseState.Pose.Orientation.x, frame.PoseState.Pose.Orientation.y,
    					 frame.PoseState.Pose.Orientation.z, frame.PoseState.Pose.Orientation.w,
    					 yaw, pitch, roll,
    					 InterpupillaryDistance, frame.DeltaSeconds );

}

void JavaSample::callDrawEyeThunk(int eye, float fovDegrees)
{
    if (!pJniRT)
        return;

    glUseProgram(0);

    pJniRT->CallVoidMethod( app->GetJavaObject(), DrawEyeThunkMethodId, eye, fovDegrees );
}

