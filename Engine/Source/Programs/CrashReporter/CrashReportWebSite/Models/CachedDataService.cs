// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Web.Caching;

namespace Tools.CrashReporter.CrashReportWebSite.Models
{
	/// <summary>
	/// A class to handle caching of database results on the web server.
	/// </summary>
	public class CachedDataService : IDisposable
	{
		private CrashRepository Crashes;
		private BuggRepository Buggs;

		private Cache CacheInstance;

		private const string CacheKeyPrefix = "_CachedDataService_";
		private const string CallstackKeyPrefix = "_CallStack_";
		private const string FunctionCallKeyPrefix = "_FunctionCallStack_";
		private const string UserNameKeyPrefix = "_UserName_";

		/// <summary>
		/// Link the Http context cache to a crash repository.
		/// </summary>
		/// <param name="InCache">The current Http context cache.</param>
		/// <param name="InCrashRepository">The repository to associate the cache with.</param>
		public CachedDataService( Cache InCache, CrashRepository InCrashRepository )
		{
			CacheInstance = InCache;
			Crashes = InCrashRepository;
		}

		/// <summary>
		/// Link the Http context cache to a Bugg repository.
		/// </summary>
		/// <param name="InCache">The current Http context cache.</param>
		/// <param name="InBuggRepository">The repository to associate the cache with.</param>
		public CachedDataService( Cache InCache, BuggRepository InBuggRepository )
		{
			CacheInstance = InCache;
			Buggs = InBuggRepository;
		}

		/// <summary>
		/// Implementing Dispose.
		/// </summary>
		public void Dispose()
		{
			Dispose( true );
			GC.SuppressFinalize( this );
		}

		/// <summary>
		/// Disposes the resources.
		/// </summary>
		/// <param name="Disposing">true if the Dispose call is from user code, and not system code.</param>
		protected virtual void Dispose( bool Disposing )
		{
			Crashes.Dispose();
			Buggs.Dispose();
		}

		/// <summary>
		/// Retrieve a cached (and pre-parsed) callstack container from the cache, or parse the raw callstack, and add to the cache.
		/// </summary>
		/// <param name="CurrentCrash">The crash to retrieve the parsed callstack for.</param>
		/// <returns>A parsed callstack.</returns>
		public CallStackContainer GetCallStackFast( Crash CurrentCrash )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(CrashId=" + CurrentCrash.Id + ")" ) )
			{
				string Key = CacheKeyPrefix + CallstackKeyPrefix + CurrentCrash.Id;
				CallStackContainer CallStack = (CallStackContainer)CacheInstance[Key];
				if( CallStack == null )
				{
					CallStack = new CallStackContainer( CurrentCrash );
					CallStack.bDisplayFunctionNames = true;
					CacheInstance.Insert( Key, CallStack );
				}

				return CallStack;
			}
		}
	}
}