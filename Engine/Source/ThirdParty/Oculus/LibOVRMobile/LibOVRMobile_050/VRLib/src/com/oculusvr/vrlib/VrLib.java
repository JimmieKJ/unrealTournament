/************************************************************************************

Filename    :   VrLib.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

package com.oculusvr.vrlib;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ContentResolver;
import android.media.AudioManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.util.Log;
import android.view.Choreographer;
import android.view.Surface;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Environment;
import android.os.StatFs;
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
import android.content.Context;
import android.content.ComponentName;
import android.media.AudioManager;
import android.media.AudioManager.OnAudioFocusChangeListener;
import android.widget.Toast;

import java.io.File;
import java.lang.Class;
import java.lang.reflect.Method;
import java.util.Locale;

/*
 * Static methods holding java code needed by VrLib. 
 */
public class VrLib implements android.view.Choreographer.FrameCallback,
		AudioManager.OnAudioFocusChangeListener {

	public static final String TAG = "VrLib";
	public static VrLib handler = new VrLib();

	public static final String INTENT_KEY_CMD = "intent_cmd";
	public static final String INTENT_KEY_FROM_PKG = "intent_pkg";

	public static String getCommandStringFromIntent( Intent intent ) {
		String commandStr = "";
		if ( intent != null ) {
			commandStr = intent.getStringExtra( INTENT_KEY_CMD );
			if ( commandStr == null ) {
				commandStr = "";
			}
		}
		return commandStr;
	}

	public static String getPackageStringFromIntent( Intent intent ) {
		String packageStr = "";
		if ( intent != null ) {
			packageStr = intent.getStringExtra( INTENT_KEY_FROM_PKG );
			if ( packageStr == null ) {
				packageStr = "";
			}
		}
		return packageStr;
	}

	public static String getUriStringFromIntent( Intent intent ) {
		String uriString = "";
		if ( intent != null ) {
			Uri uri = intent.getData();
			if ( uri != null ) {
				uriString = uri.toString();
				if ( uriString == null ) {
					uriString = "";
				}
			}
		}
		return uriString;
	}

	public static void sendIntent( Activity activity, Intent intent ) {
		try {
			Log.d( TAG, "startActivity " + intent );
			intent.addFlags( Intent.FLAG_ACTIVITY_NO_ANIMATION );
			activity.startActivity( intent );
			activity.overridePendingTransition( 0, 0 );
		}
		catch( ActivityNotFoundException e ) {
			Log.d( TAG, "startActivity " + intent + " not found!" );	
		}
		catch( Exception e ) {
			Log.e( TAG, "sendIntentFromNative threw exception " + e );
		}
	}

	// this creates an explicit intent
	public static void sendIntentFromNative( Activity activity, String actionName, String toPackageName, String toClassName, String commandStr, String uriStr ) {
		Log.d( TAG, "SendIntentFromNative: action: '" + actionName + "' toPackage: '" + toPackageName + "/" + toClassName + "' command: '" + commandStr + "' uri: '" + uriStr + "'" );
			
		Intent intent = new Intent( actionName );
		if ( toPackageName != null && !toPackageName.isEmpty() && toClassName != null && !toClassName.isEmpty() ) {
			// if there is no class name, this is an implicit intent. For launching another app with an
			// action + data, an implicit intent is required (see: http://developer.android.com/training/basics/intents/sending.html#Build)
			// i.e. we cannot send a non-launch intent
			ComponentName cname = new ComponentName( toPackageName, toClassName );
			intent.setComponent( cname );
			Log.d( TAG, "Sending explicit intent: " + cname.flattenToString() );
		}

		if ( uriStr.length() > 0 ) {
			intent.setData( Uri.parse( uriStr ) );
		}

		intent.putExtra( INTENT_KEY_CMD, commandStr );
		intent.putExtra( INTENT_KEY_FROM_PKG, activity.getApplicationContext().getPackageName() );

		sendIntent( activity, intent );
	}

	public static void broadcastIntent( Activity activity, String actionName, String toPackageName, String toClassName, String commandStr, String uriStr ) {
		Log.d( TAG, "broadcastIntent: action: '" + actionName + "' toPackage: '" + toPackageName + "/" + toClassName + "' command: '" + commandStr + "' uri: '" + uriStr + "'" );
		
		Intent intent = new Intent( actionName );
		if ( toPackageName != null && !toPackageName.isEmpty() && toClassName != null && !toClassName.isEmpty() ) {
			// if there is no class name, this is an implicit intent. For launching another app with an
			// action + data, an implicit intent is required (see: http://developer.android.com/training/basics/intents/sending.html#Build)
			// i.e. we cannot send a non-launch intent
			ComponentName cname = new ComponentName( toPackageName, toClassName );
			intent.setComponent( cname );
			Log.d( TAG, "Sending explicit broadcast: " + cname.flattenToString() );
		}

		if ( uriStr.length() > 0 ) {
			intent.setData( Uri.parse( uriStr ) );
		}

		intent.putExtra( INTENT_KEY_CMD, commandStr );
		intent.putExtra( INTENT_KEY_FROM_PKG, activity.getApplicationContext().getPackageName() );

		activity.sendBroadcast( intent );
	}

	// this gets the launch intent from the specified package name
	public static void sendLaunchIntent( Activity activity, String packageName, String commandStr, String uriStr ) {
		Log.d( TAG, "sendLaunchIntent: '" + packageName + "' command: '" + commandStr + "' uri: '" + uriStr + "'" );

		Intent launchIntent = activity.getPackageManager().getLaunchIntentForPackage( packageName );
		if ( launchIntent == null ) {
			Log.d( TAG, "sendLaunchIntent: null destination activity" );
			return;
		}
		
		launchIntent.putExtra( INTENT_KEY_CMD, commandStr );
		launchIntent.putExtra( INTENT_KEY_FROM_PKG, activity.getApplicationContext().getPackageName() );

		if ( uriStr.length() > 0 ) {
			launchIntent.setData( Uri.parse( uriStr ) );
		}

		sendIntent( activity, launchIntent );
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

	public static void logApplicationVrType( Activity act )
	{
		String vrType = "<none>";
		final PackageManager packageMgr = act.getApplicationContext().getPackageManager();
		if ( packageMgr != null )
		{
			try {
				PackageInfo packageInfo = packageMgr.getPackageInfo( act.getApplicationContext().getPackageName(), 0 );
				ApplicationInfo appInfo = act.getPackageManager().getApplicationInfo( act.getApplicationContext().getPackageName(), PackageManager.GET_META_DATA );
				if ( ( appInfo != null ) && ( appInfo.metaData != null ) )
				{
					vrType = appInfo.metaData.getString( "com.samsung.android.vr.application.mode", "<none>" );
				}
			} catch ( PackageManager.NameNotFoundException e ) {
				vrType = "<none>";
			}
		}

		Log.d( TAG, "APP VR_TYPE = " + vrType );
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

	public static boolean isDeveloperMode( final Activity act ) {
		return Settings.Global.getInt( act.getContentResolver(), "vrmode_developer_mode", 0 ) != 0;
	}

	public static int setSchedFifoStatic( final Activity activity, int tid, int rtPriority ) {
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

    		activity.runOnUiThread( new Runnable()
    		{
			 @Override
    			public void run()
    			{
					Toast toast = Toast.makeText( activity.getApplicationContext(), 
							"Security exception: make sure your application is signed for VR.", 
							Toast.LENGTH_LONG );
					toast.show();
				}
			} );
			// if we don't wait here, the app can exit before we see the toast
			long startTime = System.currentTimeMillis();
			do {
			} while( System.currentTimeMillis() - startTime < 5000 );

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
	}

	public static void releaseSystemPerformanceStatic( Activity activity )
	{
		Log.d(TAG, "releaseSystemPerformanceStatic");
		
		android.app.IVRManager vr = (android.app.IVRManager)activity.getSystemService(android.app.IVRManager.VR_MANAGER);
		if ( vr == null ) {
			Log.d(TAG, "VRManager was not found");
			return;
		}
		
		// release the frequency locks
		vr.relFreq( activity.getPackageName() );
		Log.d(TAG, "Releasing frequency lock");
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

	//--------------- Audio Focus -----------------------

	public static void requestAudioFocus( Activity act )
	{
		AudioManager audioManager = (AudioManager)act.getSystemService( Context.AUDIO_SERVICE );
		
		// Request audio focus
		int result = audioManager.requestAudioFocus( handler, AudioManager.STREAM_MUSIC,
			AudioManager.AUDIOFOCUS_GAIN );
		if ( result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED ) 
		{
			Log.d(TAG,"requestAudioFocus(): GRANTED audio focus");
		} 
		else if ( result == AudioManager.AUDIOFOCUS_REQUEST_FAILED ) 
		{
			Log.d(TAG,"requestAudioFocus(): FAILED to gain audio focus");
		}
	}

	public static void releaseAudioFocus( Activity act )
	{
		AudioManager audioManager = (AudioManager)act.getSystemService( Context.AUDIO_SERVICE );
		audioManager.abandonAudioFocus( handler );
	}
	
    public void onAudioFocusChange(int focusChange) 
    {
		switch( focusChange ) 
		{
		case AudioManager.AUDIOFOCUS_GAIN:
			// resume() if coming back from transient loss, raise stream volume if duck applied
			Log.d(TAG, "onAudioFocusChangedListener: AUDIOFOCUS_GAIN");
			break;
		case AudioManager.AUDIOFOCUS_LOSS:				// focus lost permanently
			// stop() if isPlaying
			Log.d(TAG, "onAudioFocusChangedListener: AUDIOFOCUS_LOSS");		
			break;
		case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:	// focus lost temporarily
			// pause() if isPlaying
			Log.d(TAG, "onAudioFocusChangedListener: AUDIOFOCUS_LOSS_TRANSIENT");	
			break;
		case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:	// focus lost temporarily
			// lower stream volume
			Log.d(TAG, "onAudioFocusChangedListener: AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK");		
			break;
		default:
			break;
		}
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
    			act.overridePendingTransition(0, 0);
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
    			act.overridePendingTransition(0, 0);
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

	// returns true if time settings specifies 24 hour format
	public static boolean isTime24HourFormat( Activity act ) {
		ContentResolver cr = act.getContentResolver();
		String v = Settings.System.getString( cr, android.provider.Settings.System.TIME_12_24 );
		if ( v == null || v.isEmpty() || v.equals( "12" ) ) {
			return false;
		}
		return true;
	}

	public static boolean packageIsInstalled( Activity act, String packageName ) { 
		PackageManager pm = act.getPackageManager();
		try {
			pm.getPackageInfo( packageName,PackageManager.GET_META_DATA );
		} catch ( NameNotFoundException e ) {
			Log.d( TAG, "Package " + packageName + " does NOT exist on device" );	
			return false;
		}

		Log.d( TAG, "Package " + packageName + " exists on device" );
		return true;
	}

	public static boolean isActivityInPackage( Activity act, String packageName, String activityName ) {
		Log.d( TAG, "isActivityInPackage( '" + packageName + "', '" + activityName + "' )" );
		PackageManager pm = act.getPackageManager();
		try {
			PackageInfo pi = pm.getPackageInfo( packageName, PackageManager.GET_ACTIVITIES );
			if ( pi == null ) {
				Log.d( TAG, "Could not get package info for " + packageName + "!" );
				return false;
			}
			if ( pi.activities == null ) {
				Log.d( TAG, "Package " + packageName + "has no activities!" );
				return false;
			}

			for ( int i = 0; i < pi.activities.length; i++ ) {
				Log.d( TAG, "activity[" + i + "] = " + pi.activities[i] );
				if ( pi.activities[i] != null && pi.activities[i].name.equals( activityName ) ) {
					Log.d( TAG, "Found activity " + activityName + " in package " + packageName + "!" );
					return true;
				}
			}
		}
		catch ( NameNotFoundException s ) {
			Log.d( TAG, "isActivityInPackage: package " + packageName + " does NOT exist on device!" );
		}
		return false;
	}

	public static boolean isHybridApp( final Activity act ) {
		try {
		    ApplicationInfo appInfo = act.getPackageManager().getApplicationInfo(act.getPackageName(), PackageManager.GET_META_DATA);
		    Bundle bundle = appInfo.metaData;
		    String applicationMode = bundle.getString("com.samsung.android.vr.application.mode");
		    return (applicationMode.equals("dual"));
		} catch( NameNotFoundException e ) {
			e.printStackTrace();
		} catch( NullPointerException e ) {
		    Log.e(TAG, "Failed to load meta-data, NullPointer: " + e.getMessage());         
		} 
		
		return false;
	}

	public static boolean isWifiConnected( final Activity act ) {
		ConnectivityManager connManager = ( ConnectivityManager ) act.getSystemService( Context.CONNECTIVITY_SERVICE );
		NetworkInfo mWifi = connManager.getNetworkInfo( ConnectivityManager.TYPE_WIFI );
		return mWifi.isConnected();
	}

	public static String getExternalStorageDirectory() {
		return Environment.getExternalStorageDirectory().getAbsolutePath();
	}
	
	// Converts some thing like "/sdcard" to "/sdcard/", always ends with "/" to indicate folder path
	public static String toFolderPathFormat( String inStr ) {
		if( inStr == null ||
			inStr.length() == 0	)
		{
			return "/";
		}
		
		if( inStr.charAt( inStr.length() - 1 ) != '/' )
		{
			return inStr + "/";
		}
		
		return inStr;
	}
	
	/*** Internal Storage ***/
	public static String getInternalStorageRootDir() {
		return toFolderPathFormat( Environment.getDataDirectory().getPath() );
	}
	
	public static String getInternalStorageFilesDir( Activity act ) {
		if ( act != null )
		{
			return toFolderPathFormat( act.getFilesDir().getPath() );
		}
		else
		{
			Log.e( TAG, "Activity is null in getInternalStorageFilesDir method" );
		}
		return "";
	}
	
	public static String getInternalStorageCacheDir( Activity act ) {
		if ( act != null )
		{
			return toFolderPathFormat( act.getCacheDir().getPath() );
		}
		else
		{
			Log.e( TAG, "activity is null getInternalStorageCacheDir method" );
		}
		return "";
	}

	public static long getInternalCacheMemoryInBytes( Activity act )
	{
		if ( act != null )
		{
			String path = getInternalStorageCacheDir( act );
			StatFs stat = new StatFs( path );
			return stat.getAvailableBytes();
		}
		else
		{
			Log.e( TAG, "activity is null getInternalCacheMemoryInBytes method" );
		}
		return 0;
	}
	
	/*** External Storage ***/
	public static String getExternalStorageFilesDirAtIdx( Activity act, int idx ) {
		if ( act != null )
		{
			File[] filesDirs = act.getExternalFilesDirs(null);
			if( filesDirs != null && filesDirs.length > idx && filesDirs[idx] != null )
			{
				return toFolderPathFormat( filesDirs[idx].getPath() );
			}
		}
		else
		{
			Log.e( TAG, "activity is null getExternalStorageFilesDirAtIdx method" );
		}
		return "";
	}
	
	public static String getExternalStorageCacheDirAtIdx( Activity act, int idx ) {
		if ( act != null )
		{
			File[] cacheDirs = act.getExternalCacheDirs();
			if( cacheDirs != null && cacheDirs.length > idx && cacheDirs[idx] != null )
			{
				return toFolderPathFormat( cacheDirs[idx].getPath() );
			}
		}
		else
		{
			Log.e( TAG, "activity is null in getExternalStorageCacheDirAtIdx method with index " + idx );
		}
		return "";
	}
	
	// Primary External Storage
	public static final int PRIMARY_EXTERNAL_STORAGE_IDX = 0;
	public static String getPrimaryExternalStorageRootDir( Activity act ) {
		return toFolderPathFormat( Environment.getExternalStorageDirectory().getPath() );
	}
	
	public static String getPrimaryExternalStorageFilesDir( Activity act ) {
		return getExternalStorageFilesDirAtIdx( act, PRIMARY_EXTERNAL_STORAGE_IDX );
	}
	
	public static String getPrimaryExternalStorageCacheDir( Activity act ) {
		return getExternalStorageCacheDirAtIdx( act, PRIMARY_EXTERNAL_STORAGE_IDX );
	}

	// Secondary External Storage
	public static final int SECONDARY_EXTERNAL_STORAGE_IDX = 1;
	public static String getSecondaryExternalStorageRootDir() {
		return "/storage/extSdCard/";
	}
	
	public static String getSecondaryExternalStorageFilesDir( Activity act ) {
		return getExternalStorageFilesDirAtIdx( act, SECONDARY_EXTERNAL_STORAGE_IDX );
	}
	
	public static String getSecondaryExternalStorageCacheDir( Activity act ) {
		return getExternalStorageCacheDirAtIdx( act, SECONDARY_EXTERNAL_STORAGE_IDX );
	}

	// we need this string to track when setLocale has change the language, otherwise if
	// the language is changed with setLocale, we can't determine the current language of
	// the application.
	private static String currentLanguage = null;

	public static void setCurrentLanguage( String lang ) {
		currentLanguage = lang;
		Log.d( TAG, "Current language set to '" + lang + "'." );
	}

	public static String getCurrentLanguage() {
		// In the case of Unity, the activity onCreate does not set the current langage
		// so we need to assume it is defaulted if setLocale() has never been called
		if ( currentLanguage == null || currentLanguage.isEmpty() ) {
			currentLanguage = Locale.getDefault().getLanguage();
		}
		return currentLanguage;
	}
}
