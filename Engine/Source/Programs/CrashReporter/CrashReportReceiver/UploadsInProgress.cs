// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Tools.CrashReporter.CrashReportCommon;
using System.Threading;
using System.Diagnostics;

namespace Tools.CrashReporter.CrashReportReceiver
{
	/// <summary>Manager to keep track of what reports the server is currently dealing with </summary>
	/// <remarks>
	/// Mainly to cope with multiple clients fighting to upload the same report. This shouldn't happen, but the service
	/// should be resilient against it even so.
	/// </remarks>
	public class FUploadsInProgress
	{
		/// <summary> Background thread used to monitor CPU/network usage. </summary>
		private Thread PerformancePooler;

		/// <summary> Performance counter used to continuously monitor CPU usage. </summary>
		private PerformanceCounter PerfCounterProcessorTime = new PerformanceCounter( "Processor", "% Processor Time", "_Total" );

		/// <summary> The total amount of data received via reading from the stream. </summary>
		public Int64 TotalReceivedData = 0;

		/// <summary> The current amount of data received via reading from the stream, for the last second. </summary>
		public Int64 CurrentReceivedData = 0;

		/// <summary> The current CPU usage, from 0 to 100. </summary>
		private float CurrentCPUUsage = 0.0f;

		/// <summary> 
		/// Maximum allowed bandwidth that can be used by the crash receiver service.
		/// If this value is exceeded the service will start to ignore larger files like CrashVideo.avi or large log files.
		/// Currently 10MB/s.
		/// Default 1024MB/s.
		/// @todo Should be set based on the current network connection.
		/// </summary>
		private long MaxAllowedBandwidth = 1024;

		/// <summary> 
		/// Maximum allowed CPU utilization for the machine where the crash receiver service is running.
		/// If this value is exceeded the service will start to ignore larger files like CrashVideo.avi or large log files. 
		/// Currently 75%.
		/// </summary>
		private float MaxAllowedCPUtilizationPct = 75.0f;

		/// <summary> 
		/// Files smaller than this size will not be ignored during the dynamic balancing.
		/// Currently 376KB. Should handle all WER XML files, most of the log and minidump files.
		/// Will be reduced once crash report client will support compressing of the data.
		/// </summary>
		private Int64 MaxForcedFilesize = 3 * 128 * 1024;

		private Int64 NumFilesReceived = 0;
		private Int64 NumFilesSkipped = 0;

		/// <summary> To convert bytes into megabytes. </summary>
		public const double MBInv = 1.0 / 1024.0 / 1024.0;

		/// <summary> Periodically pools the performance data. </summary>
		private static void StaticPoolPerformanceData( object Obj )
		{
			FUploadsInProgress This = (FUploadsInProgress)Obj;
			This.PoolPerformanceData();
		}

		private void PoolPerformanceData()
		{
			while( true )
			{
				Int64 NumTotalFiles = NumFilesReceived + NumFilesSkipped;

				// Get the CPU for the last second.
				CurrentCPUUsage = PerfCounterProcessorTime.NextValue();

				if( CurrentReceivedData > 0 && CurrentTotalUploadCount > 0 )
				{
					CrashReporterReceiverServicer.WritePerf
					(
						string.Format( "CPU Usage: {0:00}%, Received data: {1,5}MB/{2:00.00} of {4:00.00} MB, Uploads:{3,3}, Files:{5,6},{6,6} : {7:00}%",
						CurrentCPUUsage, 
						(Int64)Math.Truncate( MBInv * TotalReceivedData ), 
						MBInv * CurrentReceivedData, 
						UploadCount, 
						MBInv * MaxAllowedBandwidth,
						NumFilesReceived, NumFilesSkipped,
						NumTotalFiles > 0 ? 100.0f * NumFilesSkipped / NumTotalFiles : 0.0f )
					);
				}

				Interlocked.Add( ref TotalReceivedData, CurrentReceivedData );

				// Reset the the performance data.
				Interlocked.Exchange( ref CurrentReceivedData, 0 );
				Interlocked.Exchange( ref CurrentTotalUploadCount, 0 );

				// Sleep.
				System.Threading.Thread.Sleep( 1000 );
			}
		}

		/// <summary>
		/// Constructor: set up throttle from app settings
		/// </summary>
		public FUploadsInProgress()
		{
			var Settings = Properties.Settings.Default;
			MaxAllowedBandwidth = Settings.MaxAllowedBandwidthMB * 1024 * 1024;
			MaxAllowedCPUtilizationPct = Settings.MaxAllowedCPUtilizationPct;			

			PerformancePooler = new Thread( Tools.CrashReporter.CrashReportReceiver.FUploadsInProgress.StaticPoolPerformanceData );
			PerformancePooler.Start( this );
		}

		/// <summary>
		/// Check for parallel attempts to upload the same file or complete a report
		/// </summary>
		/// <param name="ReportId">Directory name being used as report id</param>
		/// <param name="Filename">Leaf name of file to be uploaded</param>
		/// <param name="Filesize">Size of the file</param>
		/// <param name="FailureMessage">Message to log and return in case of failure</param>
		/// <param name="bIsOverloaded">If True, this attempt will temporary overload the connection</param>
		/// <returns>True if this is the first client to attempt to upload this file</returns>
		public bool TryReceiveFile( string ReportId, string Filename, long Filesize, ref string FailureMessage, ref bool bIsOverloaded )
		{
			lock (Mutex)
			{
				var ReportRecord = EnsureRecord(ReportId);
				if (ReportRecord.bReportComplete)
				{
					FailureMessage = string.Format("Attempt to upload file \"{0}\" to completed report \"{1}\"", Filename, ReportId);
					return false;
				}

				string FilenameKey = Filename.ToLower();
				if (ReportRecord.FilesUploaded.Contains(FilenameKey))
				{
					FailureMessage = string.Format("File \"{0}\" has already been uploaded to report \"{1}\"", Filename, ReportId);
					return false;
				}

				if( !CanStartOperation( Filename, Filesize, ref bIsOverloaded ) )
				{
					FailureMessage = string.Format( "Skipping \"{0}\"{1:0.0}MB in report \"{2}\" due to excessive load", Filename, MBInv*Filesize, ReportId );
					return false;
				}

				ReportRecord.FilesUploaded.Add( FilenameKey );
				Interlocked.Increment( ref UploadCount );
				Interlocked.Increment( ref CurrentTotalUploadCount );
				return true;
			}
		}


		private bool CanStartOperation( string Filename, long Filesize, ref bool bIsOverloaded )
		{
			bool bCPUExceeded = CurrentCPUUsage > MaxAllowedCPUtilizationPct;
			bool bBandwidthExceeded = CurrentReceivedData > MaxAllowedBandwidth;

			if( bCPUExceeded || bBandwidthExceeded )
			{
				if( Filesize > MaxForcedFilesize || MaxAllowedBandwidth < 0 )
				{
					return TryWaitForStartOperation();
				}
				else
				{
					// We're temporary overloading the connection, so reduce the max allowed bandwidth.
					Interlocked.Add( ref MaxAllowedBandwidth, -Filesize );
					bIsOverloaded = true;
					return true;
				}
			}

			return true;
		}

		/// <summary> Spin loop for a while, maybe we hit an opportunity to start the upload. </summary>
		private bool TryWaitForStartOperation()
		{
			bool bCPUExceeded = false;
			bool bBandwidthExceeded = false;
			for( int Index = 0; Index < 4; Index++ )
			{
				Thread.Sleep( 1000 );
				bCPUExceeded = CurrentCPUUsage > MaxAllowedCPUtilizationPct;
				bBandwidthExceeded = CurrentReceivedData > MaxAllowedBandwidth;

				if( !bCPUExceeded && !bBandwidthExceeded )
				{
					// Found, continue.
					CrashReporterReceiverServicer.WritePerf( "TryWaitForStartOperation was successful" );
					return true;
				}
			}

			// Give up.
			Interlocked.Increment( ref NumFilesSkipped );
			return false;
		}

		/// <summary>
		/// Notification that a file upload has finished, regardless of success
		/// <param name="Filesize">Size of the file</param>
		/// <param name="bIsOverloaded">If true, it means that we need to restore the max allowed bandwidth.</param>
		/// 
		/// </summary>
		public void FileUploadAttemptDone( long Filesize, bool bIsOverloaded )
		{
			Interlocked.Increment( ref NumFilesReceived );
			Interlocked.Decrement( ref UploadCount );
			if( bIsOverloaded )
			{
				// Uploading is done, so restore the max allowed bandwidth.
				Interlocked.Add( ref MaxAllowedBandwidth, Filesize );
			}
		}

		/// <summary>
		/// Check for parallel attempts to complete a report
		/// </summary>
		/// <param name="ReportId">Directory name being used as report id</param>
		/// <returns>True if this is the first client to attempt to complete this report</returns>
		public bool TrySetReportComplete(string ReportId)
		{
			lock (Mutex)
			{
				var UploadRecord = EnsureRecord(ReportId);

				if (UploadRecord.bReportComplete)
				{
					return false;
				}

				UploadRecord.bReportComplete = true;
				UploadRecord.FilesUploaded = null;

				return true;
			}
		}

		/// <summary>
		/// Get the last access time of a report if we know about it
		/// </summary>
		/// <param name="ReportId">Report to look for</param>
		/// <param name="LastAccessTime">Time to fill in of last attempted file upload to report, if found</param>
		/// <returns>Whether report was found</returns>
		public bool TryGetLastAccessTime(string ReportId, ref DateTime LastAccessTime)
		{
			lock (Mutex)
			{
				FReportRecord UploadRecord;
				if (UploadRecords.TryGetValue(ReportId.ToLower(), out UploadRecord))
				{
					LastAccessTime = UploadRecord.LastAccessTime;
					return true;
				}	
				return false;
			}
		}

		#region Private
		/// <summary>
		/// Create a record of files that have been uploaded for a report, if there's not one already
		/// </summary>
		/// <param name="ReportId">Directory name being used as report id</param>
		/// <returns>The record object with a set of files that have been uploaded so far</returns>
		FReportRecord EnsureRecord(string ReportId)
		{
			string ReportIdKey = ReportId.ToLower();
			FReportRecord UploadRecord;
			if (!UploadRecords.TryGetValue(ReportIdKey, out UploadRecord))
			{
				UploadRecord = new FReportRecord();
				UploadRecords[ReportIdKey] = UploadRecord;
			}

			UploadRecord.LastAccessTime = DateTime.Now;

			DoHouseKeeping();

			return UploadRecord;
		}

		/// <summary>
		/// Clear out records that have been around for a while (see MinutesToRememberReports below)
		/// </summary>
		void DoHouseKeeping()
		{
			var Now = DateTime.Now;

			if ((Now - OldestAccessTime).Minutes > MinutesToRememberReports)
			{
				OldestAccessTime = Now;

				// This is doing a little bit of work while a lock is held,
				// but only happens at most once a minute
				var OldUploadRecords = UploadRecords;
				UploadRecords = new Dictionary<string, FReportRecord>();
				foreach (var Entry in OldUploadRecords)
				{
					var LastAccessTime = Entry.Value.LastAccessTime;
					if ((Now - LastAccessTime).Minutes > MinutesToRememberReports)
					{
						// Old report: forget it
						continue;
					}
					UploadRecords[Entry.Key] = Entry.Value;
					if (LastAccessTime < OldestAccessTime)
					{
						OldestAccessTime = LastAccessTime;
					}
				}
			}
		}

		/// <summary>
		/// A record of which files have been uploaded for a report and whether the report is complete
		/// </summary>
		class FReportRecord
		{
			/// <summary>Flag to indicate report has been completed</summary>	
			public bool bReportComplete = false;

			/// <summary>Lowercase filenames of files uploaded for this report</summary>	
			public HashSet<string> FilesUploaded = new HashSet<string>();

			/// <summary>Most recent time EnsureRecord was called on this record</summary>
			public DateTime LastAccessTime;
		};

		/// <summary>Dictionary to keep track of existence of files</summary>
		Dictionary<string, FReportRecord> UploadRecords = new Dictionary<string, FReportRecord>();

		/// <summary>Remember reports for a day</summary>
		const int MinutesToRememberReports = 60 * 24;

		/// <summary>Oldest report in UploadRecords, to avoid trawling the whole dictionary on each house-keeping pass</summary>
		DateTime OldestAccessTime = DateTime.Now;

		/// <summary>Number of simultaneous file uploads in progress</summary>
		int UploadCount = 0;

		/// <summary>Total number of file uploads, for the last second.</summary>
		int CurrentTotalUploadCount = 0;

		/// <summary>Mutex to ensure synchronous access to members of this class</summary>
		object Mutex = new object();
		#endregion
	}
}
