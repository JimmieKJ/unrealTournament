// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using Tools.CrashReporter.CrashReportCommon;
using Tools.DotNETCommon.XmlHandler;

namespace Tools.CrashReporter.CrashReportProcess
{
	using ReportIdSet = HashSet<string>;

	interface IReportQueue : IDisposable
	{
		int CheckForNewReports();
		bool TryDequeueReport(out FGenericCrashContext Context);
		void CleanLandingZone();

		string QueueId { get; }
		string LandingZonePath { get; }
	}

	abstract class ReportQueueBase : IReportQueue
	{
		// PROPERTIES

		protected int QueueCount
		{
			get
			{
				lock (NewReportsLock)
				{
					return NewCrashContexts.Count;
				}
			}
		}

		protected int LastQueueSizeOnDisk { get; private set; }

		public string QueueId
		{
			get { return QueueName; }
		}

		public string LandingZonePath
		{
			get { return LandingZone; }
		}

		protected abstract string QueueProcessingStartedEventName { get; }

		// METHODS

		protected ReportQueueBase(string InQueueName, string LandingZonePath)
		{
			QueueName = InQueueName;
			LandingZone = LandingZonePath;
		}

		/// <summary>
		/// Look for new report folders and add them to the publicly available thread-safe queue.
		/// </summary>
		public int CheckForNewReports()
		{
			try
			{
				if (QueueCount >= Config.Default.MinDesiredMemoryQueueSize)
				{
					CrashReporterProcessServicer.WriteEvent(string.Format(
						"CheckForNewReports: {0} skipped busy queue size {1} in {2}", QueueName, QueueCount, LandingZone));
				}
				else
				{
					var NewReportPath = "";
					var ReportsInLandingZone = new ReportIdSet();

					DirectoryInfo[] DirectoriesInLandingZone;
					if (GetCrashesFromLandingZone(out DirectoriesInLandingZone))
					{
						LastQueueSizeOnDisk = DirectoriesInLandingZone.Length;

						int EnqueuedCount = 0;
						CrashReporterProcessServicer.WriteEvent(string.Format("CheckForNewReports: {0} reports in disk landing zone {1}",
							DirectoriesInLandingZone.Length, LandingZone));

						// Add any new reports
						for (int DirIndex = 0; DirIndex < DirectoriesInLandingZone.Length && QueueCount < Config.Default.MaxMemoryQueueSize; DirIndex++)
						{
							var SubDirInfo = DirectoriesInLandingZone[DirIndex];
							try
							{
								if (Directory.Exists(SubDirInfo.FullName))
								{
									NewReportPath = SubDirInfo.FullName;
									ReportsInLandingZone.Add(NewReportPath);
									if (!ReportsInLandingZoneLastTimeWeChecked.Contains(NewReportPath))
									{
										if (EnqueueNewReport(NewReportPath))
										{
											EnqueuedCount++;
										}
									}
								}
							}
							catch (Exception Ex)
							{
								CrashReporterProcessServicer.WriteException("CheckForNewReportsInner: " + Ex.ToString());
								ReportProcessor.MoveReportToInvalid(NewReportPath, "NEWRECORD_FAIL_" + DateTime.Now.Ticks);
							}
						}

						ReportsInLandingZoneLastTimeWeChecked = ReportsInLandingZone;

						CrashReporterProcessServicer.WriteEvent(string.Format(
							"CheckForNewReports: {0} enqueued to queue size {1} from {2}", EnqueuedCount, QueueCount, LandingZone));
						CrashReporterProcessServicer.WriteEvent(string.Format("CheckForNewReports: Enqueue rate {0:N1}/minute from {1}",
							EnqueueCounter.EventsPerSecond*60, LandingZone));
					}
					else
					{
						LastQueueSizeOnDisk = 0;
					}
				}
			}
			catch (Exception Ex)
			{
				CrashReporterProcessServicer.WriteException("CheckForNewReportsOuter: " + Ex.ToString());
			}

			return GetTotalWaitingCount();
		}

		/// <summary>
		/// Tidy up the landing zone folder
		/// </summary>
		public void CleanLandingZone()
		{
			var DirInfo = new DirectoryInfo(LandingZone);
			foreach (var SubDirInfo in DirInfo.GetDirectories())
			{
				if (SubDirInfo.LastWriteTimeUtc.AddDays(Config.Default.DeleteWaitingReportsDays) < DateTime.UtcNow)
				{
					SubDirInfo.Delete(true);
				}
			}
		}

		public virtual void Dispose()
		{
			// Attempt to remove the queued crashes from the ReportIndex
			lock (NewReportsLock)
			{
				CrashReporterProcessServicer.WriteEvent(string.Format("{0} shutting down", QueueName));
				CrashReporterProcessServicer.WriteEvent(string.Format("{0} dequeuing {1} crashes for next time", QueueName, NewCrashContexts.Count));

				while (NewCrashContexts.Count > 0)
				{
					try
					{
						string ReportName = Path.GetFileName(NewCrashContexts.Peek().CrashDirectory);
						ReportIndex.TryRemoveReport(ReportName);
					}
					finally
					{
						NewCrashContexts.Dequeue();
					}
				}
			}
		}

		protected abstract int GetTotalWaitingCount();

		protected virtual bool GetCrashesFromLandingZone(out DirectoryInfo[] OutDirectories)
		{
			// Check the landing zones for new reports
			DirectoryInfo DirInfo = new DirectoryInfo(LandingZone);

			// Couldn't find a landing zone, skip and try again later.
			// Crash receiver will recreate them if needed.
			if (!DirInfo.Exists)
			{
				CrashReporterProcessServicer.WriteFailure("LandingZone not found: " + LandingZone);
				OutDirectories = new DirectoryInfo[0];
				return false;
			}

			OutDirectories = DirInfo.GetDirectories().OrderBy(dirinfo => dirinfo.CreationTimeUtc).ToArray();
			return true;
		}

		/// <summary> Looks for the CrashContext.runtime-xml, if found, will return a new instance of the FGenericCrashContext. </summary>
		private static FGenericCrashContext FindCrashContext(string NewReportPath)
		{
			string CrashContextPath = Path.Combine(NewReportPath, FGenericCrashContext.CrashContextRuntimeXMLName);
			bool bExist = File.Exists(CrashContextPath);

			if (bExist)
			{
				return FGenericCrashContext.FromFile(CrashContextPath);
			}

			return null;
		}

		/// <summary> Converts WER metadata xml file to the crash context. </summary>
		private static void ConvertMetadataToCrashContext(FReportData ReportData, string NewReportPath, ref FGenericCrashContext OutCrashContext)
		{
			if (OutCrashContext == null)
			{
				OutCrashContext = new FGenericCrashContext();
			}

			OutCrashContext.PrimaryCrashProperties.CrashVersion = (int)ECrashDescVersions.VER_1_NewCrashFormat;

			//OutCrashContext.PrimaryCrashProperties.ProcessId = 0; don't overwrite valid ids, zero is default anyway
			OutCrashContext.PrimaryCrashProperties.CrashGUID = new DirectoryInfo(NewReportPath).Name;
			// 			OutCrashContext.PrimaryCrashProperties.IsInternalBuild
			// 			OutCrashContext.PrimaryCrashProperties.IsPerforceBuild
			// 			OutCrashContext.PrimaryCrashProperties.IsSourceDistribution
			// 			OutCrashContext.PrimaryCrashProperties.SecondsSinceStart
			OutCrashContext.PrimaryCrashProperties.GameName = ReportData.GameName;
			// 			OutCrashContext.PrimaryCrashProperties.ExecutableName
			// 			OutCrashContext.PrimaryCrashProperties.BuildConfiguration
			// 			OutCrashContext.PrimaryCrashProperties.PlatformName
			// 			OutCrashContext.PrimaryCrashProperties.PlatformNameIni
			OutCrashContext.PrimaryCrashProperties.PlatformFullName = ReportData.Platform;
			OutCrashContext.PrimaryCrashProperties.EngineMode = ReportData.EngineMode;
			OutCrashContext.PrimaryCrashProperties.EngineVersion = ReportData.GetEngineVersion();
			OutCrashContext.PrimaryCrashProperties.CommandLine = ReportData.CommandLine;
			// 			OutCrashContext.PrimaryCrashProperties.LanguageLCID

			// Create a locate and get the language.
			int LanguageCode = 0;
			int.TryParse(ReportData.Language, out LanguageCode);
			try
			{
				if (LanguageCode > 0)
				{
					CultureInfo LanguageCI = new CultureInfo(LanguageCode);
					OutCrashContext.PrimaryCrashProperties.AppDefaultLocale = LanguageCI.Name;
				}
			}
			catch (Exception)
			{
				OutCrashContext.PrimaryCrashProperties.AppDefaultLocale = null;
			}

			if (string.IsNullOrEmpty(OutCrashContext.PrimaryCrashProperties.AppDefaultLocale))
			{
				// Default to en-US
				OutCrashContext.PrimaryCrashProperties.AppDefaultLocale = "en-US";
			}

			// 			OutCrashContext.PrimaryCrashProperties.IsUE4Release
			string WERUserName = ReportData.UserName;
			if (!string.IsNullOrEmpty(WERUserName) || string.IsNullOrEmpty(OutCrashContext.PrimaryCrashProperties.UserName))
			{
				OutCrashContext.PrimaryCrashProperties.UserName = WERUserName;
			}

			OutCrashContext.PrimaryCrashProperties.BaseDir = ReportData.BaseDir;
			// 			OutCrashContext.PrimaryCrashProperties.RootDir
			OutCrashContext.PrimaryCrashProperties.MachineId = ReportData.MachineId;
			OutCrashContext.PrimaryCrashProperties.EpicAccountId = ReportData.EpicAccountId;
			// 			OutCrashContext.PrimaryCrashProperties.CallStack
			// 			OutCrashContext.PrimaryCrashProperties.SourceContext
			OutCrashContext.PrimaryCrashProperties.UserDescription = string.Join("\n", ReportData.UserDescription);

			// 			OutCrashContext.PrimaryCrashProperties.CrashDumpMode
			// 			OutCrashContext.PrimaryCrashProperties.Misc.NumberOfCores
			// 			OutCrashContext.PrimaryCrashProperties.Misc.NumberOfCoresIncludingHyperthreads
			// 			OutCrashContext.PrimaryCrashProperties.Misc.Is64bitOperatingSystem
			// 			OutCrashContext.PrimaryCrashProperties.Misc.CPUVendor
			// 			OutCrashContext.PrimaryCrashProperties.Misc.CPUBrand
			// 			OutCrashContext.PrimaryCrashProperties.Misc.PrimaryGPUBrand
			// 			OutCrashContext.PrimaryCrashProperties.Misc.OSVersionMajor
			// 			OutCrashContext.PrimaryCrashProperties.Misc.OSVersionMinor
			// 			OutCrashContext.PrimaryCrashProperties.Misc.AppDiskTotalNumberOfBytes
			// 			OutCrashContext.PrimaryCrashProperties.Misc.AppDiskNumberOfFreeBytes
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.TotalPhysical
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.TotalVirtual
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.PageSize
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.TotalPhysicalGB
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.AvailablePhysical
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.AvailableVirtual
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.UsedPhysical
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.PeakUsedPhysical
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.UsedVirtual
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.PeakUsedVirtual
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.bIsOOM
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.OOMAllocationSize
			// 			OutCrashContext.PrimaryCrashProperties.MemoryStats.OOMAllocationAlignment
			OutCrashContext.PrimaryCrashProperties.TimeofCrash = new DateTime(ReportData.Ticks);
			OutCrashContext.PrimaryCrashProperties.bAllowToBeContacted = ReportData.AllowToBeContacted;

			OutCrashContext.PrimaryCrashProperties.DeploymentName = ReportData.DeploymentName;
			OutCrashContext.PrimaryCrashProperties.IsEnsure = ReportData.IsEnsure;
			OutCrashContext.PrimaryCrashProperties.IsAssert = ReportData.IsAssert;
			OutCrashContext.PrimaryCrashProperties.CrashType = ReportData.CrashType;

			GetErrorMessageFromMetadata(ReportData, OutCrashContext);
		}

		private static void GetErrorMessageFromMetadata(FReportData ReportData, FGenericCrashContext CrashContext)
		{
			if (string.IsNullOrEmpty(CrashContext.PrimaryCrashProperties.ErrorMessage))
			{
				CrashContext.PrimaryCrashProperties.ErrorMessage = string.Join("\n", ReportData.ErrorMessage);
			}
		}

		/// <summary> Looks for the WER metadata xml file, if found, will return a new instance of the WERReportMetadata. </summary>
		private static WERReportMetadata FindMetadata(string NewReportPath)
		{
			WERReportMetadata MetaData = null;

			// Just to verify that the report is still there.
			DirectoryInfo DirInfo = new DirectoryInfo(NewReportPath);
			if (!DirInfo.Exists)
			{
				CrashReporterProcessServicer.WriteFailure("FindMetadata: Directory not found " + NewReportPath);
			}
			else
			{
				// Check to see if we wish to suppress processing of this report.	
				foreach (var Info in DirInfo.GetFiles("*.xml"))
				{
					var MetaDataToCheck = XmlHandler.ReadXml<WERReportMetadata>(Info.FullName);
					if (CheckMetaData(MetaDataToCheck))
					{
						MetaData = MetaDataToCheck;
						break;
					}
				}
			}

			return MetaData;
		}

		/// <summary> Looks for the Diagnostics.txt file and returns true if exists. </summary>
		private static bool FindDiagnostics(string NewReportPath)
		{
			bool bFound = false;

			DirectoryInfo DirInfo = new DirectoryInfo(NewReportPath);
			foreach (var Info in DirInfo.GetFiles(CrashReporterConstants.DiagnosticsFileName))
			{
				bFound = true;
				break;
			}
			return bFound;
		}

		/// <summary>
		/// Optionally don't process some reports based on the Windows Error report meta data.
		/// </summary>
		/// <param name="WERData">The Windows Error Report meta data.</param>
		/// <returns>false to reject the report.</returns>
		private static bool CheckMetaData(WERReportMetadata WERData)
		{
			if (WERData == null)
			{
				return false;
			}

			// Reject any crashes with the invalid metadata.
			if (WERData.ProblemSignatures == null || WERData.DynamicSignatures == null || WERData.OSVersionInformation == null)
			{
				return false;
			}

			// Reject any crashes from the minidump processor.
			if (WERData.ProblemSignatures.Parameter0 != null && WERData.ProblemSignatures.Parameter0.ToLower() == "MinidumpDiagnostics".ToLower())
			{
				return false;
			}

			return true;
		}

		// From CrashUpload.cpp
		/*
		struct FCompressedCrashFile : FNoncopyable
		{
			int32 CurrentFileIndex; // 4 bytes for file index
			FString Filename; // 4 bytes for length + 260 bytes for char data
			TArray<uint8> Filedata; // 4 bytes for length + N bytes for data
		}
		*/

		/// <summary> Enqueues a new crash. </summary>
		private bool EnqueueNewReport(string NewReportPath)
		{
			string ReportName = Path.GetFileName(NewReportPath);

			string CompressedReportPath = Path.Combine(NewReportPath, ReportName + ".ue4crash");
			string MetadataPath = Path.Combine(NewReportPath, ReportName + ".xml");
			bool bIsCompressed = File.Exists(CompressedReportPath) && File.Exists(MetadataPath);
			if (bIsCompressed)
			{
				FCompressedCrashInformation CompressedCrashInformation = XmlHandler.ReadXml<FCompressedCrashInformation>(MetadataPath);
				bool bResult = DecompressReport(CompressedReportPath, CompressedCrashInformation);
				if (!bResult)
				{
					ReportProcessor.MoveReportToInvalid(NewReportPath, "DECOMPRESSION_FAIL_" + DateTime.Now.Ticks + "_" + ReportName);
					return false;
				}
				else
				{
					// Rename metadata file to not interfere with the WERReportMetadata.
					File.Move(MetadataPath, Path.ChangeExtension(MetadataPath, "processed_xml"));
				}
			}

			// Unified crash reporting
			FGenericCrashContext GenericContext = FindCrashContext(NewReportPath);
			FGenericCrashContext Context = GenericContext;

			bool bContextDirty = false;
			WERReportMetadata MetaData = FindMetadata(NewReportPath);
			if (MetaData != null)
			{
				if (Context == null)
				{
					// Missing crash context
					FReportData ReportData = new FReportData(MetaData, NewReportPath);
					ConvertMetadataToCrashContext(ReportData, NewReportPath, ref Context);
					bContextDirty = true;
				}
				else if (!Context.HasProcessedData())
				{
					// Missing data - try to get from WER metadata
					FReportData ReportData = new FReportData(MetaData, NewReportPath);
					GetErrorMessageFromMetadata(ReportData, Context);
					bContextDirty = true;
				}
			}

			if (Context == null)
			{
				CrashReporterProcessServicer.WriteFailure("! NoCntx  : Path=" + NewReportPath);
				ReportProcessor.CleanReport(new DirectoryInfo(NewReportPath));
				return false;
			}

			Context.CrashDirectory = NewReportPath;
			Context.PrimaryCrashProperties.SetPlatformFullName();

			// Added data from WER, save to the crash context file.
			if (bContextDirty)
			{
				Context.ToFile();
			}

			FEngineVersion EngineVersion = new FEngineVersion(Context.PrimaryCrashProperties.EngineVersion);

			uint BuiltFromCL = EngineVersion.Changelist;

			string BranchName = EngineVersion.Branch;
			if (string.IsNullOrEmpty(BranchName))
			{
				CrashReporterProcessServicer.WriteEvent("% Warning NoBranch: BuiltFromCL=" + string.Format("{0,7}", BuiltFromCL) + " Path=" + NewReportPath + " EngineVersion=" + Context.PrimaryCrashProperties.EngineVersion);
				Context.PrimaryCrashProperties.ProcessorFailedMessage = "Engine version has no branch name. EngineVersion=" + Context.PrimaryCrashProperties.EngineVersion;
                Context.ToFile();
            }
			else if (BranchName.Equals(CrashReporterConstants.LicenseeBranchName, StringComparison.InvariantCultureIgnoreCase))
			{
				CrashReporterProcessServicer.WriteEvent("% Warning branch is UE4-QA  : BuiltFromCL=" + string.Format("{0,7}", BuiltFromCL) + " Path=" + NewReportPath);
				Context.PrimaryCrashProperties.ProcessorFailedMessage = "Branch was the forbidden LicenseeBranchName=" + BranchName;
                Context.ToFile();
            }

			// Look for the Diagnostics.txt, if we have a diagnostics file we can continue event if the CL is marked as broken.
			bool bHasDiagnostics = FindDiagnostics(NewReportPath);

			if (BuiltFromCL == 0 && (!bHasDiagnostics && !Context.HasProcessedData()))
			{
				// For now ignore all locally made crashes.
				CrashReporterProcessServicer.WriteEvent("% Warning CL=0 and no useful data : BuiltFromCL=" + string.Format("{0,7}", BuiltFromCL) + " Path=" + NewReportPath);
				Context.PrimaryCrashProperties.ProcessorFailedMessage = "Report from CL=0 has no diagnostics, callstack or error msg";
                Context.ToFile();
            }

			// Check static reports index for duplicated reports
			// This WILL happen when running multiple, non-mutually exclusive crash sources (e.g. Receiver + Data Router)
			// This can be safely discontinued when all crashes come from the same SQS
			// This DOES NOT scale to multiple CRP instances
			lock (ReportIndexLock)
			{
				if (!ReportIndex.TryAddNewReport(ReportName))
				{
					// Crash report not accepted by index
					CrashReporterProcessServicer.WriteEvent(string.Format(
						"EnqueueNewReport: Duplicate report skipped {0} in queue {1}", NewReportPath, QueueName));
					CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.DuplicateRejected);
					ReportProcessor.CleanReport(new DirectoryInfo(NewReportPath));
					return false;
				}
			}

			lock (NewReportsLock)
			{
				NewCrashContexts.Enqueue(Context);
			}
			EnqueueCounter.AddEvent();
			CrashReporterProcessServicer.StatusReporter.IncrementCount(StatusReportingEventNames.QueuedEvent);
			CrashReporterProcessServicer.WriteEvent("+ Enqueued: BuiltFromCL=" + string.Format("{0,7}", BuiltFromCL) + " Path=" + NewReportPath);
			return true;
		}

		/// <summary> Tries to dequeue a report from the list. </summary>
		public bool TryDequeueReport(out FGenericCrashContext Context)
		{
			lock (NewReportsLock)
			{
				if (NewCrashContexts.Count > 0)
				{
					Context = NewCrashContexts.Dequeue();
					DequeueCounter.AddEvent();
					CrashReporterProcessServicer.StatusReporter.IncrementCount(QueueProcessingStartedEventName);
					CrashReporterProcessServicer.WriteEvent(string.Format("- Dequeued: {0:N1}/minute BuiltFromCL={1,7} Path={2}", DequeueCounter.EventsPerSecond * 60, Context.PrimaryCrashProperties.EngineVersion, Context.CrashDirectory));
					return true;
				}
			}

			Context = null;
			return false;
		}

		/// <summary> 
		/// Decompresses a compressed crash report. 
		/// </summary>
		unsafe static protected bool DecompressReport(string CompressedReportPath, FCompressedCrashInformation Meta)
		{
			string ReportPath = Path.GetDirectoryName(CompressedReportPath);
			using (FileStream FileStream = File.OpenRead(CompressedReportPath))
			{
				Int32 UncompressedSize = Meta.GetUncompressedSize();
				Int32 CompressedSize = Meta.GetCompressedSize();

				if (FileStream.Length != CompressedSize)
				{
					return false;
				}

				byte[] UncompressedBufferArray = new byte[UncompressedSize];
				byte[] CompressedBufferArray = new byte[CompressedSize];

				FileStream.Read(CompressedBufferArray, 0, CompressedSize);

				fixed (byte* UncompressedBufferPtr = UncompressedBufferArray, CompressedBufferPtr = CompressedBufferArray)
				{
					Int32 UncompressResult = NativeMethods.UE4UncompressMemoryZLIB((IntPtr)UncompressedBufferPtr, UncompressedSize, (IntPtr)CompressedBufferPtr, CompressedSize);
					if (UncompressResult < 0)
					{
						CrashReporterProcessServicer.WriteFailure("! DecompressReport() failed in UE4UncompressMemoryZLIB() with " + NativeMethods.GetUncompressError(UncompressResult));
						CrashReporterProcessServicer.WriteFailure("! ZLibFail in DecompressReport(): Path=" + ReportPath);
						return false;
					}
				}

				using (BinaryReader BinaryData = new BinaryReader(new MemoryStream(UncompressedBufferArray, false)))
				{
					for (int FileIndex = 0; FileIndex < Meta.GetNumberOfFiles(); FileIndex++)
					{
						if (!WriteIncomingFile(BinaryData, FileIndex, ReportPath))
						{
							CrashReporterProcessServicer.WriteFailure("! DecompressReport() failed to write file index " + FileIndex + " Path=" + ReportPath);
							return false;
						}
					}
				}
			}

			return true;
		}

		protected static bool WriteIncomingFile(BinaryReader BinaryData, int FileIndex, string DestinationFolderPath)
		{
			try
			{
				Int32 CurrentFileIndex = BinaryData.ReadInt32();
				if (CurrentFileIndex != FileIndex)
				{
					CrashReporterProcessServicer.WriteFailure("! WriteIncomingFile index mismatch: Required=" + FileIndex + " Read=" + CurrentFileIndex);
					return false;
				}

				string Filename = FBinaryReaderHelper.ReadFixedSizeString(BinaryData);
				string SafeFilename = GetSafeFilename(Filename);
				Int32 FiledataLength = BinaryData.ReadInt32();
				byte[] Filedata = BinaryData.ReadBytes(FiledataLength);

				Directory.CreateDirectory(DestinationFolderPath);
				File.WriteAllBytes(Path.Combine(DestinationFolderPath, SafeFilename), Filedata);
				return true;
			}
			catch (Exception Ex)
			{
				throw new CrashReporterException(string.Format("! WriteIncomingFile failed writing FileIndex={0} FolderPath={1}", FileIndex, DestinationFolderPath), Ex);
			}
		}

        public static string GetSafeFilename(string UnsafeName)
        {
            return string.Join("X", UnsafeName.Split(Path.GetInvalidFileNameChars()));
        }

        // FIELDS

        /// <summary> A list of freshly landed crash reports. </summary>
        private readonly Queue<FGenericCrashContext> NewCrashContexts = new Queue<FGenericCrashContext>();

		/// <summary> Object used to synchronize the access to the NewReport. </summary>
		private readonly Object NewReportsLock = new Object();

		private readonly EventCounter DequeueCounter = new EventCounter(TimeSpan.FromMinutes(10), 10);

		/// <summary>Friendly queue name</summary>
		private readonly string QueueName;

		/// <summary>Incoming report path</summary>
		protected readonly string LandingZone;

		/// <summary>Report IDs that were in the folder on the last pass</summary>
		private ReportIdSet ReportsInLandingZoneLastTimeWeChecked = new ReportIdSet();

		private readonly EventCounter EnqueueCounter = new EventCounter(TimeSpan.FromMinutes(90), 20);

		protected static Object ReportIndexLock = new Object();
	}
}

static class NativeMethods
{
	[DllImport("CompressionHelper.dll", CallingConvention = CallingConvention.Cdecl)]
	public extern static Int32 UE4UncompressMemoryZLIB( /*void**/IntPtr UncompressedBuffer, Int32 UncompressedSize, /*const void**/IntPtr CompressedBuffer, Int32 CompressedSize);

	public static string GetUncompressError(Int32 ErrorCode)
	{
		if (ErrorCode == 0) return "Z_OK";
		else if (ErrorCode == 1) return "Z_STREAM_END";
		else if (ErrorCode == 2) return "Z_NEED_DICT";
		else if (ErrorCode == -1) return "Z_ERRNO";
		else if (ErrorCode == -2) return "Z_STREAM_ERROR";
		else if (ErrorCode == -3) return "Z_DATA_ERROR";
		else if (ErrorCode == -4) return "Z_MEM_ERROR";
		else if (ErrorCode == -5) return "Z_BUF_ERROR";
		else if (ErrorCode == -6) return "Z_VERSION_ERROR";
		else return "UnknownZLibError";
	}
}