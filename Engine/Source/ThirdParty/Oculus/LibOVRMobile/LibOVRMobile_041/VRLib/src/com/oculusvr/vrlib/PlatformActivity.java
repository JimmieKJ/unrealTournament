/************************************************************************************

Filename    :   PlatformActivity.java
Content     :   The "platform UI" that behaves the same across native and Unity apps is started here.
Created     :   July, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
package com.oculusvr.vrlib;

import android.os.Bundle;
import android.util.Log;

public class PlatformActivity extends VrActivity {
	public static final String TAG = "PlatformActivity";

	static PassThroughCamera passThroughCam;

	public static native long nativeSetAppInterface(VrActivity act);

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		passThroughCam = new PassThroughCamera();
		Log.d( TAG, "PlatformActivity::onCreate " + " passThroughCam = " + passThroughCam );
		super.onCreate(savedInstanceState);
		appPtr = nativeSetAppInterface( this );		
	}	

	@Override
	protected void onDestroy() {
		Log.d( TAG, "PlatformActivity::onDestroy()" );
		passThroughCam = null;
		super.onDestroy();
		//System.gc();
		//System.gc();
	}
}
