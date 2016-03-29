// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Tools.CrashReporter.CrashReportCommon;
using Tools.DotNETCommon.LaunchProcess;
using Tools.DotNETCommon.SimpleWebRequest;
using Tools.DotNETCommon.XmlHandler;

namespace Tools.CrashReporter.CrashReportProcess
{

	/// <summary>
	/// A class to handle processing of received crash reports.
	/// </summary>
	public sealed class ReportProcessor : IDisposable
	{
		/// <summary>Absolute path to Perforce (default installation assumed)</summary>
		const string PerforceExePath = "C:\\Program Files\\Perforce\\p4.exe";

		/// <summary>Give up on call stack analysis after 30 minutes</summary>
		const int MinidumpDiagnosticsTimeoutSeconds = 30 * 60;

		/// <summary>Give up on Perforce syncs after 2 minutes</summary>
		const int SyncTimeoutSeconds = 2 * 60;

		/// <summary> Number of all processed reports. </summary>
		public static int ProcessedReports = 1;

		/// <summary> Number of processed reports that have been added to the website. </summary>
		public static int WebAddedReports = 0;

		/// <summary> Global timer used to measure web added reports per day. </summary>
		public static Stopwatch Timer = Stopwatch.StartNew();

		/// <summary>The thread that handles detection of new crashes.</summary>
		public ReportWatcher Watcher = null;

		/// <summary>Task to handle processing.</summary>
		Task ProcessorTask;

		/// <summary>Object to allow the task to be cancelled at shut-down.</summary>
		CancellationTokenSource CancelSource;

		/// <summary> Task to handle adding a new record to the database. </summary>
		Task AddReportTask;

		/// <summary>
		/// Global initialisation of the processor.
		/// </summary>
		/// <param name="InWatcher">The object that watches for new crash reports coming in.</param>
		public ReportProcessor( ReportWatcher InWatcher )
		{
			// Create the thread to handle processing
			Watcher = InWatcher;
			CancelSource = new CancellationTokenSource();
			ProcessNewReports();
		}

		/// <summary>
		/// Shutdown: stop the thread
		/// </summary>
		public void Dispose()
		{
			CancelSource.Cancel();
			ProcessorTask.Wait();
	
			CancelSource.Dispose();
		}

		/// <summary>
		/// Main processing thread.
		/// </summary>
		/// <remarks>All exceptions are caught and written to the event log.</remarks>
		private void ProcessNewReports()
		{
#if !DEBUG
			if (!SyncRequiredFiles())
			{
				return;
			}
#endif

			DateTime LastCleanupTime = DateTime.Now;

			int LastDay = DateTime.UtcNow.Day;

			var Cancel = CancelSource.Token;
			ProcessorTask = Task.Factory.StartNew(() =>
			{
				while (!Cancel.IsCancellationRequested)
				{
					try
					{
						foreach( var Queue in Watcher.ReportQueues )
						{
							FGenericCrashContext NewContext = null;
							if (Queue.TryDequeueReport( out NewContext ))
							{
								ProcessReport( NewContext );

								int CurrentDay = DateTime.UtcNow.Day;
								if( CurrentDay > LastDay )
								{
									// Check the log and create a new one for a new day.
									CrashReporterProcessServicer.Log.CreateNewLogFile();
									LastDay = CurrentDay;
								}

								// The effect of this break is to prioritize ReportQueues by their order in the list, from highest to lowest
								break;
							}
						}
					}
					catch (Exception Ex)
					{
						CrashReporterProcessServicer.WriteException( "ProcessNewReports: " + Ex.ToString() );
					}

					// Remove all folders older than n days to prevent unbounded growth
					if ((DateTime.Now - LastCleanupTime) > TimeSpan.FromMinutes(15))
					{
						CleanRepository();
						LastCleanupTime = DateTime.Now;
					}

					// Don't use the CPU if we don't need.
					Thread.Sleep( 100 );
				}
			});
		}

		/// <summary> 
		/// Finalizes report files. 
		/// Thread-safe.
		/// </summary>
		private void FinalizeReport( bool bAdded, DirectoryInfo DirInfo, FGenericCrashContext NewContext )
		{
			// Only remove if we added the report, otherwise leave all files to further investigation.
			if( bAdded )
			{
				// Remove the report files as we're done with them.
				CleanReport( DirInfo );
			}
			else
			{
				MoveReportToInvalid( NewContext.CrashDirectory, NewContext.GetAsFilename() );
			}
		}

		/// <summary>
		/// Delete a report directory.
		/// Thread-safe.
		/// </summary>
		/// <param name="DirInfo">The directory to delete</param>
		public static void CleanReport(DirectoryInfo DirInfo)
		{
			bool bWriteException = true;
			for( int Retry = 0; Retry < 3; ++Retry )
			{
				try
				{
					foreach( FileInfo Info in DirInfo.GetFiles() )
					{
						Info.IsReadOnly = false;
					}
					DirInfo.Delete( true /* delete contents */);
					break;
				}
				catch( Exception Ex )
				{
					if( bWriteException )
					{
						CrashReporterProcessServicer.WriteException( "CleanReport: " + Ex.ToString() );
						bWriteException = false;
					}

				}
				System.Threading.Thread.Sleep( 100 );
			}
		}

		/// <summary>
		/// Delete a report directory.
		/// Thread-safe.
		/// </summary>
		/// <param name="DirPath">The directory to delete</param>
		public static void CleanReport(string DirPath)
		{
			CleanReport( new DirectoryInfo( DirPath ) );
		}

		/// <summary> 
		/// Moves specified report to the directory where are stored invalid reports. 
		/// Thread-safe.
		/// </summary>
		public static void MoveReportToInvalid( string ReportName, string ReportNameAsFilename )
		{
			try
			{
				DirectoryInfo DirInfo = new DirectoryInfo( ReportName );
				// Rename the report directory, so we should be able to quickly find the invalid reports.
				Directory.CreateDirectory( Properties.Settings.Default.InvalidReportsDirectory );
				string CleanFilename = String.Concat( ReportNameAsFilename.Split( Path.GetInvalidFileNameChars() ) );
				string DestinationDirectory = Path.Combine( Properties.Settings.Default.InvalidReportsDirectory, CleanFilename );

				Directory.CreateDirectory( DestinationDirectory );

				// Copy all files from the source directory. We can't use MoveTo due to different disc location.
				foreach( FileInfo File in DirInfo.GetFiles() )
				{
					string DestinationFilepath = Path.Combine( DestinationDirectory, File.Name );
					File.CopyTo( DestinationFilepath, true );
				}
				DirInfo.Delete( true );

				CrashReporterProcessServicer.WriteEvent( string.Format( "Moved to {0}", DestinationDirectory ) );
				UpdateProcessedReports();
			}
			catch( System.Exception Ex )
			{
				CrashReporterProcessServicer.WriteException( "MoveReportToInvalid: " + Ex.ToString() );
			}	
		}

		/// <summary> 
		/// Update the number of processed reports.
		/// Thread-safe.
		/// </summary>
		private static void UpdateProcessedReports()
		{
			Interlocked.Increment( ref ProcessedReports );
		}

		/// <summary> 
		/// Delete report folders older than a certain age to avoid unbounded growth of the crash repository.
		/// </summary>
		/// <remarks>The folders for the deduplication process older than the property 'DaysToSunsetReport' days old are deleted.</remarks>
		private void CleanRepository()
		{
			try
			{
				foreach (var Queue in Watcher.ReportQueues)
				{
					Queue.CleanLandingZone();
				}

				CrashReporterProcessServicer.Log.CleanOutOldLogs(Properties.Settings.Default.DaysToSunsetReport);
			}
			catch( Exception Ex )
			{
				CrashReporterProcessServicer.WriteException( "CleanRepository: " + Ex.ToString() );
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
				CrashDescription NewCrash = new CrashDescription();

				NewCrash.BranchName = EngineVersion.GetCleanedBranch();
				NewCrash.GameName = NewContext.PrimaryCrashProperties.GameName;
				NewCrash.Platform = NewContext.PrimaryCrashProperties.PlatformFullName;
				NewCrash.EngineMode = NewContext.PrimaryCrashProperties.EngineMode;
				NewCrash.BuildVersion = EngineVersion.VersionNumber;
				NewCrash.CommandLine = NewContext.PrimaryCrashProperties.CommandLine;
				NewCrash.BaseDir = NewContext.PrimaryCrashProperties.BaseDir;

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
					CrashReporterProcessServicer.WriteFailure( "! BadStack: BuiltFromCL=" + string.Format( "{0,7}", NewContext.PrimaryCrashProperties.EngineVersion ) + " Path=" + NewContext.CrashDirectory );
				}
				else
				{
					XmlPayload = XmlHandler.ToXmlString<CrashDescription>( NewCrash );
				}
			}
			catch (Exception Ex)
			{
				CrashReporterProcessServicer.WriteException( "CreateCrash: " + Ex.ToString() );
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
				if( Properties.Settings.Default.CrashReportWebSite.Length > 0 )
				{

					bool bDebug = false;
					string RequestString;
					if( !bDebug )
					{
						RequestString = "http://" + Properties.Settings.Default.CrashReportWebSite + ":80/Crashes/AddCrash/-1";
					}
					else
					{ 
						RequestString = "http://localhost:80/Crashes/AddCrash/-1"; 
					}

					string ErrorMessage = string.Empty;

					for (int Retry = 0; Retry < 3; ++Retry)
					{
						string ResponseString = SimpleWebRequest.GetWebServiceResponse(RequestString, Payload);
						if (ResponseString.Length > 0)
						{
							// Convert response into a string
							CrashReporterResult Result = XmlHandler.FromXmlString<CrashReporterResult>(ResponseString);
							if (Result.ID > 0)
							{
								NewID = Result.ID;
								break;
							}
							ErrorMessage = Result.Message;
						}
						Thread.Sleep(200);
					}

					if (NewID == -1)
					{
						CrashReporterProcessServicer.WriteFailure("UploadCrash: " + ErrorMessage);
					}
				}
			}
			catch( Exception Ex )
			{
				CrashReporterProcessServicer.WriteException( "UploadCrash: " + Ex.ToString() );
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
					CrashReporterProcessServicer.WriteFailure( "! NoDetail: Path=" + NewContext.CrashDirectory );
					return false;
				}

				// Upload the crash to the database, and retrieve the new row id
				int ReportID = UploadCrash(CrashDetails);
				if (ReportID <= 0)
				{
					CrashReporterProcessServicer.WriteFailure( "! NoUpload: Path=" + NewContext.CrashDirectory );
					string PayloadFailedFileName = Path.Combine(DirInfo.FullName, "PayloadFailed.xml");
					File.WriteAllText(PayloadFailedFileName, CrashDetails);
					return false;
				}

				string IDThenUnderscore = string.Format("{0}_", ReportID);

				// Use the row id to name and move the files the way the web site requires
				string DestinationFolder = Path.Combine(Properties.Settings.Default.ProcessedReports, IDThenUnderscore);

				// Move report files to crash reporter file store
				if (LogFileName != null)
				{
					LogFileName = Path.Combine(DirInfo.FullName, LogFileName);
					FileInfo LogInfo = new FileInfo(LogFileName);
					if(LogInfo.Exists)
					{
						LogInfo.MoveTo(DestinationFolder + "Launch.log");
					}
				}

				string CrashContextRuntimeName = Path.Combine( DirInfo.FullName, FGenericCrashContext.CrashContextRuntimeXMLName );
				FileInfo CrashContextInfo = new FileInfo( CrashContextRuntimeName );
				if (CrashContextInfo.Exists)
				{
					CrashContextInfo.MoveTo( DestinationFolder + FGenericCrashContext.CrashContextRuntimeXMLName );
				}

// 				WERMetaDataName = Path.Combine(DirInfo.FullName, WERMetaDataName);
// 				FileInfo MetaInfo = new FileInfo(WERMetaDataName);
// 				if (MetaInfo.Exists)
// 				{
// 					MetaInfo.MoveTo(DestinationFolder + "WERMeta.xml");
// 				}

				if( DumpFileName != null )
				{
					DumpFileName = Path.Combine( DirInfo.FullName, DumpFileName );
					FileInfo DumpInfo = new FileInfo( DumpFileName );
					if (DumpInfo.Exists && NewContext.PrimaryCrashProperties.CrashDumpMode != 1 /* ECrashDumpMode.FullDump = 1*/ )
					{
						DumpInfo.MoveTo( DestinationFolder + "MiniDump.dmp" );
					}
				}

// 				DiagnosticsFileName = Path.Combine(DirInfo.FullName, DiagnosticsFileName);
// 				FileInfo DiagnosticsInfo = new FileInfo(DiagnosticsFileName);
// 				if (DiagnosticsInfo.Exists)
// 				{
// 					DiagnosticsInfo.MoveTo(DestinationFolder + CrashReporterConstants.DiagnosticsFileName);
// 				}

				// Move the video (if it exists) to an alternate store
				if (VideoFileName != null)
				{
					DestinationFolder = Path.Combine(Properties.Settings.Default.ProcessedVideos, IDThenUnderscore);

					VideoFileName = Path.Combine(DirInfo.FullName, VideoFileName);
					FileInfo VideoInfo = new FileInfo(VideoFileName);
					if (VideoInfo.Exists)
					{
						VideoInfo.MoveTo(DestinationFolder + CrashReporterConstants.VideoFileName);
					}
				}

				CrashReporterProcessServicer.WriteEvent( "# WebAdded: ReportID   =" + string.Format( "{0,7}", ReportID ) + " Path=" + NewContext.CrashDirectory );

				UpdateProcessedReports();
				WebAddedReports++;
				double Ratio = (double)WebAddedReports / (double)ProcessedReports * 100;

				double AddedPerDay = (double)WebAddedReports / Timer.Elapsed.TotalDays;

				CrashReporterProcessServicer.WriteEvent( string.Format( "Ratio={0,2} Processed={1,7} WebAdded={2,7} AddReportTime={3} AddedPerDay={4}", (int)Ratio, ProcessedReports, WebAddedReports, AddReportTime.Elapsed.TotalSeconds.ToString( "0.00" ), (int)AddedPerDay ) );
				return true;
			}
			catch( Exception Ex )
			{
				CrashReporterProcessServicer.WriteException( "AddReport: " + DirInfo.Name + "\n\n" + Ex.ToString() );
			}

			return false;
		}

		/// <summary>
		/// Sync the MinidumpDiagnostics binary and the engine config files to #head.
		/// </summary>
		/// <returns>true if all the files synced without issue, false otherwise.</returns>
		/// <remarks>As MinidumpDiagnostics is synced to #head, it requires the engine config files that match to run properly.</remarks>
		private bool SyncRequiredFiles()
		{
			// Use the latest MinidumpDiagnostics from the main branch.
			string SyncBinariesString = Path.Combine(Properties.Settings.Default.DepotRoot, Properties.Settings.Default.SyncBinariesFromDepot);
			using (var MDDSyncProc = new LaunchProcess(PerforceExePath, null, CrashReporterProcessServicer.WriteP4, "sync", SyncBinariesString))
			{
				if( MDDSyncProc.WaitForExit( SyncTimeoutSeconds * 1000 ) == EWaitResult.TimedOut )
				{
					CrashReporterProcessServicer.WriteFailure("Failed to sync MinidumpDiagnostics " + SyncBinariesString);
					return false;
				}
			}

			string SyncConfigString = Path.Combine(Properties.Settings.Default.DepotRoot, Properties.Settings.Default.SyncConfigFromDepot);
			using (var ConfigSyncProc = new LaunchProcess(PerforceExePath, null, CrashReporterProcessServicer.WriteP4, "sync", SyncConfigString))
			{
				if (ConfigSyncProc.WaitForExit(SyncTimeoutSeconds * 1000) == EWaitResult.TimedOut)
				{
					CrashReporterProcessServicer.WriteFailure("Failed to sync config files " + SyncConfigString);
					return false;
				}
			}

			string SyncThirdPartyString = Path.Combine(Properties.Settings.Default.DepotRoot, Properties.Settings.Default.SyncThirdPartyFromDepot); // Required by Perforce
			using (var MiscSyncProc = new LaunchProcess(PerforceExePath, null, CrashReporterProcessServicer.WriteP4, "sync", SyncThirdPartyString))
			{
				if (MiscSyncProc.WaitForExit(SyncTimeoutSeconds * 1000) == EWaitResult.TimedOut)
				{
					CrashReporterProcessServicer.WriteFailure("Failed to sync OpenSSL files " + SyncThirdPartyString);
					return false;
				}
			}

			return true;
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
				CrashReporterProcessServicer.WriteEvent( "ProcessReport: Directory not found: " + NewContext.CrashDirectory );
				return;
			}

			double WaitTime = ReadProcessAddReport( DirInfo, NewContext );
			
			// Make sure any log messages have been written to disk
			CrashReporterProcessServicer.WriteEvent( string.Format( "ProcessReportTime={0} WaitTime={1}", ProcessReportSW.Elapsed.TotalSeconds.ToString( "0.0" ), WaitTime.ToString( "0.00" ) ) );
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
				if( NewContext.HasProcessedData() )
				{
					Stopwatch WaitSW = Stopwatch.StartNew();
					// Wait for previous task, should not really happen.
					if( AddReportTask != null )
					{
						AddReportTask.Wait();
					}
					double WaitTime = WaitSW.Elapsed.TotalSeconds;

					//bool bAdded = AddReport(DirInfo, NewContext, LogFileName, DumpFileName, VideoFileName );
					
					// Save/update crash context to the file.
					NewContext.ToFile();
					AddReportTask = Task.Factory.StartNew( () =>
					{
						bool bAdded = AddReport( DirInfo, NewContext, LogFileName, DumpFileName, VideoFileName );
						FinalizeReport( bAdded, DirInfo, NewContext );
					} );

					return WaitTime;
				}
				else
				{
					CrashReporterProcessServicer.WriteFailure( "! Invalid : BuiltFromCL=" + string.Format( "{0,7}", NewContext.PrimaryCrashProperties.EngineVersion ) + " Path=" + NewContext.CrashDirectory );
					FinalizeReport( false, DirInfo, NewContext );
				}
			}
			catch( Exception Ex )
			{
				CrashReporterProcessServicer.WriteException( "ProcessReport: " + NewContext.CrashDirectory + "\n\n: " + Ex.ToString() );
			}

			return 0.0;
		}

		void ProcessDumpFile( string DiagnosticsPath, FGenericCrashContext NewContext )
		{
			// Use the latest MinidumpDiagnostics from the main branch.
			string Win64BinariesDirectory = Path.Combine(Properties.Settings.Default.DepotRoot, Properties.Settings.Default.MDDBinariesFolderInDepot);
#if DEBUG
			// Note: the debug executable must be built locally or synced from Perforce manually
			string MinidumpDiagnosticsName = Path.Combine(Win64BinariesDirectory, "MinidumpDiagnostics-Win64-Debug.exe");
#else
			string MinidumpDiagnosticsName = Path.Combine(Win64BinariesDirectory, "MinidumpDiagnostics.exe");
#endif
			// Purge logs every 2048 processed crashes
			string PurgeLogsDays = ProcessedReports % 2048 == 0 ? "2" : "-1";

			FEngineVersion EngineVersion = new FEngineVersion( NewContext.PrimaryCrashProperties.EngineVersion );

			// Pass Windows variants (Win32/64) to MinidumpDiagnostics
			string PlatformVariant = NewContext.PrimaryCrashProperties.PlatformName;
			if (PlatformVariant != null && NewContext.PrimaryCrashProperties.PlatformFullName != null && PlatformVariant.ToUpper().Contains("WINDOWS"))
			{
				if (NewContext.PrimaryCrashProperties.PlatformFullName.Contains("Win32") ||
				    NewContext.PrimaryCrashProperties.PlatformFullName.Contains("32b"))
				{
					PlatformVariant = "Win32";
				}
				else if (NewContext.PrimaryCrashProperties.PlatformFullName.Contains("Win64") ||
						NewContext.PrimaryCrashProperties.PlatformFullName.Contains("64b"))
				{
					PlatformVariant = "Win64";
				}
			}

			List<string> MinidumpDiagnosticsParams = new List<string>
			(
				new string[] 
				{
					"\""+DiagnosticsPath+"\"",
					"-BranchName=" + EngineVersion.Branch,			// Backward compatibility
					"-BuiltFromCL=" + EngineVersion.Changelist,		// Backward compatibility
					"-GameName=" + NewContext.PrimaryCrashProperties.GameName,
					"-EngineVersion=" + NewContext.PrimaryCrashProperties.EngineVersion,
					"-PlatformName=" + NewContext.PrimaryCrashProperties.PlatformName,
					"-PlatformVariantName=" + PlatformVariant,
					"-bUsePDBCache=true",
					"-Annotate",
					"-SyncSymbols",
					"-SyncMicrosoftSymbols",
					"-unattended",
					"-Log=" + NewContext.GetAsFilename() + "-backup-.log",
					"-DepotIndex=" + Properties.Settings.Default.DepotIndex,
					"-ini:Engine:[LogFiles]:PurgeLogsDays="+PurgeLogsDays,
					"-LOGTIMESINCESTART"
				}
			);

			LaunchProcess.CaptureMessageDelegate CaptureMessageDelegate = null;
			if (Environment.UserInteractive)
			{
				CaptureMessageDelegate = CrashReporterProcessServicer.WriteMDD;
			}
			else
			{
				MinidumpDiagnosticsParams.AddRange
				(
					new[] 
					{ 
						"-buildmachine", 
						"-forcelogflush" 
					}
				);

				// Write some debugging message.
				CrashReporterProcessServicer.WriteMDD( MinidumpDiagnosticsName + " Params: " + String.Join( " ", MinidumpDiagnosticsParams ) );
			}

			LaunchProcess ReportParser = new LaunchProcess(MinidumpDiagnosticsName, Path.GetDirectoryName(MinidumpDiagnosticsName), CaptureMessageDelegate, MinidumpDiagnosticsParams.ToArray());

			if (ReportParser.WaitForExit(MinidumpDiagnosticsTimeoutSeconds * 1000) == EWaitResult.TimedOut)
			{
				CrashReporterProcessServicer.WriteFailure( "ProcessDumpFile: Timed out running MinidumpDiagnostics" );
			}

			ReadDiagnosticsFile( NewContext );
		}
	}
}
