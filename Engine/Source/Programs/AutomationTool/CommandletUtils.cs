// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;


namespace AutomationTool
{
	public partial class CommandUtils
	{
		#region Commandlets

		/// <summary>
		/// Runs Cook commandlet.
		/// </summary>
		/// <param name="ProjectName">Project name.</param>
		/// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
		/// <param name="Maps">List of maps to cook, can be null in which case -MapIniSection=AllMaps is used.</param>
		/// <param name="Dirs">List of directories to cook, can be null</param>
		/// <param name="TargetPlatform">Target platform.</param>
		/// <param name="Parameters">List of additional parameters.</param>
		public static void CookCommandlet(string ProjectName, string UE4Exe = "UE4Editor-Cmd.exe", string[] Maps = null, string[] Dirs = null, string InternationalizationPreset = "", string[] Cultures = null, string TargetPlatform = "WindowsNoEditor", string Parameters = "-Unversioned")
		{
			string MapsToCook = "";
			if (IsNullOrEmpty(Maps))
			{
				// MapsToCook = "-MapIniSection=AllMaps";
			}
			else
			{
				MapsToCook = "-Map=" + CombineCommandletParams(Maps);
				MapsToCook.Trim();
			}

			string DirsToCook = "";
			if (!IsNullOrEmpty(Dirs))
			{
				DirsToCook = "-CookDir=" + CombineCommandletParams(Dirs);
				DirsToCook.Trim();		
			}

            string CulturesToCook = "";
            if (!IsNullOrEmpty(Cultures))
            {
                CulturesToCook = "-CookCultures=" + CombineCommandletParams(Cultures);
                CulturesToCook.Trim();
            }

            RunCommandlet(ProjectName, UE4Exe, "Cook", String.Format("{0} {1} -I18NPreset={2} {3} -TargetPlatform={4} {5}", MapsToCook, DirsToCook, InternationalizationPreset, CulturesToCook, TargetPlatform, Parameters));
		}

        /// <summary>
        /// Runs DDC commandlet.
        /// </summary>
        /// <param name="ProjectName">Project name.</param>
        /// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
        /// <param name="Maps">List of maps to cook, can be null in which case -MapIniSection=AllMaps is used.</param>
        /// <param name="TargetPlatform">Target platform.</param>
        /// <param name="Parameters">List of additional parameters.</param>
        public static void DDCCommandlet(string ProjectName, string UE4Exe = "UE4Editor-Cmd.exe", string[] Maps = null, string TargetPlatform = "WindowsNoEditor", string Parameters = "")
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
        /// Runs GenerateDistillFileSets commandlet.
        /// </summary>
        /// <param name="ProjectName">Project name.</param>
        /// <param name="UE4Exe">The name of the UE4 Editor executable to use.</param>
        /// <param name="Maps">List of maps to cook, can be null in which case -MapIniSection=AllMaps is used.</param>
        /// <param name="TargetPlatform">Target platform.</param>
        /// <param name="Parameters">List of additional parameters.</param>
        public static List<string> GenerateDistillFileSetsCommandlet(string ProjectName, string ManifestFile, string UE4Exe = "UE4Editor-Cmd.exe", string[] Maps = null, string Parameters = "")
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
        public static void UpdateGameProjectCommandlet(string ProjectName, string UE4Exe = "UE4Editor-Cmd.exe", string Parameters = "")
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
		public static void RunCommandlet(string ProjectName, string UE4Exe, string Commandlet, string Parameters = null)
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

			string LocalLogFile = LogUtils.GetUniqueLogName(CombinePaths(CmdEnv.EngineSavedFolder, Commandlet));
			Log("Commandlet log file is {0}", LocalLogFile);
			string Args = String.Format(
				"{0} -run={1} {2} -abslog={3} -stdout -FORCELOGFLUSH -CrashForUAT -unattended -AllowStdOutLogVerbosity {4}",
				(ProjectName == null) ? "" : CommandUtils.MakePathSafeToUseWithCommandLine(ProjectName),
				Commandlet,
				String.IsNullOrEmpty(Parameters) ? "" : Parameters,
				CommandUtils.MakePathSafeToUseWithCommandLine(LocalLogFile),
				IsBuildMachine ? "-buildmachine" : ""
			);
			ERunOptions Opts = ERunOptions.Default;
			if (GlobalCommandLine.UTF8Output)
			{
				Args += " -UTF8Output";
				Opts |= ERunOptions.UTF8Output;
			}
			var RunResult = Run(EditorExe, Args, Options: Opts);
			PopDir();

			// Copy the local commandlet log to the destination folder.
			string DestLogFile = LogUtils.GetUniqueLogName(CombinePaths(CmdEnv.LogFolder, Commandlet));
			if (!CommandUtils.CopyFile_NoExceptions(LocalLogFile, DestLogFile))
			{
				CommandUtils.LogWarning("Commandlet {0} failed to copy the local log file from {1} to {2}. The log file will be lost.", Commandlet, LocalLogFile, DestLogFile);
			}

			// Whether it was copied correctly or not, delete the local log as it was only a temporary file. 
			CommandUtils.DeleteFile_NoExceptions(LocalLogFile);

			if (RunResult.ExitCode != 0)
			{
				throw new AutomationException("BUILD FAILED: Failed while running {0} for {1}; see log {2}", Commandlet, ProjectName, DestLogFile);
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
					throw new NotImplementedException();
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
