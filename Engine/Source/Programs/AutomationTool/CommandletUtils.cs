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
				MapsToCook = "-MapIniSection=AllMaps";
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

			string LogFile = LogUtils.GetUniqueLogName(CombinePaths(CmdEnv.LogFolder, Commandlet));
			Log("Commandlet log file is {0}", LogFile);
			var RunResult = Run(EditorExe, String.Format("{0} -run={1} {2} -abslog={3} -stdout -FORCELOGFLUSH -CrashForUAT -unattended -AllowStdOutLogVerbosity {4}", 
                CommandUtils.MakePathSafeToUseWithCommandLine(ProjectName), 
                Commandlet, 
				String.IsNullOrEmpty(Parameters) ? "" : Parameters, 
				CommandUtils.MakePathSafeToUseWithCommandLine(LogFile),
                IsBuildMachine ? "-buildmachine" : ""
                ));
			PopDir();

			if (RunResult.ExitCode != 0)
			{
				throw new AutomationException("BUILD FAILED: Failed while running {0} for {1}; see log {2}", Commandlet, ProjectName, LogFile);
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
