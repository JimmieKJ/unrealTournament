/************************************************************************************

Filename    :   VrActivity.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculusvr.vrlib;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityGroup;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.AssetFileDescriptor;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ApplicationInfo;
import android.graphics.Canvas;
import android.graphics.PorterDuff.Mode;
import android.graphics.SurfaceTexture;
import android.media.AudioManager;
import android.media.SoundPool;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.util.DisplayMetrics;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Toast;
//DVFS import android.os.DVFSHelper;


@SuppressWarnings("deprecation")
public class VrActivity extends ActivityGroup implements SurfaceHolder.Callback {

	public static final String TAG = "VrActivity";

	public static native void nativeNewIntent(long appPtr, String fromPackageName, String command, String uriString);
	public static native void nativeSurfaceChanged(long appPtr, Surface s);
	public static native void nativeSurfaceDestroyed(long appPtr);
	public static native void nativePause(long appPtr);
	public static native void nativeResume(long appPtr);
	public static native void nativeDestroy(long appPtr);
	public static native void nativeKeyEvent(long appPtr, int keyNum, boolean down, int repeatCount );
	public static native void nativeJoypadAxis(long appPtr, float lx, float ly, float rx, float ry);
	public static native void nativeTouch(long appPtr, int action, float x, float y );
	public static native SurfaceTexture nativeGetPopupSurfaceTexture(long appPtr);
	public static native void nativePopup(long appPtr, int width, int height, float seconds);

	// Pass down to native code so we talk to the right App object,
	// since there can be at least two with the PlatformUI open.
	//
	// This is set by the subclass in onCreate
	// 		appPtr = nativeSetAppInterface( this, ... );		
	public long	appPtr;
	

	// For trivial feedback sound effects
	SoundPool soundPool;
	List<Integer>	soundPoolSoundIds;
	List<String>	soundPoolSoundNames;

	public void playSoundPoolSound(String name) {
		for ( int i = 0 ; i < soundPoolSoundNames.size() ; i++ ) {
			if ( soundPoolSoundNames.get(i).equals( name ) ) {
				soundPool.play( soundPoolSoundIds.get( i ), 1.0f, 1.0f, 1, 0, 1 );
				return;
			}
		}

		Log.d(TAG, "playSoundPoolSound: loading "+name);
		
		// check first if this is a raw resource
		int soundId = 0;
		if ( name.indexOf( "res/raw/" ) == 0 ) {
			String resourceName = name.substring( 4, name.length() - 4 );
			int id = getResources().getIdentifier( resourceName, "raw", getPackageName() );
			if ( id == 0 ) {
				Log.e( TAG, "No resource named " + resourceName );
			} else {
				AssetFileDescriptor afd = getResources().openRawResourceFd( id );
				soundId = soundPool.load( afd, 1 );
			}
		} else {
			try {
				AssetFileDescriptor afd = getAssets().openFd( name );
				soundId = soundPool.load( afd, 1 );
			} catch ( IOException t ) {
				Log.e( TAG, "Couldn't open " + name + " because " + t.getMessage() );
			}
		}

		if ( soundId == 0 )
		{
			// Try to load the sound directly - works for absolute path - for wav files for sdcard for ex.
			soundId = soundPool.load( name, 1 );
		}

		soundPoolSoundNames.add( name );
		soundPoolSoundIds.add( soundId );
		
		soundPool.play( soundPoolSoundIds.get( soundPoolSoundNames.size() - 1 ), 1.0f, 1.0f, 1, 0, 1 );
	}
		
	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
		Log.d(TAG, this + " surfaceChanged() format: " + format + " width: " + width + " height: " + height);
		if ( width < height )
		{	// Samsung said that a spurious surface changed event with the wrong
			// orientation happens due to an Android issue.
			Log.d( TAG, "Ignoring a surface that is not in landscape mode" );
			return;
		}
		nativeSurfaceChanged(appPtr, holder.getSurface());
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
		Log.d(TAG, this + " surfaceCreated()");
		// needed for interface, but surfaceChanged() will always be called next
	}
	
	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		Log.d(TAG, this + " surfaceDestroyed()");
		nativeSurfaceDestroyed(appPtr);
	}

	public void finishActivity() {
		finish();
	}

	void adjustVolume(int direction) {
		AudioManager audio = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
		audio.adjustStreamVolume(AudioManager.STREAM_MUSIC, direction, 0);
	}
	
	float deadBand(float f) {
		// The K joypad seems to need a little more dead band to prevent
		// creep.
		if (f > -0.01f && f < 0.01f) {
			return 0.0f;
		}
		return f;
	}

	int axisButtons(int deviceId, float axisValue, int negativeButton, int positiveButton,
			int previousState) {
		int currentState;
		if (axisValue < -0.5f) {
			currentState = -1;
		} else if (axisValue > 0.5f) {
			currentState = 1;
		} else {
			currentState = 0;
		}
		if (currentState != previousState) {
			if (previousState == -1) {
				// negativeButton up
				buttonEvent(deviceId, negativeButton, false, 0);
			} else if (previousState == 1) {
				// positiveButton up
				buttonEvent(deviceId, positiveButton, false, 0);
			}

			if (currentState == -1) {
				// negativeButton down
				buttonEvent(deviceId, negativeButton, true, 0);
			} else if (currentState == 1) {
				// positiveButton down
				buttonEvent(deviceId, positiveButton, true, 0);
			}
		}
		return currentState;
	}

	int[] axisState = new int[6];
	int[] axisAxis = { MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y, MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_RX, MotionEvent.AXIS_RY };
	int[] axisNegativeButton = { JoyEvent.KEYCODE_DPAD_LEFT, JoyEvent.KEYCODE_DPAD_UP, JoyEvent.KEYCODE_LSTICK_LEFT, JoyEvent.KEYCODE_LSTICK_UP, JoyEvent.KEYCODE_RSTICK_LEFT, JoyEvent.KEYCODE_RSTICK_UP };
	int[] axisPositiveButton = { JoyEvent.KEYCODE_DPAD_RIGHT, JoyEvent.KEYCODE_DPAD_DOWN, JoyEvent.KEYCODE_LSTICK_RIGHT, JoyEvent.KEYCODE_LSTICK_DOWN, JoyEvent.KEYCODE_RSTICK_RIGHT, JoyEvent.KEYCODE_RSTICK_DOWN };
			
	@Override
	public boolean dispatchGenericMotionEvent(MotionEvent event) {
		if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0
				&& event.getAction() == MotionEvent.ACTION_MOVE) {
			// The joypad sends a single event with all the axis
			
			// Unfortunately, the Moga joypad in HID mode uses AXIS_Z
			// where the Samsnung uses AXIS_RX, and AXIS_RZ instead of AXIS_RY
			
			// Log.d(TAG,
			// String.format("onTouchEvent: %f %f %f %f",
			// event.getAxisValue(MotionEvent.AXIS_X),
			// event.getAxisValue(MotionEvent.AXIS_Y),
			// event.getAxisValue(MotionEvent.AXIS_RX),
			// event.getAxisValue(MotionEvent.AXIS_RY)));
			nativeJoypadAxis(appPtr, deadBand(event.getAxisValue(MotionEvent.AXIS_X)),
					deadBand(event.getAxisValue(MotionEvent.AXIS_Y)),
					deadBand(event.getAxisValue(MotionEvent.AXIS_RX))
						+ deadBand(event.getAxisValue(MotionEvent.AXIS_Z)),		// Moga uses  Z for R-stick X
					deadBand(event.getAxisValue(MotionEvent.AXIS_RY))
						+ deadBand(event.getAxisValue(MotionEvent.AXIS_RZ)));	// Moga uses RZ for R-stick Y

			// Turn the hat and thumbstick axis into buttons
			for ( int i = 0 ; i < 6 ; i++ ) {
				axisState[i] = axisButtons( event.getDeviceId(),
						event.getAxisValue(axisAxis[i]),
						axisNegativeButton[i], axisPositiveButton[i],
						axisState[i]);
			}
					
			return true;
		}
		return super.dispatchGenericMotionEvent(event);
	}

	public boolean dispatchKeyEvent(KeyEvent event) {
		//Log.d(TAG, "dispatchKeyEvent  " + event );
		boolean down;
		int keyCode = event.getKeyCode();
		int deviceId = event.getDeviceId();

		if (event.getAction() == KeyEvent.ACTION_DOWN) {
			down = true;
		} else if (event.getAction() == KeyEvent.ACTION_UP) {
			down = false;
		} else {
			Log.d(TAG,
					"source " + event.getSource() + " action "
							+ event.getAction() + " keyCode " + keyCode);

			return super.dispatchKeyEvent(event);
		}

		Log.d(TAG, "source " + event.getSource() + " keyCode " + keyCode + " down " + down + " repeatCount " + event.getRepeatCount() );

		if (keyCode == KeyEvent.KEYCODE_VOLUME_UP) {
			if ( down ) {
				adjustVolume(1);
			}
			return true;
		}

		if (keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
			if ( down ) {
				adjustVolume(-1);	
			}
			return true;
		}

		
		// Joypads will register as keyboards, but keyboards won't
		// register as position classes
		// || event.getSource() != 16777232)
		// Volume buttons are source 257
		if (event.getSource() == 1281) {
			keyCode |= JoyEvent.BUTTON_JOYPAD_FLAG;			
		}
		return buttonEvent(deviceId, keyCode, down, event.getRepeatCount() );
	}

	/*
	 * Called for real key events from dispatchKeyEvent(), and also
	 * the synthetic dpad 
	 * 
	 * This is where the framework can intercept a button press before
	 * it is passed on to the application.
	 * 
	 * Keyboard buttons will be standard Android key codes, but joystick buttons
	 * will have BUTTON_JOYSTICK or'd into it, because you may want arrow keys
	 * on the keyboard to perform different actions from dpad buttons on a
	 * joypad.
	 * 
	 * The BUTTON_* values are what you get in joyCode from our reference
	 * joypad.
	 * 
	 * @return Return true if this event was consumed.
	 */
	public boolean buttonEvent(int deviceId, int keyCode, boolean down, int repeatCount ) {
//			Log.d(TAG, "buttonEvent " + deviceId + " " + keyCode + " " + down);
		
		// DispatchKeyEvent will cause the K joypads to spawn other
		// apps on select and "game", which we don't want, so manually call
		// onKeyDown or onKeyUp
		
		KeyEvent ev = new KeyEvent( 0, 0, down ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP, keyCode, repeatCount );
		
// This was confusing because it called VrActivity::onKeyDown.  Activity::onKeyDown is only supposed to be 
// called if the app views didn't consume any keys. Since we intercept dispatchKeyEvent and always returning true
// for ACTION_UP and ACTION_DOWN, we effectively consume ALL key events that would otherwise go to Activity::onKeyDown
// Activity::onKeyUp, so calling them here means they're getting called when we consume events, even though the 
// VrActivity versions were effectively the consumers by calling nativeKeyEvent.  Instead, call nativeKeyEvent
// here directly.
		if ( down ) {
			nativeKeyEvent( appPtr, keyCode, true, ev.getRepeatCount() );
		}
		else
		{
			nativeKeyEvent( appPtr, keyCode, false, 0 );
		}
		return true;
	}

	/*
	 * These only happen if the application did not swallow the event.
	 */
	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		//Log.d(TAG, "onKeyDown " + keyCode + ", event = " + event );
		return super.onKeyDown(keyCode, event);
	}

	@Override
	public boolean onKeyUp(int keyCode, KeyEvent event) {
		//Log.d(TAG, "onKeyUp " + keyCode + ", event = " + event );
		return super.onKeyDown(keyCode, event);
	}
	
//	public boolean onTouchEvent( MotionEvent e ) {
	
	@Override
	public boolean dispatchTouchEvent( MotionEvent e ) {
		// Log.d(TAG, "real:" + e );		
		int action = e.getAction();
		float x = e.getRawX();
		float y = e.getRawY();
		Log.d(TAG, "onTouch dev:" + e.getDeviceId() + " act:" + action + " ind:" + e.getActionIndex() + " @ "+ x + "," + y );
		nativeTouch( appPtr, action, x, y );
		return true;
	}

	private void setLocale( String localeName )
	{
		Configuration conf = getResources().getConfiguration();
		conf.locale = new Locale( localeName );
		
		VrLib.setCurrentLanguage( conf.locale.getLanguage() );

		DisplayMetrics metrics = new DisplayMetrics();
		getWindowManager().getDefaultDisplay().getMetrics( metrics );
		
		// the next line just changes the application's locale. It changes this
		// globally and not just in the newly created resource
		Resources res = new Resources( getAssets(), metrics, conf );
		// since we don't need the resource, just allow it to be garbage collected.
		res = null;

		Log.d( TAG, "setLocale: locale set to " + localeName );
	}

	private void setDefaultLocale()
	{
		setLocale( "en" );
	}

	@Override
	protected void onStart() {
		Log.d(TAG, this + " onStart()");
		super.onStart();
	}
	
	@Override
	protected void onStop() {
		Log.d(TAG, this + " onStop()");
		super.onStop();
	}
	
	@Override
	protected void onDestroy() {
		Log.d(TAG, this + " onDestroy()");
		super.onDestroy();

		// prevent nativeSurfaceDestroyed from trying to use appPtr after the AppLocal it's pointing to has been freed.
		long tempAppPtr = appPtr;
		appPtr = 0;
		nativeDestroy( tempAppPtr );

		soundPool.release();
		soundPoolSoundIds.clear();
		soundPoolSoundNames.clear();
	 }
	
	@Override
	protected void onRestart() {
		Log.d(TAG, this + " onRestart()");		
		super.onRestart();
	}
	
	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		Log.d(TAG, this + " onConfigurationChanged()");
		super.onConfigurationChanged(newConfig);
	}

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		Log.d(TAG, this + " onCreate()" );
		super.onCreate(savedInstanceState);

		VrLib.setCurrentLanguage( Locale.getDefault().getLanguage() );

		// Create the SoundPool
		soundPool = new SoundPool(3 /* voices */, AudioManager.STREAM_MUSIC, 100);
		soundPoolSoundIds = new ArrayList<Integer>();
		soundPoolSoundNames = new ArrayList<String>();
				
		AudioManager audioMgr;
		audioMgr = (AudioManager)getSystemService(Context.AUDIO_SERVICE);
		String rate = audioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
		String size = audioMgr.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
		Log.d( TAG, "rate = " + rate );
		Log.d( TAG, "size = " + size );
		
		// Check preferences
		SharedPreferences prefs = getApplicationContext().getSharedPreferences( "oculusvr", MODE_PRIVATE );
		String username = prefs.getString( "username", "guest" );
		Log.d( TAG, "username = " + username );
		
		// Check for being launched with a particular intent
		Intent intent = getIntent();

		String commandString = VrLib.getCommandStringFromIntent( intent );
		String fromPackageNameString = VrLib.getPackageStringFromIntent( intent );
		String uriString = VrLib.getUriStringFromIntent( intent );

		Log.d( TAG, "action:" + intent.getAction() );
		Log.d( TAG, "type:" + intent.getType() );
		Log.d( TAG, "fromPackageName:" + fromPackageNameString );
		Log.d( TAG, "command:" + commandString );
		Log.d( TAG, "uri:" + uriString );

		SurfaceView sv = new SurfaceView( this );
		setContentView( sv );

		sv.getHolder().addCallback( this );

		// Force the screen to stay on, rather than letting it dim and shut off
		// while the user is watching a movie.
		getWindow().addFlags( WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON );

		// Force screen brightness to stay at maximum
		WindowManager.LayoutParams params = getWindow().getAttributes();
		params.screenBrightness = 1.0f;
		getWindow().setAttributes( params );
	}

	@Override
	protected void onNewIntent( Intent intent ) {
		Log.d(TAG, "onNewIntent()");

		String commandString = VrLib.getCommandStringFromIntent( intent );
		String fromPackageNameString = VrLib.getPackageStringFromIntent( intent );
		String uriString = VrLib.getUriStringFromIntent( intent );

		Log.d(TAG, "action:" + intent.getAction() );
		Log.d(TAG, "type:" + intent.getType() );
		Log.d(TAG, "fromPackageName:" + fromPackageNameString );
		Log.d(TAG, "command:" + commandString );
		Log.d(TAG, "uri:" + uriString );

		nativeNewIntent( appPtr, fromPackageNameString, commandString, uriString );
	}
	
	
	@Override
	protected void onPause() {
		Log.d(TAG, this + " onPause()");
		
		if( getApplication() instanceof VrApplication )
		{
			VrApplication vrApp = (VrApplication)getApplication();
			vrApp.setHostActivity( null );
		}

		super.onPause();
		nativePause(appPtr);
	}

	@Override
	protected void onResume() {
		Log.d(TAG, this + " onResume()");

		if( getApplication() instanceof VrApplication )
		{
			VrApplication vrApp = (VrApplication)getApplication();
			vrApp.setHostActivity( this );
		}

		super.onResume();
		nativeResume(appPtr);
	}

	// ==================================================================================
/*
	public void sendIntent( String packageName, String data ) {
		Log.d(TAG, "sendIntent " + packageName + " : " + data );
		
		Intent launchIntent = getPackageManager().getLaunchIntentForPackage(packageName);
		if ( launchIntent == null )
		{
			Log.d( TAG, "sendIntent null activity" );
			return;
		}
		if ( data.length() > 0 ) {
			launchIntent.setData( Uri.parse( data ) );
		}
		try {
			Log.d(TAG, "startActivity " + launchIntent );
			launchIntent.addFlags( Intent.FLAG_ACTIVITY_NO_ANIMATION );
			startActivity(launchIntent);
		} catch( ActivityNotFoundException e ) {
			Log.d( TAG, "startActivity " + launchIntent + " not found!" );	
			return;
		}
	
		// Make sure we don't leave any background threads running
		// This is not reliable, so it is done with native code.
		// Log.d(TAG, "System.exit" );
		// System.exit( 0 );
	}
*/
	
	public void clearVrToasts() {
		Log.v(TAG, "clearVrToasts, calling nativePopup" );
		nativePopup(appPtr, 0, 0, -1.0f);
	}

	SurfaceTexture toastTexture;
	Surface toastSurface;

	public void createVrToastOnUiThread(final String text) {
    	runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
    			VrActivity.this.createVrToast(text);
            }
    	});
	}
	
	// TODO: pass in the time delay

	// The warning about not calling show is irrelevant -- we are
	// drawing it to a texture
	@SuppressLint("ShowToast")
	public void createVrToast(String text) {
		if ( text == null ) {
			text = "null toast text!";
		}
		Log.v(TAG, "createVrToast " + text);

		// If we haven't set up the surface / surfaceTexture yet,
		// do it now.
		if (toastTexture == null) {
			toastTexture = nativeGetPopupSurfaceTexture(appPtr);
			if (toastTexture == null) {
				Log.e(TAG, "nativePreparePopup returned NULL");
				return; // not set up yet
			}
			toastSurface = new Surface(toastTexture);
		}

		Toast toast = Toast.makeText(this.getApplicationContext(), text,
				Toast.LENGTH_SHORT);

		this.createVrToast( toast.getView() );
	}
	
	public void createVrToast( View toastView )
	{
		// Find how big the toast wants to be.
		toastView.measure(0, 0);
		toastView.layout(0, 0, toastView.getMeasuredWidth(),
				toastView.getMeasuredHeight());

		Log.v(TAG,
				"toast size:" + toastView.getWidth() + "x"
						+ toastView.getHeight());
		toastTexture.setDefaultBufferSize(toastView.getWidth(),
				toastView.getHeight());
		try {
			Canvas canvas = toastSurface.lockCanvas(null);
			toastView.draw(canvas);
			toastSurface.unlockCanvasAndPost(canvas);
		} catch (Exception t) {
			Log.e(TAG, "lockCanvas threw exception");
		}

		nativePopup(appPtr, toastView.getWidth(), toastView.getHeight(), 7.0f);
	}
	
	static long downTime;
	
	public static void gazeEventFromNative( final float x, final float y, final boolean press, final boolean release, final Activity target ) {
		Log.d(TAG, "gazeEventFromNative( " + x + " " + y + " " + press + " " + release + " " + target );
	
	
		(new Handler(Looper.getMainLooper())).post(new Runnable() {
			@Override
			public void run() {
				long now = SystemClock.uptimeMillis();
				if ( press ) {
					downTime = now;
				}
					
				MotionEvent.PointerProperties pp = new MotionEvent.PointerProperties();
				pp.toolType = MotionEvent.TOOL_TYPE_FINGER;
				pp.id = 0;
				MotionEvent.PointerProperties[] ppa = new MotionEvent.PointerProperties[1];
				ppa[0] = pp;
				
				MotionEvent.PointerCoords pc = new MotionEvent.PointerCoords();
				pc.x = x;
				pc.y = y;
				MotionEvent.PointerCoords[] pca = new MotionEvent.PointerCoords[1];
				pca[0] = pc;
				
				int eventType = MotionEvent.ACTION_MOVE;
				if ( press )
				{
					eventType = MotionEvent.ACTION_DOWN;
				}
				else if ( release )
				{
					eventType = MotionEvent.ACTION_UP;
				}

				MotionEvent ev = MotionEvent.obtain( 
						downTime, now, 
						eventType,
						1, ppa, pca, 
						0, /* meta state */
						0, /* button state */
						1.0f, 1.0f, /* precision */
						10,	/* device ID */
						0,	/* edgeFlags */
						InputDevice.SOURCE_TOUCHSCREEN,
						0 /* flags */ );
				
				Log.d(TAG, "Synthetic:" + ev );
				Window w = target.getWindow();
				View v = w.getDecorView();
				v.dispatchTouchEvent( ev );
			}
			});		
	}

	public String getLocalizedString( String name ) {
		//Log.v("VrLocale", "getLocalizedString for " + name );
		String outString = "";
		int id = getResources().getIdentifier( name, "string", getPackageName() );
		if ( id == 0 )
		{
			// 0 is not a valid resource id
			Log.v("VrLocale", name + " is not a valid resource id!!" );
			return outString;
		} 
		if ( id != 0 ) 
		{
			outString = getResources().getText( id ).toString();
			//Log.v("VrLocale", "getLocalizedString resolved " + name + " to " + outString);
		}
		return outString;
	}

	public String getInstalledPackagePath( String packageName )
	{
		Log.d( TAG, "Searching installed packages for '" + packageName + "'" );
		List<ApplicationInfo> appList = getPackageManager().getInstalledApplications( 0 );
		String outString = "";
		for ( ApplicationInfo info : appList )
		{
/*
			if ( info.className != null && info.className.toLowerCase().contains( "oculus" ) )
			{
				Log.d( TAG, "info.className = '" + info.className + "'" );
			}
			else if ( info.sourceDir != null && info.sourceDir.toLowerCase().contains( "oculus" ) )
			{
				Log.d( TAG, "info.sourceDir = '" + info.sourceDir + "'" );
			}
*/
			if ( ( info.className != null && info.className.toLowerCase().contains( packageName ) ) || 
			     ( info.sourceDir != null && info.sourceDir.toLowerCase().contains( packageName ) ) )
			{			
				Log.d( TAG, "Found installed application package " + packageName );
				outString = info.sourceDir;
				return outString;
			}
		}
		return outString;
	}
}
