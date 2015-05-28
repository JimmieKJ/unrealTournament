// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Tools.CrashReporter.CrashReportCommon;
using System.IO;
using System.Runtime.CompilerServices;
using System.Diagnostics;
using System.Threading; 

/// <summary> Helper class used to log various things into the log file. </summary>
public class FLogger
{
	/// <summary>
	/// Global logger instance.
	/// </summary>
	static public FLogger Global = new FLogger( true );

	LogWriter Log = null;

	/// <summary>
	/// 
	/// </summary>
	/// <param name="bUseGlobal"></param>
	public FLogger( bool bUseGlobal = false )
	{
		Log = CreateSafeLogWriter(bUseGlobal);
	}

	/// <summary>
	/// 
	/// </summary>
	/// <returns></returns>
	static LogWriter CreateSafeLogWriter( bool bUseGlobal = false )
	{
		bool bHasDDrive = Directory.Exists( "D:" );
		bool bHasFDrive = Directory.Exists( "F:" );
		
		string LogsPath = bHasFDrive ? "F:/" : (bHasDDrive ? "D:/" : "C:/");
		LogsPath += "CrashReportWebsiteLogs";

		string LogFileName = bUseGlobal ? "CrashReportWebSite-Global-" + Guid.NewGuid().ToString( "N" ) : string.Format( "CrashReportWebSite-{1}-[{0}]", System.Threading.Thread.CurrentThread.ManagedThreadId, GetPathName() );

		LogWriter Log = new LogWriter( LogFileName,LogsPath );
		return Log;
	}

	/// <summary>
	/// Flushes and closes the log file
	/// </summary>
	public void Dispose()
	{
		Log.Dispose();
	}

	/// <summary> Returns the path name valid as filename. </summary>
	static public string GetPathName()
	{
		var Items = System.Web.HttpContext.Current.Items;
		string UserName = System.Web.HttpContext.Current.Request.LogonUserIdentity.Name;
		string Path = System.Web.HttpContext.Current.Request.Path;
		string FullName = UserName + Path + "~" + Guid.NewGuid().ToString( "N" );
		FullName = FullName.Replace( '\\', '.' ).Replace( '/', '_' );
		return FullName;
	}

	/// <summary>
	/// Writes a status update to the log file.
	/// </summary>
	public void WriteEvent( string Message )
	{
		if( Message != null && Message.Length > 2 )
		{
			Log.Print( "[EVENT ] " + Message );
		}
	}

	/// <summary>
	/// Writes an exception status to the log file.
	/// </summary>
	public void WriteException( string Message )
	{
		if( Message != null && Message.Length > 2 )
		{
			Log.Print( "[EXCEPT] " + Message );
		}
	}

	/// <summary>
	/// Writes a performance info to the log file.
	/// </summary>
	public void WritePerf( string Message )
	{
		if( Message != null && Message.Length > 2 )
		{
			Log.Print( "[C.PERF] " + Message );
		}
	}
}

/// <summary> Helper class for logging time spent in the specified scope
///	
///	Example of usage
/// 
/// using( FScopedLogTimer ParseTiming = new FScopedLogTimer( "Description" ) )
/// {
///		code...
/// }
///
/// </summary>
public class FScopedLogTimer : IDisposable
{
	/// <summary> Constructor. </summary>
	/// <param name="InDescription"> Global tag to use for logging.</param>
	/// <param name="bCreateNewLog">Whether to create a new log file</param>
	public FScopedLogTimer( string InDescription, bool bCreateNewLog = false )
	{
		if( bCreateNewLog )
		{
			Log = new FLogger();
		}

		if( Scope.Value == 0 )
		{
			//Callstack = new List<string>();
			Callstack = new ThreadLocal<List<string>>( () => { return new List<string>(); } );
			Callstack.Value.Add( "".PadLeft( 16, '-' ) );
		}
		Description = InDescription;
		Scope.Value++;
	}

	/// <summary> Destructor, logs delta time from constructor. </summary>
	public void Dispose()
	{
		Scope.Value--;
		// Don't log spam.
		if( Timer.Elapsed.TotalMilliseconds > 4.0 )
		{
			Callstack.Value.Add( Description.PadLeft( Description.Length + Scope.Value * 2 ) + ":" + (Int64)Timer.Elapsed.TotalMilliseconds );
		}

		if( Scope.Value == 0 )
		{
			// Dump the callstack to the log.
			Callstack.Value.Reverse();
			foreach( var Line in Callstack.Value )
			{
				if( Log != null )
				{
					Log.WritePerf( Line );
				}
				else
				{
					FLogger.Global.WriteEvent( Line );
				}
			}

			if( Log != null )
			{
				Log.Dispose();
			}
		}
	}

	/// <summary> Returns the elapsed time as total number of seconds. </summary>
	public double GetElapsedSeconds()
	{
		return Timer.Elapsed.TotalSeconds;
	}

	/// <summary> Global tag to use for logging. </summary>
	string Description;

	/// <summary> Timer used to measure the timing. </summary>
	Stopwatch Timer = Stopwatch.StartNew();

	/// <summary> Scope for the current call. </summary>
	static ThreadLocal<int> Scope = new ThreadLocal<int>( () => { return 0; } );
	static ThreadLocal<List<string>> Callstack = null;

	FLogger Log = null;
}

/// <summary> Helper class for logging time spent in the specified scope </summary>
public class FAutoScopedLogTimer : FScopedLogTimer
{
	/// <summary>
	/// Constructor.
	/// </summary>
	/// <param name="ClassName">Name of the class</param>
	/// <param name="MemberName">Name of class member</param>
	/// <param name="SourceFilePath">Name of the source filepath</param>
	/// <param name="bCreateNewLog">If true, creates a new log files, use only for the root calls like Index, Show etc.</param>
	/// <returns></returns>
	public FAutoScopedLogTimer( string ClassName, [CallerMemberName] string MemberName = "", [CallerFilePath] string SourceFilePath = "", bool bCreateNewLog = false )
		: base( ClassName + "." + MemberName, bCreateNewLog )
	{
	}
}