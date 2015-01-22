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
	/// <summary>Current log file to write debug progress info to</summary>
	static public LogWriter Log = new LogWriter( "CrashReportWebSite-" + GetCleanUserName(), "F:/CrashReportWebsiteLogs" );

	/// <summary> Returns the user name valid as filename. </summary>
	static public string GetCleanUserName()
	{
		string UserName = System.Web.HttpContext.Current.Request.LogonUserIdentity.Name;
		UserName = UserName.Replace( "\\", "." );
		return UserName;
	}

	/// <summary>
	/// Writes a status update to the log file.
	/// </summary>
	static public void WriteEvent( string Message )
	{
		if( Message != null && Message.Length > 2 )
		{
			Log.Print( "[EVENT ] " + Message );
		}
	}

	/// <summary>
	/// Writes a performance info to the log file.
	/// </summary>
	static public void WritePerf( string Message )
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
	public FScopedLogTimer( string InDescription )
	{
		if( Scope.Value == 0 )
		{
			FLogger.WriteEvent( "".PadLeft( 16, '-' ) );
		}
		Description = InDescription;
		Scope.Value++;
	}

	/// <summary> Destructor, logs delta time from constructor. </summary>
	public void Dispose()
	{
		Scope.Value--;
		// Don't log spam.
		if( Timer.Elapsed.TotalMilliseconds > 1.0 )
		{
			FLogger.WritePerf( Description.PadLeft( Description.Length + Scope.Value*2 ) + ":" + (Int64)Timer.Elapsed.TotalMilliseconds );
		}		
	}

	/// <summary> Global tag to use for logging. </summary>
	protected string Description;

	/// <summary> Timer used to measure the timing. </summary>
	protected Stopwatch Timer = Stopwatch.StartNew();

	/// <summary> Scope for the current call. </summary>
	protected static ThreadLocal<int> Scope = new ThreadLocal<int>( () => { return 0; } );
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
	/// <returns></returns>
	public FAutoScopedLogTimer( string ClassName, [CallerMemberName] string MemberName = "", [CallerFilePath] string SourceFilePath = "" )
		: base( ClassName + "." + MemberName )
	{
	}
}