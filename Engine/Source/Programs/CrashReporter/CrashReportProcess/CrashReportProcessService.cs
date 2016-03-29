// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportProcess
{
	/// <summary>
	/// A class to handle processing received crash reports for displaying on the website.
	/// </summary>
	public partial class CrashReporterProcessServicer : ServiceBase
	{
		/// <summary>A class the handle detection of new reports.</summary>
		public ReportWatcher Watcher = null;

		/// <summary>A class to lazily process reports as they are detected.</summary>
		public ReportProcessor Processor = null;

		/// <summary>Current log file to write debug progress info to</summary>
		static public CrashReportCommon.LogWriter Log = null;

		/// <summary>Folder in which to store log files</summary>
		static private string LogFolder = null;

		/// <summary>
		/// Write a status update to the event log.
		/// </summary>
		static public void WriteEvent( string Message )
		{
			if( Message != null && Message.Length > 2 )
			{
				Log.Print( "[STATUS] " + Message );
			}
		}

		/// <summary>
		/// Write a status update to the event log, from the MinidumpDiagnostics.
		/// </summary>
		static public void WriteMDD( string Message )
		{
			if( Message != null && Message.Length > 2 )
			{
				Log.Print( "[MDD   ] " + Message );
			}
		}

		/// <summary>
		/// Write a status update to the event log, from the P4.
		/// </summary>
		static public void WriteP4( string Message )
		{
			if( Message != null && Message.Length > 2 )
			{
				Log.Print( "[P4    ] " + Message );
			}
		}

		/// <summary>
		/// Write a failure to the event log.
		/// </summary>
		static public void WriteFailure( string Message )
		{
			if( Message != null && Message.Length > 2 )
			{
				Log.Print( "[FAILED] " + Message );
			}
		}

		/// <summary>
		/// Write an exception message to the event log.
		/// </summary>
		static public void WriteException( string Message )
		{
			if( Message != null && Message.Length > 2 )
			{
				Log.Print( "[EXCEPT] " + Message );
			}
		}

		/// <summary>
		/// Initialise all the components, and create an event log.
		/// </summary>
		public CrashReporterProcessServicer()
		{
			InitializeComponent();

			var StartupPath = Path.GetDirectoryName( Assembly.GetExecutingAssembly().Location );
			LogFolder = Path.Combine( StartupPath, "Logs" );
		}

		/// <summary>
		/// Start the service, and stop the service if there were any errors found.
		/// </summary>
		/// <param name="Arguments">Command line arguments (unused).</param>
		protected override void OnStart( string[] Arguments )
		{
			// Create a log file for any start-up messages
			Log = new CrashReportCommon.LogWriter("CrashReportProcess", LogFolder);

			// Add directory watchers
			Watcher = new ReportWatcher();

			Processor = new ReportProcessor( Watcher );

			WriteEvent( "Successfully started at " + DateTime.Now.ToString() );
		}

		/// <summary>
		/// Stop the service.
		/// </summary>
		protected override void OnStop()
		{
			// Clean up the directory watcher and crash processor threads
			Processor.Dispose();
			Processor = null;

			Watcher.Dispose();
			Watcher = null;

			// Flush the log to disk
			Log.Dispose();
			Log = null;
		}

		/// <summary>
		/// Run the service in debug mode. This spews all logging to the console rather than suppressing it.
		/// </summary>
		public void DebugRun()
		{
			OnStart( null );
			Console.WriteLine( "Press enter to exit" );
			Console.Read();
			OnStop();
		}
	}
}