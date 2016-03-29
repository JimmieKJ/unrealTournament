// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Tools.CrashReporter.CrashReportProcess
{
	/// <summary>
	/// A class to monitor the crash report repository for new crash reports.
	/// </summary>
	public sealed class ReportWatcher : IDisposable
	{
		/// <summary>A queue of freshly landed crash reports ready to be processed.</summary>
		public List<ReportQueue> ReportQueues = new List<ReportQueue>();

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

			CancelSource.Dispose();
		}

		/// <summary>
		/// A thread to watch for new crash reports landing.
		/// </summary>
		/// <remarks>The NFS storage does not support file system watchers, so this has to be done laboriously.</remarks>
		void Start()
		{
			CrashReporterProcessServicer.WriteEvent("CrashReportProcessor watching directories:");
			var Settings = Properties.Settings.Default;

			if (!string.IsNullOrEmpty(Settings.InternalLandingZone))
			{
				if( System.IO.Directory.Exists( Settings.InternalLandingZone ) )
				{
					ReportQueues.Add(new ReportQueue(Settings.InternalLandingZone));
					CrashReporterProcessServicer.WriteEvent(string.Format("\t{0} (internal, high priority)", Settings.InternalLandingZone));
				}
				else
				{
					CrashReporterProcessServicer.WriteFailure( string.Format( "\t{0} (internal, high priority) is not accessible", Settings.InternalLandingZone ) );
				}
			}

#if !DEBUG
			if (!string.IsNullOrEmpty(Settings.ExternalLandingZone))
			{
				if( System.IO.Directory.Exists( Settings.ExternalLandingZone ) )
				{
					ReportQueues.Add( new ReportQueue( Settings.ExternalLandingZone ) );
					CrashReporterProcessServicer.WriteEvent( string.Format( "\t{0}", Settings.ExternalLandingZone ) );
				}
				else
				{
					CrashReporterProcessServicer.WriteFailure( string.Format( "\t{0} is not accessible", Settings.ExternalLandingZone ) );
				}
			}
#endif //!DEBUG

			var Cancel = CancelSource.Token;
			WatcherTask = Task.Factory.StartNew(async () =>
			{
				while (!Cancel.IsCancellationRequested)
				{
					// Check the landing zones for new reports
					foreach (var Queue in ReportQueues)
					{
						Queue.CheckForNewReports();
					}
					await Task.Delay(60000, Cancel);
				}
			});
		}
	}
}