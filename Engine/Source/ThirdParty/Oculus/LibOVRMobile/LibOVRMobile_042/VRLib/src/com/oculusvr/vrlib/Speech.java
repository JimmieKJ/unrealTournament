/************************************************************************************

Filename    :   Speech.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

package com.oculusvr.vrlib;

import java.util.ArrayList;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.speech.RecognitionListener;
import android.speech.SpeechRecognizer;
import android.util.Log;

public class Speech implements RecognitionListener {
	public static native void nativeSpeech( long appPtr, String result, float confidence );
	
	public static final String TAG = "Speech";
	
	public static SpeechRecognizer	speechObject;
	public static Speech			instance;
	public static Activity			activityForUi;

	static long appPtr;
	
	public static void init( Activity act, long appPtr_ ) {
		//Log.d( TAG, "Got appPtr_ of " + Long.toHexString( appPtr_ ) );
		appPtr = appPtr_;
		activityForUi = act;
		Context appContext = act.getApplicationContext();
		if ( instance == null ) {
			instance = new Speech();
		}
		if ( speechObject == null ) {
			speechObject = SpeechRecognizer.createSpeechRecognizer( appContext );
			if ( speechObject == null ) {
				Log.d(TAG, "Failed to init SpeechObject" );		
			} else {
				Log.d(TAG, "SpeechObject initialized" );		
				speechObject.setRecognitionListener( instance );				
			}
		}
	}
	
	public static void start(long appPtr) {
		Log.d(TAG, "startVoiceControl" );
		if ( speechObject == null ) {
			return;
		}
    	activityForUi.runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
				Intent intent = new Intent();
				speechObject.startListening( intent );
    		}
    	});
	}
	
	public static void stop(long appPtr) {
		Log.d(TAG, "stopVoiceControl" );
		if ( speechObject == null ) {
			return;
		}
    	activityForUi.runOnUiThread( new Thread()
    	{
		 @Override
    		public void run()
    		{
				speechObject.stopListening();
    		}
    	});
	}
	
	@Override
	public void onReadyForSpeech(Bundle params) {
		// TODO Auto-generated method stub
		Log.d(TAG, "onReadyForSpeech" );		
		
	}
	@Override
	public void onBeginningOfSpeech() {
		// TODO Auto-generated method stub
		Log.d(TAG, "onBeginningOfSpeech" );		
		
	}
	@Override
	public void onRmsChanged(float rmsdB) {
		// TODO Auto-generated method stub
		Log.d(TAG, "onRmsChanged" );		
		
	}
	@Override
	public void onBufferReceived(byte[] buffer) {
		// TODO Auto-generated method stub
		Log.d(TAG, "onBufferReceived" );		
		
	}
	@Override
	public void onEndOfSpeech() {
		// TODO Auto-generated method stub
		Log.d(TAG, "onEndOfSpeech" );		
		
	}
	@Override
	public void onError(int error) {
		// TODO Auto-generated method stub
		Log.d(TAG, "onError" );		
		
	}
	@Override
	public void onResults(Bundle bundle) {
		// TODO Auto-generated method stub
		ArrayList<String> results = bundle.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
		if ( results.size() < 1 ) {
			Log.d(TAG, "onResults is empty" );
			return;
		}
		String result = results.get(0);
		float[] confidence = bundle.getFloatArray(SpeechRecognizer.CONFIDENCE_SCORES );
		float conf = confidence[0]; 
		Log.d(TAG, "onResults = " + result );
		nativeSpeech( appPtr, result, conf );
	}
	@Override
	public void onPartialResults(Bundle partialResults) {
		// TODO Auto-generated method stub
		Log.d(TAG, "onPartialResults" );		
		
	}
	@Override
	public void onEvent(int eventType, Bundle params) {
		// TODO Auto-generated method stub
		Log.d(TAG, "onEvent" );		
		
	}

}
