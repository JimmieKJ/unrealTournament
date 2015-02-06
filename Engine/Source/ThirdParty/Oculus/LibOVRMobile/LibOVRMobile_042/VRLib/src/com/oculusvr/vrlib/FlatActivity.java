/************************************************************************************

Filename    :   FlatActivity.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculusvr.vrlib;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

public class FlatActivity extends Activity
{
	@Override
	public void startActivityForResult(Intent intent, int requestCode)
	{
		if( getApplication() instanceof VrApplication )
		{
			VrApplication vrApp = (VrApplication)getApplication();
			vrApp.hostActivity.startFlatActivityForResult( intent, requestCode );
		}
	}
	
	@Override
	public void startActivityForResult(Intent intent, int requestCode,
			Bundle options)
	{
		if( getApplication() instanceof VrApplication )
		{
			VrApplication vrApp = (VrApplication)getApplication();
			vrApp.hostActivity.startFlatActivityForResult( intent, requestCode, options );
		}
	}

}
