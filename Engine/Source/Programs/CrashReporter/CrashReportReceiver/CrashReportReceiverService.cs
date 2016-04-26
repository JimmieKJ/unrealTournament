// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportReceiver
{
	/// <summary>
	/// A class to handle receiving of files from clients, both internally and externally.
	/// </summary>
	public partial class CrashReporterReceiverServicer : ServiceBase, IDisposable
	{
		/// <summary>An instance of the web handler class that listens for incoming crash reports.</summary>
		public WebHandler Web = null;

		/// <summary>Current log file to write debug progress info to</summary>
		static public CrashReportCommon.LogWriter Log = null;

		/// <summary>
		/// Write a status update to the event log.
		/// </summary>
		/// <param name="Message">A string containing the status update.</param>
		static public void WriteEvent( string Message )
		{
			// Status messages go to the log file and/or console
			Log.Print( "[STATUS] " + Message );
		}

		/// <summary>
		/// Write a performance update to the event log.
		/// </summary>
		static public void WritePerf( string Message )
		{
			Log.Print( "[PERF  ] " + Message );
		}

		/// <summary>
		/// Initialise all the components.
		/// </summary>
		public CrashReporterReceiverServicer()
		{
			InitializeComponent();

			var StartupPath = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
			Log = new CrashReportCommon.LogWriter( "CrashReportReceiver", Path.Combine( StartupPath, "Logs" ) );
		}

		/// <summary>
		/// Start the service, and stop the service if there were any errors found.
		/// </summary>
		/// <param name="Arguments">An array of arguments passed from the command line (unused).</param>
		protected override void OnStart( string[] Arguments )
		{
			Web = new WebHandler();
			
			if( !Web.bStartedSuccessfully )
			{
				OnStop();
			}
		}

		/// <summary>
		/// Stop the service.
		/// </summary>
		protected override void OnStop()
		{
			Web.Release();
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
