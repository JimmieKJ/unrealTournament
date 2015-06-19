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

// TODO_PLATFORMUI: after merging back to main, we should rename this to SystemActivityReceiver
// - Consider moving all receivers into a single class that handles multiple intents like this
// - Consider moving this into the VrLib class.
public class ProximityReceiver extends BroadcastReceiver {
	public static final String TAG = "VrLib";

	public static String MOUNT_HANDLED_INTENT = "com.oculus.mount_handled";
	public static String PROXIMITY_SENSOR_INTENT = "android.intent.action.proximity_sensor";
	public static String SYSTEM_ACTIVITY_INTENT = "com.oculus.system_activity";
	
	public static native void nativeProximitySensor(int onHead);
	public static native void nativeMountHandled();
	public static native void nativeSystemActivityIntent( Context context, String fromPackageName, String command, String uri );

	static ProximityReceiver Receiver = new ProximityReceiver();
	static boolean RegisteredReceiver = false;

	public static void startReceiver( Activity act )
	{
		if ( !RegisteredReceiver )
		{
			Log.d( TAG, "!!#######!! Registering Oculus System Activity receiver" );

			IntentFilter filter = new IntentFilter();
			filter.addAction( PROXIMITY_SENSOR_INTENT );
			filter.addAction( MOUNT_HANDLED_INTENT );
			filter.addAction( SYSTEM_ACTIVITY_INTENT );

			act.registerReceiver( Receiver, filter );
			RegisteredReceiver = true;
		}
		else
		{
			Log.d( TAG, "!!!!!!!!!!! Already registered Oculus System Activty receiver!" );
		}
	}
	public static void stopReceiver( Activity act )
	{
		if ( RegisteredReceiver )
		{
			Log.d( TAG, "Unregistering Oculus System Activity receiver" );
			act.unregisterReceiver( Receiver );
			RegisteredReceiver = false;
		}
	}

 	@Override
	public void onReceive(Context context, final Intent intent) {
		Log.d( TAG, "!@#!@SystemActivityReceiver.onReceive intent:" + intent );

		if ( intent.getAction().equals( PROXIMITY_SENSOR_INTENT ) )
		{
			nativeProximitySensor( Integer.parseInt( intent.getType() ) );
		}
		else if ( intent.getAction().equals( MOUNT_HANDLED_INTENT ) )
		{
			nativeMountHandled();
		}
		else if ( intent.getAction().equals( SYSTEM_ACTIVITY_INTENT ) ) 
		{
			String fromPackageName = VrLib.getPackageStringFromIntent( intent );
			String command = VrLib.getCommandStringFromIntent( intent );
			String uri = VrLib.getUriStringFromIntent( intent );
			Log.d( TAG, "fromPackageName: '" + fromPackageName + "'" );
			Log.d( TAG, "command: '" + command + "'" );
			Log.d( TAG, "uri: '" + uri + "'" );

			nativeSystemActivityIntent( context, fromPackageName, command, uri );
		}
	}

}