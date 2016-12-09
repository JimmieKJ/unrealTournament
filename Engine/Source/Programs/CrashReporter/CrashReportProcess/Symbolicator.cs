using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Tools.CrashReporter.CrashReportCommon;
using Tools.DotNETCommon.LaunchProcess;

namespace Tools.CrashReporter.CrashReportProcess
{
	class Symbolicator
	{
		/// <summary>
		/// Builds MDD command line args, waits for a MDD task slot, runs MDD and blocks for the result. Writes diag text into DiagnosticsPath folder if successful.
		/// </summary>
		/// <param name="DiagnosticsPath">Path of the minidump file</param>
		/// <param name="Context">The crash context</param>
		/// <param name="ProcessorIndex">Processor thread index for logging purposes</param>
		/// <returns>True, if successful</returns>
		public bool Run(string DiagnosticsPath, FGenericCrashContext Context, int ProcessorIndex)
		{
			if (!File.Exists(Config.Default.MDDExecutablePath))
			{
				CrashReporterProcessServicer.WriteEvent("Symbolicator.Run() file not found " + Config.Default.MDDExecutablePath);
				return false;
			}

            // Use MinidumpDiagnostics from MDDExecutablePath.
            CrashReporterProcessServicer.StatusReporter.AlertOnLowDisk(Config.Default.MDDExecutablePath, Config.Default.DiskSpaceAlertPercent);

			// Don't purge logs
			// TODO: make this clear to logs once a day or something (without letting MDD check on every run!)
			string PurgeLogsDays = "-1";

			FEngineVersion EngineVersion = new FEngineVersion(Context.PrimaryCrashProperties.EngineVersion);

			// Pass Windows variants (Win32/64) to MinidumpDiagnostics
			string PlatformVariant = Context.PrimaryCrashProperties.PlatformName;
			if (PlatformVariant != null && Context.PrimaryCrashProperties.PlatformFullName != null && PlatformVariant.ToUpper().Contains("WINDOWS"))
			{
				if (Context.PrimaryCrashProperties.PlatformFullName.Contains("Win32") ||
					Context.PrimaryCrashProperties.PlatformFullName.Contains("32b"))
				{
					PlatformVariant = "Win32";
				}
				else if (Context.PrimaryCrashProperties.PlatformFullName.Contains("Win64") ||
						Context.PrimaryCrashProperties.PlatformFullName.Contains("64b"))
				{
					PlatformVariant = "Win64";
				}
			}

			// Build the absolute log file path for MinidumpDiagnostics
			string BaseFolder = CrashReporterProcessServicer.SymbolicatorLogFolder;
		    DateTime WriteTime = DateTime.UtcNow;
            string DateFolder = WriteTime.ToString("yyyy_MM_dd");
            string HourFolder = WriteTime.ToString("HH");
            string Folder = Path.Combine(BaseFolder, DateFolder, HourFolder);
			string AbsLogPath = Path.Combine(Folder, Context.GetAsFilename() + ".log");
			Directory.CreateDirectory(Folder);

			List<string> MinidumpDiagnosticsParams = new List<string>
			(
				new string[]
				{
					"\""+DiagnosticsPath+"\"",
					"-BranchName=" + EngineVersion.Branch,          // Backward compatibility
					"-BuiltFromCL=" + EngineVersion.Changelist,     // Backward compatibility
					"-GameName=" + Context.PrimaryCrashProperties.GameName,
					"-EngineVersion=" + Context.PrimaryCrashProperties.EngineVersion,
					"-BuildVersion=" + (string.IsNullOrWhiteSpace(Context.PrimaryCrashProperties.BuildVersion) ?
										string.Format("{0}-CL-{1}", EngineVersion.Branch, EngineVersion.Changelist).Replace('/', '+') :
										Context.PrimaryCrashProperties.BuildVersion),
					"-PlatformName=" + Context.PrimaryCrashProperties.PlatformName,
					"-PlatformVariantName=" + PlatformVariant,
					"-bUsePDBCache=true",
					"-PDBCacheDepotRoot=" + Config.Default.DepotRoot,
					"-PDBCachePath=" + Config.Default.MDDPDBCachePath,
					"-PDBCacheSizeGB=" + Config.Default.MDDPDBCacheSizeGB,
					"-PDBCacheMinFreeSpaceGB=" + Config.Default.MDDPDBCacheMinFreeSpaceGB,
					"-PDBCacheFileDeleteDays=" + Config.Default.MDDPDBCacheFileDeleteDays,
					"-MutexPDBCache",
					"-PDBCacheLock=CrashReportProcessPDBCacheLock",
					"-NoTrimCallstack",
					"-SyncSymbols",
					"-NoP4Symbols",
					"-ForceUsePDBCache",
					"-MutexSourceSync",
					"-SourceSyncLock=CrashReportProcessSourceSyncLock",
					"-SyncMicrosoftSymbols",
					"-unattended",
					"-AbsLog=" + AbsLogPath,
					"-DepotIndex=" + Config.Default.DepotIndex,
					"-P4User=" + Config.Default.P4User,
					"-P4Client=" + Config.Default.P4Client,
					"-ini:Engine:[LogFiles]:PurgeLogsDays=" + PurgeLogsDays + ",[LogFiles]:MaxLogFilesOnDisk=-1",
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
						"-buildmachine"
					}
				);

				// Write some debugging message.
				CrashReporterProcessServicer.WriteMDD("MinidumpDiagnostics Params: " + String.Join(" ", MinidumpDiagnosticsParams));
			}

			Task<bool> NewSymbolicatorTask = Task.FromResult(false);
			Stopwatch WaitSW = Stopwatch.StartNew();
			double WaitForLockTime = 0.0;

			lock (Tasks)
			{
				int TaskIdx = Task.WaitAny(Tasks);

				Tasks[TaskIdx] = NewSymbolicatorTask = Task<bool>.Factory.StartNew(() =>
				{
					LaunchProcess ReportParser = new LaunchProcess(Config.Default.MDDExecutablePath, Path.GetDirectoryName(Config.Default.MDDExecutablePath), CaptureMessageDelegate,
																	MinidumpDiagnosticsParams.ToArray());

					return ReportParser.WaitForExit(Config.Default.MDDTimeoutMinutes*1000*60) == EWaitResult.Ok;
				});

				WaitForLockTime = WaitSW.Elapsed.TotalSeconds;
			}

			NewSymbolicatorTask.Wait();

			double TotalMDDTime = WaitSW.Elapsed.TotalSeconds;
			double MDDRunTime = TotalMDDTime - WaitForLockTime;
			CrashReporterProcessServicer.StatusReporter.AddToMeanCounter(StatusReportingPerfMeanNames.MinidumpDiagnostics, (int)(MDDRunTime * 1000));
			CrashReporterProcessServicer.WriteEvent(string.Format("PROC-{0} ", ProcessorIndex) + string.Format("Symbolicator.Run: Thread blocked for {0:N1}s then MDD ran for {1:N1}s", WaitForLockTime, MDDRunTime));

			return NewSymbolicatorTask.Result;
		}

		private Task<bool>[] Tasks;

		public Symbolicator()
		{
			Tasks = Enumerable
				.Repeat(Task.FromResult(true), Config.Default.MaxConcurrentMDDs)
				.ToArray();
		}
	}
}
