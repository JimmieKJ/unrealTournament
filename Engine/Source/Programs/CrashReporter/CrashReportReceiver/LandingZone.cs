// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using System.Threading;

namespace Tools.CrashReporter.CrashReportReceiver
{
	using ReportsDict = ConcurrentDictionary<string, DateTime>;

	/// <summary>
	/// Class to deal with the 'landing zone' folder, where crashes are uploaded
	/// </summary>
	public class LandingZoneMonitor
	{
		#region Public methods
		/// <summary>
		/// Constructor: set up a landing zone directory
		/// </summary>
		/// <param name="DirectoryToUse">Directory to store the WER sub-directories</param>
		/// <param name="Log">Optional file to write messages to</param>
		public LandingZoneMonitor(string DirectoryToUse, CrashReportCommon.LogWriter Log)
		{
			Directory = DirectoryToUse;
			LogFile = Log;

			DirectoryInfo DirInfo = new DirectoryInfo(Directory);
			if (DirInfo.Exists)
			{
				ReadMemorizedReceivedReports();
				MemorizeReceivedReports();
			}
			else
			{
				DirInfo.Create();
			}

			LogFile.Print("Landing zone: " + DirectoryToUse);
		}

		/// <summary>
		/// Move a successfully uploaded WER into the landing zone folder
		/// </summary>
		/// <param name="SourceDirInfo">Temporary directory WER was uploaded to</param>
		/// <param name="ReportId">Unique ID of the report, used as the folder name in the landing zone</param>
		public void ReceiveReport(DirectoryInfo SourceDirInfo, string ReportId)
		{
			string ReportPath = Path.Combine(Directory, ReportId);

			// Recreate the landing zone directory, just in case.
			System.IO.Directory.CreateDirectory( Directory );

			DirectoryInfo DirInfo = new DirectoryInfo(ReportPath);
			if (DirInfo.Exists)
			{
				DirInfo.Delete(true);
				DirInfo.Refresh();
			}

			// Retry the move in case the OS keeps some locks on the uploaded files longer than expected
			for (int Retry = 0; Retry != MoveUploadedReportRetryAttempts; ++Retry)
			{
				try
				{
					SourceDirInfo.MoveTo(DirInfo.FullName);
					break;
				}
				catch (Exception)
				{
				}
				System.Threading.Thread.Sleep(100);
			}

			CrashReporterReceiverServicer.WriteEvent( "A new report has been received into " + DirInfo.FullName );

			ReceivedReports[ReportId] = DateTime.Now;

			// Ensure the receive reports memo file doesn't grow indefinitely
			ForgetOldReports();

			// Update file on disk, in case service is restarted
			MemorizeReceivedReports();
		}

		/// <summary>
		/// Check whether a report of a given ID has already been through the landing zone
		/// </summary>
		/// <param name="ReportId">Report ID to check for</param>
		/// <returns>Whether the report of the given ID has been successfully uploaded before</returns>
		public bool HasReportAlreadyBeenReceived(string ReportId)
		{
			return ReceivedReports.ContainsKey(ReportId);
		}

		#endregion Public methods

		#region Private methods
		/// <summary>
		/// Save out the current list of received reports to disk
		/// </summary>
		void MemorizeReceivedReports()
		{
			var MemoPath = Path.Combine(Directory, MemoFilename);
			// Recreate the landing zone directory, just in case.
			System.IO.Directory.CreateDirectory( Directory );
			try
			{
				if (Interlocked.Exchange(ref MemoBeingWritten, 1) == 1)
				{
					return;
				}


				new XElement
				( 
					"Root",
					from Item in ReceivedReports.OrderBy( x => x.Value )	
					select new XElement( "CrashReport", Item.Key, new XAttribute( "time", Item.Value.ToString() ) )
				)
				.Save( MemoPath );
				CrashReporterReceiverServicer.WriteEvent( "Saved to " + MemoPath );
				Interlocked.Exchange(ref MemoBeingWritten, 0);
			}
			catch (Exception Ex)
			{
				LogFile.Print("Failed to memorize received reports: " + Ex.Message);
				Interlocked.Exchange( ref MemoBeingWritten, 0 );
			}
		}

		/// <summary>
		/// Load a list of received reports from disk
		/// </summary>
		void ReadMemorizedReceivedReports()
		{
			var MemoPath = Path.Combine(Directory, MemoFilename);
			try
			{
				ReceivedReports = new ReportsDict
				(
					from element in XElement.Load(MemoPath).Descendants()
					select new KeyValuePair<string, DateTime>(element.Value.ToString(),	DateTime.Parse(element.Attribute("time").Value))
				);
			}
			catch (Exception)
			{
				// Not an error: the file isn't there the first time
			}
		}

		/// <summary>
		/// Ensure the receive reports memo file doesn't grow indefinitely
		/// High thread contention?
		/// </summary>
		void ForgetOldReports()
		{
			// Recreate the dictionary with only recent reports
			ReceivedReports = new ReportsDict
			(
				from Item in ReceivedReports
				where Item.Value.AddDays(DaysToRememberReportsFor) > DateTime.Now
				select Item
			);
		}
		#endregion Private methods

		#region Private members
		/// <summary>
		/// Filename to use to persist received reports dictionary
		/// </summary>
		const string MemoFilename = "ReceivedReports.xml";

		/// <summary>
		/// Atomic flag to indicate memo file is being written to
		/// </summary>
		int MemoBeingWritten = 0;

		/// <summary>
		/// Landing zone root directory
		/// </summary>
		string Directory;

		/// <summary>
		/// Log file to write to
		/// </summary>
		CrashReportCommon.LogWriter LogFile;

		/// <summary>
		/// Dictionary of reports that have been seen before mapping to the time they were received
		/// </summary>
		ReportsDict ReceivedReports = new ReportsDict();

		/// <summary>
		/// Number of days to remember reports, to prevent the multiple uploads of the same report
		/// </summary>
		const int DaysToRememberReportsFor = 7;

		/// <summary>
		/// Number of times to retry moving the report folder to the landing zone once uploaded
		/// </summary>
		const int MoveUploadedReportRetryAttempts = 5;

		#endregion Private members
	}
}
