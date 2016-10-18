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
using Tools.CrashReporter.CrashReportCommon;
using Tools.CrashReporter.CrashReportProcess.Properties;

namespace Tools.CrashReporter.CrashReportProcess
{
	/// <summary>
	/// A class to handle processing received crash reports for displaying on the website.
	/// </summary>
	partial class CrashReporterProcessServicer : ServiceBase
	{
		/// <summary>A class the handle detection of new reports.</summary>
		public ReportWatcher Watcher = null;

		/// <summary>A class to lazily process reports as they are detected.</summary>
		public readonly List<ReportProcessor> Processors = new List<ReportProcessor>();

		/// <summary>Current log file to write debug progress info to</summary>
		public static LogWriter Log = null;

		private static SlackWriter Slack = null;

		public static StatusReporting StatusReporter = null;

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
				StatusReporter.IncrementCount(StatusReportingEventNames.ExceptionEvent);
			}
		}

		/// <summary>
		/// Write to Slack.
		/// </summary>
		static public void WriteSlack(string Message)
		{
			if (Message != null && Message.Length > 0)
			{
				Slack.Write(Message);
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
			Log = new LogWriter("CrashReportProcess", LogFolder);

			Config.LoadConfig();

			Slack = new SlackWriter
			{
				WebhookUrl = Config.Default.SlackWebhookUrl,
				Channel = Config.Default.SlackChannel,
				Username = Config.Default.SlackUsername,
				IconEmoji = Config.Default.SlackEmoji
			};

			StatusReporter = new StatusReporting();

			// Add directory watchers
			Watcher = new ReportWatcher();

			for (int ProcessorIndex = 0; ProcessorIndex < Config.Default.ProcessorThreadCount; ProcessorIndex++)
			{
				var Processor = new ReportProcessor(Watcher, ProcessorIndex);
				Processors.Add(Processor);
			}

			StatusReporter.Start();
			DateTime StartupTime = DateTime.UtcNow;
			WriteEvent("Successfully started at " + StartupTime);
		}

		/// <summary>
		/// Stop the service.
		/// </summary>
		protected override void OnStop()
		{
			StatusReporter.OnPreStopping();

			// Clean up the directory watcher and crash processor threads
			foreach (var Processor in Processors)
			{
				Processor.Dispose();
			}
			Processors.Clear();

			Watcher.Dispose();
			Watcher = null;

			StatusReporter.Dispose();
			StatusReporter = null;

			Slack.Dispose();
			Slack = null;

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