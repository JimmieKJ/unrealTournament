// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using System.Text;

namespace Tools.CrashReporter.CrashReportProcess
{
	/// <summary>
	/// A class to monitor the crash report repository for new crash reports.
	/// </summary>
	sealed class ReportWatcher : IDisposable
	{
		/// <summary>A queue of freshly landed crash reports ready to be processed.</summary>
		public List<IReportQueue> ReportQueues = new List<IReportQueue>();

		/// <summary>Task to periodically check for the arrival of new report folders.</summary>
		Task WatcherTask;

		/// <summary>Object providing cancellation of the watcher task.</summary>
		CancellationTokenSource CancelSource;

		/// <summary>
		/// Create a directory watcher to monitor the crash report landing zone.
		/// </summary>
		public ReportWatcher()
		{
			CancelSource = new CancellationTokenSource();
			Start();
		}

		/// <summary>
		/// Shutdown: stop the thread
		/// </summary>
		public void Dispose()
		{
			// Cancel the task and wait for it to quit
			CancelSource.Cancel();
			WatcherTask.Wait();

			foreach (var Queue in ReportQueues)
			{
				Queue.Dispose();
			}
			ReportIndex.WriteToFile();

			CancelSource.Dispose();
		}

		/// <summary>
		/// A thread to watch for new crash reports landing.
		/// </summary>
		/// <remarks>The NFS storage does not support file system watchers, so this has to be done laboriously.</remarks>
		void Start()
		{
			CrashReporterProcessServicer.WriteEvent("CrashReportProcessor watching directories:");
			var Settings = Config.Default;

			if (!string.IsNullOrEmpty(Settings.DataRouterLandingZone))
			{
				if (System.IO.Directory.Exists(Settings.DataRouterLandingZone))
				{
					ReportQueues.Add(new DataRouterReportQueue("DataRouter Crashes", Settings.DataRouterLandingZone));
					CrashReporterProcessServicer.WriteEvent(string.Format("\t{0} (all crashes from data router)", Settings.DataRouterLandingZone));
				}
				else
				{
					CrashReporterProcessServicer.WriteFailure(string.Format("\t{0} (all crashes from data router) is not accessible", Settings.DataRouterLandingZone));
				}
			}

			if (!string.IsNullOrEmpty(Settings.InternalLandingZone))
			{
				if( System.IO.Directory.Exists( Settings.InternalLandingZone ) )
				{
					ReportQueues.Add(new ReceiverReportQueue("Epic Crashes", Settings.InternalLandingZone));
					CrashReporterProcessServicer.WriteEvent(string.Format("\t{0} (internal, high priority (legacy))", Settings.InternalLandingZone));
				}
				else
				{
					CrashReporterProcessServicer.WriteFailure(string.Format("\t{0} (internal, high priority (legacy)) is not accessible", Settings.InternalLandingZone));
				}
			}

#if !DEBUG
			if (!string.IsNullOrEmpty(Settings.ExternalLandingZone))
			{
				if( System.IO.Directory.Exists( Settings.ExternalLandingZone ) )
				{
					ReportQueues.Add(new ReceiverReportQueue("External Crashes", Settings.ExternalLandingZone));
					CrashReporterProcessServicer.WriteEvent( string.Format( "\t{0} (legacy)", Settings.ExternalLandingZone ) );
				}
				else
				{
					CrashReporterProcessServicer.WriteFailure( string.Format( "\t{0} (legacy) is not accessible", Settings.ExternalLandingZone ) );
				}
			}
#endif //!DEBUG

			// Init queue entries in StatusReporter
			foreach (var Queue in ReportQueues)
			{
				CrashReporterProcessServicer.StatusReporter.InitQueue(Queue.QueueId, Queue.LandingZonePath);
			}

			ReportIndex.Filepath = Config.Default.ProcessedReportsIndexPath;
			ReportIndex.Retention = TimeSpan.FromDays(Config.Default.ReportsIndexRetentionDays);
			ReportIndex.ReadFromFile();

			var Cancel = CancelSource.Token;
			WatcherTask = Task.Factory.StartNew(async () =>
			{
				DateTime LastQueueSizeReport = DateTime.MinValue;
				while (!Cancel.IsCancellationRequested)
				{
					// Check the landing zones for new reports
					DateTime StartTime = DateTime.Now;

					foreach (var Queue in ReportQueues)
					{
						int QueueSize = Queue.CheckForNewReports();
						CrashReporterProcessServicer.StatusReporter.SetQueueSize(Queue.QueueId, QueueSize);
					}

					TimeSpan TimeTaken = DateTime.Now - StartTime;
					CrashReporterProcessServicer.WriteEvent(string.Format("Checking Landing Zones took {0:F2} seconds", TimeTaken.TotalSeconds));
					await Task.Delay(30000, Cancel);
				}
			});
		}
	}
}