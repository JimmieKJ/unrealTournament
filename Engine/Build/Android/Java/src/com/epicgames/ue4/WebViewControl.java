// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
package com.epicgames.ue4;

import android.os.Bundle;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.webkit.WebViewClient;
	
// Simple layout to apply absolute positioning for the WebView
class WebViewPositionLayout extends ViewGroup
{
	public WebViewPositionLayout(Context context, WebViewControl inWebViewControl)
	{
        super(context);
		webViewControl = inWebViewControl;
	}

	@Override
	protected void onLayout(boolean changed, int left, int top, int right, int bottom)
	{
		webViewControl.webView.layout(webViewControl.curX,webViewControl.curY,webViewControl.curX+webViewControl.curW,webViewControl.curY+webViewControl.curH);
	}

	private WebViewControl webViewControl;
}

// Wrapper for the layout and WebView for the C++ to call
class WebViewControl
{
	public WebViewControl()
	{
		final WebViewControl w = this;

		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				// create the WebView
				webView = new WebView(GameActivity._activity);
				webView.setWebViewClient(new WebViewClient());
				webView.getSettings().setJavaScriptEnabled(true);
				webView.getSettings().setLightTouchEnabled(true);
				webView.setFocusableInTouchMode(true);

				// Wrap the webview in a layout that will do absolute positioning for us
				positionLayout = new WebViewPositionLayout(GameActivity._activity, w);
				positionLayout.addView(webView);								

				bShown = false;
				NextURL = null;
				NextContent = null;
				curX = curY = curW = curH = 0;
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
				NextURL = url;
				NextContent = null;
			}
		});
	}

	public void LoadString(final String contents, final String url)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				NextURL = url;
				NextContent = contents;
			}
		});
	}

	public void ExecuteJavascript(final String script)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if(webView != null)
				{
					if(android.os.Build.VERSION.SDK_INT >= 19) /* Kitkat*/
					{
						webView.evaluateJavascript(script, null);
					}
					else
					{
						webView.loadUrl("javascript:" + script);
					}
				}
			}
		});
	}

	// called from C++ paint event
	public void Update(final int x, final int y, final int width, final int height)
	{
		GameActivity._activity.runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				if (!bShown)
				{
					bShown = true;
				
					// add to the activitiy, on top of the SurfaceView
					ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(ViewGroup.LayoutParams.FILL_PARENT, ViewGroup.LayoutParams.FILL_PARENT);			
					GameActivity._activity.addContentView(positionLayout, params);
					webView.requestFocus();
				}
				else
				{
					if(webView != null)
					{
						if(NextContent != null)
						{
							webView.loadDataWithBaseURL(NextURL, NextContent, "text/html", "UTF-8", null);
							NextURL = null;
							NextContent = null;
						}
						else
						if(NextURL != null)
						{
							webView.loadUrl(NextURL);
							NextURL = null;
						}
					}		
				}
				if(x != curX || y != curY || width != curW || height != curH)
				{
					curX = x;
					curY = y;
					curW = width;
					curH = height;
					positionLayout.requestLayout();
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
				if (bShown)
				{
					((ViewGroup)webView.getParent()).removeView(webView);
					bShown = false;
				}
			}
		});
	}
	
	public WebView webView;
	private WebViewPositionLayout positionLayout;
	public int curX, curY, curW, curH;
	private boolean bShown;
	private String NextURL;
	private String NextContent;
}
