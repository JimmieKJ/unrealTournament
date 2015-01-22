/************************************************************************************

Filename    :   VrLib.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

package com.oculusvr.vrlib;

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.net.Uri;
import android.util.Log;
import android.view.Choreographer;
import android.view.Surface;
import android.net.wifi.WifiManager;
import android.os.Environment;
import android.provider.Settings;
import android.view.View;
import android.view.WindowManager;
import android.telephony.PhoneStateListener;
import android.telephony.ServiceState;
import android.telephony.SignalStrength;
import android.telephony.TelephonyManager;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.PackageInfo;
import android.os.Build;

import java.lang.Class;
import java.lang.reflect.Method;

/*
 * Static methods holding java code needed by VrLib. 
 */
public class VrLib implements android.view.Choreographer.FrameCallback {
	public static final String TAG = "VrLib";
	public static VrLib handler = new VrLib();

	public static void startPackageActivity( Activity activity, String className, String commandString ) {
		Log.d(TAG, "startPackageActivity" );

		try {
			Intent intent = new Intent(activity, Class.forName( className ) );
			intent.setData( Uri.parse( commandString ) );
			try {
				activity.startActivity(intent);
				activity.overridePendingTransition(0, 0);
			} catch (Exception t) {
				Log.e(TAG, "startPlatformUi threw exception" + t);
			}
		} catch( Exception t ) {
			Log.e(TAG, "Class.forName( " + className + " ) threw exception" + t);			
		}
		Log.d(TAG, "startPackageActivity: after startActivity" );
	}
	
	public static String MOUNT_HANDLED_INTENT = "com.oculus.mount_handled";
	
	public static void notifyMountHandled( Activity activity )
	{
		Log.d( TAG, "notifyMountHandled" );
		Intent i = new Intent( MOUNT_HANDLED_INTENT );
		activity.sendBroadcast( i );
	}

	public static void logApplicationName( Activity act )
	{
		int stringId = act.getApplicationContext().getApplicationInfo().labelRes;
		String name = act.getApplicationContext().getString( stringId );
		Log.d( TAG, "APP = " + name );
	}

	public static void logApplicationVersion( Activity act )
	{
		String versionName = "<none>";
        String internalVersionName = "<none>";
		int versionCode = 0;
		final PackageManager packageMgr = act.getApplicationContext().getPackageManager();
		if ( packageMgr != null )
		{
			try {
				PackageInfo packageInfo = packageMgr.getPackageInfo( act.getApplicationContext().getPackageName(), 0 );
				versionName = packageInfo.versionName;
				versionCode = packageInfo.versionCode;
				ApplicationInfo appInfo = act.getPackageManager().getApplicationInfo( act.getApplicationContext().getPackageName(), PackageManager.GET_META_DATA );
				if ( ( appInfo != null ) && ( appInfo.metaData != null ) )
				{
					internalVersionName = appInfo.metaData.getString("internalVersionName", "<none>" );
				}
			} catch ( PackageManager.NameNotFoundException e ) {
				versionName = "<none>";
				versionCode = 0;
			}
		}

		Log.d( TAG, "APP VERSION = " + versionName + " versionCode " + versionCode + " internalVersionName " + internalVersionName);
	}
	
	public static void setActivityWindowFullscreen( final Activity act )
	{
		act.runOnUiThread( new Runnable()
		{
			@Override
			public void run()
			{
				Log.d( TAG, "getWindow().addFlags( WindowManager.LayoutParams.FLAG_FULLSCREEN )" );
				act.getWindow().addFlags( WindowManager.LayoutParams.FLAG_FULLSCREEN );
			}
		});
	}

	public static int setSchedFifoStatic( Activity activity, int tid, int rtPriority ) {
		Log.d(TAG, "setSchedFifoStatic tid:" + tid + " pto:" + rtPriority );

		android.app.IVRManager vr = (android.app.IVRManager)activity.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found" );
			return -1;
		}

		try
		{
			try
			{
				if ( vr.setThreadSchedFifo(activity.getPackageName(), android.os.Process.myPid(), tid, rtPriority) ) {
					Log.d(TAG, "VRManager set thread priority to " + rtPriority );				
					return 0;
				} else {
					Log.d(TAG, "VRManager failed to set thread priority" );
					return -1;
				}
			} catch ( NoSuchMethodError e ) {
				Log.d(TAG, "Thread priority API does not exist");
				return -2;
			}
		} catch( SecurityException s ) {
			Log.d(TAG, "Thread priority security exception");
			return -3;
		}
	}
	
	static int [] defaultClockLevels = { -1, -1, -1, -1 };
	public static int[] getAvailableFreqLevels(  Activity activity )
	{
		android.app.IVRManager vr = (android.app.IVRManager)activity.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found");
			return defaultClockLevels;
		}
		
		try {
			int [] values = vr.return2EnableFreqLev();
			// display available levels
			Log.d(TAG, "Available levels: {GPU MIN, GPU MAX, CPU MIN, CPU MAX}");
			for ( int i = 0; i < values.length; i++ ) {
				Log.d(TAG, "-> " + "/ " + values[i]);
			}			
			return values;
		} catch (NoSuchMethodError e ) {
			return defaultClockLevels;
		}
	}
	
	static int [] defaultClockFreq = { -1, -1, 0, 0 };	
	public static int [] setSystemPerformanceStatic( Activity activity,
			int cpuLevel, int gpuLevel )
	{
		Log.d(TAG, "setSystemPerformance cpu: " + cpuLevel + " gpu: " + gpuLevel);
		
		android.app.IVRManager vr = (android.app.IVRManager)activity.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found");
			return defaultClockFreq;
		}
		
		// FIXME: Is there a better way to test for if the api is present?
		boolean clockLockApiExists = false;
		try
		{
			vr.return2EnableFreqLev();
			clockLockApiExists = true;
			Log.d(TAG,"Fixed clock level API exists");
		} catch ( NoSuchMethodError e ) {
			clockLockApiExists = false;
			Log.d(TAG,"Fixed clock level API does not exist");
		}
		
		if ( clockLockApiExists ) {
			if ( cpuLevel >= 0 && gpuLevel >= 0 ) {
				// lock the frequency
				try
				{
					int[] values = vr.SetVrClocks( activity.getPackageName(), cpuLevel, gpuLevel );
					Log.d(TAG, "SetVrClocks: {CPU CLOCK, GPU CLOCK, POWERSAVE CPU CLOCK, POWERSAVE GPU CLOCK}" );
					for ( int i = 0; i < values.length; i++ ) {
						Log.d(TAG, "-> " + "/ " + values[i]);
					}
					return values;
				} catch( NoSuchMethodError e ) {
					// G906S api differs from Note4
					int[] values = { 0, 0, 0, 0 };
					boolean success = vr.setFreq( activity.getPackageName(), cpuLevel, gpuLevel );
					Log.d(TAG, "setFreq returned " + success );
					return values;
				}
			} else {
				// release the frequency
				vr.relFreq( activity.getPackageName() );
				Log.d(TAG, "Releasing frequency lock");
				return defaultClockFreq;
			}
		} else {
			// Convert CPU and GPU level to Mhz for the minimum clock
			// level API.
			
			// GPU mhz: 200,320,389,462,578
			// CPU mhz: 300,423,653,730,884,960,1037,1191,1268,1498,1575,1728,1959	( 2266,2458 not supported by VrManager)
			
			int cpuMhz = 0;
			switch( cpuLevel )
			{
				case -1: cpuMhz = 0;	break;
				case  0: cpuMhz = 1037;	break;
				case  1: cpuMhz = 1268;	break;
				case  2: cpuMhz = 1575;	break;
				case  3: cpuMhz = 1959;	break;
				default: 
					break;
			}			
			
			int gpuMhz = 0;
			switch( gpuLevel )
			{
				case -1: gpuMhz = 0;	break;
				case  0: gpuMhz = 240;	break;
				case  1: gpuMhz = 300;	break;
				case  2: gpuMhz = 389;	break;
				case  3: gpuMhz = 500;	break;
				default:
					break;
			}			
	
			int cpuMinLockValue = cpuMhz * 1000;
			int gpuMinLockValue = gpuMhz * 1000000;
			int[] values = { cpuMinLockValue, gpuMinLockValue, cpuMinLockValue, gpuMinLockValue };
			
			Log.d(TAG, "Converting cpu: " + cpuLevel + " : " + cpuMinLockValue + " gpu: " + gpuLevel + " : " + gpuMinLockValue);

			try
			{
				if ( cpuMinLockValue > 0 ) {
					Log.d(TAG,"Setting Cpu Min Lock");
					int cpuValues[] = { cpuMinLockValue, cpuMinLockValue };
					vr.setCPUClockMhz( activity.getPackageName(), cpuValues, 2 );
				} else {
					Log.d(TAG,"Releasing Cpu Min Lock");	
					vr.releaseCPUMhz( activity.getPackageName() );
				}
			
				if ( gpuMinLockValue > 0 ) {
					Log.d(TAG,"Setting Gpu Min Lock");	
					vr.setGPUClockMhz( activity.getPackageName(), gpuMinLockValue );
				} else {
					Log.d(TAG,"Releasing Gpu Min Lock");		
					vr.releaseGPUMhz( activity.getPackageName() );
				}
			} catch ( NoSuchMethodError e ) {
				Log.d(TAG,"Fixed clock frequency API does not exist");
				return defaultClockFreq;
			}

			return values;
		}
	}

	public static int getPowerLevelState( Activity act ) {
		//Log.d(TAG, "getPowerLevelState" );

		int level = 0;	
		
		android.app.IVRManager vr = (android.app.IVRManager)act.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found" );
			return level;
		}

		try {
			level = vr.GetPowerLevelState();
		} catch (NoSuchMethodError e) {
			//Log.d( TAG, "getPowerLevelState api does not exist");
		}
		
		return level;
	}
		
	// Get the system brightness level, return value in the [0,255] range
	public static int getSystemBrightness( Activity act ) {
		Log.d(TAG, "getSystemBrightness" );
		
		int bright = 50;
		
		// Get the current system brightness level by way of VrManager
		android.app.IVRManager vr = (android.app.IVRManager)act.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found" );
			return bright;
		}

		String result = vr.getSystemOption( android.app.IVRManager.VR_BRIGHTNESS );
		bright = Integer.parseInt( result );
		return bright;
	}

	// Set system brightness level, input value in the [0,255] range
	public static void setSystemBrightness( Activity act, int brightness ) {
		Log.d(TAG, "setSystemBrightness " + brightness );
		//assert brightness >= 0 && brightness <= 255;

		android.app.IVRManager vr = (android.app.IVRManager)act.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found" );
			return;
		}

		vr.setSystemOption( android.app.IVRManager.VR_BRIGHTNESS, Integer.toString( brightness ));
	}

	// Comfort viewing mode is a low blue light mode.

	// Returns true if system comfortable view mode is enabled
	public static boolean getComfortViewModeEnabled( Activity act ) {
		Log.d(TAG, "getComfortViewModeEnabled" );

		android.app.IVRManager vr = (android.app.IVRManager)act.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found" );
			return false;
		}

		String result = vr.getSystemOption( android.app.IVRManager.VR_COMFORT_VIEW );
		return ( result.equals( "1" ) );
	}

	// Enable system comfort view mode
	public static void enableComfortViewMode( Activity act, boolean enable ) {
		Log.d(TAG, "enableComfortableMode " + enable );

		android.app.IVRManager vr = (android.app.IVRManager)act.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found" );
			return;
		}

		vr.setSystemOption( android.app.IVRManager.VR_COMFORT_VIEW, enable ? "1" : "0" );
	}
	
	public static void setDoNotDisturbMode( Activity act, boolean enable )
	{
		Log.d( TAG, "setDoNotDisturbMode " + enable );
		
		android.app.IVRManager vr = (android.app.IVRManager)act.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found" );
			return;
		}
		
		vr.setSystemOption( android.app.IVRManager.VR_DO_NOT_DISTURB, ( enable ) ? "1" : "0" );

		String result = vr.getSystemOption( android.app.IVRManager.VR_DO_NOT_DISTURB );
		Log.d( TAG, "result after set = " + result );
	}

	public static boolean getDoNotDisturbMode( Activity act )
	{
		Log.d( TAG, "getDoNotDisturbMode " );
		
		android.app.IVRManager vr = (android.app.IVRManager)act.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found" );
			return false;
		}
		
		String result = vr.getSystemOption( android.app.IVRManager.VR_DO_NOT_DISTURB );
		//Log.d( TAG, "getDoNotDisturb result = " + result );
		return ( result.equals( "1" ) );	
	}
	
	// Note that displayMetrics changes in landscape vs portrait mode! 
	public static float getDisplayWidth( Activity act ) {
		android.util.DisplayMetrics display = new android.util.DisplayMetrics();
		act.getWindowManager().getDefaultDisplay().getMetrics(display);		
		final float METERS_PER_INCH = 0.0254f;
		return (display.widthPixels / display.xdpi) * METERS_PER_INCH;
	}
	
	public static float getDisplayHeight( Activity act ) {
		android.util.DisplayMetrics display = new android.util.DisplayMetrics();
		act.getWindowManager().getDefaultDisplay().getMetrics(display);
		final float METERS_PER_INCH = 0.0254f;
		return (display.heightPixels / display.ydpi) * METERS_PER_INCH;
	}

	public static boolean isLandscapeApp( Activity act ) {
		int r = act.getWindowManager().getDefaultDisplay().getRotation();
		Log.d(TAG, "getRotation():" + r );		
		return ( r == Surface.ROTATION_90 ) || ( r == Surface.ROTATION_270 );
	}

	//--------------- Vsync ---------------
	public static native void nativeVsync(long lastVsyncNano);

	public static Choreographer choreographerInstance;

	public static void startVsync( Activity act ) {
    	act.runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
				// Look this up now, so the callback (which will be on the same thread)
				// doesn't have to.
				choreographerInstance = Choreographer.getInstance();

				// Make sure we never get multiple callbacks going.
				choreographerInstance.removeFrameCallback(handler);

				// Start up our vsync callbacks.
				choreographerInstance.postFrameCallback(handler);
    		}
    	});
		
	}

	// It is important to stop the callbacks when the app is paused,
	// because they consume several percent of a cpu core!
	public static void stopVsync( Activity act ) {
		// This may not need to be run on the UI thread, but it doesn't hurt.
    	act.runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
				choreographerInstance.removeFrameCallback(handler);
    		}
    	});		
	}

	public void doFrame(long frameTimeNanos) {
		nativeVsync(frameTimeNanos);
		choreographerInstance.postFrameCallback(this);
	}

	//--------------- Broadcast Receivers ---------------

	//==========================================================
	// headsetReceiver
	public static native void nativeHeadsetEvent(int state);	
	
	private static class HeadsetReceiver extends BroadcastReceiver {
	
		public Activity act;
		
		@Override
		public void onReceive(Context context, final Intent intent) {
	
			act.runOnUiThread(new Runnable() {
				public void run() {
					Log.d( TAG, "!$$$$$$! headsetReceiver::onReceive" );
					if (intent.hasExtra("state")) 
					{
						int state = intent.getIntExtra("state", 0);
						nativeHeadsetEvent( state );
					}
				}
			});
		}
	}

	public static HeadsetReceiver headsetReceiver = null;
	public static IntentFilter headsetFilter = null;

	public static void startHeadsetReceiver( Activity act ) {
	
		Log.d( TAG, "Registering headset receiver" );
		if ( headsetFilter == null ) {
			headsetFilter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);
		}
		if ( headsetReceiver == null ) {
			headsetReceiver = new HeadsetReceiver();
		}
		headsetReceiver.act = act;

		act.registerReceiver(headsetReceiver, headsetFilter);
		
		// initialize with the current headset state
		int state = act.getIntent().getIntExtra("state", 0);
		Log.d( TAG, "startHeadsetReceiver: " + state );
		nativeHeadsetEvent( state );
	}
	
	public static void stopHeadsetReceiver( Activity act ) {
		Log.d( TAG, "Unregistering headset receiver" ); 
		act.unregisterReceiver(headsetReceiver);
	}

	//==========================================================
	// BatteryReceiver
	public static native void nativeBatteryEvent(int status, int level, int temperature);		
	
	private static class BatteryReceiver extends BroadcastReceiver {

		public int batteryLevel = 0;
		public int batteryStatus = 0;
		public int batteryTemperature = 0;
		
		public Activity act;
		
		@Override
		public void onReceive(Context context, final Intent intent) {
	
			act.runOnUiThread(new Runnable() {
				public void run() {
					Log.d(TAG, "OnReceive BATTERY_ACTION_CHANGED");
					// TODO: The following functions should use the proper extra strings, ie BatteryManager.EXTRA_SCALE
					boolean isPresent = intent.getBooleanExtra("present", false);
					//String technology = intent.getStringExtra("technology");
					//int plugged = intent.getIntExtra("plugged", -1);
					//int health = intent.getIntExtra("health", 0);
		
					//Bundle bundle = intent.getExtras();
					//Log.i("BatteryLevel", bundle.toString());
		
					if(isPresent) {
						int status = intent.getIntExtra("status", 0);
						int rawlevel = intent.getIntExtra("level", -1);
						int scale = intent.getIntExtra("scale", -1);
						// temperature is in tenths of a degree centigrade
						int temperature = intent.getIntExtra("temperature", 0);
						//Log.d( TAG, "Battery: status = " + status + ", rawlevel = " + rawlevel + ", scale = " + scale );
						// always tell native code what the battery level is
 						int level = 0;
						if ( rawlevel >= 0 && scale > 0) {
							level = (rawlevel * 100) / scale;
 						}
						if ( status != batteryStatus ||
							 level != batteryLevel ||
							 temperature != batteryTemperature) {
							
							batteryStatus = status;
							batteryLevel = level;
							batteryTemperature = temperature;
							
							nativeBatteryEvent(status,level,temperature);
						}
					}
				}
			});
		}
	}

	public static BatteryReceiver batteryReceiver = null;
	public static IntentFilter batteryFilter = null;

	public static void startBatteryReceiver( Activity act ) {
	
		Log.d( TAG, "Registering battery receiver" );
		if ( batteryFilter == null ) {
			batteryFilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
		}
		if ( batteryReceiver == null ) {
			batteryReceiver = new BatteryReceiver();
		}
		batteryReceiver.act = act;

		act.registerReceiver(batteryReceiver, batteryFilter);
	
		// initialize with the current battery state
		// TODO: The following functions should use the proper extra strings, ie BatteryManager.EXTRA_SCALE
		boolean isPresent = act.getIntent().getBooleanExtra("present", false);
		if (isPresent) {
		    int status = act.getIntent().getIntExtra("status", 0);
		    int rawlevel = act.getIntent().getIntExtra("level", -1);
		    int scale = act.getIntent().getIntExtra("scale", -1);
		    // temperature is in tenths of a degree centigrade 
		    int temperature = act.getIntent().getIntExtra("temperature", 0);

		    int level = 0;
		    if ( rawlevel >= 0 && scale > 0) {
			    level = ( rawlevel * 100 ) / scale;
		    }

			Log.d( TAG, "startBatteryReceiver: " + level + " " + scale + " " + temperature );
			nativeBatteryEvent(status,level,temperature);
		}
	}
	
	public static void stopBatteryReceiver( Activity act ) {
		Log.d( TAG, "Unregistering battery receiver" ); 
		act.unregisterReceiver(batteryReceiver);
	}
	
	//==========================================================
	// WifiReceiver
	public static native void nativeWifiEvent(int state, int level);
	
	private static class WifiReceiver extends BroadcastReceiver {
		// we cache the last level and state so that we can send both independent of which
		// event we receive.
		public int wifiLevel = 0;
		public int wifiState = 0;
		
		public Activity act;

		@Override
		public void onReceive(Context context, final Intent intent) {
			act.runOnUiThread(new Runnable() {
				public void run() {
					Log.d( TAG, "onReceive Wifi, intent = " + intent );
					if ( intent.getAction() == WifiManager.RSSI_CHANGED_ACTION )
					{
						int rssi = intent.getIntExtra( WifiManager.EXTRA_NEW_RSSI,  0);
						wifiLevel = WifiManager.calculateSignalLevel( rssi, 5 );
						Log.d( TAG, "onReceive rssi = " + rssi + " level = " + wifiLevel );
						nativeWifiEvent( wifiState, wifiLevel );
					}
					else if ( intent.getAction() == WifiManager.WIFI_STATE_CHANGED_ACTION )
					{
						wifiState = intent.getIntExtra( WifiManager.EXTRA_WIFI_STATE, 0 );
						Log.d( TAG, "onReceive wifiState = " + wifiState );
						nativeWifiEvent( wifiState, wifiLevel );
					}
				}
			});
		}
	}

	public static WifiReceiver wifiReceiver = null;
	public static IntentFilter wifiFilter = null;

	public static void startWifiReceiver( Activity act ) {
		Log.d( TAG, "Registering wifi receiver" );
		if ( wifiFilter == null ) {
			wifiFilter = new IntentFilter();
			wifiFilter.addAction(WifiManager.RSSI_CHANGED_ACTION);
			wifiFilter.addAction(WifiManager.WIFI_STATE_CHANGED_ACTION );
		}
		if ( wifiReceiver == null ) {
			wifiReceiver = new WifiReceiver();
		}
		wifiReceiver.act = act;

		act.registerReceiver(wifiReceiver, wifiFilter);
	}
	
	public static void stopWifiReceiver( Activity act ) {
		Log.d( TAG, "Unregistering wifi receiver" ); 
		act.unregisterReceiver(wifiReceiver);
	}

	//==========================================================
	// PhoneStateListener
	//
	public static native void nativeCellularStateEvent( int state );
	public static native void nativeCellularSignalEvent( int level );
	
	private static class OvrPhoneStateListener extends PhoneStateListener {

		@Override
		public void onServiceStateChanged (ServiceState serviceState)
		{
			super.onServiceStateChanged( serviceState );

			Log.d( TAG, "OvrPhoneStateListener.onServiceStateChanged" + serviceState ); 

			nativeCellularStateEvent( serviceState.getState() );
		}
		
		@Override
		public void onSignalStrengthsChanged (SignalStrength signalStrength)
		{
			super.onSignalStrengthsChanged(signalStrength);

			int signalStrengthValue = 0;
			try
			{
				// getLevel is a hidden method in API 18-20 so need to use reflection to make it accessible
				Class signalStrengthClass = signalStrength.getClass();
				Method getLevelMethod = signalStrengthClass.getMethod("getLevel", new Class[]{});
				getLevelMethod.setAccessible(true);
				signalStrengthValue = (Integer) getLevelMethod.invoke(signalStrength, new Object[]{});
			}
			catch( Exception e )
			{
				Log.d( TAG, "Failed to get metthod: " + e.getMessage() ); 
			}
		
			Log.d( TAG, "OvrPhoneStateListener.onSignalStrengthsChanged" + signalStrengthValue ); 

			nativeCellularSignalEvent( signalStrengthValue );
		}
	}

	public static OvrPhoneStateListener phoneListener = null; 
	
	public static void startCellularReceiver( final Activity act ) {
		Log.d( TAG, "Registering OvrPhoneStateListener" );

    	act.runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
				final TelephonyManager phoneMgr = (TelephonyManager)act.getSystemService(Context.TELEPHONY_SERVICE);
				if ( phoneListener == null ) {
					phoneListener = new OvrPhoneStateListener();
				}
				phoneMgr.listen(phoneListener, PhoneStateListener.LISTEN_SERVICE_STATE | PhoneStateListener.LISTEN_SIGNAL_STRENGTHS);
    		}
    	});
	}
	public static void stopCellularReceiver( final Activity act ) {
		Log.d( TAG, "Unregistering OvrPhoneStateListener" );

    	act.runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
				final TelephonyManager phoneMgr = (TelephonyManager)act.getSystemService(Context.TELEPHONY_SERVICE);
				phoneMgr.listen(phoneListener, PhoneStateListener.LISTEN_NONE);
    		}
    	});
	}

	//==========================================================
	// VolumeReceiver
	public static native void nativeVolumeEvent(int volume);		

	private static class VolumeReceiver extends BroadcastReceiver {

		public Activity act;
		
		@Override
		public void onReceive(Context context, final Intent intent) {
	
			act.runOnUiThread(new Runnable() {
				public void run() {
					Log.d(TAG, "OnReceive VOLUME_CHANGED_ACTION" );
					int stream = ( Integer )intent.getExtras().get( "android.media.EXTRA_VOLUME_STREAM_TYPE" );
					int volume = ( Integer )intent.getExtras().get( "android.media.EXTRA_VOLUME_STREAM_VALUE" );
					if ( stream == AudioManager.STREAM_MUSIC )
					{
						Log.d(TAG, "calling nativeVolumeEvent()" );
						nativeVolumeEvent( volume );
					}
					else
					{
						Log.d(TAG, "skipping volume change from stream " + stream );						
					}
				}
			});
		}
	}

	public static VolumeReceiver volumeReceiver = null;
	public static IntentFilter volumeFilter = null;

	public static void startVolumeReceiver( Activity act ) {
	
		Log.d( TAG, "Registering volume receiver" );
		if ( volumeFilter == null ) {
			volumeFilter = new IntentFilter();
			volumeFilter.addAction( "android.media.VOLUME_CHANGED_ACTION" );			
		}
		if ( volumeReceiver == null ) {
			volumeReceiver = new VolumeReceiver();
		}
		volumeReceiver.act = act;

		act.registerReceiver(volumeReceiver, volumeFilter);
	
		AudioManager audio = (AudioManager)act.getSystemService(Context.AUDIO_SERVICE);
		int volume = audio.getStreamVolume(AudioManager.STREAM_MUSIC);
		Log.d( TAG, "startVolumeReceiver: " + volume );
		//nativeVolumeEvent( volume );
	}
	
	public static void stopVolumeReceiver( Activity act ) {
		Log.d( TAG, "Unregistering volume receiver" ); 
		act.unregisterReceiver(volumeReceiver);
	}

	public static void startReceivers( Activity act ) {
		startBatteryReceiver( act );
		startHeadsetReceiver( act );
		startVolumeReceiver( act );
		startWifiReceiver( act );
		startCellularReceiver( act );
	}

	public static void stopReceivers( Activity act ) {
		stopBatteryReceiver( act );
		stopHeadsetReceiver( act );
		stopVolumeReceiver( act );
		stopWifiReceiver( act );
		stopCellularReceiver( act );
	}

	public static void finishOnUiThread( final Activity act ) {
    	act.runOnUiThread( new Runnable()
    	{
		 @Override
    		public void run()
    		{
			 	Log.d(TAG, "finishOnUiThread calling finish()" );
    			act.finish();
    		//	act.overridePendingTransition(0, 0);
            }
    	});
	}
	
	public static void finishAffinityOnUiThread( final Activity act ) {
    	act.runOnUiThread( new Runnable()
    	{
		 @Override
    		public void run()
    		{
			 	Log.d(TAG, "finishAffinityOnUiThread calling finish()" );
				act.finishAffinity();
    		//	act.overridePendingTransition(0, 0);
            }
    	});
	}

	public static boolean getBluetoothEnabled( final Activity act ) {
		return Settings.Global.getInt( act.getContentResolver(), 
				Settings.Global.BLUETOOTH_ON, 0 ) != 0;
	}

	public static boolean isAirplaneModeEnabled( final Activity act ) {        
		return Settings.Global.getInt( act.getContentResolver(), 
				Settings.Global.AIRPLANE_MODE_ON, 0 ) != 0;
	}

	public static String getExternalStorageDirectory() {
		return Environment.getExternalStorageDirectory().getAbsolutePath();
	}
}
