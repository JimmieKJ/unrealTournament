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
		private CrashRepository LocalCrashRepository;
		private BuggRepository BuggRepositoryInstance;

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
			LocalCrashRepository = InCrashRepository;
		}

		/// <summary>
		/// Link the Http context cache to a Bugg repository.
		/// </summary>
		/// <param name="InCache">The current Http context cache.</param>
		/// <param name="InBuggRepository">The repository to associate the cache with.</param>
		public CachedDataService( Cache InCache, BuggRepository InBuggRepository )
		{
			CacheInstance = InCache;
			BuggRepositoryInstance = InBuggRepository;
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
			LocalCrashRepository.Dispose();
			BuggRepositoryInstance.Dispose();
		}

		/// <summary>
		/// Retrieve a cached (and pre-parsed) callstack container from the cache, or parse the raw callstack, and add to the cache.
		/// </summary>
		/// <param name="CurrentCrash">The crash to retrieve the parsed callstack for.</param>
		/// <returns>A parsed callstack.</returns>
		public CallStackContainer GetCallStack( Crash CurrentCrash )
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

		/// <summary>
		/// Retrieve a list of crashes from the cache, or retrieve a list from the database, and add to the cache.
		/// </summary>
		/// <param name="DateFrom">The date of the earliest crash to consider.</param>
		/// <param name="DateTo">The date of the most recent crash to consider.</param>
		/// <returns>A list of crashes between the start and end dates.</returns>
		public List<Crash> GetCrashes( DateTime DateFrom, DateTime DateTo )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() ) )
			{
				string Key = CacheKeyPrefix + DateFrom + DateTo;
				List<Crash> Data = (List<Crash>)CacheInstance[Key];
				if( Data == null )
				{
					IQueryable<Crash> DataQuery = LocalCrashRepository.ListAll();
					DataQuery = LocalCrashRepository.FilterByDate( DataQuery, DateFrom, DateTo );
					Data = DataQuery.ToList();
					CacheInstance.Insert( Key, Data );
				}

				return Data;
			}
		}

		/// <summary>
		/// Retrieve a list of function call names from an encoded pattern. This is either from the cache, or from the database and then added to the cache.
		/// </summary>
		/// <param name="Pattern">A callstack pattern in the form '1+3+34+2'.</param>
		/// <returns>A list of function names.</returns>
		public List<string> GetFunctionCalls( string Pattern )
		{
			using( FAutoScopedLogTimer LogTimer = new FAutoScopedLogTimer( this.GetType().ToString() + "(Count=" + CacheInstance.Count + ")" ) )
			{
				string Key = CacheKeyPrefix + FunctionCallKeyPrefix + Pattern;
				List<string> FunctionCalls = (List<string>)CacheInstance[Key];
				if( FunctionCalls == null )
				{
					string[] Ids = Pattern.Split( "+".ToCharArray() );
					List<int> IdList = new List<int>();
					foreach( string id in Ids )
					{
						int i;
						if( int.TryParse( id, out i ) )
						{
							IdList.Add( i );
						}
					}

					FunctionCalls = BuggRepositoryInstance.GetFunctionCalls( IdList );
					CacheInstance.Insert( Key, FunctionCalls );
				}

				return FunctionCalls;
			}
		}
	}
}