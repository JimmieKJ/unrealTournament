/************************************************************************************

Filename    :   LayoutEnumerator.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculusvr.vrlib.util;

/*
 * LayoutEnumerator allows us to initialize and recursively process
 * individual views and view groups starting with our base viewgroup 
 */

import android.view.View;
import android.view.ViewGroup;

public class LayoutEnumerator
{
	public interface Processor
	{
		void process(View view);
	}

	private final Processor processor;

	private LayoutEnumerator(Processor processor)
	{
		this.processor = processor;
	}

	public static LayoutEnumerator init(Processor processor)
	{
		return new LayoutEnumerator( processor );
	}

	// traverse allows us to walk the view heirarchy recursively
	public void traverse(ViewGroup viewGroup)
	{
		final int childCount = viewGroup.getChildCount();

		for( int i = 0; i < childCount; ++i ) {
			final View child = viewGroup.getChildAt( i );
			processor.process( child );

			if( child instanceof ViewGroup ) {
				traverse( (ViewGroup) child );
			}
		}
	}
}