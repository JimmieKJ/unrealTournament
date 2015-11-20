//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.ue4;

import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;

public class SplashActivity extends Activity
{
	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		Intent intent = new Intent(this, GameActivity.class);
		intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
		intent.putExtra("UseSplashScreen", "true");
		startActivity(intent);
		finish();
		overridePendingTransition(0, 0);
	}

	@Override
	protected void onPause()
	{
		super.onPause();
		finish();
		overridePendingTransition(0, 0);
	}

}
