// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using Amazon.S3.Model;
using Tools.CrashReporter.CrashReportCommon;
using Tools.DotNETCommon.LaunchProcess;
using Tools.DotNETCommon.SimpleWebRequest;
using Tools.DotNETCommon.XmlHandler;

namespace Tools.CrashReporter.CrashReportProcess
{

	/// <summary>
	/// A class to handle processing of received crash reports.
	/// </summary>
	sealed class ReportProcessor : IDisposable
	{
		/// <summary>Absolute path to Perforce (default installation assumed)</summary>
		const string PerforceExePath = "C:\\Program Files\\Perforce\\p4.exe";

		/// <summary>Give up on Perforce syncs after 2 minutes</summary>
		const int SyncTimeoutSeconds = 2*60;

		/// <summary> Number of all processed reports. </summary>
		public static int ProcessedReportCount = 1;

		/// <summary> Global timer used to measure web added reports per day. </summary>
		public static Stopwatch Timer = Stopwatch.StartNew();

		private static EventCounter WebAddCounter = new EventCounter(TimeSpan.FromMinutes(10), 10);

		private static DateTime LastCleanupTime = DateTime.Now;

		private static DateTime LoggingDate = DateTime.UtcNow.Date;

		private static Object TickStaticLock = new Object();

		private static Object MinidumpDiagnosticsLock = new Object();

		private static Object FailedUploadLock = new Object();
		private static int ConsecutiveFailedUploads = 0;

		/// <summary>The thread that handles detection of new crashes.</summary>
		public ReportWatcher Watcher = null;

		private readonly int ProcessorIndex;

		/// <summary>Task to handle processing.</summary>
		Task ProcessorTask;

		/// <summary>Object to allow the task to be cancelled at shut-down.</summary>
		CancellationTokenSource CancelSource;

		/// <summary> Tasks to handle adding a new record to the database. </summary>
		readonly List<Task> AddReportTasks = new List<Task>();

		/// <summary>
		/// Global initialization of the processor.
		/// </summary>
		/// <param name="InWatcher">The object that watches for new crash reports coming in.</param>
		/// <param name="InProcessorIndex">The index of this processor in the application's thread list</param>
		public ReportProcessor(ReportWatcher InWatcher, int InProcessorIndex)
		{
			// Create the thread to handle processing
			Watcher = InWatcher;
			ProcessorIndex = InProcessorIndex;
			CancelSource = new CancellationTokenSource();
			Init();
		}

		/// <summary>
		/// Shutdown: stop the thread. Will request stop first if RequestStop() hasn't been called. Then blocks until the processor thread exits.
		/// </summary>
		public void Dispose()
		{
			if (!CancelSource.IsCancellationRequested)
			{
				CancelSource.Cancel();
			}
			ProcessorTask.Wait();
			ProcessorTask.Dispose();

			CancelSource.Dispose();
		}

		/// <summary>
		/// Main processing thread.
		/// </summary>
		/// <remarks>All exceptions are caught and written to the event log.</remarks>
		private void Init()
		{
			var Cancel = CancelSource.Token;
			ProcessorTask = new Task(() =>
			                         {
				                         while (!Cancel.IsCancellationRequested)
				                         {
					                         try
					                         {
						                         bool bIdle = true;

						                         foreach (var Queue in Watcher.ReportQueues)
						                         {
							                         FGenericCrashContext NewContext = null;
							                         if (Queue.TryDequeueReport(out NewContext))
							                         {
								                         ProcessReport(NewContext);

								                         // The effect of this break is to prioritize ReportQueues by their order in the list, from highest to lowest
								                         bIdle = false;
								                         break;
							                         }
						                         }

						                         if (bIdle)
						                         {
							                         // Don't use the CPU if we don't need.
							                         Thread.Sleep(1000);
						                         }
					                         }
					                         catch (Exception Ex)
					                         {
						                         CrashReporterProcessServicer.WriteException(string.Format("PROC-{0} ", ProcessorIndex) + "ProcessNewReports: " + Ex, Ex);
					                         }

					                         TickStatic(Watcher);
				                         }
			                         });
		}

		/// <summary>
		/// Starts the processor thread.
		/// </summary>
		public void Start()
		{
			if (ProcessorTask != null)
			{
				ProcessorTask.Start();
			}
		}

		/// <summary>
		/// Requests stopping the processor thread but doesn't block.
		/// </summary>
		public void RequestStop() { CancelSource.Cancel(); }

		/// <summary> 
		/// Finalizes report files. 
		/// Thread-safe.
		/// </summary>
		private void FinalizeReport(bool bAdded, DirectoryInfo DirInfo, FGenericCrashContext NewContext)
		{
			// Only remove if we added the report, otherwise leave all files to further investigation.
			if (bAdded)
			{
				// Remove the report files as we're done with them.
				CleanReport(DirInfo);
			}
			else
			{
				MoveReportToInvalid(NewContext.CrashDirectory, ReportQueueBase.GetSafeFilename(NewContext.GetAsFilename()));
			}
		}

		/// <summary>
		/// Delete a report directory.
		/// Thread-safe.
		/// </summary>
		/// <param name="DirInfo">The directory to delete</param>
		public static void CleanReport(DirectoryInfo DirInfo)
		{
			const int MaxRetries = 3;

			bool bWriteException = true;
			for (int Retry = 0; Retry < MaxRetries; ++Retry)
			{
				try
				{
					if (!DirInfo.Exists)
					{
						break;
					}

					foreach (FileInfo Info in DirInfo.GetFiles())
					{
						Info.IsReadOnly = false;
						Info.Attributes = FileAttributes.Normal;
					}
					DirInfo.Delete(true /* delete contents */);

					// Random failures to delete with no exception seen regularly - retry
					DirInfo.Refresh();
					if (DirInfo.Exists)
					{
						CrashReporterProcessServicer.WriteEvent("CleanReport: Failed to delete folder without an Exception " + DirInfo);
						Thread.Sleep(500);
						continue;
					}

					break;
				}
				catch (Exception Ex)
				{
					if (bWriteException)
					{
						CrashReporterProcessServicer.WriteException("CleanReport: " + Ex, Ex);
						bWriteException = false;
					}

				}
				System.Threading.Thread.Sleep(100);
			}

			DirInfo.Refresh();
			if (DirInfo.Exists)
			{
				CrashReporterProcessServicer.WriteEvent(string.Format("CleanReport: Failed to delete folder {0} after {1} retries", DirInfo, MaxRetries));
			}
		}

		/// <summary>
		/// Delete a report directory.
		/// Thread-safe.
		/// </summary>
		/// <param name="DirPath">The directory to delete</param>
		public static void CleanReport(string DirPath) { CleanReport(new DirectoryInfo(DirPath)); }

		/// <summary> 
		/// Moves specified report to the directory where are stored invalid reports. 
		/// Thread-safe.
		/// </summary>
		public static void MoveReportToInvalid(string ReportName, string ReportNameAsFilename)
		{
			try
			{
				CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.ProcessingFailedEvent);

				DirectoryInfo DirInfo = new DirectoryInfo(ReportName);

				if (Config.Default.InvalidReportsToAWS)
				{
					string LeafReportName = Path.GetFileName(ReportName);
					DateTime S3KeyTime = DateTime.UtcNow;

					// Copy all files from the source directory. We can't use MoveTo due to different disc location.
					int FilesMoved = 0;
					foreach (FileInfo InvalidFile in DirInfo.GetFiles())
					{
						try
						{
							string S3IDPrefix = string.Format("/{0}_{1}_{2}/{3}/{4}/", S3KeyTime.Year, S3KeyTime.Month, S3KeyTime.Day, S3KeyTime.Hour, LeafReportName);
							UploadFileToS3(InvalidFile, Config.Default.AWSS3InvalidKeyPrefix + S3IDPrefix + InvalidFile.Name);
							FilesMoved++;
						}
						catch (Exception Ex)
						{
							CrashReporterProcessServicer.WriteEvent("MoveReportToInvalid: Failed to write report file " + LeafReportName + ": " + InvalidFile.Name);
							CrashReporterProcessServicer.WriteException("MoveReportToInvalid: " + Ex, Ex);
						}
					}

					CrashReporterProcessServicer.WriteEvent(string.Format("MoveReportToInvalid moved {0} file(s) from {1} to S3", FilesMoved, LeafReportName));
				}

				DirInfo.Delete( true );
				UpdateProcessedReports();
			}
			catch( Exception Ex )
			{
				CrashReporterProcessServicer.WriteException("MoveReportToInvalid: " + Ex, Ex);
			}	
		}

		/// <summary> 
		/// Update the number of processed reports.
		/// Thread-safe.
		/// </summary>
		private static void UpdateProcessedReports()
		{
			Interlocked.Increment( ref ProcessedReportCount );
		}

		/// <summary> 
		/// Delete report folders older than a certain age to avoid unbounded growth of the crash repository.
		/// </summary>
		/// <remarks>The folders for the deduplication process older than the property 'DeleteWaitingReportsDays' days old are deleted.</remarks>
		private static void CleanRepository(ReportWatcher InWatcher)
		{
			try
			{
				foreach (var Queue in InWatcher.ReportQueues)
				{
					Queue.CleanLandingZone();
				}

				CrashReporterProcessServicer.Log.CleanOutOldLogs(Config.Default.DeleteWaitingReportsDays);
			}
			catch( Exception Ex )
			{
				CrashReporterProcessServicer.WriteException("CleanRepository: " + Ex, Ex);
			}
		}

		/// <summary>
		/// Create an Xml payload representing a new crash.
		/// </summary>
		/// <param name="DirInfo">The DirectoryInfo of the report folder.</param>
		/// <param name="NewContext">The generic crash context.</param>
		/// <param name="bHasVideoFile">Whether the report contains a video file.</param>
		/// <param name="bHasLog">Whether the report contains a log file.</param>
		/// <param name="bHasMinidump">Whether the report contains a minidump.</param>
		/// <returns>A string of Xml payload representing the newly found crash report.</returns>
		private string CreateCrash( DirectoryInfo DirInfo, FGenericCrashContext NewContext, bool bHasVideoFile, bool bHasLog, bool bHasMinidump )
		{
			string XmlPayload = "";
			try
			{
				FEngineVersion EngineVersion = new FEngineVersion( NewContext.PrimaryCrashProperties.EngineVersion ); 

				// Create a new crash description for uploading
				var NewCrash = new CrashDescription();

				NewCrash.CrashType = NewContext.PrimaryCrashProperties.GetCrashType();
				NewCrash.BranchName = EngineVersion.GetCleanedBranch();
				NewCrash.GameName = NewContext.PrimaryCrashProperties.GameName;
				NewCrash.Platform = NewContext.PrimaryCrashProperties.PlatformFullName;
				NewCrash.EngineMode = NewContext.PrimaryCrashProperties.EngineMode;
				NewCrash.EngineVersion = EngineVersion.VersionNumber;
			    NewCrash.BuildVersion = NewContext.PrimaryCrashProperties.BuildVersion;

				NewCrash.CommandLine = NewContext.PrimaryCrashProperties.CommandLine;
				NewCrash.BaseDir = NewContext.PrimaryCrashProperties.BaseDir;
			    NewCrash.bProcessorFailed = !string.IsNullOrWhiteSpace(NewContext.PrimaryCrashProperties.ProcessorFailedMessage);
				NewCrash.Language = NewContext.PrimaryCrashProperties.AppDefaultLocale;

// 				// Create a locate and get the system language.
// 				int SystemLanguageCode = 0;
// 				int.TryParse( ReportData.SystemLanguage, out SystemLanguageCode );
// 				try
// 				{
// 					if( SystemLanguageCode > 0 )
// 					{
// 						CultureInfo SystemLanguageCI = new CultureInfo( SystemLanguageCode );
// 						NewCrash.SystemLanguage = SystemLanguageCI.Name;
// 					}
// 				}
// 				catch( System.Exception )
// 				{
// 					// Default to en-US
// 					NewCrash.SystemLanguage = "en-US";
// 				}

				NewCrash.MachineGuid = NewContext.PrimaryCrashProperties.MachineId; // Valid for all kind of builds, previously only for UE4 releases.
				NewCrash.UserName = NewContext.PrimaryCrashProperties.UserName; // Only valid for non-UE4 releases.
				NewCrash.EpicAccountId = NewContext.PrimaryCrashProperties.EpicAccountId; // Only valid for UE4 releases.
				NewCrash.CallStack = NewContext.PrimaryCrashProperties.GetCallstack();
				NewCrash.SourceContext = NewContext.PrimaryCrashProperties.GetSourceContext();
				NewCrash.ErrorMessage = NewContext.PrimaryCrashProperties.GetErrorMessage();
				NewCrash.UserDescription = NewContext.PrimaryCrashProperties.GetUserDescription();
				NewCrash.UserActivityHint = NewContext.PrimaryCrashProperties.UserActivityHint;
				NewCrash.CrashGUID = NewContext.PrimaryCrashProperties.CrashGUID;

				// Iterate through all files and find a file with the earliest date.
				DateTime TimeOfCrash = DateTime.UtcNow;
				foreach( var File in DirInfo.GetFiles() )
				{
					if( File.CreationTimeUtc < TimeOfCrash )
					{
						TimeOfCrash = File.CreationTimeUtc;
					}
				}

				//NewCrash.TimeofCrash = NewContext.PrimaryCrashProperties.TimeofCrash;
				NewCrash.TimeofCrash = TimeOfCrash;
				
				NewCrash.bHasMiniDump = bHasMinidump;
				NewCrash.bHasLog = bHasLog;
				NewCrash.bHasVideo = bHasVideoFile;
				NewCrash.BuiltFromCL = (int)EngineVersion.Changelist;
				NewCrash.bAllowToBeContacted = NewContext.PrimaryCrashProperties.bAllowToBeContacted;

				// Ignore any callstack that is shorter than expected, usually the callstack is invalid.
				if( NewCrash.CallStack.Length <= CrashReporterConstants.MinCallstackDepth )
				{
					CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + "! BadStack: BuiltFromCL=" + string.Format( "{0,7}", NewContext.PrimaryCrashProperties.EngineVersion ) + " Path=" + NewContext.CrashDirectory );
					NewContext.PrimaryCrashProperties.ProcessorFailedMessage = string.Format("Callstack was too small. {0} lines (minimum {1})", NewCrash.CallStack.Length, CrashReporterConstants.MinCallstackDepth);
                    NewContext.ToFile();
                }
				
				XmlPayload = XmlHandler.ToXmlString<CrashDescription>( NewCrash );
			}
			catch (Exception Ex)
			{
				CrashReporterProcessServicer.WriteException(string.Format("PROC-{0} ", ProcessorIndex) + "CreateCrash: " + Ex, Ex);
			}

			return XmlPayload;
		}

		/// <summary>
		/// Call the web service function with an Xml payload to add a crash to the database.
		/// </summary>
		/// <param name="Payload">Xml representation of a new crash to upload.</param>
		/// <returns>The database id of the newly added row.</returns>
		private int UploadCrash( string Payload )
		{
			int NewID = -1;

			try
			{
				// Simple suppression by blanking out the URL for local testing
				if( Config.Default.CrashReportWebSite.Length > 0 )
				{

					bool bDebug = false;
					string RequestString;
					if( !bDebug )
					{
						RequestString = "http://" + Config.Default.CrashReportWebSite + ":80/Crashes/AddCrash/-1";
					}
					else
					{ 
						RequestString = "http://localhost:80/Crashes/AddCrash/-1"; 
					}

					string ErrorMessage = string.Empty;

					const int MaxRetries = 1;
					for (int AddCrashTry = 0; AddCrashTry < MaxRetries + 1; ++AddCrashTry)
					{
						string ResponseString = SimpleWebRequest.GetWebServiceResponse(RequestString, Payload, Config.Default.AddCrashRequestTimeoutMillisec);
						if (ResponseString.Length > 0)
						{
							// Convert response into a string
							CrashReporterResult Result = XmlHandler.FromXmlString<CrashReporterResult>(ResponseString);
							if (Result.ID > 0)
							{
								NewID = Result.ID;
								break;
							}
							CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} UploadCrash response timeout (attempt {1} of {2}): {3}", ProcessorIndex, AddCrashTry + 1, MaxRetries + 1, ErrorMessage));
							ErrorMessage = Result.Message;
						}
						Thread.Sleep(Config.Default.AddCrashRetryDelayMillisec);
					}

					if (NewID == -1)
					{
						CrashReporterProcessServicer.WriteFailure(string.Format("PROC-{0} ", ProcessorIndex) + "UploadCrash failed: " + ErrorMessage);
					}
				}
#if DEBUG
				else
				{
					// No website set - simulate successful upload
					var Rnd = new Random();
					NewID = Rnd.Next(1, 99999999);
				}
#endif
			}
			catch( Exception Ex )
			{
				CrashReporterProcessServicer.WriteException(string.Format("PROC-{0} ", ProcessorIndex) + "UploadCrash: " + Ex, Ex);
			}

			return NewID;
		}

		/// <summary>
		/// A function to add a report to the database, and rename the relevant files.
		/// Thread-safe.
		/// </summary>
		/// <param name="DirInfo">The DirectoryInfo of the report folder.</param>
		/// <param name="NewContext">The generic crash context.</param>
		/// <param name="LogFileName">The file name of the log file in the report.</param>
		/// <param name="DumpFileName">The file name of the minidump in the report.</param>
		/// <param name="VideoFileName">The file name of the video file in the report, or null if there isn't one.</param>
		private bool AddReport( DirectoryInfo DirInfo, FGenericCrashContext NewContext, string LogFileName, string DumpFileName, string VideoFileName )
		{
			try
			{
				Stopwatch AddReportTime = Stopwatch.StartNew();

				// Create an XML representation of a crash
				string CrashDetails = CreateCrash( DirInfo, NewContext, VideoFileName != null, LogFileName != null, DumpFileName != null );
				if (CrashDetails == "")
				{
					CrashReporterProcessServicer.WriteFailure(string.Format("PROC-{0} ", ProcessorIndex) + "! CreateCrash no payload: Path=" + NewContext.CrashDirectory);
					return false;
				}

				// Upload the crash to the database, and retrieve the new row id
				int ReportID = UploadCrash(CrashDetails);

				lock (FailedUploadLock)
				{
					if (ReportID <= 0)
					{
						// Upload failed
						ConsecutiveFailedUploads++;
						if (ConsecutiveFailedUploads == Config.Default.ConsecutiveFailedWebAddLimit)
						{
							CrashReporterProcessServicer.StatusReporter.Alert("ReportProcessor.AddReport.NoContact", "Cannot contact Crash Report website.", Config.Default.SlackAlertRepeatMinimumMinutes);
							CrashReporterProcessServicer.WriteFailure("Cannot contact Crash Report website.");
						}
						CrashReporterProcessServicer.WriteFailure(string.Format("PROC-{0} ", ProcessorIndex) + "! NoUpload: Path=" + NewContext.CrashDirectory);
						string PayloadFailedFileName = Path.Combine(DirInfo.FullName, "PayloadFailed.xml");
						File.WriteAllText(PayloadFailedFileName, CrashDetails);
						return false;
					}

					// Upload succeeded
					ConsecutiveFailedUploads = 0;
				}

				bool bToS3 = Config.Default.CrashFilesToAWS && CrashReporterProcessServicer.OutputAWS.IsS3Valid;
				bool bToDisk = Config.Default.CrashFilesToDisk;

				// Use the row id to name and move the files the way the web site requires
				string IDThenUnderscore = string.Format("{0}_", ReportID);
				int ReportIDSegment = (ReportID/10000)*10000;
				string S3IDPrefix = string.Format("/{0}/{1}/{1}_", ReportIDSegment, ReportID);
				string DestinationFolder = Path.Combine(Config.Default.ProcessedReports, IDThenUnderscore);
				Stopwatch WriteToS3Timer = new Stopwatch();
				Stopwatch WriteToDiskTimer = new Stopwatch();

				if (bToDisk)
				{
					CrashReporterProcessServicer.StatusReporter.AlertOnLowDisk(DestinationFolder, Config.Default.DiskSpaceAlertPercent);
				}

				// Save log file
				if (LogFileName != null)
				{
					LogFileName = Path.Combine(DirInfo.FullName, LogFileName);
					FileInfo LogInfo = new FileInfo(LogFileName);
					if (LogInfo.Exists)
					{
						if (bToS3)
						{
							WriteToS3Timer.Start();
							UploadFileToS3(LogInfo, Config.Default.AWSS3OutputKeyPrefix + S3IDPrefix + "Launch.log");
							WriteToS3Timer.Stop();
						}
						if (bToDisk)
						{
							WriteToDiskTimer.Start();
							LogInfo.MoveTo(DestinationFolder + "Launch.log");
							WriteToDiskTimer.Stop();
						}
					}
				}

				// Save crash context file
				string CrashContextRuntimeName = Path.Combine(DirInfo.FullName, FGenericCrashContext.CrashContextRuntimeXMLName);
				FileInfo CrashContextInfo = new FileInfo(CrashContextRuntimeName);
				if (CrashContextInfo.Exists)
				{
					if (bToS3)
					{
						WriteToS3Timer.Start();
						UploadFileToS3(CrashContextInfo, Config.Default.AWSS3OutputKeyPrefix + S3IDPrefix + FGenericCrashContext.CrashContextRuntimeXMLName);
						WriteToS3Timer.Stop();
					}
					if (bToDisk)
					{
						WriteToDiskTimer.Start();
						CrashContextInfo.MoveTo(DestinationFolder + FGenericCrashContext.CrashContextRuntimeXMLName);
						WriteToDiskTimer.Stop();
					}
				}

				if (DumpFileName != null)
				{
					DumpFileName = Path.Combine(DirInfo.FullName, DumpFileName);
					FileInfo DumpInfo = new FileInfo(DumpFileName);
					if (DumpInfo.Exists && NewContext.PrimaryCrashProperties.CrashDumpMode != 1 /* ECrashDumpMode.FullDump = 1*/)
					{
						if (bToS3)
						{
							WriteToS3Timer.Start();
							UploadFileToS3(DumpInfo, Config.Default.AWSS3OutputKeyPrefix + S3IDPrefix + "MiniDump.dmp");
							WriteToS3Timer.Stop();
						}
						if (bToDisk)
						{
							WriteToDiskTimer.Start();
							DumpInfo.MoveTo(DestinationFolder + "MiniDump.dmp");
							WriteToDiskTimer.Stop();
						}
					}
				}

				// Move the video (if it exists) to an alternate store
				if (VideoFileName != null)
				{
					DestinationFolder = Path.Combine(Config.Default.ProcessedVideos, IDThenUnderscore);

					VideoFileName = Path.Combine(DirInfo.FullName, VideoFileName);
					FileInfo VideoInfo = new FileInfo(VideoFileName);
					if (VideoInfo.Exists)
					{
						if (bToS3)
						{
							WriteToS3Timer.Start();
							UploadFileToS3(VideoInfo, Config.Default.AWSS3OutputKeyPrefix + S3IDPrefix + CrashReporterConstants.VideoFileName);
							WriteToS3Timer.Stop();
						}
						if (bToDisk)
						{
							CrashReporterProcessServicer.StatusReporter.AlertOnLowDisk(DestinationFolder, Config.Default.DiskSpaceAlertPercent);

							WriteToDiskTimer.Start();
							VideoInfo.MoveTo(DestinationFolder + CrashReporterConstants.VideoFileName);
							WriteToDiskTimer.Stop();
						}
					}
				}

				string TimeTakenString = string.Empty;
				if (bToS3)
				{
					TimeTakenString = string.Format("S3UploadTime={0:F1} ", WriteToS3Timer.Elapsed.TotalSeconds);
				}
				if (bToDisk)
				{
					TimeTakenString += string.Format("DiskMoveTime={0:F1} ", WriteToDiskTimer.Elapsed.TotalSeconds);
				}

				CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} AddReport: ReportID={1,8} {2}Path={3}", ProcessorIndex, ReportID, TimeTakenString, NewContext.CrashDirectory));

				UpdateProcessedReports();
				WebAddCounter.AddEvent();
				CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.ProcessingSucceededEvent);
				double Ratio = (double)WebAddCounter.TotalEvents / (double)ProcessedReportCount * 100;

				double AddedPerDay = (double)WebAddCounter.TotalEvents / Timer.Elapsed.TotalDays;

				CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + 
					string.Format(
						"AddReport: Ratio={0,2} Processed={1,7} WebAdded={2,7} AddReportTime={3} AddedPerDay={4} AddedPerMinute={5:N1}", (int) Ratio,
						ProcessedReportCount, WebAddCounter.TotalEvents, AddReportTime.Elapsed.TotalSeconds.ToString("0.00"), (int)AddedPerDay, WebAddCounter.EventsPerSecond * 60));
				return true;
			}
			catch( Exception Ex )
			{
				CrashReporterProcessServicer.WriteException(string.Format("PROC-{0} ", ProcessorIndex) + "AddReport: " + DirInfo.Name + "\n\n" + Ex, Ex);
			}

			return false;
		}

		private static void UploadFileToS3(FileInfo FileInfo, string DestFilename)
		{
			try
			{
				PutObjectResponse Response = CrashReporterProcessServicer.OutputAWS.PutS3ObjectFromFile(Config.Default.AWSS3OutputBucket,
																										DestFilename,
																										FileInfo.FullName);

				if (Response == null || Response.HttpStatusCode != HttpStatusCode.OK)
				{
					throw new CrashReporterException(string.Format("Failed to upload {0} to {1}", FileInfo.FullName, DestFilename));
				}
			}
			catch (Exception Ex)
			{
				CrashReporterProcessServicer.WriteException("UploadFileToS3: " + Ex, Ex);
			}
		}

		/// <summary>
		/// A function to process a newly landed crash report.
		/// </summary>
		/// <param name="NewContext">An instance of the generic crash context</param>
		private void ProcessReport( FGenericCrashContext NewContext )
		{
			Stopwatch ProcessReportSW = Stopwatch.StartNew();

			// Just to verify that the report is still there.
			DirectoryInfo DirInfo = new DirectoryInfo( NewContext.CrashDirectory );
			if( !DirInfo.Exists )
			{
				// Something very odd happened.
				CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + "ProcessReport: Directory not found: " + NewContext.CrashDirectory);
				return;
			}

			double WaitTime = ReadProcessAddReport( DirInfo, NewContext );
			
			// Make sure any log messages have been written to disk
			CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + string.Format("ProcessReportTime={0} WaitTime={1}", ProcessReportSW.Elapsed.TotalSeconds.ToString("0.0"), WaitTime.ToString("0.00")));
		}

		/// <summary> Reads callstack, source context and error message from the diagnostics file, if exists. </summary>
		private void ReadDiagnosticsFile( FGenericCrashContext NewContext )
		{
			// Read the callstack and the source context from the diagnostics files.
			string CrashDiagnosticsPath = Path.Combine( NewContext.CrashDirectory, CrashReporterConstants.DiagnosticsFileName );

			if (File.Exists( CrashDiagnosticsPath ))
			{
				NewContext.PrimaryCrashProperties.CallStack = FGenericCrashContext.EscapeXMLString( string.Join( "\n", FReportData.GetCallStack( CrashDiagnosticsPath ) ) );
				NewContext.PrimaryCrashProperties.SourceContext = FGenericCrashContext.EscapeXMLString( string.Join( "\n", FReportData.GetSourceContext( CrashDiagnosticsPath ) ) );

				if (NewContext.PrimaryCrashProperties.ErrorMessage.Length == 0)
				{
					NewContext.PrimaryCrashProperties.ErrorMessage = FGenericCrashContext.EscapeXMLString( string.Join( "\n", FReportData.GetExceptionDescription( CrashDiagnosticsPath ) ) );
				}
			}
		}

		double ReadProcessAddReport( DirectoryInfo DirInfo, FGenericCrashContext NewContext )
		{
			try
			{
				string DumpFileName = null;
				string LogFileName = null;
				string DiagnosticsFileName = "";
				string VideoFileName = null;

				string CrashContextName = "";

				foreach( FileInfo Info in DirInfo.GetFiles() )
				{
					switch( Info.Extension.ToLower() )
					{
					case ".avi":
					case ".mp4":
						VideoFileName = Info.Name;
						break;

					case ".runtime-xml":
						CrashContextName = Info.Name;
						break;

					case ".log":
						LogFileName = Info.Name;
						break;

					case ".txt":
						if (string.Compare( Info.Name, CrashReporterConstants.DiagnosticsFileName, true ) == 0)
						{
							DiagnosticsFileName = Info.Name;
							ReadDiagnosticsFile( NewContext );				
						}
						break;

					case ".dmp":
					case ".mdmp":
						DumpFileName = Info.Name;
						// Check to see if this has been processed locally
						FileInfo DiagnosticsInfo = new FileInfo( Path.Combine( DirInfo.FullName, CrashReporterConstants.DiagnosticsFileName ) );
						if (!DiagnosticsInfo.Exists && !NewContext.HasProcessedData())
						{
							ProcessDumpFile( Info.FullName, NewContext );
						}

						// Check to see if a new one has been created
						DiagnosticsInfo.Refresh();
						if( DiagnosticsInfo.Exists )
						{
							DiagnosticsFileName = DiagnosticsInfo.Name;	
						}
						break;

					default:
						break;
					}
				}

				// Check if the new context has processed data.
				if (!NewContext.HasProcessedData())
				{
					CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + "% Warning no callstack or error msg : BuiltFromCL=" + string.Format("{0,7}", NewContext.PrimaryCrashProperties.EngineVersion) + " Path=" + NewContext.CrashDirectory);
					NewContext.PrimaryCrashProperties.ProcessorFailedMessage = "No callstack or error message. Diagnostics missing or failed.";
				}

				Stopwatch WaitSW = Stopwatch.StartNew();

				// Wait for previous task, should not really happen.
				int AddReportTaskSlot = WaitForFreeAddReportTask();
				CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + string.Format("Starting AddReportTask running on slot {0} of {1} ({2} active)", AddReportTaskSlot, Config.Default.AddReportsPerProcessor, GetActiveAddReportTasks()));
				double WaitTime = WaitSW.Elapsed.TotalSeconds;

				//bool bAdded = AddReport(DirInfo, NewContext, LogFileName, DumpFileName, VideoFileName );
					
				// Save/update crash context to the file.
				NewContext.ToFile();
				AddReportTasks[AddReportTaskSlot] = Task.Factory.StartNew(() =>
				{
					bool bAdded = AddReport( DirInfo, NewContext, LogFileName, DumpFileName, VideoFileName );
					FinalizeReport( bAdded, DirInfo, NewContext );
				} );

				return WaitTime;

			}
			catch( Exception Ex )
			{
				CrashReporterProcessServicer.WriteException(string.Format("PROC-{0} ", ProcessorIndex) + "ProcessReport: " + NewContext.CrashDirectory + "\n\n: " + Ex, Ex);
			}

			return 0.0;
		}

		int WaitForFreeAddReportTask()
		{
			lock (AddReportTasks)
			{
				// Is a slot available because it has finished?
				for (int Index = 0; Index < AddReportTasks.Count; Index++)
				{
					if (AddReportTasks != null && AddReportTasks[Index].IsCompleted)
					{
						AddReportTasks[Index] = null;
						return Index;
					}
				}

				// Can we make a new slot?
				if (AddReportTasks.Count < Config.Default.AddReportsPerProcessor)
				{
					// Can create another
					AddReportTasks.Add(null);
					return AddReportTasks.Count - 1;
				}

				// Wait for a slot
				int CompletedIndex = Task.WaitAny(AddReportTasks.ToArray());
				AddReportTasks[CompletedIndex] = null;
				return CompletedIndex;
			}
		}

		int GetActiveAddReportTasks()
		{
			return AddReportTasks.Sum(AddReportTask => AddReportTask == null || AddReportTask.IsCompleted ? 0 : 1);
		}

		void ProcessDumpFile( string DumpPath, FGenericCrashContext NewContext )
		{
			CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + "ProcessDumpFile: Waiting to run MDD on " + DumpPath);

			if (TrySimulateSymbolication(NewContext))
			{
				CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + "ProcessDumpFile: Simulated symbolication (skipped MDD) " + NewContext.CrashDirectory);
			}
			else if(CrashReporterProcessServicer.Symbolicator.Run(DumpPath, NewContext, ProcessorIndex))
			{
				CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + "ProcessDumpFile: MDD finished running on " + DumpPath);
				ReadDiagnosticsFile(NewContext);
			}
			else
			{
				CrashReporterProcessServicer.WriteFailure(string.Format("PROC-{0} ", ProcessorIndex) + "ProcessDumpFile: MDD didn't run on " + DumpPath);
			}

			if (string.IsNullOrWhiteSpace(NewContext.PrimaryCrashProperties.CallStack))
			{
				CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.SymbolicationFailedEvent);
			}
			else
			{
				CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.SymbolicationSucceededEvent);
			}
		}

		bool TrySimulateSymbolication(FGenericCrashContext NewContext)
		{
			// Check for crashes we don't want to attempt to symbolicate

			if (!string.IsNullOrWhiteSpace(NewContext.PrimaryCrashProperties.ErrorMessage) &&
			    NewContext.PrimaryCrashProperties.ErrorMessage.Contains("Hang detected on"))
			{
				// Hangs from FThreadHeartBeat::Run() start wth "Hang detected on" and have a useful callstack already in the log
				// Callstack and source from MDD will be useless so simulate a callstack that points the user to the log.
				NewContext.PrimaryCrashProperties.CallStack = "See log for callstack from hang detection code()\nSee log for callstack from hang detection code()\nSee log for callstack from hang detection code()";
				return true;
			}

			return false;
		}

		private static void TickStatic(ReportWatcher InWatcher)
		{
			if (Monitor.TryEnter(TickStaticLock, 0))
			{
				try
				{
					// Remove all folders older than n days to prevent unbounded growth
					if ((DateTime.Now - LastCleanupTime) > TimeSpan.FromMinutes(15))
					{
						CleanRepository(InWatcher);
						LastCleanupTime = DateTime.Now;
					}

					DateTime CurrentDate = DateTime.UtcNow.Date;
					if (CurrentDate > LoggingDate)
					{
						// Check the log and create a new one for a new day.
						CrashReporterProcessServicer.Log.CreateNewLogFile();
						LoggingDate = CurrentDate;
					}
				}
				finally
				{
					Monitor.Exit(TickStaticLock);
				}
			}
		}
	}
}
