/************************************************************************************

Filename    :   VrApplication.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculusvr.vrlib;

import android.app.Application;

public class VrApplication extends Application
{
	protected VrActivity hostActivity;
	
	public VrApplication()
	{
	}

	public void setHostActivity( VrActivity anActivity )
	{
		hostActivity = anActivity;
	}
	
	public VrActivity getHostActivity()
	{
		return hostActivity;
	}
	
}
