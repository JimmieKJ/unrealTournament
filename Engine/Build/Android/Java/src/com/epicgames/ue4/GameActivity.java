// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import java.io.File;

import java.util.Map;
import java.util.HashMap;
import java.util.ArrayList;

import android.app.NativeActivity;
import android.os.Bundle;
import android.util.Log;

import android.os.Vibrator;

import android.app.AlertDialog;
import android.app.Dialog;
import android.widget.EditText;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.inputmethod.EditorInfo;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.content.IntentSender.SendIntentException;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import android.media.AudioManager;
import android.util.DisplayMetrics;

import android.view.InputDevice;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewConfiguration;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.PopupWindow;

import android.media.AudioManager;

import com.google.android.gms.auth.GoogleAuthUtil;
import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.common.GooglePlayServicesUtil;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.games.Games;

import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.AdView;
import com.google.android.gms.ads.AdSize;
import com.google.android.gms.ads.AdListener;

import com.google.android.gms.plus.Plus;

import java.net.URL;
import java.net.HttpURLConnection;

import com.epicgames.ue4.GooglePlayStoreHelper;
import com.epicgames.ue4.GooglePlayLicensing;

// Console commands listener, only for debug builds
import com.epicgames.ue4.ConsoleCmdReceiver;

import android.os.Build;

// TODO: use the resources from the UE4 lib project once we've got the packager up and running
//import com.epicgames.ue4.R;

import com.epicgames.ue4.DownloadShim;

//$${gameActivityImportAdditions}$$

//Extending NativeActivity so that this Java class is instantiated
//from the beginning of the program.  This will allow the user
//to instantiate other Java libraries from here, that the user
//can then use the functions from C++
//NOTE -- This class is not necessary for the UnrealEngine C++ code
//  to startup, as this is handled through the base NativeActivity class.
//  This class's functionality is to provide a way to instantiate other
//  Java libraries at the startup of the program and store references 
//  to them in this class.

public class GameActivity extends NativeActivity implements SurfaceHolder.Callback2,
															GoogleApiClient.ConnectionCallbacks,
															GoogleApiClient.OnConnectionFailedListener
{
	public static Logger Log = new Logger("UE4");
	
	public static final int DOWNLOAD_ACTIVITY_ID = 80001; // so we can identify the activity later
	public static final int DOWNLOAD_NO_RETURN_CODE = 0; // we didn't get a return code - will need to log and debug as this shouldn't happen
	public static final int DOWNLOAD_FILES_PRESENT = 1;  // we already had the files we needed
	public static final int DOWNLOAD_COMPLETED_OK = 2; // downloaded ok (practically the same as above)
	public static final int DOWNLOAD_USER_QUIT = 3;    // user aborted the download
	public static final int DOWNLOAD_FAILED = 4;
	public static final int DOWNLOAD_INVALID = 5;
	public static final int DOWNLOAD_NO_PLAY_KEY = 6;
	public static final String DOWNLOAD_RETURN_NAME = "Result";
	
	static GameActivity _activity;
	static Bundle _bundle;

	protected Dialog mSplashDialog;
	private int noActionAnimID = -1;

	// Console
	AlertDialog consoleAlert;
	EditText consoleInputBox;
	ArrayList<String> consoleHistoryList;
	int consoleHistoryIndex;
	float consoleDistance;
	float consoleVelocity;

	// Virtual keyboard
	AlertDialog virtualKeyboardAlert;
	EditText virtualKeyboardInputBox;
	String virtualKeyboardPreviousContents;

	// Console commands receiver
	ConsoleCmdReceiver consoleCmdReceiver;

	// default the PackageDataInsideApk to an invalid value to make sure we don't get it too early
	private static int PackageDataInsideApkValue = -1;
	private static int HasOBBFiles = -1;
	
	// depthbuffer preference from manifest
	int DepthBufferPreference = 0;
	
	/** AssetManger reference - populated on start up and used when the OBB is packed into the APK */
	private AssetManager			AssetManagerReference;
	
	private GoogleApiClient googleClient;

	/** AdMob support */
	private PopupWindow adPopupWindow;
	private AdView adView;
	private boolean adInit = false;
	private LinearLayout adLayout;
	private int adGravity = Gravity.TOP;

	// layout required by popups, e.g ads, native controls
	LinearLayout activityLayout;

	/** true when the application has requested that an ad be displayed */
	private boolean adWantsToBeShown = false;

	/** true when an ad is available to be displayed */
	private boolean adIsAvailable = false;

	/** true when an ad request is in flight */
	private boolean adIsRequested = false;

	/** Request code to use when launching the Google Services resolution activity */
    private static final int GOOGLE_SERVICES_REQUEST_RESOLVE_ERROR = 1001;

	/** Unique tag for the error dialog fragment */
    private static final String DIALOG_ERROR = "dialog_error";

	/** Unique ID to identify Google Play Services error dialog */
	private static final int PLAY_SERVICES_DIALOG_ID = 1;
	
	private static String appPackageName = "";

	/** Check to see if we have all the files */
	private boolean HasAllFiles = false;
	
	/** Check to see if we should be verifying the files once we have them */
	public boolean VerifyOBBOnStartUp = false;

	/** Flag to ensure we have finished startup before allowing nativeOnActivityResult to get called */
	private boolean InitCompletedOK = false;
	
	private boolean ShouldHideUI = false;

	/** Whether this application is for distribution */
	private boolean IsForDistribution = false;

	/** Used for SurfaceHolder.setFixedSize buffer scaling workaround on early Amazon devices and some others */
	private boolean bUseSurfaceView = false;
	private SurfaceView MySurfaceView;
	private int DesiredHolderWidth = 0;
	private int DesiredHolderHeight = 0;
	
	enum EAlertDialogType
	{
		None,
		Console,
		Keyboard
	}
	private EAlertDialogType CurrentDialogType = EAlertDialogType.None;
	
	/** Access singleton activity for game. **/
	public static GameActivity Get()
	{
		return _activity;
	}
	
	/**
	Get the SDK level of the OS we are running in.
	We do this instead of accessing the SDK_INT
	with JNI from C++ as the new ART runtime seems to have
	problems dynamically finding/loading static inner classes.
	*/
	public static final int ANDROID_BUILD_VERSION = android.os.Build.VERSION.SDK_INT;
	
	private StoreHelper IapStoreHelper;

//$${gameActivityClassAdditions}$$

	@Override
	public void onStart()
	{
		super.onStart();
		
		if (IsForDistribution == false)
		{
			// Create console command broadcast listener
			consoleCmdReceiver = new ConsoleCmdReceiver(this);
			registerReceiver(consoleCmdReceiver, new IntentFilter(Intent.ACTION_RUN));
		}
		
//$${gameActivityOnStartAdditions}$$
		Log.debug("==================================> Inside onStart function in GameActivity");
	}

	public int getDeviceDefaultOrientation() 
	{

		// WindowManager windowManager =  (WindowManager) getSystemService(WINDOW_SERVICE);
		WindowManager windowManager =  getWindowManager();

		Configuration config = getResources().getConfiguration();

		int rotation = windowManager.getDefaultDisplay().getRotation();

		if ( ((rotation == android.view.Surface.ROTATION_0 || rotation == android.view.Surface.ROTATION_180) &&
				config.orientation == Configuration.ORIENTATION_LANDSCAPE)
			|| ((rotation == android.view.Surface.ROTATION_90 || rotation == android.view.Surface.ROTATION_270) &&    
				config.orientation == Configuration.ORIENTATION_PORTRAIT)) 
		{
			return Configuration.ORIENTATION_LANDSCAPE;
		}
		else 
		{
			return Configuration.ORIENTATION_PORTRAIT;
		}
	}

	private int getResourceId(String VariableName, String ResourceName, String PackageName)
	{
		try {
			return getResources().getIdentifier(VariableName, ResourceName, PackageName);
		}
		catch (Exception e) {
			e.printStackTrace();
			return -1;
		} 
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// create splashscreen dialog (if launched by SplashActivity)
		Bundle intentBundle = getIntent().getExtras();
		if (intentBundle != null)
		{
			ShouldHideUI = intentBundle.getString("ShouldHideUI") != null;
			if (intentBundle.getString("UseSplashScreen") != null)
			{
				try {
					// try to get the splash theme (can't use R.style.UE4SplashTheme since we don't know the package name until runtime)
					int SplashThemeId = getResources().getIdentifier("UE4SplashTheme", "style", getPackageName());
					mSplashDialog = new Dialog(this, SplashThemeId);
					mSplashDialog.setCancelable(false);
					if (ShouldHideUI)
					{
						View decorView = mSplashDialog.getWindow().getDecorView(); 
						// only do this on KitKat and above
						if(android.os.Build.VERSION.SDK_INT >= 19) {
							decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
														| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
														| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
														| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
														| View.SYSTEM_UI_FLAG_FULLSCREEN
														| View.SYSTEM_UI_FLAG_IMMERSIVE);  // NOT sticky.. will be set to sticky later! 
						}
					}
					mSplashDialog.show();
				}
				catch (Exception e) {
					e.printStackTrace();
				}
				try {
					noActionAnimID = getResources().getIdentifier("noaction", "anim", getPackageName());
				}
				catch (Exception e) {
					e.printStackTrace();
				}
			}
			try {
				noActionAnimID = getResources().getIdentifier("noaction", "anim", getPackageName());
			}
			catch (Exception e) {
				e.printStackTrace();
			}
		}
		
		// Suppress java logs in Shipping builds
		if (nativeIsShippingBuild())
		{
			Logger.SuppressLogs();
		}

		_activity = this;

		// layout required by popups, e.g ads, native controls
		MarginLayoutParams params = new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
		params.setMargins(0, 0, 0, 0);
		activityLayout = new LinearLayout(_activity);
		_activity.setContentView(activityLayout, params);

/*
		// Turn on and unlock screen.. Assumption is that this
		// will only really have an effect when for debug launching
		// as otherwise the screen is already unlocked.
		this.getWindow().addFlags(
			WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON |
//			WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED |
			WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
		// On some devices we can also unlock a key-locked screen by disabling the
		// keylock guard. To be safe we only do this on < Android 3.2. As the API
		// is deprecated from 3.2 onward.
		if (ANDROID_BUILD_VERSION < 13)
		{
			android.app.KeyguardManager keyman = (android.app.KeyguardManager)getSystemService(KEYGUARD_SERVICE);
			android.app.KeyguardManager.KeyguardLock keylock = keyman.newKeyguardLock("Unlock");
			keylock.disableKeyguard();
		}
*/

/*
		// log a list of input devices for debugging
		{
			int[] deviceIds = InputDevice.getDeviceIds();
			for (int deviceIndex=0; deviceIndex < deviceIds.length; deviceIndex++)
			{
				InputDevice inputDevice = InputDevice.getDevice(deviceIds[deviceIndex]);
				Log.debug("Device index " + deviceIndex + ": (deviceId=" + inputDevice.getId() + 
				", controllerNumber=" + inputDevice.getControllerNumber() + ", sources=" + String.format("%08x", inputDevice.getSources()) +
				", vendorId=" + String.format("%04x", inputDevice.getVendorId()) + ", productId=" + String.format("%04x", inputDevice.getProductId()) + 
				", descriptor=" + inputDevice.getDescriptor() +	", deviceName=" + inputDevice.getName() + ")");

				// is it a joystick?
				if ((inputDevice.getSources() & InputDevice.SOURCE_JOYSTICK) != 0)
				{
					Log.debug("Gamepad detected: (deviceIndex=" + deviceIndex + ", deviceId=" + inputDevice.getId() + ", deviceName=" + inputDevice.getName() + ")");
				}
			}
		}
*/

		// tell Android that we want volume controls to change the media volume, aka music
		setVolumeControlStream(AudioManager.STREAM_MUSIC);
		
		// is this a native landscape device (tablet, tv)?
		if ( getDeviceDefaultOrientation() == Configuration.ORIENTATION_LANDSCAPE )
		{
			boolean bForceLandscape = false;

			// check for a Google TV by checking system feature support
			if (getPackageManager().hasSystemFeature("com.google.android.tv"))
			{
				Log.debug( "Detected Google TV, will default to landscape" );
				bForceLandscape = true;
			} else

			// check NVidia devices
			if (android.os.Build.MANUFACTURER.equals("NVIDIA"))
			{
				// is it a Shield? (checking exact model)
				if (android.os.Build.MODEL.equals("SHIELD"))
				{
					Log.debug( "Detected NVidia Shield, will default to landscape" );
					bForceLandscape = true;
				}
			} else

			// check Ouya
			if (android.os.Build.MANUFACTURER.equals("OUYA"))
			{
				// only one so far (ouya_1_1) but check prefix anyway
				if (android.os.Build.MODEL.toLowerCase().startsWith("ouya_"))
				{
					Log.debug( "Detected Ouya console (" + android.os.Build.MODEL + "), will default to landscape" );
					bForceLandscape = true;
				}
			} else

			// check Amazon devices
			if (android.os.Build.MANUFACTURER.equals("Amazon"))
			{
				// is it a Kindle Fire TV? (Fire TV FAQ says AFTB, but to check for AFT)
				if (android.os.Build.MODEL.startsWith("AFT"))
				{
					Log.debug( "Detected Kindle Fire TV (" + android.os.Build.MODEL + "), will default to landscape" );
					bForceLandscape = true;
				}
			}

			// apply the force request if we found a device above
			if (bForceLandscape)
			{
				Log.debug( "Setting screen orientation to landscape because we have detected landscape device" );
				_activity.setRequestedOrientation( android.content.pm.ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE );
			}
		}
		
		// Grab a reference to the asset manager
		AssetManagerReference = this.getAssets();

		// Read metadata from AndroidManifest.xml
		appPackageName = getPackageName();
		String ProjectName = getPackageName();
		ProjectName = ProjectName.substring(ProjectName.lastIndexOf('.') + 1);
		try {
			ApplicationInfo ai = getPackageManager().getApplicationInfo(getPackageName(), PackageManager.GET_META_DATA);
			Bundle bundle = ai.metaData;
			_bundle = bundle;

			if ((ai.flags & ApplicationInfo.FLAG_DEBUGGABLE) == 0) 
			{
				IsForDistribution = true;
			}

			// Get the preferred depth buffer size from AndroidManifest.xml
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.DepthBufferPreference"))
			{
				DepthBufferPreference = bundle.getInt("com.epicgames.ue4.GameActivity.DepthBufferPreference");
				Log.debug( "Found DepthBufferPreference = " + DepthBufferPreference);
			}
			else
			{
				Log.debug( "Did not find DepthBufferPreference, using default.");
			}

			// Determine if data is embedded in APK from AndroidManifest.xml
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.bPackageDataInsideApk"))
			{
				PackageDataInsideApkValue = bundle.getBoolean("com.epicgames.ue4.GameActivity.bPackageDataInsideApk") ? 1 : 0;
				Log.debug( "Found bPackageDataInsideApk = " + PackageDataInsideApkValue);
			}
			else
			{
				PackageDataInsideApkValue = 0;
				Log.debug( "Did not find bPackageDataInsideApk, using default.");
			}

			// Get the project name from AndroidManifest.xml
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.ProjectName"))
			{
				ProjectName = bundle.getString("com.epicgames.ue4.GameActivity.ProjectName");
				Log.debug( "Found ProjectName = " + ProjectName);
			}
			else
			{
				Log.debug( "Did not find ProjectName, using package name = " + ProjectName);
			}
			
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.bHasOBBFiles"))
			{
				HasOBBFiles = bundle.getBoolean("com.epicgames.ue4.GameActivity.bHasOBBFiles") ? 1 : 0;
				Log.debug( "Found bHasOBBFiles = " + HasOBBFiles);
			}
			else
			{
				HasOBBFiles = 0;
				Log.debug( "Did not find bHasOBBFiles, using default.");
			}
			
			if (bundle.containsKey("com.epicgames.ue4.GameActivity.bVerifyOBBOnStartUp"))
			{
				VerifyOBBOnStartUp = bundle.getBoolean("com.epicgames.ue4.GameActivity.bVerifyOBBOnStartUp");
				Log.debug( "Found bVerifyOBBOnStartUp = " + VerifyOBBOnStartUp);
			}
			else
			{
				VerifyOBBOnStartUp = false;
				Log.debug( "Did not find bVerifyOBBOnStartUp, using default.");
			}
				
			if(bundle.containsKey("com.epicgames.ue4.GameActivity.bShouldHideUI"))
			{
				ShouldHideUI = bundle.getBoolean("com.epicgames.ue4.GameActivity.bShouldHideUI");
				Log.debug( "UI hiding set to " + ShouldHideUI);
			}
			else
			{
				Log.debug( "UI hiding not found. Leaving as " + ShouldHideUI);
			}

//$${gameActivityReadMetadataAdditions}$$
		}
		catch (NameNotFoundException e)
		{
			Log.debug( "Failed to load meta-data: NameNotFound: " + e.getMessage());
		}
		catch (NullPointerException e)
		{
			Log.debug( "Failed to load meta-data: NullPointer: " + e.getMessage());
		}

		// tell the engine if this is a portrait app
		nativeSetGlobalActivity();
		nativeSetWindowInfo(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT, DepthBufferPreference);

		// get the full language code, like en-US
		// note: this may need to be Locale.getDefault().getLanguage()
		String Language = java.util.Locale.getDefault().toString();

		Log.debug( "Android version is " + android.os.Build.VERSION.RELEASE );
		Log.debug( "Android manufacturer is " + android.os.Build.MANUFACTURER );
		Log.debug( "Android model is " + android.os.Build.MODEL );
		Log.debug( "OS language is set to " + Language );

		nativeSetAndroidVersionInformation( android.os.Build.VERSION.RELEASE, android.os.Build.MANUFACTURER, android.os.Build.MODEL, Language );

		try
		{
			int Version = getPackageManager().getPackageInfo(getPackageName(), 0).versionCode;
			int PatchVersion = 0;
			nativeSetObbInfo(ProjectName, getApplicationContext().getPackageName(), Version, PatchVersion);
		}
		catch (Exception e)
		{
			// if the above failed, then, we can't use obbs
			Log.debug("==================================> PackageInfo failure getting .obb info: " + e.getMessage());
		}
		
		// enable the physical volume controls to the game
		this.setVolumeControlStream(AudioManager.STREAM_MUSIC);

		AlertDialog.Builder builder;

		consoleInputBox = new EditText(this);
		consoleInputBox.setInputType(0x00080001); // TYPE_CLASS_TEXT | TYPE_TEXT_FLAG_NO_SUGGESTIONS);
		consoleHistoryList = new ArrayList<String>();
		consoleHistoryIndex = 0;

		final ViewConfiguration vc = ViewConfiguration.get(this);
        DisplayMetrics dm = getResources().getDisplayMetrics();
        consoleDistance = vc.getScaledPagingTouchSlop() * dm.density;
        consoleVelocity = vc.getScaledMinimumFlingVelocity() / 1000.0f;

		consoleInputBox.setOnTouchListener(new OnTouchListener() {
			private long downTime;
			private float downX;

			public void swipeLeft() {
				if (!consoleHistoryList.isEmpty() && consoleHistoryIndex + 1 < consoleHistoryList.size()) {
					consoleInputBox.setText(consoleHistoryList.get(++consoleHistoryIndex));
				}
			}

			public void swipeRight() {
				if (!consoleHistoryList.isEmpty() && consoleHistoryIndex > 0) {
					consoleInputBox.setText(consoleHistoryList.get(--consoleHistoryIndex));
				}
			}

			public boolean onTouch(View v, MotionEvent event) {
				switch (event.getAction()) {
					case MotionEvent.ACTION_DOWN: {
						// remember down time and position
						downTime = System.currentTimeMillis();
						downX = event.getX();
						return true;
					}
					case MotionEvent.ACTION_UP: {
						long deltaTime = System.currentTimeMillis() - downTime;
						float delta = event.getX() - downX;
						float absDelta = Math.abs(delta);

						if (absDelta > consoleDistance && absDelta > deltaTime * consoleVelocity)
						{
							if (delta < 0)
								this.swipeLeft();
							else
								this.swipeRight();
							return true;
						}
						return false;
					}
				}
				return false;
			}
		});

		builder = new AlertDialog.Builder(this);
		builder.setTitle("Console Window - Enter Command")
		.setMessage("")
		.setView(consoleInputBox)
		.setPositiveButton("Ok", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				String message = consoleInputBox.getText().toString().trim();

				// remove it if already in history
				int index = consoleHistoryList.indexOf(message);
				if (index >= 0)
					consoleHistoryList.remove(index);

				// add it to the end
				consoleHistoryList.add(message);

				nativeConsoleCommand(message);
				consoleInputBox.setText(" ");
				dialog.dismiss();
				CurrentDialogType = EAlertDialogType.None;
			}
		})
		.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				consoleInputBox.setText(" ");
				dialog.dismiss();
				CurrentDialogType = EAlertDialogType.None;
			}
		});
		consoleAlert = builder.create();

		virtualKeyboardInputBox = new EditText(this);
		if (ANDROID_BUILD_VERSION < 11)
		{
			virtualKeyboardInputBox.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
		}
		else
		{
			virtualKeyboardInputBox.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_FLAG_NO_FULLSCREEN);
		}
		virtualKeyboardInputBox.addTextChangedListener(new TextWatcher() {
			@Override
			public void beforeTextChanged(CharSequence charSequence, int start, int count, int after) {
			}

			@Override
			public void afterTextChanged(Editable s) {
				String message = virtualKeyboardInputBox.getText().toString();
				nativeVirtualKeyboardChanged(message);
			}

			@Override
			public void onTextChanged(CharSequence charSequence, int start, int before, int count) {
			}
		});

		builder = new AlertDialog.Builder(this);
		builder.setTitle("")
		.setView(virtualKeyboardInputBox)
		.setPositiveButton("Ok", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				String message = virtualKeyboardInputBox.getText().toString();
				nativeVirtualKeyboardResult(true, message);
				virtualKeyboardInputBox.setText(" ");
				dialog.dismiss();
				CurrentDialogType = EAlertDialogType.None;
			}
		})
		.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int id) {
				nativeVirtualKeyboardChanged(virtualKeyboardPreviousContents);
				nativeVirtualKeyboardResult(false, " ");
				virtualKeyboardInputBox.setText(" ");
				dialog.dismiss();
				CurrentDialogType = EAlertDialogType.None;
			}
		});
		virtualKeyboardAlert = builder.create();

		GooglePlayLicensing.GoogleLicensing = new GooglePlayLicensing();
		GooglePlayLicensing.GoogleLicensing.Init(this, Log);

		// Build Google Play API Client
		googleClient = new GoogleApiClient.Builder(this)
			.addConnectionCallbacks(this)
			.addOnConnectionFailedListener(this)
			.addApi(Games.API).addScope(Games.SCOPE_GAMES)
			.addApi(Plus.API).addScope(Plus.SCOPE_PLUS_LOGIN)
			.build();

		// Now okay for event handler to be set up on native side
		//	nativeResumeMainInit();
				
		// Try to establish a connection to Google Play
		// AndroidThunkJava_GooglePlayConnect();

		// If we have data in the apk or just loose then carry on init as normal
		/*Log.debug(this.getObbDir().getAbsolutePath());
		String path = this.getObbDir().getAbsolutePath() + "/main.1.com.epicgames.StrategyGame.obb";
		File obb = new File(path);
		Log.debug("=+=+=+=+=+=+=> File exists: " + (obb.exists() ? "True" : "False"));
		*/
		if(PackageDataInsideApkValue == 1 || HasOBBFiles == 0)
		{
			HasAllFiles = true;
		}

		// Need to create our surface view here regardless of if we are going to end up using it
		getWindow().takeSurface(null);
		MySurfaceView = new SurfaceView(this);
		MySurfaceView.getHolder().addCallback(this);
		setContentView(MySurfaceView);

//$${gameActivityOnCreateAdditions}$$
		
		Log.debug("==============> GameActive.onCreate complete!");
	}
	
	@Override
	public void onResume()
	{
		super.onResume();

		// invalidate window cache
		nativeSetWindowInfo(getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT, DepthBufferPreference);
		
		// only do this on KitKat and above
		if (ShouldHideUI)
		{ 
		
			View decorView = getWindow().getDecorView(); 
			if(android.os.Build.VERSION.SDK_INT >= 19) {
				decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
											| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
											| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_FULLSCREEN
											| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY); 
			} /*else {
				decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
											| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
											| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_FULLSCREEN);

			}*/
			
			decorView.setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener() {
				@Override public void onSystemUiVisibilityChange(int visibility) {
							View decorView = getWindow().getDecorView(); 
							if(android.os.Build.VERSION.SDK_INT >= 19) {
								decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
															| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
															| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
															| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
															| View.SYSTEM_UI_FLAG_FULLSCREEN
															| View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY); 
							} /*else {
								decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
															| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
															| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
															| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
															| View.SYSTEM_UI_FLAG_FULLSCREEN);

							}*/
						}
			});
		}
		
		if(HasAllFiles)
		{
			Log.debug("==============> Resuming main init");
			nativeResumeMainInit();
			InitCompletedOK = true;
		}
		else
		{
			// Start the check activity here
			Log.debug("==============> Starting activity to check files and download if required");
			Intent intent = new Intent(this, DownloadShim.GetDownloaderType());
			intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
			startActivityForResult(intent, DOWNLOAD_ACTIVITY_ID);
			if (noActionAnimID != -1)
			{
				overridePendingTransition(noActionAnimID, noActionAnimID);
			}
		}

//$${gameActivityOnResumeAdditions}$$
		Log.debug("==============> GameActive.onResume complete!");
	}

	@Override
	protected void onPause()
	{
		super.onPause();
		if(CurrentDialogType != EAlertDialogType.None)
		{
			//	If an AlertDialog is showing when the application is paused, it can cause our main window to be terminated
			//	Hide the dialog here. It will be shown again via AndroidThunkJava_ShowHiddenAlertDialog called from native code
			_activity.runOnUiThread(new Runnable()
			{
				public void run()
				{
					switch(CurrentDialogType)
					{
						case Keyboard:
							virtualKeyboardAlert.hide(); 
							break;
						case Console:
							consoleAlert.hide(); 
							break;
						default:
							Log.debug("ERROR: Unknown EAlertDialogType!");
							break;
					}
				}
			});
		}
//$${gameActivityOnPauseAdditions}$$
		Log.debug("==============> GameActive.onPause complete!");
	}

	@Override
	public void onStop()
	{
		super.onStop();

		if (consoleCmdReceiver != null)
		{
			unregisterReceiver(consoleCmdReceiver);
		}

//$${gameActivityOnStopAdditions}$$
		Log.debug("==============> GameActive.onStop complete!");
	}

	@Override
	public void onDestroy()
	{
		super.onDestroy();
		if( IapStoreHelper != null )
		{
			IapStoreHelper.onDestroy();
		}
//$${gameActivityOnDestroyAdditions}$$
		Log.debug("==============> GameActive.onDestroy complete!");
	}

	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
	{
		if(bUseSurfaceView)
		{
			int newWidth = (DesiredHolderWidth > 0) ? DesiredHolderWidth : width;
			int newHeight = (DesiredHolderHeight > 0) ? DesiredHolderHeight : height;

			super.surfaceChanged(holder, format, newWidth, newHeight);

			holder.setFixedSize(newWidth, newHeight);

			nativeSetSurfaceViewInfo(holder.getSurfaceFrame().width(), holder.getSurfaceFrame().height());
		}
		else
		{
			super.surfaceChanged(holder, format, width, height);
		}
	}

	public void AndroidThunkJava_ShowHiddenAlertDialog()
	{
		if(CurrentDialogType != EAlertDialogType.None)
		{
			Log.debug("==============> [JAVA] AndroidThunkJava_ShowHiddenAlertDialog() - Showing " + CurrentDialogType);
		
			//	If an AlertDialog was showing onPause and we hid it, show it again
			_activity.runOnUiThread(new Runnable()
			{
				public void run()
				{
					switch(CurrentDialogType)
					{
						case Keyboard:
							virtualKeyboardAlert.show(); 
							break;
						case Console:
							consoleAlert.show(); 
							break;
						default:
							Log.debug("ERROR: Unknown EAlertDialogType!");
							break;
					}
				}
			});
		}
	}

	// handle ad popup visibility and requests
	private void updateAdVisibility(boolean loadIfNeeded)
	{
		if (!adInit || (adPopupWindow == null))
		{
			return;
		}

		// request an ad if we don't have one available or requested, but would like one
		if (adWantsToBeShown && !adIsAvailable && !adIsRequested && loadIfNeeded)
		{
			AdRequest adRequest = new AdRequest.Builder().build();		// add test devices here
			_activity.adView.loadAd(adRequest);

			adIsRequested = true;
		}

		if (adIsAvailable && adWantsToBeShown)
		{
			if (adPopupWindow.isShowing())
			{
				return;
			}

			adPopupWindow.showAtLocation(activityLayout, adGravity, 0, 0);
			adPopupWindow.update();
		}
		else
		{
			if (!adPopupWindow.isShowing())
			{
				return;
			}

			adPopupWindow.dismiss();
			adPopupWindow.update();
		}
	}

	public void AndroidThunkJava_KeepScreenOn(boolean Enable)
	{
		if (Enable)
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					_activity.getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
				}
			});
		}
		else
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					_activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
				}
			});
		}
	}

	private class VibrateRunnable implements Runnable {
		private int duration;
		private Vibrator vibrator;

		VibrateRunnable(final int Duration, final Vibrator vibrator)
		{
			this.duration = Duration;
			this.vibrator = vibrator;
		}
		public void run ()
		{
			if (duration < 1)
			{
				vibrator.cancel();
			} else {
				vibrator.vibrate(duration);
			}
		}
	}

	public void AndroidThunkJava_Vibrate(int Duration)
	{
		Vibrator vibrator = (Vibrator)getSystemService(VIBRATOR_SERVICE);
		if (vibrator != null)
		{
			_activity.runOnUiThread(new VibrateRunnable(Duration, vibrator));
		}
	}

	// Called from event thread in NativeActivity	
	public void AndroidThunkJava_ShowConsoleWindow(String Formats)
	{
		if (consoleAlert.isShowing() == true)
		{
			Log.debug("Console already showing.");
			return;
		}

		// start at end of console history
		consoleHistoryIndex = consoleHistoryList.size();

		consoleAlert.setMessage("[Available texture formats: " + Formats + "]");
		_activity.runOnUiThread(new Runnable()
		{
			public void run()
			{
				if (consoleAlert.isShowing() == false)
				{
					Log.debug("Console not showing yet");
					consoleAlert.show(); 
					CurrentDialogType = EAlertDialogType.Console;
				}
			}
		});
	}

	public void AndroidThunkJava_ShowVirtualKeyboardInput(int InputType, String Label, String Contents)
	{
		if (virtualKeyboardAlert.isShowing() == true)
		{
			Log.debug("Virtual keyboard already showing.");
			return;
		}

		// Set label and starting contents
		virtualKeyboardAlert.setTitle(Label);
		virtualKeyboardInputBox.setText("");
		virtualKeyboardInputBox.append(Contents);
		virtualKeyboardPreviousContents = Contents;

		// configure for type of input
		virtualKeyboardInputBox.setInputType(InputType);

		_activity.runOnUiThread(new Runnable()
		{
			public void run()
			{
				if (virtualKeyboardAlert.isShowing() == false)
				{
					Log.debug("Virtual keyboard not showing yet");
					virtualKeyboardAlert.show(); 
					CurrentDialogType = EAlertDialogType.Keyboard;
				}
			}
		});
	}
	
	public void AndroidThunkJava_LaunchURL(String URL)
	{
		try
		{
			Intent BrowserIntent = new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(URL));
			startActivity(BrowserIntent);
		}
		catch (Exception e)
		{
			Log.debug("LaunchURL failed with exception " + e.getMessage());
		}
	}

	public void AndroidThunkJava_ResetAchievements()
	{
		try
        {
			String email = Plus.AccountApi.getAccountName(googleClient);
			Log.debug("AndroidThunkJava_ResetAchievements: using email " + email);

            String accesstoken = GoogleAuthUtil.getToken(this, email, "oauth2:https://www.googleapis.com/auth/games");

			String ResetURL = "https://www.googleapis.com/games/v1management/achievements/reset?access_token=" + accesstoken;
			Log.debug("AndroidThunkJava_ResetAchievements: using URL " + ResetURL);

			URL url = new URL(ResetURL);
			HttpURLConnection urlConnection = (HttpURLConnection)url.openConnection();

			try
			{
				urlConnection.setRequestMethod("POST");
				int status = urlConnection.getResponseCode();
				Log.debug("AndroidThunkJava_ResetAchievements: HTTP response is " + status);
			}
			finally
			{
				urlConnection.disconnect();
			}
        }
        catch(Exception e)
        {
            Log.debug("AndroidThunkJava_ResetAchievements failed: " + e.getMessage());
        }
	}

	public void AndroidThunkJava_ShowAdBanner(String AdMobAdUnitID, boolean bShowOnBottonOfScreen)
	{
		Log.debug("In AndroidThunkJava_ShowAdBanner");
		Log.debug("AdID: " + AdMobAdUnitID);

		adGravity = bShowOnBottonOfScreen ? Gravity.BOTTOM : Gravity.TOP;

		if (adInit)
		{
			// already created, make it visible
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					if ((adPopupWindow == null) || adPopupWindow.isShowing())
					{
						return;
					}

					adWantsToBeShown = true;
					updateAdVisibility(true);
				}
			});

			return;
		}

		// init our AdMob window
		adView = new AdView(this);
		adView.setAdUnitId(AdMobAdUnitID);
		adView.setAdSize(AdSize.BANNER);

		if (adView != null)
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					adInit = true;

					final DisplayMetrics dm = getResources().getDisplayMetrics();
					final float scale = dm.density;
					adPopupWindow = new PopupWindow(_activity);
					adPopupWindow.setWidth((int)(320*scale));
					adPopupWindow.setHeight((int)(50*scale));
					adPopupWindow.setWindowLayoutMode(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
					adPopupWindow.setClippingEnabled(false);

					adLayout = new LinearLayout(_activity);

					final int padding = (int)(-5*scale);
					adLayout.setPadding(padding,padding,padding,padding);

					MarginLayoutParams params = new MarginLayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);;

					params.setMargins(0,0,0,0);

					adLayout.setOrientation(LinearLayout.VERTICAL);
					adLayout.addView(adView, params);
					adPopupWindow.setContentView(adLayout);

					// set up our ad callbacks
					_activity.adView.setAdListener(new AdListener()
					{
						 @Override
						public void onAdLoaded()
						{
							adIsAvailable = true;
							adIsRequested = false;

							updateAdVisibility(true);
						}

						 @Override
						public void onAdFailedToLoad(int errorCode)
						{
							adIsAvailable = false;
							adIsRequested = false;

							// don't immediately request a new ad on failure, wait until the next show
							updateAdVisibility(false);
						}
					});

					adWantsToBeShown = true;
					updateAdVisibility(true);
				}
			});
		}
	}

	public void AndroidThunkJava_HideAdBanner()
	{
		Log.debug("In AndroidThunkJava_HideAdBanner");

		if (!adInit)
		{
			return;
		}

		_activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				adWantsToBeShown = false;
				updateAdVisibility(true);
			}
		});
	}

	public void AndroidThunkJava_CloseAdBanner()
	{
		Log.debug("In AndroidThunkJava_CloseAdBanner");

		if (!adInit)
		{
			return;
		}

		// currently the same as hide.  should we do a full teardown?
		_activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				adWantsToBeShown = false;
				updateAdVisibility(true);
			}
		});
	}

	public void AndroidThunkJava_GoogleClientConnect()
	{
		if (googleClient != null)
		{
			googleClient.connect();
		}
	}

	public void AndroidThunkJava_GoogleClientDisconnect()
	{
		if (googleClient != null)
		{
			googleClient.disconnect();
		}
	}

	// Google Client connected successfully
	@Override
	public void onConnected(Bundle connectionHint)
	{
		if (googleClient != null && googleClient.isConnected())
		{
			new Thread(new Runnable()
			{
				public void run()
				{
					try
					{
						String email = Plus.AccountApi.getAccountName(googleClient);
						Log.debug("Google Client Connect using email " + email);

						String accesstoken = GoogleAuthUtil.getToken(GameActivity.Get(), email, "oauth2:https://www.googleapis.com/auth/games");
						Log.debug("Google Client Connect using Access Token " + accesstoken);

						nativeGoogleClientConnectCompleted(true, accesstoken);
					}
					catch (Exception e)
					{
						Log.debug("Google Client Connect failed: " + e.getMessage());

						nativeGoogleClientConnectCompleted(false, "");
					}
				}
			}).start();
		}
		else
		{
			nativeGoogleClientConnectCompleted(false, "");
		}
	}

	// Google Client connection failed
	@Override
	public void onConnectionFailed(ConnectionResult connectionResult)
	{
		nativeGoogleClientConnectCompleted(false, "");
	}

	// Google Client connection suspended
	@Override
	public void onConnectionSuspended(int cause)
	{
	}

	public AssetManager AndroidThunkJava_GetAssetManager()
	{
		if(AssetManagerReference == null)
		{
			Log.debug("No reference to asset manager found!");
		}

		return AssetManagerReference;
	}

	public static boolean isOBBInAPK()
	{
		Log.debug("Asking if osOBBInAPK? " + (PackageDataInsideApkValue == 1));
		return PackageDataInsideApkValue == 1;
	}

	public void AndroidThunkJava_Minimize()
	{
		Intent startMain = new Intent(Intent.ACTION_MAIN);
		startMain.addCategory(Intent.CATEGORY_HOME);
		startMain.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
		startActivity(startMain);
	}

	public void AndroidThunkJava_ForceQuit()
	{
		System.exit(0);
		// finish();
	}

	// call back into native code from the Java UI thread, initializing any available VR HMD modules
	public void AndroidThunkJava_InitHMDs()
	{
		_activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				nativeInitHMDs();
			}
		});
	}

	public static String AndroidThunkJava_GetFontDirectory()
	{
		// Parse and find the first known fonts directory on the device
		String[] fontdirs = { "/system/fonts", "/system/font", "/data/fonts" };

		String targetDir = null;

		for ( String fontdir : fontdirs )
        {
            File dir = new File( fontdir );

			if(dir.exists())
			{
				targetDir = fontdir;
				break;
			}
		}
		
		return targetDir + "/";
	}
	
	public static String getAppPackageName()
	{
		return appPackageName;
	}

	public boolean AndroidThunkJava_IsMusicActive()
	{
		AudioManager audioManager = (AudioManager)this.getSystemService(Context.AUDIO_SERVICE);
		return audioManager.isMusicActive();
	}
	
	// In app purchase functionality
	public void AndroidThunkJava_IapSetupService(String InProductKey)
	{
		if (getPackageManager().checkPermission("com.android.vending.BILLING", getPackageName()) == getPackageManager().PERMISSION_GRANTED)
		{
			IapStoreHelper = new GooglePlayStoreHelper(InProductKey, this, Log);
			if (IapStoreHelper != null)
			{
				Log.debug("[JAVA] - AndroidThunkJava_IapSetupService - Failed to setup IAP service");
			}
		}
		else
		{
			Log.debug("[JAVA] - AndroidThunkJava_IapSetupService - You do not have the appropriate permission setup.");
			Log.debug("[JAVA] - AndroidThunkJava_IapSetupService - Please ensure com.android.vending.BILLING is added to the manifest.");
		}
	}
	

	private String[] CachedQueryProductIDs;
	private boolean[] CachedQueryConsumables;
	public boolean AndroidThunkJava_IapQueryInAppPurchases(String[] ProductIDs, boolean[] bConsumable)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapQueryInAppPurchases");
		CachedQueryProductIDs = ProductIDs;
		CachedQueryConsumables = bConsumable;

		boolean bTriggeredQuery = false;
		if( IapStoreHelper != null )
		{
			bTriggeredQuery = true;

			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					IapStoreHelper.QueryInAppPurchases(CachedQueryProductIDs, CachedQueryConsumables);
				}
			});
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bTriggeredQuery;
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data)
	{
		if( requestCode == DOWNLOAD_ACTIVITY_ID)
		{
			int errorCode = 0;
			if(resultCode == RESULT_OK)
			{
								
				errorCode = data.getIntExtra(DOWNLOAD_RETURN_NAME, DOWNLOAD_NO_RETURN_CODE);
				
				String logMsg = "DownloadActivity Returned with ";
				switch(errorCode)
				{
				case DOWNLOAD_FILES_PRESENT:
					logMsg += "Download Files Present";
					break;
				case DOWNLOAD_COMPLETED_OK:
					logMsg += "Download Completed OK";
					break;
				case DOWNLOAD_NO_RETURN_CODE:
					logMsg += "Download No Return Code";
					break;
				case DOWNLOAD_USER_QUIT:
					logMsg += "Download User Quit";
					break;
				case DOWNLOAD_FAILED:
					logMsg += "Download Failed";
					break;
				case DOWNLOAD_INVALID:
					logMsg += "Download Invalid";
					break;
				case DOWNLOAD_NO_PLAY_KEY:
					logMsg +="Download No Play Key";
					break;
				default:
					logMsg += "Unknown message!";
					break;
				}
				
				Log.debug(logMsg);
			}
			else
			{
				Log.debug("Download activity cancelled by user.");
				errorCode = DOWNLOAD_USER_QUIT;
			}
			
			HasAllFiles = (errorCode == DOWNLOAD_FILES_PRESENT || errorCode == DOWNLOAD_COMPLETED_OK);
			
			if(errorCode == DOWNLOAD_NO_RETURN_CODE 
			|| errorCode == DOWNLOAD_USER_QUIT 
			|| errorCode == DOWNLOAD_FAILED 
			|| errorCode == DOWNLOAD_INVALID
			|| errorCode == DOWNLOAD_NO_PLAY_KEY)
			{
				finish();
				return;
			}
		}
		else if( IapStoreHelper != null )
		{
			if(!IapStoreHelper.onActivityResult(requestCode, resultCode, data))
			{
				super.onActivityResult(requestCode, resultCode, data);
			}
			else
			{
				Log.debug("[JAVA] - Store Helper handled onActivityResult");
			}
		}
		else
		{
			super.onActivityResult(requestCode, resultCode, data);
		}

//$${gameActivityOnActivityResultAdditions}$$
		
		if(InitCompletedOK)
		{
			nativeOnActivityResult(this, requestCode, resultCode, data);
		}
	}
	
	public boolean AndroidThunkJava_IapBeginPurchase(String ProductId, boolean bConsumable)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapBeginPurchase");
		boolean bTriggeredPurchase = false;
		if( IapStoreHelper != null )
		{
			bTriggeredPurchase = IapStoreHelper.BeginPurchase(ProductId, bConsumable);
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bTriggeredPurchase;
	}

	public boolean AndroidThunkJava_IapIsAllowedToMakePurchases()
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapIsAllowedToMakePurchases");
		boolean bIsAllowedToMakePurchase = false;
		if( IapStoreHelper != null )
		{
			bIsAllowedToMakePurchase = IapStoreHelper.IsAllowedToMakePurchases();
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bIsAllowedToMakePurchase;
	}

	public boolean AndroidThunkJava_IapRestorePurchases(String[] InProductIDs, boolean[] bConsumable)
	{
		Log.debug("[JAVA] - AndroidThunkJava_IapRestorePurchases");
		boolean bTriggeredRestore = false;
		if( IapStoreHelper != null )
		{
			Log.debug("[JAVA] - AndroidThunkJava_IapRestorePurchases - Kick off logic here!");
			bTriggeredRestore = IapStoreHelper.RestorePurchases(InProductIDs, bConsumable);
		}
		else
		{
			Log.debug("[JAVA] - Store Helper is invalid");
		}
		return bTriggeredRestore;
	}

	public void AndroidThunkJava_DismissSplashScreen()
	{
		if (mSplashDialog != null)
		{
			mSplashDialog.dismiss();
			mSplashDialog = null;
		}
	}

	private static class DeviceInfoData {
		public final int vendorId;
		public final int productId;
		public final String name;

		DeviceInfoData(int vid, int pid, String inName)
		{
			vendorId = vid;
			productId = pid;
			name = inName;
		}

		boolean IsMatch(int vid, int pid)
		{
			return (vendorId == vid && productId == pid);
		}
	}

	// List of vendor/product ids
	private static final DeviceInfoData[] DeviceInfoList = {
		new DeviceInfoData(0x04e8, 0xa000, "Samsung Game Pad EI-GP20"),
		new DeviceInfoData(0x0955, 0x7203, "NVIDIA Corporation NVIDIA Controller v01.01"),
		new DeviceInfoData(0x0955, 0x7210, "NVIDIA Corporation NVIDIA Controller v01.03"),
		new DeviceInfoData(0x1949, 0x0404, "Amazon Fire TV Remote"),
		new DeviceInfoData(0x1949, 0x0406, "Amazon Fire Game Controller")
	};

	public class InputDeviceInfo {
		public int deviceId;
		public int vendorId;
		public int productId;
		public int controllerId;
		public String name;
		public String descriptor;

		InputDeviceInfo(int did, int vid, int pid, int cid, String inName, String inDescriptor)
		{
			deviceId = did;
			vendorId = vid;
			productId = pid;
			controllerId = cid;
			name = inName;
			descriptor = inDescriptor;
		}
	}

	public InputDeviceInfo AndroidThunkJava_GetInputDeviceInfo(int deviceId)
	{
		int[] deviceIds = InputDevice.getDeviceIds();
		for (int deviceIndex=0; deviceIndex < deviceIds.length; deviceIndex++)
		{
			InputDevice inputDevice = InputDevice.getDevice(deviceIds[deviceIndex]);
			if (inputDevice.getId() == deviceId)
			{
				int vendorId = 0;
				int productId = 0;
				int controllerNumber = 0;

				// requires 4.1 (Jellybean)
				String descriptor = ANDROID_BUILD_VERSION >= 16 ? inputDevice.getDescriptor() : Integer.toString(deviceId);

				// supported on 4.4 (KitKat) onward
				if (ANDROID_BUILD_VERSION >= 19)
				{
					vendorId = inputDevice.getVendorId();
					productId = inputDevice.getProductId();
					controllerNumber = inputDevice.getControllerNumber();

					// note: inputDevice.getName may not return a proper name so check vendor and product id first
					for (DeviceInfoData deviceInfo : DeviceInfoList)
					{
						if (deviceInfo.IsMatch(vendorId, productId))
						{
							return new InputDeviceInfo(deviceId, vendorId, productId, controllerNumber, deviceInfo.name, descriptor);
						}
					}
				}

				// use device name as fallback (may be generic like "Bluetooth HID" so not always useful)
				return new InputDeviceInfo(deviceId, vendorId, productId, controllerNumber, inputDevice.getName(), descriptor);
			}
		}
		return new InputDeviceInfo(deviceId, 0, 0, -1, "Unknown", "Unknown");
	}

	public void AndroidThunkJava_UseSurfaceViewWorkaround()
	{
		// We only need apply a change to the SurfaceHolder on the first call
		// Once bUseSurfaceView is true, it is never changed back
		if(bUseSurfaceView)
		{
			return;
		}

		bUseSurfaceView = true;
		Log.debug("[JAVA] Using SurfaceView sizing workaround for this device");

		if(DesiredHolderWidth > 0 && 
			DesiredHolderHeight > 0 &&
			MySurfaceView != null)
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					MySurfaceView.getHolder().setFixedSize(DesiredHolderWidth, DesiredHolderHeight);
				}
			});
		}
	}

	public void AndroidThunkJava_SetDesiredViewSize(int width, int height)
	{
		if (width == DesiredHolderWidth && height == DesiredHolderHeight)
		{
			return;
		}

		Log.debug("[JAVA] - SetDesiredViewSize width=" + width + " and height=" + height);
		DesiredHolderWidth = width;
		DesiredHolderHeight = height;

		if(bUseSurfaceView && MySurfaceView != null)
		{
			_activity.runOnUiThread(new Runnable()
			{
				@Override
				public void run()
				{
					MySurfaceView.getHolder().setFixedSize(DesiredHolderWidth, DesiredHolderHeight);
				}
			});
		}
	}

	public boolean AndroidThunkJava_HasMetaDataKey(String key)
	{
		if (_bundle == null || key == null)
		{
			return false;
		}
		return _bundle.containsKey(key);
	}

	public boolean AndroidThunkJava_GetMetaDataBoolean(String key)
	{
		if (_bundle == null || key == null)
		{
			return false;
		}
		return _bundle.getBoolean(key);
	}

	public int AndroidThunkJava_GetMetaDataInt(String key)
	{
		if (_bundle == null || key == null)
		{
			return 0;
		}
		return _bundle.getInt(key);
	}

	public String AndroidThunkJava_GetMetaDataString(String key)
	{
		if (_bundle == null || key == null)
		{
			return null;
		}
		return _bundle.getString(key);
	}

	public native boolean nativeIsShippingBuild();
	public native void nativeSetGlobalActivity();
	public native void nativeSetWindowInfo(boolean bIsPortrait, int DepthBufferPreference);
	public native void nativeSetObbInfo(String ProjectName, String PackageName, int Version, int PatchVersion);
	public native void nativeSetAndroidVersionInformation( String AndroidVersion, String PhoneMake, String PhoneModel, String OSLanguage );

	public native void nativeSetSurfaceViewInfo(int width, int height);

	public native void nativeConsoleCommand(String commandString);
	public native void nativeVirtualKeyboardChanged(String contents);
	public native void nativeVirtualKeyboardResult(boolean update, String contents);

	public native void nativeInitHMDs();

	public native void nativeResumeMainInit();

	public native void nativeOnActivityResult(GameActivity activity, int requestCode, int resultCode, Intent data);

	public native void nativeGoogleClientConnectCompleted(boolean bSuccess, String accessToken);
		
	static
	{
		System.loadLibrary("gnustl_shared");
//$${soLoadLibrary}$$
		System.loadLibrary("UE4");
	}
}

