/************************************************************************************

Filename    :   ProximityReceiver.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculusvr.vrlib;

import android.app.Activity;
import android.util.Log;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.BroadcastReceiver;

public class ProximityReceiver extends BroadcastReceiver {
	public static final String TAG = "VrLib";

	public static String MOUNT_HANDLED_INTENT = "com.oculus.mount_handled";
	public static String PROXIMITY_SENSOR_INTENT = "android.intent.action.proximity_sensor";
	
	public static native void nativeProximitySensor(int onHead);
	public static native void nativeMountHandled();

	static ProximityReceiver receiver = new ProximityReceiver();
	static boolean registeredProximityReceiver = false;

	public static void startProximityReceiver( Activity act )
	{
		if ( !registeredProximityReceiver )
		{
			Log.d( TAG, "!!#######!! Registering proximity receiver" );

			IntentFilter proximityFilter = new IntentFilter();
			proximityFilter.addAction( PROXIMITY_SENSOR_INTENT );
			proximityFilter.addAction( MOUNT_HANDLED_INTENT );

			act.registerReceiver( receiver, proximityFilter );
			registeredProximityReceiver = true;
		}
		else
		{
			Log.d( TAG, "!!!!!!!!!!! Already registered proximity receiver!" );
		}
	}
	public static void stopProximityReceiver( Activity act )
	{
		if ( registeredProximityReceiver )
		{
			Log.d( TAG, "Unregistering proximity receiver" );
			act.unregisterReceiver( receiver );
			registeredProximityReceiver = false;
		}
	}

 	@Override
	public void onReceive(Context context, final Intent intent) {
		Log.d( TAG, "!@#!@ProximityReceiver action:" + intent );

		if (intent.getAction().equals( PROXIMITY_SENSOR_INTENT ))
		{
			nativeProximitySensor( Integer.parseInt( intent.getType() ) );
		}
		else if ( intent.getAction().equals( MOUNT_HANDLED_INTENT ))
		{
			nativeMountHandled();
		}
	}

}