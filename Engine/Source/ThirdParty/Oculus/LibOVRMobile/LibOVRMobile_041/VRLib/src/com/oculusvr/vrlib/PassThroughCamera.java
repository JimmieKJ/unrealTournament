/************************************************************************************

Filename    :   PassThroughCamera.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculusvr.vrlib;

import java.io.IOException;
import java.util.List;

import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.util.Log;

public class PassThroughCamera implements android.graphics.SurfaceTexture.OnFrameAvailableListener {
	
	public static final String TAG = "VrCamera";

	SurfaceTexture cameraTexture;
	Camera	camera;

	boolean gotFirstFrame;

	boolean previewStarted;

	long	appPtr = 0;	// this must be cached for the onFrameAvailable callback :(
	
	boolean	hackVerticalFov = false;	// 60 fps preview forces 16:9 aspect, but doesn't report it correctly
	long	startPreviewTime;

	
	public native SurfaceTexture nativeGetCameraSurfaceTexture(long appPtr);
	public native void nativeSetCameraFov(long appPtr, float fovHorizontal, float fovVertical);

	public PassThroughCamera()
	{
		Log.d( TAG, "new PassThroughCamera()" );
	}

	public void enableCameraPreview( long appPtr_ )
	{
		//Log.d( TAG, "enableCameraPreview appPtr is " + Long.toHexString( appPtr ) + " : " + Long.toHexString( appPtr_ ) );
		
		if ( BuildConfig.DEBUG && ( appPtr != appPtr_ ) && ( appPtr != 0 ) )
		{ 
			//Log.d( TAG, "enableCameraPreview: appPtr changed!" );
			assert false; // if this asserts then the wrong instance is being called
		} 

		appPtr = appPtr_;
		
		if ( !previewStarted ) 
		{
			//Log.d( TAG, "startCameraPreview" );
			startCameraPreview(appPtr_);
		}
		else
		{
			//Log.d( TAG, "Preview already started" );
		}
	}

	public void disableCameraPreview(long appPtr_)
	{
		//Log.d( TAG, "disableCameraPreview appPtr is " + Long.toHexString( appPtr ) + " : " + Long.toHexString( appPtr_ ) );

		if ( BuildConfig.DEBUG && ( appPtr != appPtr_ ) && ( appPtr != 0 ) )
		{ 
			//Log.d( TAG, "disableCameraPreview: appPtr changed!" );
			assert false; // if this asserts then the wrong instance is being called
		} 

		appPtr = appPtr_;
		
		if ( previewStarted ) 
		{
			stopCameraPreview(appPtr_);
		}
	}

	public void onFrameAvailable(SurfaceTexture surfaceTexture) {
		//Log.d( TAG, "onFrameAvailable" );
		if (BuildConfig.DEBUG && (appPtr == 0)) 
		{
			//Log.d( TAG, "onFrameAvailable: appPtr is NULL!" );
			assert false; // if this asserts then the wrong instance is being called
		}  // if this asserts, then init was not called first
		
		if (gotFirstFrame) {
			// We might want to stop the updates now that we have a frame.
			return;
		}
		if ( camera == null ) {
			// camera was turned off before it was displayed
			return;
		}
		gotFirstFrame = true;

		// Now that there is an image ready, tell native code to display it
		Camera.Parameters parms = camera.getParameters();
		float fovH = parms.getHorizontalViewAngle();
		float fovV = parms.getVerticalViewAngle();
		
		// hack for 60/120 fps camera, recalculate fovV from 16:9 ratio fovH
		if ( hackVerticalFov ) {
			fovV = 2.0f * (float)( Math.atan( Math.tan( fovH/180.0 * Math.PI * 0.5f ) / 16.0f * 9.0f ) / Math.PI * 180.0f ); 
		}

		Log.v(TAG, "camera view angles:" + fovH + " " + fovV + " from " + parms.getVerticalViewAngle() );

		long now = System.nanoTime();
		Log.v(TAG,  "seconds to first frame: " + (now-startPreviewTime) * 10e-10f);
		nativeSetCameraFov(appPtr, fovH, fovV);
	}

	public void startExposureLock( long appPtr_, boolean locked ) {
		//Log.d( TAG, "startExposureLock appPtr_ is " + Long.toHexString( appPtr ) + " : " + Long.toHexString( appPtr_ ) );
		if ( BuildConfig.DEBUG && ( appPtr != appPtr_ ) && ( appPtr != 0 ) )
		{ 
			//Log.d( TAG, "startExposureLock: appPtr changed!" );
			assert false; // if this asserts then the wrong instance is being called
		} 

		Log.v(TAG, "startExposureLock:" + locked);
		
		if ( camera == null ) {
			return;
		}
		
		// Magic set of parameters from jingoolee@samsung.com
		Camera.Parameters parms = camera.getParameters();

		parms.setAutoExposureLock( locked );
		parms.setAutoWhiteBalanceLock( locked );
		
		camera.setParameters( parms );
	}
	
	public void startCameraPreview(long appPtr_) {
		startPreviewTime = System.nanoTime();
		
		//Log.d( TAG, "startCameraPreview appPtr_ is " + Long.toHexString( appPtr ) + " : " + Long.toHexString( appPtr_ ) );
		if ( BuildConfig.DEBUG && ( appPtr != appPtr_ ) && ( appPtr != 0 ) )
		{ 
			//Log.d( TAG, "startCameraPreview: appPtr changed!" );
			assert false; // if this asserts then the wrong instance is being called
		} 

		// If we haven't set up the surface / surfaceTexture yet,
		// do it now.
		if (cameraTexture == null) 
		{
			//Log.d( TAG, "cameraTexture = null" );
			cameraTexture = nativeGetCameraSurfaceTexture(appPtr_);
			if (cameraTexture == null) {
				Log.e(TAG, "nativeGetCameraSurfaceTexture returned NULL");
				return; // not set up yet
			}
		}
		cameraTexture.setOnFrameAvailableListener( this );

		if (camera != null) 
		{
			// already previewing
			//Log.d(TAG, "restart camera.startPreview");
			gotFirstFrame = false;
			camera.startPreview();		
			previewStarted = true;			
			return;
		}

		//Log.d( TAG, "camera is null" );

		camera = Camera.open();
		Camera.Parameters parms = camera.getParameters();
		//Log.d(TAG, "Camera.Parameters: " + parms.flatten() );

		//check if the device supports vr mode preview 
		if ("true".equalsIgnoreCase(parms.get("vrmode-supported"))) 
		{
			Log.v(TAG, "VR Mode supported!");
			
			//set vr mode
			parms.set("vrmode", 1); 
	 
			// true if the apps intend to record videos using MediaRecorder
			parms.setRecordingHint(true); 
			this.hackVerticalFov = true;	// will always be 16:9			
			
			// set preview size 
			parms.setPreviewSize(960, 540); 
			// parms.setPreviewSize(800, 600); 
			// parms.setPreviewSize(640, 480); 
			// parms.setPreviewSize(640, 360); 
			
			// for 120fps
			parms.set("fast-fps-mode", 2); // 2 for 120fps 
			parms.setPreviewFpsRange(120000, 120000); 
			
			// for 60fps 
			//parms.set("fast-fps-mode", 1); // 1 for 60fps 
			//parms.setPreviewFpsRange(60000, 60000); 
			
			// for 30fps 
			//parms.set("fast-fps-mode", 0); // 0 for 30fps 
			//parms.setPreviewFpsRange(30000, 30000); 
						
			// for auto focus
			parms.set("focus-mode", "continuous-video");
			
		} else { // not support vr mode }
			Camera.Size preferredSize = parms.getPreferredPreviewSizeForVideo();
			Log.v(TAG, "preferredSize: " + preferredSize.width + " x " + preferredSize.height );
			
			List<Integer> formats = parms.getSupportedPreviewFormats();
			for (int i = 0; i < formats.size(); i++) {
				Log.v(TAG, "preview format: " + formats.get(i) );
			}
		
			// YV12 format, documented YUV format exposed to software
//			parms.setPreviewFormat( 842094169 );

			// set the preview size to something small
			List<Camera.Size> previewSizes = parms.getSupportedPreviewSizes();
			for (int i = 0; i < previewSizes.size(); i++) {
				Log.v(TAG, "preview size: " + previewSizes.get(i).width + ","
						+ previewSizes.get(i).height);
			}

			Log.v(TAG, "isAutoExposureLockSupported: " + parms.isAutoExposureLockSupported() );
			Log.v(TAG, "isAutoWhiteBalanceLockSupported: " + parms.isAutoWhiteBalanceLockSupported() );
			Log.v(TAG, "minExposureCompensation: " + parms.getMinExposureCompensation() );
			Log.v(TAG, "maxExposureCompensation: " + parms.getMaxExposureCompensation() );
			
			float fovH = parms.getHorizontalViewAngle();
			float fovV = parms.getVerticalViewAngle();
			Log.v(TAG, "camera view angles:" + fovH + " " + fovV);
			
			// 16:9
			// parms.setPreviewSize( 720, 480 );
	
			// Camera seems to be 4:3 internally
			// parms.setPreviewSize(800, 600);
	//		 parms.setPreviewSize(960, 720);
			 parms.setPreviewSize(800, 480);
	//		 parms.setPreviewSize(1024, 576);
	//		 parms.setPreviewSize(320,240);
	
			// set the preview fps to maximum
			List<int[]> fpsRanges = parms.getSupportedPreviewFpsRange();
			for (int i = 0; i < fpsRanges.size(); i++) 
			{
				Log.v(TAG, "fps range: " + fpsRanges.get(i)[0] + ","
								+ fpsRanges.get(i)[1]);
			}
	
			int[] maxFps = fpsRanges.get(fpsRanges.size() - 1);
			this.hackVerticalFov = false;
			parms.setPreviewFpsRange(maxFps[0], maxFps[1]);						
		}
		
		camera.setParameters(parms);
		
		Log.v(TAG, "camera.setPreviewTexture");
		try
		 {
			camera.setPreviewTexture(cameraTexture);
		} catch (IOException e) {
			Log.v(TAG, "startCameraPreviewToTextureId: setPreviewTexture exception");
		}
		
		Log.v(TAG, "camera.startPreview");
		gotFirstFrame = false;
		camera.startPreview();		
		previewStarted = true;
	}

	public void stopCameraPreview(long appPtr_) {
		if ( BuildConfig.DEBUG && ( appPtr != appPtr_ ) && ( appPtr != 0 ) )
		{ 
			//Log.d( TAG, "stopCameraPreview: appPtr changed!" );
			assert false; // if this asserts then the wrong instance is being called
		} 

		previewStarted = false;
		nativeSetCameraFov( appPtr_,0.0f, 0.0f );
		if ( cameraTexture != null )
		{
			cameraTexture.setOnFrameAvailableListener( null );
		}
		if ( camera != null )
		{
			Log.v(TAG, "camera.stopPreview");
			camera.stopPreview();
			camera.release();
			camera = null;
		}
	}

}
