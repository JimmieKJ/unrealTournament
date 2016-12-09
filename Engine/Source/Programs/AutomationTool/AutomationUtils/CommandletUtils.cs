// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading;
using UnrealBuildTool;


namespace AutomationTool
{
	public partial class CommandUtils
	{
		#region Commandlets

		/// <summary>
		/// Runs Cook commandlet.
		/// </summary>
		[Obsolete("CookCommandlet now takes a FileReference for project argument")]
		public static void CookCommandlet(string ProjectName)
		{
			FileReference ProjectFile;
			if(ProjectName.Contains('/') || ProjectName.Contains('\\'))
			{
				ProjectFile = new FileReference(ProjectName);
			}
			else if(!UProjectInfo.TryGetProjectFileName(ProjectName, out ProjectFile))
			{
				throw new AutomationException("Cannot determine project path for {0}", ProjectName);
			}
			CookCommandlet(ProjectFile);
		}

		/// <summary>
		/// Runs Cook commandlet.
		/// </summary>
		/// <param name="ProjectName">Project name.</param>
		/// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
		/// <param name="Maps">List of maps to cook, can be null in which case -MapIniSection=AllMaps is used.</param>
		/// <param name="Dirs">List of directories to cook, can be null</param>
        /// <param name="InternationalizationPreset">The name of a prebuilt set of internationalization data to be included.</param>
        /// <param name="CulturesToCook">List of culture names whose localized assets should be cooked, can be null (implying defaults should be used).</param>
		/// <param name="TargetPlatform">Target platform.</param>
		/// <param name="Parameters">List of additional parameters.</param>
		public static void CookCommandlet(FileReference ProjectName, string UE4Exe = "UE4Editor-Cmd.exe", string[] Maps = null, string[] Dirs = null, string InternationalizationPreset = "", string[] CulturesToCook = null, string TargetPlatform = "WindowsNoEditor", string Parameters = "-Unversioned")
		{
            string CommandletArguments = "";

			if (IsNullOrEmpty(Maps))
			{
				// MapsToCook = "-MapIniSection=AllMaps";
			}
			else
			{
				string MapsToCookArg = "-Map=" + CombineCommandletParams(Maps);
                MapsToCookArg.Trim();
                CommandletArguments += (CommandletArguments.Length > 0 ? " " : "") + MapsToCookArg;
			}

			if (!IsNullOrEmpty(Dirs))
			{
				string DirsToCookArg = "-CookDir=" + CombineCommandletParams(Dirs);
                DirsToCookArg.Trim();
                CommandletArguments += (CommandletArguments.Length > 0 ? " " : "") + DirsToCookArg;
            }

            if (!String.IsNullOrEmpty(InternationalizationPreset))
            {
                CommandletArguments += (CommandletArguments.Length > 0 ? " " : "") + InternationalizationPreset;
            }

            if (!IsNullOrEmpty(CulturesToCook))
            {
                string CulturesToCookArg = "-CookCultures=" + CombineCommandletParams(CulturesToCook);
                CulturesToCookArg.Trim();
                CommandletArguments += (CommandletArguments.Length > 0 ? " " : "") + CulturesToCookArg;
            }

            RunCommandlet(ProjectName, UE4Exe, "Cook", String.Format("{0} -TargetPlatform={1} {2}",  CommandletArguments, TargetPlatform, Parameters));
		}

        /// <summary>
        /// Runs DDC commandlet.
        /// </summary>
        /// <param name="ProjectName">Project name.</param>
        /// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
        /// <param name="Maps">List of maps to cook, can be null in which case -MapIniSection=AllMaps is used.</param>
        /// <param name="TargetPlatform">Target platform.</param>
        /// <param name="Parameters">List of additional parameters.</param>
        public static void DDCCommandlet(FileReference ProjectName, string UE4Exe = "UE4Editor-Cmd.exe", string[] Maps = null, string TargetPlatform = "WindowsNoEditor", string Parameters = "")
        {
            string MapsToCook = "";
            if (!IsNullOrEmpty(Maps))
            {
                MapsToCook = "-Map=" + CombineCommandletParams(Maps);
                MapsToCook.Trim();
            }

            RunCommandlet(ProjectName, UE4Exe, "DerivedDataCache", String.Format("{0} -TargetPlatform={1} {2}", MapsToCook, TargetPlatform, Parameters));
        }

		/// <summary>
		/// Runs RebuildLightMaps commandlet.
		/// </summary>
		/// <param name="ProjectName">Project name.</param>
		/// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
		/// <param name="Maps">List of maps to rebuild light maps for. Can be null in which case -MapIniSection=AllMaps is used.</param>
		/// <param name="Parameters">List of additional parameters.</param>
		public static void RebuildLightMapsCommandlet(FileReference ProjectName, string UE4Exe = "UE4Editor-Cmd.exe", string[] Maps = null, string Parameters = "")
		{
			string MapsToRebuildLighting = "";
			if (!IsNullOrEmpty(Maps))
			{
				MapsToRebuildLighting = "-Map=" + CombineCommandletParams(Maps);
				MapsToRebuildLighting.Trim();
			}

			RunCommandlet(ProjectName, UE4Exe, "ResavePackages", String.Format("-buildlighting -MapsOnly -ProjectOnly -AllowCommandletRendering -SkipSkinVerify {0} {1}", MapsToRebuildLighting, Parameters));
		}

        /// <summary>
        /// Runs RebuildLightMaps commandlet.
        /// </summary>
        /// <param name="ProjectName">Project name.</param>
        /// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
        /// <param name="Maps">List of maps to rebuild light maps for. Can be null in which case -MapIniSection=AllMaps is used.</param>
        /// <param name="Parameters">List of additional parameters.</param>
        public static void ResavePackagesCommandlet(FileReference ProjectName, string UE4Exe = "UE4Editor-Cmd.exe", string[] Maps = null, string Parameters = "")
        {
            string MapsToRebuildLighting = "";
            if (!IsNullOrEmpty(Maps))
            {
                MapsToRebuildLighting = "-Map=" + CombineCommandletParams(Maps);
                MapsToRebuildLighting.Trim();
            }

            RunCommandlet(ProjectName, UE4Exe, "ResavePackages", String.Format((!String.IsNullOrEmpty(MapsToRebuildLighting) ? "-MapsOnly" : "") + "-ProjectOnly {0} {1}", MapsToRebuildLighting, Parameters));
        }

        /// <summary>
        /// Runs GenerateDistillFileSets commandlet.
        /// </summary>
        /// <param name="ProjectName">Project name.</param>
        /// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
        /// <param name="Maps">List of maps to cook, can be null in which case -MapIniSection=AllMaps is used.</param>
        /// <param name="TargetPlatform">Target platform.</param>
        /// <param name="Parameters">List of additional parameters.</param>
        public static List<string> GenerateDistillFileSetsCommandlet(FileReference ProjectName, string ManifestFile, string UE4Exe = "UE4Editor-Cmd.exe", string[] Maps = null, string Parameters = "")
        {
            string MapsToCook = "";
            if (!IsNullOrEmpty(Maps))
            {
                MapsToCook = CombineCommandletParams(Maps, " ");
                MapsToCook.Trim();
            }
            var Dir = Path.GetDirectoryName(ManifestFile);
            var Filename = Path.GetFileName(ManifestFile);
            if (String.IsNullOrEmpty(Dir) || String.IsNullOrEmpty(Filename))
            {
                throw new AutomationException("GenerateDistillFileSets should have a full path and file for {0}.", ManifestFile);
            }
            CreateDirectory_NoExceptions(Dir);
            if (FileExists_NoExceptions(ManifestFile))
            {
                DeleteFile(ManifestFile);
            }

            RunCommandlet(ProjectName, UE4Exe, "GenerateDistillFileSets", String.Format("{0} -OutputFolder={1} -Output={2} {3}", MapsToCook, CommandUtils.MakePathSafeToUseWithCommandLine(Dir), Filename, Parameters));

            if (!FileExists_NoExceptions(ManifestFile))
            {
                throw new AutomationException("GenerateDistillFileSets did not produce a manifest for {0}.", ProjectName);
            }
            var Lines = new List<string>(ReadAllLines(ManifestFile));
            if (Lines.Count < 1)
            {
                throw new AutomationException("GenerateDistillFileSets for {0} did not produce any files.", ProjectName);
            }
            var Result = new List<string>();
            foreach (var ThisFile in Lines)
            {
                var TestFile = CombinePaths(ThisFile);
                if (!FileExists_NoExceptions(TestFile))
                {
                    throw new AutomationException("GenerateDistillFileSets produced {0}, but {1} doesn't exist.", ThisFile, TestFile);
                }
                // we correct the case here
                var TestFileInfo = new FileInfo(TestFile);
                var FinalFile = CombinePaths(TestFileInfo.FullName);
                if (!FileExists_NoExceptions(FinalFile))
                {
                    throw new AutomationException("GenerateDistillFileSets produced {0}, but {1} doesn't exist.", ThisFile, FinalFile);
                }
                Result.Add(FinalFile);
            }
            return Result;
        }

        /// <summary>
        /// Runs UpdateGameProject commandlet.
        /// </summary>
        /// <param name="ProjectName">Project name.</param>
        /// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
        /// <param name="Parameters">List of additional parameters.</param>
        public static void UpdateGameProjectCommandlet(FileReference ProjectName, string UE4Exe = "UE4Editor-Cmd.exe", string Parameters = "")
        {
            RunCommandlet(ProjectName, UE4Exe, "UpdateGameProject", Parameters);
        }

		/// <summary>
		/// Runs a commandlet using Engine/Binaries/Win64/UE4Editor-Cmd.exe.
		/// </summary>
		/// <param name="ProjectName">Project name.</param>
		/// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
		/// <param name="Commandlet">Commandlet name.</param>
		/// <param name="Parameters">Command line parameters (without -run=)</param>
		[Obsolete("RunCommandlet now takes a uproject path as first argument")]
		public static void RunCommandlet(string ProjectName, string UE4Exe, string Commandlet, string Parameters = null)
		{
			FileReference ProjectFile;
			if(!UProjectInfo.TryGetProjectFileName(ProjectName, out ProjectFile))
			{
				throw new AutomationException("Cannot determine project file for {0}", ProjectName);
			}
			RunCommandlet(ProjectFile, UE4Exe, Commandlet, Parameters);
		}

		/// <summary>
		/// Runs a commandlet using Engine/Binaries/Win64/UE4Editor-Cmd.exe.
		/// </summary>
		/// <param name="ProjectFile">Project name.</param>
		/// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
		/// <param name="Commandlet">Commandlet name.</param>
		/// <param name="Parameters">Command line parameters (without -run=)</param>
		public static void RunCommandlet(FileReference ProjectName, string UE4Exe, string Commandlet, string Parameters = null)
		{
			Log("Running UE4Editor {0} for project {1}", Commandlet, ProjectName);

            var CWD = Path.GetDirectoryName(UE4Exe);

            string EditorExe = UE4Exe;

            if (String.IsNullOrEmpty(CWD))
            {
                EditorExe = HostPlatform.Current.GetUE4ExePath(UE4Exe);
                CWD = CombinePaths(CmdEnv.LocalRoot, HostPlatform.Current.RelativeBinariesFolder);
            }
			
			PushDir(CWD);

			DateTime StartTime = DateTime.UtcNow;

			string LocalLogFile = LogUtils.GetUniqueLogName(CombinePaths(CmdEnv.EngineSavedFolder, Commandlet));
			Log("Commandlet log file is {0}", LocalLogFile);
			string Args = String.Format(
				"{0} -run={1} {2} -abslog={3} -stdout -FORCELOGFLUSH -CrashForUAT -unattended {5}{4}",
				(ProjectName == null) ? "" : CommandUtils.MakePathSafeToUseWithCommandLine(ProjectName.FullName),
				Commandlet,
				String.IsNullOrEmpty(Parameters) ? "" : Parameters,
				CommandUtils.MakePathSafeToUseWithCommandLine(LocalLogFile),
				IsBuildMachine ? "-buildmachine" : "",
                (GlobalCommandLine.Verbose || GlobalCommandLine.AllowStdOutLogVerbosity) ? "-AllowStdOutLogVerbosity " : ""
			);
			ERunOptions Opts = ERunOptions.Default;
			if (GlobalCommandLine.UTF8Output)
			{
				Args += " -UTF8Output";
				Opts |= ERunOptions.UTF8Output;
			}
			var RunResult = Run(EditorExe, Args, Options: Opts);
			PopDir();

			// Draw attention to signal exit codes on Posix systems, rather than just printing the exit code
			if(RunResult.ExitCode > 128 && RunResult.ExitCode < 128 + 32)
			{
				if(RunResult.ExitCode == 139)
				{
					CommandUtils.LogError("Editor terminated abnormally due to a segmentation fault");
				}
				else
				{
					CommandUtils.LogError("Editor terminated abnormally with signal {0}", RunResult.ExitCode - 128);
				}
			}

			// If we're running on a Mac, dump all the *.crash files that were generated while the editor was running.
			if(HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Mac)
			{
				// If the exit code indicates the main process crashed, introduce a small delay because the crash report is written asynchronously.
				// If we exited normally, still check without waiting in case SCW or some other child process crashed.
				if(RunResult.ExitCode > 128)
				{
					CommandUtils.Log("Pausing before checking for crash logs...");
					Thread.Sleep(10 * 1000);
				}
				
				// Create a list of directories containing crash logs, and add the system log folder
				List<string> CrashDirs = new List<string>();
				CrashDirs.Add("/Library/Logs/DiagnosticReports");
					
				// Add the user's log directory too
				string HomeDir = Environment.GetEnvironmentVariable("HOME");
				if(!String.IsNullOrEmpty(HomeDir))
				{
					CrashDirs.Add(Path.Combine(HomeDir, "Library/Logs/DiagnosticReports"));
				}

				// Check each directory for crash logs
				List<FileInfo> CrashFileInfos = new List<FileInfo>();
				foreach(string CrashDir in CrashDirs)
				{
					try
					{
						DirectoryInfo CrashDirInfo = new DirectoryInfo(CrashDir);
						if (CrashDirInfo.Exists)
						{
							CrashFileInfos.AddRange(CrashDirInfo.EnumerateFiles("*.crash", SearchOption.TopDirectoryOnly).Where(x => x.LastWriteTimeUtc >= StartTime));
						}
					}
					catch (UnauthorizedAccessException)
					{
						// Not all account types can access /Library/Logs/DiagnosticReports
					}
				}
				
				// Dump them all to the log
				foreach(FileInfo CrashFileInfo in CrashFileInfos)
				{
					// snmpd seems to often crash (suspect due to it being starved of CPU cycles during cooks)
					if(!CrashFileInfo.Name.StartsWith("snmpd_"))
					{
						CommandUtils.Log("Found crash log - {0}", CrashFileInfo.FullName);
						try
						{
							string[] Lines = File.ReadAllLines(CrashFileInfo.FullName);
							foreach(string Line in Lines)
							{
								CommandUtils.Log("Crash: {0}", Line);
							}
						}
						catch(Exception Ex)
						{
							CommandUtils.LogWarning("Failed to read file ({0})", Ex.Message);
						}
					}
				}
			}

			// Copy the local commandlet log to the destination folder.
			string DestLogFile = LogUtils.GetUniqueLogName(CombinePaths(CmdEnv.LogFolder, Commandlet));
			if (!CommandUtils.CopyFile_NoExceptions(LocalLogFile, DestLogFile))
			{
				CommandUtils.LogWarning("Commandlet {0} failed to copy the local log file from {1} to {2}. The log file will be lost.", Commandlet, LocalLogFile, DestLogFile);
			}
            string ProjectStatsDirectory = CombinePaths((ProjectName == null)? CombinePaths(CmdEnv.LocalRoot, "Engine") : Path.GetDirectoryName(ProjectName.FullName), "Saved", "Stats");
            if (Directory.Exists(ProjectStatsDirectory))
            {
                string DestCookerStats = CmdEnv.LogFolder;
                foreach (var StatsFile in Directory.EnumerateFiles(ProjectStatsDirectory, "*.csv"))
                {
                    if (!CommandUtils.CopyFile_NoExceptions(StatsFile, CombinePaths(DestCookerStats, Path.GetFileName(StatsFile))))
                    {
						CommandUtils.LogWarning("Commandlet {0} failed to copy the local log file from {1} to {2}. The log file will be lost.", Commandlet, StatsFile, CombinePaths(DestCookerStats, Path.GetFileName(StatsFile)));
                    }
                }
            }
//			else
//			{
//				CommandUtils.LogWarning("Failed to find directory {0} will not save stats", ProjectStatsDirectory);
//			}

			// Whether it was copied correctly or not, delete the local log as it was only a temporary file. 
			CommandUtils.DeleteFile_NoExceptions(LocalLogFile);

			if (RunResult.ExitCode != 0)
			{
				throw new AutomationException(DestLogFile, RunResult.ExitCode, "BUILD FAILED: Failed while running {0} for {1}; see log {2}", Commandlet, ProjectName, DestLogFile);
			}
		}
		
		/// <summary>
		/// Returns the default path of the editor executable to use for running commandlets.
		/// </summary>
		/// <param name="BuildRoot">Root directory for the build</param>
		/// <param name="HostPlatform">Platform to get the executable for</param>
		/// <returns>Path to the editor executable</returns>
		public static string GetEditorCommandletExe(string BuildRoot, UnrealBuildTool.UnrealTargetPlatform HostPlatform)
		{
			switch(HostPlatform)
			{
				case UnrealBuildTool.UnrealTargetPlatform.Mac:
					return CommandUtils.CombinePaths(BuildRoot, "Engine/Binaries/Mac/UE4Editor.app/Contents/MacOS/UE4Editor");
				case UnrealBuildTool.UnrealTargetPlatform.Win64:
					return CommandUtils.CombinePaths(BuildRoot, "Engine/Binaries/Win64/UE4Editor-Cmd.exe");
				case UnrealBuildTool.UnrealTargetPlatform.Linux:
					return CommandUtils.CombinePaths(BuildRoot, "Engine/Binaries/Linux/UE4Editor");
				default:
					throw new AutomationException("EditorCommandlet is not supported for platform {0}", HostPlatform);
			}
		}

		/// <summary>
		/// Combines a list of parameters into a single commandline param separated with '+':
		/// For example: Map1+Map2+Map3
		/// </summary>
		/// <param name="ParamValues">List of parameters (must not be empty)</param>
		/// <returns>Combined param</returns>
		public static string CombineCommandletParams(string[] ParamValues, string Separator = "+")
		{
			if (!IsNullOrEmpty(ParamValues))
			{
				var CombinedParams = ParamValues[0];
				for (int Index = 1; Index < ParamValues.Length; ++Index)
				{
                    CombinedParams += Separator + ParamValues[Index];
				}
				return CombinedParams;
			}
			else
			{
				return String.Empty;
			}
		}

		#endregion
	}
}
