// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

package com.epicgames.ue4;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.view.Window;
import android.view.WindowManager;
import android.view.View;
import android.view.MotionEvent;
import android.view.Gravity;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.PopupWindow;

import android.webkit.WebView;
import android.webkit.WebViewClient;

class WebViewControl
{
	public WebViewControl()
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView = new WebView(GameActivity._activity);
				webView.setWebViewClient(new WebViewClient());
				webView.getSettings().setJavaScriptEnabled(true);
				webView.loadUrl("about:blank");
			}
		});
	}

	public void LoadURL(final String url)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				webView.loadUrl(url);
			}
		});
	}

	public void Update(final int x, final int y, final int width, final int height)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if (webPopup == null)
				{
					webPopup = new PopupWindow(GameActivity._activity);
					webPopup.setWidth(width);
					webPopup.setHeight(height);
					webPopup.setClippingEnabled(false);
					webPopup.setBackgroundDrawable((Drawable)null); // no border
					webPopup.setFocusable(true); // required for keyboard

					webLayout = new LinearLayout(GameActivity._activity);
					webLayout.setOrientation(LinearLayout.VERTICAL);
					webLayout.setPadding(0, 0, 0, 0);

					MarginLayoutParams params = new MarginLayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
					params.setMargins(0, 0, 0, 0);
					webLayout.addView(webView, params);

					webPopup.setContentView(webLayout);
					webPopup.showAtLocation(GameActivity._activity.activityLayout, Gravity.NO_GRAVITY, x, y);
					webPopup.update();

					// allow touch outside the popup. setOutsideTouchable(false) does nothing when the popup is focusable.
					WindowManager.LayoutParams p = (WindowManager.LayoutParams)webLayout.getLayoutParams();
					p.flags |= WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
					WindowManager windowManager = GameActivity._activity.getWindowManager();
					windowManager.updateViewLayout(webLayout, p);
				}
				else
				{
					webPopup.update(x, y, width, height);
				}
			}
		});
	}

	public void Close()
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if (webPopup != null)
				{
					webPopup.dismiss();
					webPopup = null;
				}
			}
		});
	}
	
	// Web Views
	private PopupWindow webPopup;
	private LinearLayout webLayout;
	private WebView webView;
}
