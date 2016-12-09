// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.ServiceProcess;
using Amazon.Runtime;
using Amazon.S3;
using Amazon.SQS;
using Tools.CrashReporter.CrashReportCommon;

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

		public static AmazonClient DataRouterAWS { get; private set; }

		public static AmazonClient OutputAWS { get; private set; }

		public static StatusReporting StatusReporter { get; private set; }

		public static Symbolicator Symbolicator { get; private set; }

		public static ReportIndex ReportIndex { get; private set; }

		/// <summary>Folder in which to store log files</summary>
		private static string LogFolder = null;

		private bool bDisposing = false;
		private bool bDisposed = false;

		/// <summary>Folder in which to store symbolication log files</summary>
		static public string SymbolicatorLogFolder { get; private set; }

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
		static public void WriteException( string Message, Exception Ex )
		{
			if (Ex != null)
			{
				if (Ex is OutOfMemoryException)
				{
					StatusReporter.Alert("OOM", "Out of memory.", 2);
				}
			}

			if (Message != null && Message.Length > 2)
			{
				Log.Print("[EXCEPT] " + Message);
			}
			else
			{
				Log.Print("[EXCEPT] NO MESSAGE");
			}
			StatusReporter.IncrementCount(StatusReportingEventNames.ExceptionEvent);
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
			LogFolder = Path.Combine(StartupPath, "Logs");
			SymbolicatorLogFolder = Path.Combine(StartupPath, "MDDLogs");
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

			Symbolicator = new Symbolicator();

			StatusReporter = new StatusReporting();

			ReportIndex = new ReportIndex
			{
				IsEnabled = !string.IsNullOrWhiteSpace(Config.Default.ProcessedReportsIndexPath),
                Filepath = Config.Default.ProcessedReportsIndexPath,
				Retention = TimeSpan.FromDays(Config.Default.ReportsIndexRetentionDays)
			};

			ReportIndex.ReadFromFile();

			WriteEvent("Initializing AWS");
			string AWSError;
			AWSCredentials AWSCredentialsForDataRouter = new StoredProfileAWSCredentials(Config.Default.AWSProfileInputName, Config.Default.AWSCredentialsFilepath);
			AmazonSQSConfig SqsConfigForDataRouter = new AmazonSQSConfig
			{
				ServiceURL = Config.Default.AWSSQSServiceInputURL
			};
			AmazonS3Config S3ConfigForDataRouter = new AmazonS3Config
			{
				ServiceURL = Config.Default.AWSS3ServiceInputURL
			};
			DataRouterAWS = new AmazonClient(AWSCredentialsForDataRouter, SqsConfigForDataRouter, S3ConfigForDataRouter, out AWSError);
			if (!DataRouterAWS.IsSQSValid || !DataRouterAWS.IsS3Valid)
			{
				WriteFailure("AWS failed to initialize profile for DataRouter access. Error:" + AWSError);
				StatusReporter.Alert("AWSFailInput", "AWS failed to initialize profile for DataRouter access", 0);
			}
			AWSCredentials AWSCredentialsForOutput = new StoredProfileAWSCredentials(Config.Default.AWSProfileOutputName, Config.Default.AWSCredentialsFilepath);
			AmazonS3Config S3ConfigForOutput = new AmazonS3Config
			{
				ServiceURL = Config.Default.AWSS3ServiceOutputURL
			};
			OutputAWS = new AmazonClient(AWSCredentialsForOutput, null, S3ConfigForOutput, out AWSError);
			if (!OutputAWS.IsS3Valid)
			{
				WriteFailure("AWS failed to initialize profile for output S3 bucket access. Error:" + AWSError);
				StatusReporter.Alert("AWSFailOutput", "AWS failed to initialize profile for output S3 bucket access", 0);
			}

			// Add directory watchers
			WriteEvent("Creating ReportWatcher");
			Watcher = new ReportWatcher();

			WriteEvent("Creating ReportProcessors");
			for (int ProcessorIndex = 0; ProcessorIndex < Config.Default.ProcessorThreadCount; ProcessorIndex++)
			{
				var Processor = new ReportProcessor(Watcher, ProcessorIndex);
				Processors.Add(Processor);
			}

			// Init events by enumerating event names
			WriteEvent("Initializing Event Counters");
			FieldInfo[] EventNameFields = typeof(StatusReportingEventNames).GetFields(BindingFlags.Static | BindingFlags.Public);
			StatusReporter.InitCounters(EventNameFields.Select(EventNameField => (string)EventNameField.GetValue(null)));

			WriteEvent("Initializing Performance Mean Counters");
			FieldInfo[] MeanNameFields = typeof(StatusReportingPerfMeanNames).GetFields(BindingFlags.Static | BindingFlags.Public);
			StatusReporter.InitMeanCounters(MeanNameFields.Select(MeanNameField => (string)MeanNameField.GetValue(null)));

			WriteEvent("Initializing Folder Monitors");
			Dictionary<string, string> FoldersToMonitor = new Dictionary<string, string>();
			FoldersToMonitor.Add(Config.Default.ProcessedReports, "Processed Reports");
			FoldersToMonitor.Add(Config.Default.ProcessedVideos, "Processed Videos");
			FoldersToMonitor.Add(Config.Default.DepotRoot, "P4 Workspace");
			FoldersToMonitor.Add(Config.Default.InternalLandingZone, "CRR Landing Zone");
			FoldersToMonitor.Add(Config.Default.DataRouterLandingZone, "Data Router Landing Zone");
			FoldersToMonitor.Add(Config.Default.PS4LandingZone, "PS4 Landing Zone");
			FoldersToMonitor.Add(Assembly.GetExecutingAssembly().Location, "CRP Binaries and Logs");
			FoldersToMonitor.Add(Config.Default.MDDPDBCachePath, "MDD PDB Cache");
			StatusReporter.InitFolderMonitors(FoldersToMonitor);

			WriteEvent("Starting StatusReporter");
			StatusReporter.Start();

			// Start the threads now
			Watcher.Start();
			foreach (var Processor in Processors)
			{
				Processor.Start();
			}

			DateTime StartupTime = DateTime.UtcNow;
			WriteEvent("Successfully started at " + StartupTime);
		}

		/// <summary>
		/// Stop the service.
		/// </summary>
		protected override void OnStop()
		{
			StatusReporter.OnPreStopping();

			OnDispose();
		}

		private void OnDispose()
		{
			bDisposing = true;

			// Clean up the directory watcher and crash processor threads
			foreach (var Processor in Processors)
			{
				Processor.RequestStop();
			}
			foreach (var Processor in Processors)
			{
				Processor.Dispose();
			}
			Processors.Clear();

			Watcher.Dispose();
			Watcher = null;

			ReportIndex.WriteToFile();
			ReportIndex = null;

			OutputAWS.Dispose();
			OutputAWS = null;

			DataRouterAWS.Dispose();
			DataRouterAWS = null;

			StatusReporter.Dispose();
			StatusReporter = null;

			Slack.Dispose();
			Slack = null;

			// Flush the log to disk
			Log.Dispose();
			Log = null;

			bDisposed = true;
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