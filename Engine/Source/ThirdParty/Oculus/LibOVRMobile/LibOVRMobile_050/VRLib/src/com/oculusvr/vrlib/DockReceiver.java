/************************************************************************************

Filename    :   DockReceiver.java
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

public class DockReceiver extends BroadcastReceiver {
	public static final String TAG = "VrLib";

	public static native void nativeDockEvent( int state );

	static DockReceiver receiver = new DockReceiver();
	static boolean registeredDockReceiver = false;

	public static String HMT_DISCONNECT = "com.samsung.intent.action.HMT_DISCONNECTED";
	public static String HMT_CONNECT = "com.samsung.intent.action.HMT_CONNECTED";
	
	public static void startDockReceiver( Activity act )
	{
		if ( !registeredDockReceiver )
		{
			Log.d( TAG, "!!#######!! Registering dock receiver" );

			IntentFilter dockFilter = new IntentFilter();
			dockFilter.addAction( HMT_DISCONNECT );
			dockFilter.addAction( HMT_CONNECT );	
			act.registerReceiver( receiver, dockFilter );
			registeredDockReceiver = true;
		}
		else
		{
			Log.d( TAG, "!!!!!!!!!!! Already registered dock receiver!" );
		}
	}
	public static void stopDockReceiver( Activity act )
	{
		if ( registeredDockReceiver )
		{
			Log.d( TAG, "Unregistering dock receiver" );
			act.unregisterReceiver( receiver );
			registeredDockReceiver = false;
		}
	}

 	@Override
	public void onReceive(Context context, final Intent intent) {
		Log.d( TAG, "!@#!@DockReceiver action:" + intent.getAction() );

		if ( intent.getAction().equals( HMT_DISCONNECT ) )	
		{
			nativeDockEvent( 0 );
		}
		else if ( intent.getAction().equals( HMT_CONNECT ) )
		{
			nativeDockEvent( 1 );
		}
	}

}
