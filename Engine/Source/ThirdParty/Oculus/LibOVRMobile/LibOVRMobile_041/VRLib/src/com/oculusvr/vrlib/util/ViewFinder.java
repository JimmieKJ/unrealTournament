/************************************************************************************

Filename    :   ViewFinder.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
package com.oculusvr.vrlib.util;

/*
 * Static utility class that allows us to find a list of views by type
 */

import java.util.ArrayList;
import java.util.List;
import java.util.SortedMap;
import java.util.TreeMap;

import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;

final public class ViewFinder
{
	private ViewFinder()
	{
	}

	// Static method for finding a map of views given a particular class type
	public static <T extends View> SortedMap<String, T> find(ViewGroup root, Class<T> type)
	{
		FinderByType<T> finderByType = new FinderByType<T>( type );
		LayoutEnumerator.init( finderByType ).traverse( root );
		return finderByType.getViews();
	}
	
	// Convenience method to aggregate all the views that are pushable into
	// a single ordered list
	public static List<View> findPushableViews( ViewGroup root )
	{
		@SuppressWarnings("rawtypes")
		List<Class> classList = new ArrayList<Class>();
		
		classList.add( ImageButton.class );
		classList.add( Button.class );
		
		SortedMap<String, View> viewResults = find( root, classList );
		
		return new ArrayList<View>( viewResults.values() );
	}

	// Generate a sorted map from a union of the discovered views
	// The resulting sorted map should be properly ordered by
	// their arrangement in the view hierarchy.
	public static SortedMap<String, View> find(ViewGroup root,
			@SuppressWarnings("rawtypes") List<Class> types)
	{
		SortedMap<String, View> result = new TreeMap<String, View>();

		for( @SuppressWarnings("rawtypes") Class aClass : types )
		{
			@SuppressWarnings("unchecked")
			FinderByType<View> finderByType = new FinderByType<View>( aClass );
			LayoutEnumerator.init( finderByType ).traverse( root );
			result.putAll( finderByType.getViews() );
		}
		
		return result;
	}
	
	
	// Recursively build a view index string
	// This will eventually produce something to the effect of
	// 1.001.005.007.001
	// This string will in turn be used as a key in our SortedMap, resulting in an automatic
	// sort of the objects within the view hierarchy regardless of the View type.
	private static void viewIndex( View view, StringBuilder indexString )
	{
		if( view.getParent() instanceof ViewGroup )
		{
			ViewGroup vg = (ViewGroup)view.getParent();
			int index = vg.indexOfChild( view );
			
			indexString.insert( 0, String.format( ".%03d", index ) );
			viewIndex( vg, indexString );
		}
		else
		{
			indexString.insert( 0, "1" );
		}
	}

	// Generic FinderByType that mananges and recursively builds a map of views given
	// a starting root view
	private static class FinderByType<T extends View> implements
			LayoutEnumerator.Processor
	{
		private final Class<T> type;
		private final SortedMap<String, T> views;

		private FinderByType(Class<T> type)
		{
			this.type = type;
			views = new TreeMap<String, T>();
		}

		@Override
		@SuppressWarnings("unchecked")
		public void process(View view)
		{
			if( type.isInstance( view ) ) {
				StringBuilder sb = new StringBuilder();
				viewIndex( view, sb );
				views.put( sb.toString(),  (T) view );
			}
		}

		public SortedMap<String, T> getViews()
		{
			return views;
		}
	}
}